#include "quiver/binary/binary_comparator.h"

#include "quiver/binary/binary.h"

#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

bool is_approx(double a, double b, double atol, double rtol) {
    if (std::isnan(a) && std::isnan(b))
        return true;
    if (std::isnan(a) || std::isnan(b))
        return false;
    return std::abs(a - b) <= atol + rtol * std::abs(b);
}

}  // namespace

namespace quiver {

struct BinaryComparator::Impl {
    Binary binary1;
    Binary binary2;
    CompareOptions options;
};

BinaryComparator::BinaryComparator(Binary binary1, Binary binary2, const CompareOptions& options)
    : impl_(std::make_unique<Impl>(Impl{std::move(binary1), std::move(binary2), options})) {}

BinaryComparator::~BinaryComparator() = default;

BinaryComparator::BinaryComparator(BinaryComparator&& other) noexcept = default;
BinaryComparator& BinaryComparator::operator=(BinaryComparator&& other) noexcept = default;

CompareResult
BinaryComparator::compare(const std::string& file_path1, const std::string& file_path2, const CompareOptions& options) {
    auto binary1 = Binary::open_file(file_path1, 'r');
    auto binary2 = Binary::open_file(file_path2, 'r');
    BinaryComparator comparator(std::move(binary1), std::move(binary2), options);
    return comparator.run();
}

CompareResult BinaryComparator::run() {
    auto& binary1 = impl_->binary1;
    auto& binary2 = impl_->binary2;
    const auto& metadata1 = binary1.get_metadata();
    const auto& metadata2 = binary2.get_metadata();

    std::string report;

    if (metadata1 != metadata2) {
        if (impl_->options.detailed_report) {
            report = "Metadata mismatch between files '" + binary1.get_file_path() + "' and '" +
                     binary2.get_file_path() + "'.\n";
        }
        return {CompareStatus::MetadataMismatch, report};
    }

    const auto& metadata = metadata1;
    const auto& dimensions = metadata.dimensions;

    // Get initial dimension values
    std::vector<int64_t> initial_dimensions;
    for (const auto& dim : dimensions) {
        if (dim.is_time_dimension()) {
            initial_dimensions.push_back(dim.time->initial_value);
        } else {
            initial_dimensions.push_back(1);
        }
    }

    // Calculate maximum number of lines
    int64_t max_lines = 1;
    for (const auto& dim : dimensions) {
        max_lines *= dim.size;
    }

    CompareStatus status = CompareStatus::FileMatch;
    int report_lines = 0;

    auto current_dims = initial_dimensions;
    for (int64_t i = 0; i < max_lines; ++i) {
        std::unordered_map<std::string, int64_t> dims;
        for (size_t j = 0; j < dimensions.size(); ++j) {
            dims[dimensions[j].name] = current_dims[j];
        }

        auto data1 = binary1.read(dims, true);
        auto data2 = binary2.read(dims, true);

        for (size_t k = 0; k < data1.size(); ++k) {
            if (!is_approx(data1[k], data2[k], impl_->options.absolute_tolerance, impl_->options.relative_tolerance)) {
                status = CompareStatus::DataMismatch;

                if (!impl_->options.detailed_report) {
                    // Early exit — no report needed
                    return {status, report};
                }

                // Initialize report header on first mismatch
                if (report.empty()) {
                    report = "Data mismatch between '" + binary1.get_file_path() + "' and '" + binary2.get_file_path() +
                             "':\n";
                }

                // Build dimension string
                std::string dim_str;
                for (size_t j = 0; j < dimensions.size(); ++j) {
                    if (!dim_str.empty())
                        dim_str += ", ";
                    dim_str += dimensions[j].name + "=" + std::to_string(current_dims[j]);
                }

                // Build data string
                std::string data_str = "'" + metadata.labels[k] + "': ";
                if (std::isnan(data1[k])) {
                    data_str += "NaN";
                } else {
                    data_str += std::to_string(data1[k]);
                }
                data_str += " vs ";
                if (std::isnan(data2[k])) {
                    data_str += "NaN";
                } else {
                    data_str += std::to_string(data2[k]);
                }

                report += "  {" + dim_str + "}, " + data_str + "\n";
                report_lines++;
                if (report_lines >= impl_->options.max_report_lines) {
                    report += "  more ...\n";
                    return {status, report};
                }
            }
        }

        current_dims = binary1.next_dimensions(current_dims);
        if (current_dims == initial_dimensions)
            break;
    }

    if (impl_->options.detailed_report && status == CompareStatus::FileMatch) {
        report = "Files '" + binary1.get_file_path() + "' and '" + binary2.get_file_path() + "' match.";
    }

    return {status, report};
}

}  // namespace quiver
