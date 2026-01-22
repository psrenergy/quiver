#ifndef QUIVER_VALUE_H
#define QUIVER_VALUE_H

#include <cstdint>
#include <string>
#include <variant>

namespace quiver {

using Value = std::variant<std::nullptr_t, int64_t, double, std::string>;

}  // namespace quiver

#endif  // QUIVER_VALUE_H
