#ifndef QUIVER_VALUE_H
#define QUIVER_VALUE_H

#include <cstdint>
#include <ostream>
#include <string>
#include <variant>

namespace quiver {

using Value = std::variant<std::nullptr_t, int64_t, double, std::string>;

std::ostream& operator<<(std::ostream& os, const Value& val);

}  // namespace quiver

#endif  // QUIVER_VALUE_H
