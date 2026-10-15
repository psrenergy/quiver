#include "json/json_read.h"
#include "sandbox/internal.h"

#include <nlohmann/json.hpp>
#include <sol/sol.hpp>

#include <cstdint>
#include <limits>
#include <string>

namespace quiver::lua_internal {

namespace {

sol::object json_to_lua(sol::state_view& lua, const nlohmann::json& value) {
    if (value.is_null()) {
        return sol::make_object(lua, sol::lua_nil);
    }
    if (value.is_boolean()) {
        return sol::make_object(lua, value.get<bool>());
    }
    // is_number_integer also accepts unsigned values; check them first to avoid wrapping into int64.
    if (value.is_number_unsigned()) {
        const auto number = value.get<uint64_t>();
        if (number <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
            return sol::make_object(lua, static_cast<int64_t>(number));
        }
        return sol::make_object(lua, static_cast<double>(number));
    }
    if (value.is_number_integer()) {
        return sol::make_object(lua, value.get<int64_t>());
    }
    if (value.is_number_float()) {
        return sol::make_object(lua, value.get<double>());
    }
    if (value.is_string()) {
        return sol::make_object(lua, value.get_ref<const std::string&>());
    }

    auto result = lua.create_table();
    if (value.is_array()) {
        int64_t index = 1;
        for (const auto& child : value) {
            result.raw_set(index++, json_to_lua(lua, child));
        }
    } else {
        for (auto it = value.begin(); it != value.end(); ++it) {
            result.raw_set(it.key(), json_to_lua(lua, it.value()));
        }
    }
    return sol::make_object(lua, result);
}

}  // namespace

void bind_json(sol::usertype<Database>& bind) {
    bind.set_function("read_json", [](Database& self, const sol::object& path, sol::this_state s) -> sol::object {
        if (path.get_type() != sol::type::string) {
            throw lua_type_error("read_json", "path", "a string", path);
        }
        const auto original = path.as<std::string>();
        const auto resolved = resolve_sandbox_path(self, "read_json", original);
        sol::state_view lua(s);
        return json_to_lua(lua, json_read::read_file(resolved, original));
    });
}

}  // namespace quiver::lua_internal
