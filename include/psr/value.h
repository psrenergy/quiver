#ifndef PSR_VALUE_H
#define PSR_VALUE_H

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace psr {

using Value = std::variant<std::nullptr_t, int64_t, double, std::string, std::vector<uint8_t>, std::vector<int64_t>,
                           std::vector<double>, std::vector<std::string>>;

}  // namespace psr

#endif  // PSR_VALUE_H
