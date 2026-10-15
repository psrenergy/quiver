#ifndef QUIVER_SRC_JSON_JSON_READ_H
#define QUIVER_SRC_JSON_JSON_READ_H

#include <nlohmann/json_fwd.hpp>

#include <string>

namespace quiver::json_read {

// Internal, Lua-only reader. The caller resolves the sandbox path before any file access.
nlohmann::json read_file(const std::string& resolved_path, const std::string& original_path);

}  // namespace quiver::json_read

#endif  // QUIVER_SRC_JSON_JSON_READ_H
