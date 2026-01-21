#ifndef MARGAUX_VALUE_H
#define MARGAUX_VALUE_H

#include <cstdint>
#include <string>
#include <variant>

namespace margaux {

using Value = std::variant<std::nullptr_t, int64_t, double, std::string>;

}  // namespace margaux

#endif  // MARGAUX_VALUE_H
