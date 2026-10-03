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

sol::table scalar_metadata_lua(sol::state_view& lua, const ScalarMetadata& attribute) {
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
sol::table group_metadata_lua(sol::state_view& lua, const GroupMetadata& metadata) {
    auto t = lua.create_table();
    t["group_name"] = metadata.group_name;
    if (!metadata.dimension_column.empty()) {
        t["dimension_column"] = metadata.dimension_column;
    }
    auto cols = lua.create_table();
    for (size_t i = 0; i < metadata.value_columns.size(); ++i) {
        cols[i + 1] = scalar_metadata_lua(lua, metadata.value_columns[i]);
    }
    t["value_columns"] = cols;
    return t;
}

sol::table list_scalar_metadata_lua(Database& db, const std::string& collection, sol::this_state s) {
    sol::state_view lua(s);
    auto metadata_list = db.list_scalar_attributes(collection);
    auto t = lua.create_table();
    for (size_t i = 0; i < metadata_list.size(); ++i) {
        t[i + 1] = scalar_metadata_lua(lua, metadata_list[i]);
    }
    return t;
}

sol::table list_vector_metadata_lua(Database& db, const std::string& collection, sol::this_state s) {
    sol::state_view lua(s);
    auto t = lua.create_table();
    const auto groups = db.list_vector_groups(collection);
    for (size_t i = 0; i < groups.size(); ++i) {
        t[i + 1] = group_metadata_lua(lua, groups[i]);
    }
    return t;
}

sol::table list_set_metadata_lua(Database& db, const std::string& collection, sol::this_state s) {
    sol::state_view lua(s);
    auto t = lua.create_table();
    const auto groups = db.list_set_groups(collection);
    for (size_t i = 0; i < groups.size(); ++i) {
        t[i + 1] = group_metadata_lua(lua, groups[i]);
    }
    return t;
}

sol::table get_scalar_metadata_lua(
    Database& db,
    const std::string& collection,
    const std::string& attribute,
    sol::this_state s
) {
    sol::state_view lua(s);
    auto metadata = db.get_scalar_metadata(collection, attribute);
    return scalar_metadata_lua(lua, metadata);
}

sol::table get_vector_metadata_lua(
    Database& db,
    const std::string& collection,
    const std::string& group_name,
    sol::this_state s
) {
    sol::state_view lua(s);
    return group_metadata_lua(lua, db.get_vector_metadata(collection, group_name));
}

sol::table get_set_metadata_lua(
    Database& db,
    const std::string& collection,
    const std::string& group_name,
    sol::this_state s
) {
    sol::state_view lua(s);
    return group_metadata_lua(lua, db.get_set_metadata(collection, group_name));
}

// ========================================================================
// Time series metadata
// ========================================================================

sol::table get_time_series_metadata_lua(
    Database& db,
    const std::string& collection,
    const std::string& group_name,
    sol::this_state s
) {
    sol::state_view lua(s);
    return group_metadata_lua(lua, db.get_time_series_metadata(collection, group_name));
}

sol::table list_time_series_groups_lua(Database& db, const std::string& collection, sol::this_state s) {
    sol::state_view lua(s);
    auto t = lua.create_table();
    const auto groups = db.list_time_series_groups(collection);
    for (size_t i = 0; i < groups.size(); ++i) {
        t[i + 1] = group_metadata_lua(lua, groups[i]);
    }
    return t;
}

}  // namespace

void bind_metadata(sol::usertype<Database>& bind) {
    bind.set_function("get_scalar_metadata", &get_scalar_metadata_lua);
    bind.set_function("get_vector_metadata", &get_vector_metadata_lua);
    bind.set_function("get_set_metadata", &get_set_metadata_lua);
    bind.set_function("get_time_series_metadata", &get_time_series_metadata_lua);

    bind.set_function("list_scalar_attributes", &list_scalar_metadata_lua);
    bind.set_function("list_vector_groups", &list_vector_metadata_lua);
    bind.set_function("list_set_groups", &list_set_metadata_lua);
    bind.set_function("list_time_series_groups", &list_time_series_groups_lua);
}

}  // namespace quiver::lua_internal
