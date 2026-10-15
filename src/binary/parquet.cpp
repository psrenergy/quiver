#include "quiver/binary/parquet.h"

#include "binary_utils.h"
#include "quiver/binary/binary_file.h"
#include "quiver/binary/iteration.h"

#include <arrow/api.h>
#include <arrow/io/api.h>
#include <parquet/arrow/writer.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <random>
#include <stdexcept>
#include <system_error>
#include <unordered_set>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace quiver {
namespace {

namespace fs = std::filesystem;

void check(const arrow::Status& status) {
    if (!status.ok()) {
        throw std::runtime_error(status.ToString());
    }
}

template <typename T>
T take(arrow::Result<T> result) {
    check(result.status());
    return std::move(result).ValueOrDie();
}

int64_t multiply(int64_t left, int64_t right) {
    if (right > std::numeric_limits<int64_t>::max() / left) {
        throw std::invalid_argument("binary dimensions exceed the supported file size");
    }
    return left * right;
}

// Creating a directory exclusively also keeps the temporary file away from pre-existing symlinks.
struct TemporaryExport {
    fs::path directory;

    explicit TemporaryExport(const fs::path& destination) {
        std::random_device random;
        for (int attempt = 0; attempt < 16; ++attempt) {
            const auto candidate =
                destination.parent_path() / (destination.filename().string() + ".tmp-" + std::to_string(random()));
            if (fs::create_directory(candidate)) {
                directory = candidate;
                return;
            }
        }
        throw std::runtime_error("failed to create a temporary export directory");
    }

    TemporaryExport(const TemporaryExport&) = delete;
    TemporaryExport& operator=(const TemporaryExport&) = delete;

    ~TemporaryExport() {
        std::error_code ignored;
        fs::remove_all(directory, ignored);
    }

    fs::path file() const {
        return directory / "snapshot.parquet";
    }
};

void publish(const fs::path& source, const fs::path& destination) {
#ifdef _WIN32
    if (!MoveFileExW(source.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "replace export");
    }
#else
    fs::rename(source, destination);
#endif
}

std::shared_ptr<arrow::Array> finish(arrow::ArrayBuilder& builder) {
    std::shared_ptr<arrow::Array> array;
    check(builder.Finish(&array));
    return array;
}

void export_file(const std::string& path) {
    auto reader = BinaryFile::open_file(path, 'r');
    const auto& metadata = reader.get_metadata();
    const bool has_time = metadata.number_of_time_dimensions() > 0;
    int64_t expected_bytes = multiply(static_cast<int64_t>(metadata.labels.size()), sizeof(double));
    std::unordered_set<std::string> names;
    auto add_name = [&](const std::string& name) {
        if (!names.insert(name).second) {
            throw std::invalid_argument("duplicate output column name: '" + name + "'");
        }
    };
    for (const auto& dimension : metadata.dimensions) {
        expected_bytes = multiply(expected_bytes, dimension.size);
        add_name(dimension.name);
    }
    if (has_time) {
        add_name("datetime");
    }
    for (const auto& label : metadata.labels) {
        add_name(label);
    }
    if (fs::file_size(path + ".qvr") != static_cast<uintmax_t>(expected_bytes)) {
        throw std::invalid_argument("binary data size does not match its metadata");
    }

    const auto width = static_cast<int64_t>(names.size());
    const auto bytes_per_row = multiply(width, 8) + static_cast<int64_t>((metadata.labels.size() + 7) / 8);
    const int64_t batch_rows = std::clamp<int64_t>((32 * 1024 * 1024) / bytes_per_row, 1, 65536);
    std::vector<std::shared_ptr<arrow::Field>> fields;
    std::vector<std::unique_ptr<arrow::Int64Builder>> dimensions;
    std::vector<std::unique_ptr<arrow::DoubleBuilder>> agents;
    for (const auto& dimension : metadata.dimensions) {
        fields.push_back(arrow::field(dimension.name, arrow::int64(), false));
        dimensions.push_back(std::make_unique<arrow::Int64Builder>());
    }
    std::unique_ptr<arrow::TimestampBuilder> datetime;
    if (has_time) {
        const auto type = arrow::timestamp(arrow::TimeUnit::MICRO, "UTC");
        fields.push_back(arrow::field("datetime", type, false));
        datetime = std::make_unique<arrow::TimestampBuilder>(type, arrow::default_memory_pool());
    }
    parquet::WriterProperties::Builder properties;
    properties.compression(parquet::Compression::ZSTD)->compression_level(3)->enable_statistics();
    for (const auto& label : metadata.labels) {
        fields.push_back(arrow::field(label, arrow::float64(), true));
        agents.push_back(std::make_unique<arrow::DoubleBuilder>());
        properties.disable_dictionary(label);
    }
    const auto schema = arrow::schema(
        fields,
        arrow::key_value_metadata({"quiver:metadata", "quiver:parquet_version"}, {metadata.to_toml(), "1"})
    );
    TemporaryExport temporary(fs::path(path + ".parquet"));
    auto output = take(arrow::io::FileOutputStream::Open(temporary.file().string()));
    parquet::ArrowWriterProperties::Builder arrow_properties;
    arrow_properties.store_schema()->set_use_threads(false);
    auto writer = take(
        parquet::arrow::FileWriter::Open(
            *schema,
            arrow::default_memory_pool(),
            output,
            properties.build(),
            arrow_properties.build()
        )
    );

    auto coordinates = first_dimensions(metadata);
    std::unordered_map<std::string, int64_t> dims;
    for (const auto& dimension : metadata.dimensions) {
        dims.emplace(dimension.name, 1);
    }
    int64_t rows = 0;
    for (;;) {
        if (rows == 0) {
            for (auto& builder : dimensions) {
                check(builder->Reserve(batch_rows));
            }
            if (datetime) {
                check(datetime->Reserve(batch_rows));
            }
            for (auto& builder : agents) {
                check(builder->Reserve(batch_rows));
            }
        }
        for (size_t i = 0; i < dimensions.size(); ++i) {
            dims.at(metadata.dimensions[i].name) = coordinates[i];
            check(dimensions[i]->Append(coordinates[i]));
        }
        if (datetime) {
            const auto start = calendar_cell_start(metadata, coordinates);
            check(datetime->Append(
                std::chrono::duration_cast<std::chrono::microseconds>(start.time_since_epoch()).count()
            ));
        }
        const auto values = reader.read(dims, true);
        for (size_t i = 0; i < agents.size(); ++i) {
            check(std::isnan(values[i]) ? agents[i]->AppendNull() : agents[i]->Append(values[i]));
        }
        ++rows;
        auto next = next_dimensions(metadata, coordinates);
        if (rows == batch_rows || !next) {
            std::vector<std::shared_ptr<arrow::Array>> columns;
            for (auto& builder : dimensions) {
                columns.push_back(finish(*builder));
            }
            if (datetime) {
                columns.push_back(finish(*datetime));
            }
            for (auto& builder : agents) {
                columns.push_back(finish(*builder));
            }
            check(writer->WriteTable(*arrow::Table::Make(schema, columns), rows));
            rows = 0;
        }
        if (!next) {
            break;
        }
        coordinates = std::move(*next);
    }
    check(writer->Close());
    check(output->Close());
    publish(temporary.file(), fs::path(path + ".parquet"));
}

}  // namespace

void bin_to_parquet(const std::string& path) {
    try {
        export_file(path);
    } catch (const std::exception& e) {
        throw std::runtime_error("Cannot bin_to_parquet: path '" + path + "': " + e.what());
    }
}

}  // namespace quiver
