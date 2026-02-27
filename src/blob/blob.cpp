#include "quiver/blob/blob.h"

#include "blob_utils.h"
#include "quiver/blob/dimension.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace quiver {

struct Blob::Impl {
    std::unique_ptr<std::iostream> io;
    std::string file_path;
    BlobMetadata metadata;
};

Blob::Blob(const std::string& file_path, const BlobMetadata& metadata, std::unique_ptr<std::iostream> io)
    : impl_(std::make_unique<Impl>(Impl{std::move(io), file_path, metadata})) {}

Blob::~Blob() = default;

Blob::Blob(Blob&& other) noexcept = default;
Blob& Blob::operator=(Blob&& other) noexcept = default;

Blob Blob::open_file(const std::string& file_path, char mode, const std::optional<BlobMetadata>& metadata) {
    namespace fs = std::filesystem;
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
        auto metadata = BlobMetadata::from_toml(toml_content);

        // Open binary data file
        auto io =
            std::make_unique<std::fstream>(file_path + std::string(QVR_EXTENSION), std::ios::in | std::ios::binary);
        return Blob(file_path, metadata, std::move(io));
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
        return Blob(file_path, metadata.value(), std::move(io));
    }
    default:
        throw std::invalid_argument("Invalid file mode: " + std::string(1, mode) +
                                    ". Use 'r' for read or 'w' for write.");
    }
}

std::vector<double> Blob::read(const std::unordered_map<std::string, int64_t>& dims) {
    validate_dimension_values(dims);

    go_to_position(calculate_file_position(dims));

    std::vector<double> data(impl_->metadata.labels.size());
    impl_->io->read(reinterpret_cast<char*>(data.data()), data.size() * sizeof(double));

    return data;
}

void Blob::write(const std::vector<double>& data, const std::unordered_map<std::string, int64_t>& dims) {
    validate_dimension_values(dims);
    validate_data_length(data);

    go_to_position(calculate_file_position(dims));

    impl_->io->write(reinterpret_cast<const char*>(data.data()), data.size() * sizeof(double));
}

int64_t Blob::calculate_file_position(const std::unordered_map<std::string, int64_t>& dims) const {
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

void Blob::go_to_position(int64_t position) {
    if (position < 0) {
        throw std::invalid_argument("File position must be non-negative, got " + std::to_string(position));
    }
    validate_file_is_open();
    // No skip-vs-seek branch: fstream is always seekable and lseek/SetFilePointer
    // cost the same regardless of direction, so absolute seek suffices.
    impl_->io->seekg(position);
}

void Blob::validate_file_is_open() const {
    if (!impl_->io || !impl_->io->good()) {
        throw std::runtime_error("File is not open: " + impl_->file_path);
    }
}

void Blob::validate_dimension_values(const std::unordered_map<std::string, int64_t>& dims) {
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

void Blob::validate_data_length(const std::vector<double>& data) {
    if (data.size() != impl_->metadata.labels.size()) {
        throw std::invalid_argument("Data length " + std::to_string(data.size()) + " does not match expected length " +
                                    std::to_string(impl_->metadata.labels.size()));
    }
}

const BlobMetadata& Blob::get_metadata() const {
    return impl_->metadata;
}

const std::string& Blob::get_file_path() const {
    return impl_->file_path;
}

std::iostream& Blob::get_io() {
    return *impl_->io;
}

}  // namespace quiver