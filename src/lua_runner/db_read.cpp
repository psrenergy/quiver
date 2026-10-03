#include "lua_runner/internal.h"
#include "quiver/database.h"

#include <sol/sol.hpp>

#include <cstdint>
#include <stdexcept>
#include <string>

namespace quiver::lua_internal {

namespace {

sol::table read_scalar_strings_lua(
    Database& db,
    const std::string& collection,
    const std::string& attribute,
    sol::this_state s
) {
    sol::state_view lua(s);
    return to_lua_table(lua, db.read_scalar_strings(collection, attribute));
}

sol::table read_scalar_integers_lua(
    Database& db,
    const std::string& collection,
    const std::string& attribute,
    sol::this_state s
) {
    sol::state_view lua(s);
    return to_lua_table(lua, db.read_scalar_integers(collection, attribute));
}

sol::table read_scalar_floats_lua(
    Database& db,
    const std::string& collection,
    const std::string& attribute,
    sol::this_state s
) {
    sol::state_view lua(s);
    return to_lua_table(lua, db.read_scalar_floats(collection, attribute));
}

sol::table read_vector_integers_lua(
    Database& db,
    const std::string& collection,
    const std::string& attribute,
    sol::this_state s
) {
    sol::state_view lua(s);
    return to_lua_table(lua, db.read_vector_integers(collection, attribute));
}

sol::table read_vector_floats_lua(
    Database& db,
    const std::string& collection,
    const std::string& attribute,
    sol::this_state s
) {
    sol::state_view lua(s);
    return to_lua_table(lua, db.read_vector_floats(collection, attribute));
}

sol::table read_vector_strings_lua(
    Database& db,
    const std::string& collection,
    const std::string& attribute,
    sol::this_state s
) {
    sol::state_view lua(s);
    return to_lua_table(lua, db.read_vector_strings(collection, attribute));
}

sol::table read_element_ids_lua(Database& db, const std::string& collection, sol::this_state s) {
    sol::state_view lua(s);
    return to_lua_table(lua, db.read_element_ids(collection));
}

sol::table read_scalars_by_id_lua(Database& db, const std::string& collection, int64_t id, sol::this_state s) {
    sol::state_view lua(s);
    auto result = lua.create_table();

    for (const auto& attribute : db.list_scalar_attributes(collection)) {
        switch (attribute.data_type) {
        case DataType::Integer: {
            auto val = db.read_scalar_integer_by_id(collection, attribute.name, id);
            result[attribute.name] = val.has_value() ? sol::make_object(lua, *val) : sol::lua_nil;
            break;
        }
        case DataType::Real: {
            auto val = db.read_scalar_float_by_id(collection, attribute.name, id);
            result[attribute.name] = val.has_value() ? sol::make_object(lua, *val) : sol::lua_nil;
            break;
        }
        case DataType::Text:
        case DataType::DateTime: {
            auto val = db.read_scalar_string_by_id(collection, attribute.name, id);
            result[attribute.name] = val.has_value() ? sol::make_object(lua, *val) : sol::lua_nil;
            break;
        }
        default:
            throw std::runtime_error(
                "Cannot read_scalars_by_id: unknown data type " + std::to_string(static_cast<int>(attribute.data_type))
            );
        }
    }
    return result;
}

sol::table read_vectors_by_id_lua(Database& db, const std::string& collection, int64_t id, sol::this_state s) {
    sol::state_view lua(s);
    auto result = lua.create_table();

    for (const auto& group : db.list_vector_groups(collection)) {
        for (const auto& col : group.value_columns) {
            switch (col.data_type) {
            case DataType::Integer:
                result[col.name] = to_lua_table(lua, db.read_vector_integers_by_id(collection, col.name, id));
                break;
            case DataType::Real:
                result[col.name] = to_lua_table(lua, db.read_vector_floats_by_id(collection, col.name, id));
                break;
            case DataType::Text:
            case DataType::DateTime:
                result[col.name] = to_lua_table(lua, db.read_vector_strings_by_id(collection, col.name, id));
                break;
            default:
                throw std::runtime_error(
                    "Cannot read_vectors_by_id: unknown data type " + std::to_string(static_cast<int>(col.data_type))
                );
            }
        }
    }
    return result;
}

sol::table read_sets_by_id_lua(Database& db, const std::string& collection, int64_t id, sol::this_state s) {
    sol::state_view lua(s);
    auto result = lua.create_table();

    for (const auto& group : db.list_set_groups(collection)) {
        for (const auto& col : group.value_columns) {
            switch (col.data_type) {
            case DataType::Integer:
                result[col.name] = to_lua_table(lua, db.read_set_integers_by_id(collection, col.name, id));
                break;
            case DataType::Real:
                result[col.name] = to_lua_table(lua, db.read_set_floats_by_id(collection, col.name, id));
                break;
            case DataType::Text:
            case DataType::DateTime:
                result[col.name] = to_lua_table(lua, db.read_set_strings_by_id(collection, col.name, id));
                break;
            default:
                throw std::runtime_error(
                    "Cannot read_sets_by_id: unknown data type " + std::to_string(static_cast<int>(col.data_type))
                );
            }
        }
    }
    return result;
}

sol::table read_element_by_id_lua(Database& db, const std::string& collection, int64_t id, sol::this_state s) {
    auto scalars = read_scalars_by_id_lua(db, collection, id, s);
    auto vectors = read_vectors_by_id_lua(db, collection, id, s);
    auto sets = read_sets_by_id_lua(db, collection, id, s);

    // Merge vectors and sets into scalars
    for (auto& pair : vectors) {
        scalars[pair.first] = pair.second;
    }
    for (auto& pair : sets) {
        scalars[pair.first] = pair.second;
    }
    return scalars;
}

// ========================================================================
// Bulk set reads (same pattern as read_vector_*_lua)
// ========================================================================

sol::table read_set_integers_lua(
    Database& db,
    const std::string& collection,
    const std::string& attribute,
    sol::this_state s
) {
    sol::state_view lua(s);
    return to_lua_table(lua, db.read_set_integers(collection, attribute));
}

sol::table read_set_floats_lua(
    Database& db,
    const std::string& collection,
    const std::string& attribute,
    sol::this_state s
) {
    sol::state_view lua(s);
    return to_lua_table(lua, db.read_set_floats(collection, attribute));
}

sol::table read_set_strings_lua(
    Database& db,
    const std::string& collection,
    const std::string& attribute,
    sol::this_state s
) {
    sol::state_view lua(s);
    return to_lua_table(lua, db.read_set_strings(collection, attribute));
}

}  // namespace

void bind_read(sol::usertype<Database>& bind) {
    bind.set_function("read_element_ids", &read_element_ids_lua);

    bind.set_function("read_scalar_strings", &read_scalar_strings_lua);
    bind.set_function("read_scalar_integers", &read_scalar_integers_lua);
    bind.set_function("read_scalar_floats", &read_scalar_floats_lua);

    bind.set_function("read_vector_integers", &read_vector_integers_lua);
    bind.set_function("read_vector_floats", &read_vector_floats_lua);
    bind.set_function("read_vector_strings", &read_vector_strings_lua);

    bind.set_function("read_set_integers", &read_set_integers_lua);
    bind.set_function("read_set_floats", &read_set_floats_lua);
    bind.set_function("read_set_strings", &read_set_strings_lua);

    bind.set_function("read_scalars_by_id", &read_scalars_by_id_lua);
    bind.set_function("read_vectors_by_id", &read_vectors_by_id_lua);
    bind.set_function("read_sets_by_id", &read_sets_by_id_lua);
    bind.set_function("read_element_by_id", &read_element_by_id_lua);
}

}  // namespace quiver::lua_internal
