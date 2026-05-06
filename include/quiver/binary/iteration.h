#ifndef QUIVER_BINARY_ITERATION_H
#define QUIVER_BINARY_ITERATION_H

#include "../export.h"
#include "binary_metadata.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace quiver::binary {

// Returns the per-dimension initial position vector for `meta`.
// Non-time dims start at 1; time dims start at TimeProperties::initial_value.
// Pre-condition: meta.validate() would not throw (caller obeys contract).
QUIVER_API std::vector<int64_t> first_dimensions(const BinaryMetadata& meta);

// Advances `current` to the next position in column-major order, with
// calendar-aware reset adjustment for inner time dimensions whose parent
// dimension advances. Returns std::nullopt when iteration is complete
// (i.e., the next position would equal first_dimensions(meta)).
// Pre-condition: `current` is in the valid range produced by first_dimensions
// and successive next_dimensions calls. No validation is performed.
QUIVER_API std::optional<std::vector<int64_t>> next_dimensions(const BinaryMetadata& meta,
                                                               const std::vector<int64_t>& current);

}  // namespace quiver::binary

#endif  // QUIVER_BINARY_ITERATION_H
