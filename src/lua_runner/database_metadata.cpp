#include "sandbox/internal.h"
#include "quiver/database.h"

#include <sol/sol.hpp>

#include <string>

namespace quiver::lua_internal {

namespace {

// The Lua spelling is the ASCII lowercase of the core's name: INTEGER -> integer, DATE_TIME -> date_time.
std::string lua_data_type_name(DataType type) {
    std::string name = data_type_to_string(type);
    for (char& c : name) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return name;
}

}  // namespace

sol::table metadata_to_lua(sol::state_view& lua, const ScalarMetadata& attribute) {
    auto t = lua.create_table();
    t["name"] = attribute.name;
    t["data_type"] = lua_data_type_name(attribute.data_type);
    t["not_null"] = attribute.not_null;
    t["primary_key"] = attribute.primary_key;
    if (attribute.default_value.has_value()) {
        t["default_value"] = *attribute.default_value;
    }
    t["is_foreign_key"] = attribute.is_foreign_key;
    if (attribute.references_collection.has_value()) {
        t["references_collection"] = *attribute.references_collection;
    }
    if (attribute.references_column.has_value()) {
        t["references_column"] = *attribute.references_column;
    }
    return t;
}

// One Lua table shape for every group kind, like the C API's convert_group_to_c:
// group_name, value_columns, and dimension_column for a time series (never empty there).
sol::table metadata_to_lua(sol::state_view& lua, const GroupMetadata& metadata) {
    auto t = lua.create_table();
    t["group_name"] = metadata.group_name;
    if (!metadata.dimension_column.empty()) {
        t["dimension_column"] = metadata.dimension_column;
    }
    auto cols = lua.create_table();
    for (size_t i = 0; i < metadata.value_columns.size(); ++i) {
        cols[i + 1] = metadata_to_lua(lua, metadata.value_columns[i]);
    }
    t["value_columns"] = cols;
    return t;
}

void bind_metadata(sol::usertype<Database>& bind) {
    bind.set_function("get_scalar_metadata", &get_metadata_lua<&Database::get_scalar_metadata>);
    bind.set_function("get_vector_metadata", &get_metadata_lua<&Database::get_vector_metadata>);
    bind.set_function("get_set_metadata", &get_metadata_lua<&Database::get_set_metadata>);

    bind.set_function("list_scalar_attributes", &list_metadata_lua<&Database::list_scalar_attributes>);
    bind.set_function("list_vector_groups", &list_metadata_lua<&Database::list_vector_groups>);
    bind.set_function("list_set_groups", &list_metadata_lua<&Database::list_set_groups>);
}

}  // namespace quiver::lua_internal
