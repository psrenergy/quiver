#ifndef QUIVER_BINARY_ITERATION_H
#define QUIVER_BINARY_ITERATION_H

#include "../export.h"
#include "binary_metadata.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace quiver {

QUIVER_API std::vector<int64_t> first_dimensions(const BinaryMetadata& meta);

QUIVER_API std::optional<std::vector<int64_t>> next_dimensions(
    const BinaryMetadata& meta,
    const std::vector<int64_t>& current
);

QUIVER_API std::vector<int64_t> dimension_sizes_at_values(
    const BinaryMetadata& meta,
    const std::vector<int64_t>& dimension_values
);

QUIVER_API int64_t dimension_start_at_values(
    const BinaryMetadata& meta,
    const std::vector<int64_t>& dimension_values,
    size_t index
);

}  // namespace quiver

#endif  // QUIVER_BINARY_ITERATION_H
