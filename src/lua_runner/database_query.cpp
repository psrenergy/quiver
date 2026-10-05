#include "sandbox/internal.h"
#include "quiver/database.h"
#include "quiver/value.h"

#include <sol/sol.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace quiver::lua_internal {

namespace {

std::vector<Value> lua_table_to_values(const std::string& caller, const sol::table& parameters) {
    std::vector<Value> values;
    for (size_t i = 1; i <= parameters.size(); ++i) {
        // A skipped parameter would shift every later placeholder, so anything unsupported throws.
        values.push_back(lua_to_value(parameters.get<sol::object>(i), caller, "parameter #" + std::to_string(i)));
    }
    return values;
}

std::optional<std::string> query_string_lua(Database& db, const std::string& sql, const sol::object& parameters) {
    const auto params = optional_from_lua<sol::table>(parameters, "query_string", "params", "a table");
    return db.query_string(sql, params ? lua_table_to_values("query_string", *params) : std::vector<Value>{});
}

std::optional<int64_t> query_integer_lua(Database& db, const std::string& sql, const sol::object& parameters) {
    const auto params = optional_from_lua<sol::table>(parameters, "query_integer", "params", "a table");
    return db.query_integer(sql, params ? lua_table_to_values("query_integer", *params) : std::vector<Value>{});
}

std::optional<double> query_float_lua(Database& db, const std::string& sql, const sol::object& parameters) {
    const auto params = optional_from_lua<sol::table>(parameters, "query_float", "params", "a table");
    return db.query_float(sql, params ? lua_table_to_values("query_float", *params) : std::vector<Value>{});
}

}  // namespace

void bind_query(sol::usertype<Database>& bind) {
    bind.set_function("query_string", &query_string_lua);
    bind.set_function("query_integer", &query_integer_lua);
    bind.set_function("query_float", &query_float_lua);
}

}  // namespace quiver::lua_internal
