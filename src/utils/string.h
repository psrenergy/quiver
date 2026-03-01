#ifndef QUIVER_STRING_H
#define QUIVER_STRING_H

#include <algorithm>
#include <string>

namespace quiver::string {

inline std::string trim(const std::string& str) {
    auto start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos)
        return {};
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
