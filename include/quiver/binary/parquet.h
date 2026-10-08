#ifndef QUIVER_BINARY_PARQUET_H
#define QUIVER_BINARY_PARQUET_H

#include "../export.h"

#include <string>

namespace quiver {

// Export path.qvr + path.toml to path.parquet. The binary writer must be closed.
// Replaces an existing snapshot only after the new file has been completed.
QUIVER_API void bin_to_parquet(const std::string& path);

}  // namespace quiver

#endif  // QUIVER_BINARY_PARQUET_H
