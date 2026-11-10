#ifndef QUIVER_FILE_H
#define QUIVER_FILE_H

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <system_error>

namespace quiver::utils {

// Match names as NTFS/APFS do, including Win32 aliases for streams and trailing dots/spaces.
inline std::string filesystem_name(std::string name) {
    name.resize(std::min(name.find(':'), name.size()));
    while (!name.empty() && (name.back() == '.' || name.back() == ' ')) {
        name.pop_back();
    }
    std::ranges::transform(name, name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return name;
}

// An absolute path as NTFS/APFS compare it, every component followed by '/', so a directory's key
// prefixes its children's. The root is only case-folded: filesystem_name would cut a drive ("C:") to
// nothing. Normalized first (Win32 folds .. before following a symlink), since "." and ".." fold to "".
inline std::string filesystem_key(const std::filesystem::path& path) {
    const auto normal = path.lexically_normal();
    auto key = normal.root_path().string();
    std::ranges::transform(key, key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    for (const auto& part : normal.relative_path()) {
        key += filesystem_name(part.string()) + '/';
    }
    return key;
}

// True when writing absolute `path` would write `database` or one of its -journal/-wal/-shm files,
// or create a directory under one of those names (SQLite could then never open that journal).
inline bool writes_database_file(const std::filesystem::path& path, const std::filesystem::path& database) {
    const auto directory = filesystem_key(database.parent_path());
    const auto key = filesystem_key(path);
    if (key.size() <= directory.size() || !key.starts_with(directory)) {
        return false;
    }
    const auto name = key.substr(directory.size(), key.find('/', directory.size()) - directory.size());
    const auto base = filesystem_name(database.filename().string());
    return name == base || name == base + "-journal" || name == base + "-wal" || name == base + "-shm";
}

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
