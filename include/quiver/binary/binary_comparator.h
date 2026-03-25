#ifndef QUIVER_BINARY_COMPARATOR_H
#define QUIVER_BINARY_COMPARATOR_H

#include "../export.h"
#include "binary_file.h"

#include <memory>
#include <string>

namespace quiver {

enum class CompareStatus { FileMatch, MetadataMismatch, DataMismatch };

struct QUIVER_API CompareOptions {
    double absolute_tolerance = 1e-6;
    double relative_tolerance = 0.0;
    bool detailed_report = false;
    int max_report_lines = 100;
};

struct QUIVER_API CompareResult {
    CompareStatus status;
    std::string report;
};

class QUIVER_API BinaryComparator {
public:
    ~BinaryComparator();

    BinaryComparator(const BinaryComparator&) = delete;
    BinaryComparator& operator=(const BinaryComparator&) = delete;
    BinaryComparator(BinaryComparator&&) noexcept;
    BinaryComparator& operator=(BinaryComparator&&) noexcept;

    static CompareResult
    compare(const std::string& file_path1, const std::string& file_path2, const CompareOptions& options = {});

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    BinaryComparator(BinaryFile binary1, BinaryFile binary2, const CompareOptions& options);

    CompareResult run();
};

}  // namespace quiver

#endif  // QUIVER_BINARY_COMPARATOR_H
