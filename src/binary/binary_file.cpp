#include "quiver/binary/binary_file.h"

#include "binary_utils.h"
#include "quiver/binary/dimension.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <unordered_set>

namespace quiver {

static std::unordered_set<std::string> write_registry;

struct BinaryFile::Impl {
    std::unique_ptr<std::iostream> io;
    std::string file_path;
    std::string registered_path;  // non-empty only for writers; used to unregister on destruction
    BinaryMetadata metadata;
    int64_t current_position = -1;  // tracked file position to skip redundant seeks

    Impl(std::unique_ptr<std::iostream> io, std::string file_path, BinaryMetadata metadata)
        : io(std::move(io)), file_path(std::move(file_path)), metadata(std::move(metadata)) {}

    Impl(Impl&&) noexcept = default;
    Impl& operator=(Impl&&) noexcept = default;

    // Cleanup lives here, not in ~BinaryFile(), because unique_ptr::operator= destroys the
    // old Impl via ~Impl() without calling ~BinaryFile(). Putting it here ensures move-assignment
    // correctly flushes and unregisters the replaced writer.
    ~Impl() {
        if (!registered_path.empty()) {
            write_registry.erase(registered_path);
        }
        if (io) {
            io->flush();
        }
    }
};

BinaryFile::BinaryFile(const std::string& file_path, const BinaryMetadata& metadata, std::unique_ptr<std::iostream> io)
    : impl_(std::make_unique<Impl>(std::move(io), file_path, metadata)) {}

BinaryFile::~BinaryFile() = default;
BinaryFile::BinaryFile(BinaryFile&& other) noexcept = default;
BinaryFile& BinaryFile::operator=(BinaryFile&& other) noexcept = default;

BinaryFile
BinaryFile::open_file(const std::string& file_path, char mode, const std::optional<BinaryMetadata>& metadata) {
    namespace fs = std::filesystem;
    auto canonical = fs::weakly_canonical(file_path).string();

    if (write_registry.count(canonical)) {
        throw std::runtime_error("Cannot open_file: file is already open for writing: " + canonical);
    }

    switch (mode) {
    case 'r': {
        // Validate file exists
        if (!fs::exists(file_path + std::string(QVR_EXTENSION)) ||
            !fs::exists(file_path + std::string(TOML_EXTENSION))) {
            throw std::invalid_argument("File not found: " + file_path);
        }

        // Read TOML metadata
        std::ifstream toml_file(file_path + std::string(TOML_EXTENSION));
        std::string toml_content((std::istreambuf_iterator<char>(toml_file)), std::istreambuf_iterator<char>());
        auto metadata_from_toml = BinaryMetadata::from_toml(toml_content);

        // Open binary data file
        auto io =
            std::make_unique<std::fstream>(file_path + std::string(QVR_EXTENSION), std::ios::in | std::ios::binary);
        return BinaryFile(file_path, metadata_from_toml, std::move(io));
    }
    case 'w': {
        // Validate metadata provided
        if (!metadata.has_value()) {
            throw std::invalid_argument("Metadata must be provided when opening a file in write mode.");
        }

        // Write metadata to TOML file
        std::ofstream toml_file(file_path + std::string(TOML_EXTENSION));
        toml_file << metadata->to_toml();

        // Open binary data file
        auto io =
            std::make_unique<std::fstream>(file_path + std::string(QVR_EXTENSION), std::ios::out | std::ios::binary);
        BinaryFile binary_file(file_path, metadata.value(), std::move(io));
        binary_file.fill_file_with_nulls();  // can throw on disk-full

        write_registry.insert(canonical);
        binary_file.impl_->registered_path = canonical;
        return binary_file;
    }
    default:
        throw std::invalid_argument("Invalid file mode: " + std::string(1, mode) +
                                    ". Use 'r' for read or 'w' for write.");
    }
}

std::vector<double> BinaryFile::read(const std::unordered_map<std::string, int64_t>& dims, bool allow_nulls) {
    validate_dimension_values(dims);

    go_to_position(calculate_file_position(dims), 'r');

    std::vector<double> data(impl_->metadata.labels.size());
    auto bytes = static_cast<int64_t>(data.size() * sizeof(double));
    impl_->io->read(reinterpret_cast<char*>(data.data()), bytes);
    impl_->current_position += bytes;

    if (!allow_nulls) {
        for (size_t i = 0; i < data.size(); ++i) {
            if (std::isnan(data[i])) {
                std::string dim_str;
                for (const auto& [name, value] : dims) {
                    if (!dim_str.empty())
                        dim_str += ", ";
                    dim_str += name + "=" + std::to_string(value);
                }
                throw std::runtime_error("Cannot read: data at {" + dim_str + "} contains null values");
            }
        }
    }

    return data;
}

void BinaryFile::write(const std::vector<double>& data, const std::unordered_map<std::string, int64_t>& dims) {
    validate_dimension_values(dims);
    validate_data_length(data);

    go_to_position(calculate_file_position(dims), 'w');

    auto bytes = static_cast<int64_t>(data.size() * sizeof(double));
    impl_->io->write(reinterpret_cast<const char*>(data.data()), bytes);
    impl_->current_position += bytes;
}

int64_t BinaryFile::calculate_file_position(const std::unordered_map<std::string, int64_t>& dims) const {
    const auto& metadata = impl_->metadata;
    const auto& dimensions = metadata.dimensions;
    int64_t position = 0;
    for (size_t i = 0; i < dimensions.size(); ++i) {
        int64_t stride = 1;
        for (size_t j = i + 1; j < dimensions.size(); ++j) {
            stride *= dimensions[j].size;
        }
        position += (dims.at(dimensions[i].name) - 1) * stride;
    }
    position *= static_cast<int64_t>(metadata.labels.size());
    position *= static_cast<int64_t>(sizeof(double));
    return position;
}

void BinaryFile::go_to_position(int64_t position, char mode) {
    if (impl_->current_position == position) {
        return;
    }
    switch (mode) {
    case 'r':
        impl_->io->seekg(position);
        break;
    case 'w':
        impl_->io->seekp(position);
        break;
    default:
        throw std::invalid_argument("Invalid seek mode: " + std::string(1, mode) +
                                    ". Use 'r' for read or 'w' for write.");
    }
    impl_->current_position = position;
}

void BinaryFile::validate_file_is_open() const {
    if (!impl_->io || !impl_->io->good()) {
        throw std::runtime_error("File is not open: " + impl_->file_path);
    }
}

void BinaryFile::validate_dimension_values(const std::unordered_map<std::string, int64_t>& dims) {
    const auto& metadata = impl_->metadata;
    const auto& dimensions = metadata.dimensions;

    // Check count
    if (dims.size() != dimensions.size()) {
        throw std::invalid_argument("Expected " + std::to_string(dimensions.size()) + " dimensions, got " +
                                    std::to_string(dims.size()));
    }

    // Check all dimension names exist and values are in bounds
    for (size_t i = 0; i < dimensions.size(); ++i) {
        const auto& dim = dimensions[i];
        auto it = dims.find(dim.name);
        if (it == dims.end()) {
            throw std::invalid_argument("Missing required dimension: '" + dim.name + "'");
        }
        int64_t value = it->second;
        if (value < 1 || value > dim.size) {
            throw std::invalid_argument("Dimension '" + dim.name + "' value " + std::to_string(value) +
                                        " is out of bounds [1, " + std::to_string(dim.size) + "]");
        }
    }

    if (metadata.number_of_time_dimensions > 1) {
        // Build the datetime by accumulating offsets from each time dimension
        auto datetime = metadata.initial_datetime;
        for (const auto& dim : dimensions) {
            if (dim.is_time_dimension()) {
                datetime = dim.time->add_offset_from_int(datetime, dims.at(dim.name));
            }
        }

        // Verify that inner time dimensions are consistent with the resulting date
        bool first = true;
        for (const auto& dim : dimensions) {
            if (!dim.is_time_dimension())
                continue;
            if (first) {
                first = false;
                continue;
            }  // skip outermost time dimension

            int64_t expected_value = dims.at(dim.name);
            int64_t resulting_value = dim.time->datetime_to_int(datetime);
            if (expected_value != resulting_value) {
                throw std::invalid_argument("Invalid values for time dimensions: dimension '" + dim.name +
                                            "' has value " + std::to_string(expected_value) +
                                            " but the resulting datetime implies " + std::to_string(resulting_value));
            }
        }
    }
}

void BinaryFile::validate_data_length(const std::vector<double>& data) {
    if (data.size() != impl_->metadata.labels.size()) {
        throw std::invalid_argument("Data length " + std::to_string(data.size()) + " does not match expected length " +
                                    std::to_string(impl_->metadata.labels.size()));
    }
}

void BinaryFile::fill_file_with_nulls() {
    const auto& metadata = impl_->metadata;

    // Calculate total number of cells
    int64_t total_cells = 1;
    for (const auto& dim : metadata.dimensions) {
        total_cells *= dim.size;
    }

    int64_t total_doubles = total_cells * static_cast<int64_t>(metadata.labels.size());

    // Write NaN-filled buffer in chunks to avoid excessive memory usage
    constexpr int64_t CHUNK_DOUBLES = 1024 * 1024;  // ~8 MB per chunk
    std::vector<double> buffer(static_cast<size_t>(std::min(total_doubles, CHUNK_DOUBLES)),
                               std::numeric_limits<double>::quiet_NaN());

    impl_->io->seekp(0);
    impl_->current_position = 0;
    int64_t remaining = total_doubles;
    while (remaining > 0) {
        int64_t count = std::min(remaining, static_cast<int64_t>(buffer.size()));
        auto bytes = count * static_cast<int64_t>(sizeof(double));
        impl_->io->write(reinterpret_cast<const char*>(buffer.data()), bytes);
        impl_->current_position += bytes;
        remaining -= count;
    }
    impl_->io->flush();
}

const BinaryMetadata& BinaryFile::get_metadata() const {
    return impl_->metadata;
}

const std::string& BinaryFile::get_file_path() const {
    return impl_->file_path;
}

bool BinaryFile::is_read_mode() const {
    return impl_->registered_path.empty();
}

std::iostream& BinaryFile::get_io() {
    return *impl_->io;
}

}  // namespace quiver