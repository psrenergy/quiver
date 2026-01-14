#ifndef PSR_VALUE_H
#define PSR_VALUE_H

#include <cstdint>
#include <string>
#include <variant>

namespace psr {

using Value = std::variant<std::nullptr_t, int64_t, double, std::string>;

}  // namespace psr

#endif  // PSR_VALUE_H
