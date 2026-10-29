#ifndef QUIVER_FILE_H
#define QUIVER_FILE_H

#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>

namespace quiver::utils {

// The whole file, read in binary mode (as toml++'s parse_file reads it); nullopt when it cannot be
// opened or the read fails, so a half-read file is never mistaken for a complete one.
inline std::optional<std::string> read_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return std::nullopt;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    if (file.bad()) {
        return std::nullopt;
    }
    return buffer.str();
}

}  // namespace quiver::utils

#endif  // QUIVER_FILE_H
