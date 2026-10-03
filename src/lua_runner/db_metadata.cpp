#include "lua_runner/internal.h"
#include "quiver/database.h"

#include <sol/sol.hpp>

#include <stdexcept>
#include <string>

namespace quiver::lua_internal {

namespace {

std::string lua_data_type_name(DataType type) {
    switch (type) {
    case DataType::Integer:
        return "integer";
    case DataType::Real:
        return "real";
    case DataType::Text:
        return "text";
    case DataType::DateTime:
        return "date_time";
    default:
        throw std::runtime_error(
            "Cannot lua_data_type_name: unknown data type " + std::to_string(static_cast<int>(type))
        );
    }
}

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

// list_scalar_attributes / list_{vector,set,time_series}_groups: one metadata table per entry.
template <auto List>
sol::table list_metadata_lua(Database& db, const std::string& collection, sol::this_state s) {
    sol::state_view lua(s);
    auto t = lua.create_table();
    const auto items = (db.*List)(collection);
    for (size_t i = 0; i < items.size(); ++i) {
        t[i + 1] = metadata_to_lua(lua, items[i]);
    }
    return t;
}

// get_{scalar,vector,set,time_series}_metadata: the one named attribute or group.
template <auto Get>
sol::table get_metadata_lua(Database& db, const std::string& collection, const std::string& name, sol::this_state s) {
    sol::state_view lua(s);
    return metadata_to_lua(lua, (db.*Get)(collection, name));
}

}  // namespace

void bind_metadata(sol::usertype<Database>& bind) {
    bind.set_function("get_scalar_metadata", &get_metadata_lua<&Database::get_scalar_metadata>);
    bind.set_function("get_vector_metadata", &get_metadata_lua<&Database::get_vector_metadata>);
    bind.set_function("get_set_metadata", &get_metadata_lua<&Database::get_set_metadata>);
    bind.set_function("get_time_series_metadata", &get_metadata_lua<&Database::get_time_series_metadata>);

    bind.set_function("list_scalar_attributes", &list_metadata_lua<&Database::list_scalar_attributes>);
    bind.set_function("list_vector_groups", &list_metadata_lua<&Database::list_vector_groups>);
    bind.set_function("list_set_groups", &list_metadata_lua<&Database::list_set_groups>);
    bind.set_function("list_time_series_groups", &list_metadata_lua<&Database::list_time_series_groups>);
}

}  // namespace quiver::lua_internal
