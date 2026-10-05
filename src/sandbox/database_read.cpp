#include "sandbox/internal.h"
#include "quiver/database.h"

#include <sol/sol.hpp>

#include <cstdint>
#include <stdexcept>
#include <string>

namespace quiver::lua_internal {

namespace {

sol::table read_scalars_by_id_lua(Database& db, const std::string& collection, int64_t id, sol::this_state s) {
    sol::state_view lua(s);
    auto result = lua.create_table();

    for (const auto& attribute : db.list_scalar_attributes(collection)) {
        switch (attribute.data_type) {
        case DataType::Integer:
            result[attribute.name] = db.read_scalar_integer_by_id(collection, attribute.name, id);
            break;
        case DataType::Real:
            result[attribute.name] = db.read_scalar_float_by_id(collection, attribute.name, id);
            break;
        case DataType::Text:
        case DataType::DateTime:
            result[attribute.name] = db.read_scalar_string_by_id(collection, attribute.name, id);
            break;
        default:
            throw std::runtime_error(
                "Cannot read_scalars_by_id: unknown data type " + std::to_string(static_cast<int>(attribute.data_type))
            );
        }
    }
    return result;
}

// read_vectors_by_id / read_sets_by_id: every value column of every group of one kind, read per
// column name through that kind's typed _by_id readers (`List` lists the groups).
template <auto List, auto ReadIntegers, auto ReadFloats, auto ReadStrings>
sol::table read_groups_by_id(
    Database& db,
    const std::string& operation,
    const std::string& collection,
    int64_t id,
    sol::this_state s
) {
    sol::state_view lua(s);
    auto result = lua.create_table();

    for (const auto& group : (db.*List)(collection)) {
        for (const auto& col : group.value_columns) {
            switch (col.data_type) {
            case DataType::Integer:
                result[col.name] = to_lua_table(lua, (db.*ReadIntegers)(collection, col.name, id));
                break;
            case DataType::Real:
                result[col.name] = to_lua_table(lua, (db.*ReadFloats)(collection, col.name, id));
                break;
            case DataType::Text:
            case DataType::DateTime:
                result[col.name] = to_lua_table(lua, (db.*ReadStrings)(collection, col.name, id));
                break;
            default:
                throw std::runtime_error(
                    "Cannot " + operation + ": unknown data type " + std::to_string(static_cast<int>(col.data_type))
                );
            }
        }
    }
    return result;
}

sol::table read_vectors_by_id_lua(Database& db, const std::string& collection, int64_t id, sol::this_state s) {
    return read_groups_by_id<
        &Database::list_vector_groups,
        &Database::read_vector_integers_by_id,
        &Database::read_vector_floats_by_id,
        &Database::read_vector_strings_by_id>(db, "read_vectors_by_id", collection, id, s);
}

sol::table read_sets_by_id_lua(Database& db, const std::string& collection, int64_t id, sol::this_state s) {
    return read_groups_by_id<
        &Database::list_set_groups,
        &Database::read_set_integers_by_id,
        &Database::read_set_floats_by_id,
        &Database::read_set_strings_by_id>(db, "read_sets_by_id", collection, id, s);
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

}  // namespace

void bind_read(sol::usertype<Database>& bind) {
    bind.set_function("read_element_ids", &collection_read_lua<&Database::read_element_ids>);
    bind.set_function("number_of_elements", &Database::number_of_elements);

    bind.set_function("read_scalar_strings", &bulk_read_lua<&Database::read_scalar_strings>);
    bind.set_function("read_scalar_integers", &bulk_read_lua<&Database::read_scalar_integers>);
    bind.set_function("read_scalar_floats", &bulk_read_lua<&Database::read_scalar_floats>);

    bind.set_function("read_vector_integers", &bulk_read_lua<&Database::read_vector_integers>);
    bind.set_function("read_vector_floats", &bulk_read_lua<&Database::read_vector_floats>);
    bind.set_function("read_vector_strings", &bulk_read_lua<&Database::read_vector_strings>);

    bind.set_function("read_set_integers", &bulk_read_lua<&Database::read_set_integers>);
    bind.set_function("read_set_floats", &bulk_read_lua<&Database::read_set_floats>);
    bind.set_function("read_set_strings", &bulk_read_lua<&Database::read_set_strings>);

    bind.set_function("read_scalars_by_id", &read_scalars_by_id_lua);
    bind.set_function("read_vectors_by_id", &read_vectors_by_id_lua);
    bind.set_function("read_sets_by_id", &read_sets_by_id_lua);
    bind.set_function("read_element_by_id", &read_element_by_id_lua);
}

}  // namespace quiver::lua_internal
