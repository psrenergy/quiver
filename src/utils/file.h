#ifndef QUIVER_FILE_H
#define QUIVER_FILE_H

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <system_error>

namespace quiver::utils {

// The whole file, read in binary mode (as toml++'s parse_file reads it); nullopt when it is not a
// regular file, cannot be opened or the read fails. The read asks for the file's full size, so a
// short read (a read error, a byte-range lock) fails it and is never mistaken for EOF.
inline std::optional<std::string> read_file(const std::filesystem::path& path) {
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec)) {
        return std::nullopt;
    }
    const auto size = std::filesystem::file_size(path, ec);
    std::ifstream file(path, std::ios::binary);
    std::string content(ec ? 0 : static_cast<size_t>(size), '\0');
    if (ec || !file.read(content.data(), static_cast<std::streamsize>(content.size()))) {
        return std::nullopt;
    }
    return content;
}

}  // namespace quiver::utils

#endif  // QUIVER_FILE_H
