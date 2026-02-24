#ifndef QUIVER_STRING_H
#define QUIVER_STRING_H

#include <string>

namespace quiver {

// Trim leading and trailing whitespaces
inline std::string trim(const std::string& s) {
    const char* whitespace = " \t\n\r\f\v";
    size_t start = s.find_first_not_of(whitespace);
    if (start == std::string::npos) {
        return "";
    }
    size_t end = s.find_last_not_of(whitespace);
    return s.substr(start, end - start + 1);
}

}  // namespace quiver

#endif  // QUIVER_STRING_H