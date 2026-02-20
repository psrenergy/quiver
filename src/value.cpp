#include "quiver/value.h"

#include <type_traits>

namespace quiver {

std::ostream& operator<<(std::ostream& os, const Value& val) {
    std::visit(
        [&os](const auto& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (!std::is_same_v<T, std::nullptr_t>) {
                os << v;
            }
        },
        val);
    return os;
}

}  // namespace quiver
