#ifndef QUIVER_STRING_H
#define QUIVER_STRING_H

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>

namespace quiver::string {

// Zero means invalid UTF-8. The lead-byte bounds also reject overlong encodings,
// UTF-16 surrogates and code points above U+10FFFF.
inline std::size_t utf8_sequence_length(std::string_view str, std::size_t offset) {
    const auto ch = static_cast<unsigned char>(str[offset]);
    if (ch < 0x80) {
        return 1;
    }
    const std::size_t extra = ch >= 0xF0 ? 3 : (ch >= 0xE0 ? 2 : (ch >= 0xC2 ? 1 : 0));
    if (extra == 0 || ch > 0xF4 || extra >= str.size() - offset) {
        return 0;
    }
    const unsigned char lo = ch == 0xE0 ? 0xA0 : (ch == 0xF0 ? 0x90 : 0x80);
    const unsigned char hi = ch == 0xED ? 0x9F : (ch == 0xF4 ? 0x8F : 0xBF);
    for (std::size_t k = 1; k <= extra; ++k) {
        const auto next = static_cast<unsigned char>(str[offset + k]);
        if (k == 1 ? (next < lo || next > hi) : (next < 0x80 || next > 0xBF)) {
            return 0;
        }
    }
    return extra + 1;
}

inline bool is_valid_utf8(std::string_view str) {
    for (std::size_t i = 0; i < str.size();) {
        const auto length = utf8_sequence_length(str, i);
        if (length == 0) {
            return false;
        }
        i += length;
    }
    return true;
}

inline std::string trim(const std::string& str) {
    auto start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) {
        return {};
    }
    return str.substr(start, str.find_last_not_of(" \t\n\r") - start + 1);
}

inline char* new_c_str(const std::string& str) {
    auto result = new char[str.size() + 1];
    std::copy(str.begin(), str.end(), result);
    result[str.size()] = '\0';
    return result;
}

}  // namespace quiver::string

#endif  // QUIVER_STRING_H
