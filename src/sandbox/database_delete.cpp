#include "sandbox/internal.h"
#include "quiver/database.h"

#include <sol/sol.hpp>

namespace quiver::lua_internal {

void bind_delete(sol::usertype<Database>& bind) {
    bind.set_function("delete_element", &Database::delete_element);
    bind.set_function("delete_element_by_label", &Database::delete_element_by_label);
}

}  // namespace quiver::lua_internal
