#include "json/json_read.h"

#include "utils/string.h"

#include <nlohmann/json.hpp>

#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace quiver::json_read {

namespace {

constexpr std::size_t kMaxInputBytes = 64UL * 1024UL * 1024UL;
constexpr int kMaxDepth = 32;

}  // namespace

nlohmann::json read_file(const std::string& resolved_path, const std::string& original_path) {
    const std::string prefix = "Cannot read_json: ";
    try {
        if (!std::filesystem::exists(resolved_path)) {
            throw std::runtime_error(prefix + "file not found: " + original_path);
        }
        if (std::filesystem::is_directory(resolved_path)) {
            throw std::runtime_error(prefix + "path is a directory: " + original_path);
        }
    } catch (const std::filesystem::filesystem_error& e) {
        throw std::runtime_error(prefix + "cannot read file '" + original_path + "': " + e.code().message());
    }

    std::ifstream file(resolved_path, std::ios::binary);
    if (!file) {
        throw std::runtime_error(prefix + "cannot read file '" + original_path + "'");
    }

    std::string content;
    std::array<char, 64 * 1024> buffer;
    while (file) {
        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto count = static_cast<std::size_t>(file.gcount());
        if (count > kMaxInputBytes - content.size()) {
            throw std::runtime_error(
                prefix + "file '" + original_path + "' exceeds " + std::to_string(kMaxInputBytes) + " bytes of JSON"
            );
        }
        content.append(buffer.data(), count);
    }
    if (!file.eof() || file.bad()) {
        throw std::runtime_error(prefix + "cannot read file '" + original_path + "'");
    }

    // Raw NUL is invalid JSON, including after an otherwise complete document.
    if (const auto nul = content.find('\0'); nul != std::string::npos) {
        throw std::runtime_error(
            prefix + "cannot parse file '" + original_path + "': unexpected NUL at byte " + std::to_string(nul + 1)
        );
    }

    try {
        return nlohmann::json::parse(content, [&](int depth, nlohmann::json::parse_event_t, nlohmann::json&) {
            if (depth > kMaxDepth) {
                throw std::runtime_error(
                    prefix + "file '" + original_path + "' nests deeper than " + std::to_string(kMaxDepth) + " levels"
                );
            }
            return true;
        });
    } catch (const nlohmann::json::parse_error& e) {
        // The parser's last-token diagnostic may contain the invalid UTF-8 bytes it rejected.
        std::string reason = e.what();
        if (!quiver::string::is_valid_utf8(reason)) {
            reason = "invalid JSON at byte " + std::to_string(e.byte);
        }
        throw std::runtime_error(prefix + "cannot parse file '" + original_path + "': " + reason);
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error(prefix + "cannot parse file '" + original_path + "': " + e.what());
    }
}

}  // namespace quiver::json_read
