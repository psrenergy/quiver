#include "lua_runner/internal.h"
#include "quiver/database.h"

#include <sol/sol.hpp>

namespace quiver::lua_internal {

void bind_describe(sol::usertype<Database>& bind) {
    bind.set_function("describe", &Database::describe);
    bind.set_function("describe_collection", &Database::describe_collection);
    bind.set_function("summarize_collection", &Database::summarize_collection);
}

}  // namespace quiver::lua_internal
