#include "quiver/lua_runner.h"

#include "csv/csv_read.h"
#include "csv/csv_write.h"
#include "lua_runner/internal.h"
#include "quiver/binary/binary_file.h"
#include "quiver/binary/binary_metadata.h"
#include "quiver/binary/csv_converter.h"
#include "quiver/binary/time_properties.h"
#include "quiver/database.h"
#include "quiver/element.h"
#include "quiver/expression/expression.h"
#include "quiver/options.h"
#include "quiver/value.h"
#include "utils/datetime.h"
#include "utils/number.h"

#include <sol/sol.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

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

namespace {

// A vector/set read hands a NULL cell back as a nil hole, and `#` over a hole is an arbitrary
// border: lua_table_to_vector (bounded by t.size()) would silently cut such an array short, and
// table_to_element skips it outright when the hole is cell 1. Element arrays stay dense; the
// group writers are the ones that write a hole as NULL.
void require_dense_array(const std::string& caller, const sol::table& arr, const std::string& name) {
    size_t entries = 0;
    for ([[maybe_unused]] const auto& entry : arr) {
        ++entries;
    }
    if (entries != arr.size()) {
        throw std::runtime_error(
            "Cannot " + caller + ": array '" + name +
            "' has a nil hole or a non-integer key; write NULL cells with "
            "update_vector_group or update_set_group"
        );
    }
}

}  // namespace

Element table_to_element(const std::string& caller, const sol::table& values) {
    Element element;
    for (const auto& pair : values) {
        auto key = pair.first;
        auto val = pair.second;
        auto k = key.as<std::string>();

        if (val.is<sol::table>()) {
            auto arr = val.as<sol::table>();
            require_dense_array(caller, arr, k);
            if (arr.size() > 0) {
                sol::object first = arr[1];
                // Cell 1 only picks the element type; lua_table_to_vector checks the rest.
                // A boolean array is an INTEGER array.
                const std::string array_caller = caller + ": array '" + k + "'";
                if (is_lua_boolean(first) || first.is<int64_t>()) {
                    element.set(k, lua_table_to_vector<int64_t>(arr, array_caller));
                } else if (first.is<double>()) {
                    element.set(k, lua_table_to_vector<double>(arr, array_caller));
                } else if (first.is<std::string>()) {
                    element.set(k, lua_table_to_vector<std::string>(arr, array_caller));
                } else {
                    // Surface unsupported element types loudly instead of silently
                    // dropping the attribute (same policy as lua_to_value)
                    throw std::runtime_error("Cannot " + caller + ": array '" + k + "' has unsupported element type");
                }
            }
        } else {
            std::visit(
                [&](auto&& x) {
                    if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::nullptr_t>) {
                        element.set_null(k);
                    } else {
                        element.set(k, x);
                    }
                },
                lua_to_value(val, caller, "attribute '" + k + "'")
            );
        }
    }
    return element;
}

std::vector<GroupColumn> collect_group_columns(const std::string& caller, const sol::table& columns) {
    std::vector<GroupColumn> result;
    for (auto& pair : columns) {
        // Check the key's type before converting it. sol2's string getter is unchecked in
        // Release (SOL_SAFE_GETTER off): key 1 became column "1" and a boolean key column "",
        // so an array of row tables got a misleading error there and a raw sol2 panic in Debug.
        if (pair.first.get_type() != sol::type::string) {
            throw std::runtime_error(
                "Cannot " + caller +
                ": column names must be strings; pass { column = { values... } }, "
                "not an array of row tables"
            );
        }
        auto name = pair.first.as<std::string>();
        if (!pair.second.is<sol::table>()) {
            throw std::runtime_error("Cannot " + caller + ": column '" + name + "' must be an array of values");
        }
        GroupColumn column{name, pair.second.as<sol::table>()};
        for (auto& cell : column.values) {
            if (!cell.first.is<int64_t>() || cell.first.as<int64_t>() < 1) {
                throw std::runtime_error("Cannot " + caller + ": column '" + name + "' must be an array of values");
            }
            column.extent = std::max(column.extent, static_cast<size_t>(cell.first.as<int64_t>()));
            ++column.count;
        }
        result.push_back(std::move(column));
    }
    return result;
}

// Transpose the collected columns into row maps where every row carries every named column,
// with explicit NULL for the cells the caller left out (which is how nil holes from a read
// round-trip). The NULL pre-fill is what makes an all-nil column such as `flag = {}` reach the
// core at all: it is validated (an unknown name still throws) and written as NULL rather than
// left to the column DEFAULT.
std::vector<std::map<std::string, Value>> columns_to_cpp_rows(
    const std::string& caller,
    const std::vector<GroupColumn>& lua_columns,
    size_t row_count
) {
    std::vector<std::map<std::string, Value>> cpp_rows(row_count);
    for (const auto& column : lua_columns) {
        for (auto& row : cpp_rows) {
            row[column.name] = nullptr;
        }
        const std::string what = "column '" + column.name + "'";
        for (auto& cell : column.values) {
            const auto index = static_cast<size_t>(cell.first.as<int64_t>());
            cpp_rows[index - 1][column.name] = lua_to_value(cell.second, caller, what);
        }
    }
    return cpp_rows;
}

// Join the column names for the "no rows" message below.
std::string join_column_names(const std::vector<GroupColumn>& lua_columns) {
    std::string joined;
    for (const auto& column : lua_columns) {
        joined += (joined.empty() ? "" : ", ") + column.name;
    }
    return joined;
}

namespace {

int64_t create_element_lua(Database& db, const std::string& collection, const sol::table& values) {
    auto element = table_to_element("create_element", values);
    return db.create_element(collection, element);
}

void update_element_lua(Database& db, const std::string& collection, int64_t id, const sol::table& values) {
    auto element = table_to_element("update_element", values);
    db.update_element(collection, id, element);
}

void update_element_by_label_lua(
    Database& db,
    const std::string& collection,
    const std::string& label,
    const sol::table& values
) {
    auto element = table_to_element("update_element_by_label", values);
    db.update_element_by_label(collection, label, element);
}

// nil/missing clears; sol::object (not sol::optional) so a wrong type still throws.
std::optional<std::string> relation_target_from_lua(const sol::object& target_label, const std::string& caller) {
    if (!target_label.valid() || target_label.get_type() == sol::type::lua_nil) {
        return std::nullopt;
    }
    if (target_label.get_type() != sol::type::string) {
        throw std::runtime_error("Cannot " + caller + ": target_label has unsupported Lua type");
    }
    return target_label.as<std::string>();
}

void update_relation_lua(
    Database& db,
    const std::string& collection_from,
    const std::string& collection_to,
    const std::string& relation_type,
    int64_t id,
    const sol::object& target_label
) {
    db.update_relation(
        collection_from,
        collection_to,
        relation_type,
        id,
        relation_target_from_lua(target_label, "update_relation")
    );
}

void update_relation_by_label_lua(
    Database& db,
    const std::string& collection_from,
    const std::string& collection_to,
    const std::string& relation_type,
    const std::string& label,
    const sol::object& target_label
) {
    db.update_relation_by_label(
        collection_from,
        collection_to,
        relation_type,
        label,
        relation_target_from_lua(target_label, "update_relation_by_label")
    );
}

// Vector and set groups have no dimension column, so the row count is the largest index any
// column reaches; shorter or sparse columns write NULL in the gaps, mirroring the time series
// writer's treatment of value columns. Named columns that reach no index at all throw instead
// of silently clearing the group; an empty table {} clears.
std::vector<std::map<std::string, Value>> group_rows_from_lua(const std::string& caller, const sol::table& columns) {
    auto lua_columns = collect_group_columns(caller, columns);
    if (lua_columns.empty()) {
        return {};
    }

    size_t row_count = 0;
    for (const auto& column : lua_columns) {
        row_count = std::max(row_count, column.extent);
    }
    if (row_count == 0) {
        throw std::runtime_error(
            "Cannot " + caller + ": columns [" + join_column_names(lua_columns) +
            "] contain no rows; pass an empty table {} to clear the group"
        );
    }
    return columns_to_cpp_rows(caller, lua_columns, row_count);
}

// NOLINTBEGIN(performance-unnecessary-value-param) sol2 lambda bindings require pass-by-value for type
// deduction
void update_vector_group_lua(
    Database& db,
    const std::string& collection,
    const std::string& group,
    int64_t id,
    sol::table columns
) {
    db.update_vector_group(collection, group, id, group_rows_from_lua("update_vector_group", columns));
}

void update_vector_group_by_label_lua(
    Database& db,
    const std::string& collection,
    const std::string& group,
    const std::string& label,
    sol::table columns
) {
    db.update_vector_group_by_label(
        collection,
        group,
        label,
        group_rows_from_lua("update_vector_group_by_label", columns)
    );
}

void update_set_group_lua(
    Database& db,
    const std::string& collection,
    const std::string& group,
    int64_t id,
    sol::table columns
) {
    db.update_set_group(collection, group, id, group_rows_from_lua("update_set_group", columns));
}

void update_set_group_by_label_lua(
    Database& db,
    const std::string& collection,
    const std::string& group,
    const std::string& label,
    sol::table columns
) {
    db.update_set_group_by_label(collection, group, label, group_rows_from_lua("update_set_group_by_label", columns));
}

}  // namespace

void bind_write(sol::usertype<Database>& bind) {
    bind.set_function("delete_element", [](Database& self, const std::string& collection, int64_t id) {
        self.delete_element(collection, id);
    });
    bind.set_function(
        "delete_element_by_label",
        [](Database& self, const std::string& collection, const std::string& label) {
            self.delete_element_by_label(collection, label);
        }
    );

    bind.set_function("create_element", &create_element_lua);

    bind.set_function("update_element", &update_element_lua);
    bind.set_function("update_element_by_label", &update_element_by_label_lua);
    bind.set_function("update_relation", &update_relation_lua);
    bind.set_function("update_relation_by_label", &update_relation_by_label_lua);
    bind.set_function("update_vector_group", &update_vector_group_lua);
    bind.set_function("update_vector_group_by_label", &update_vector_group_by_label_lua);
    bind.set_function("update_set_group", &update_set_group_lua);
    bind.set_function("update_set_group_by_label", &update_set_group_by_label_lua);
}
// NOLINTEND(performance-unnecessary-value-param)

namespace {

sol::object value_to_lua_object(sol::state_view& lua, const Value& val) {
    return std::visit(
        [&](auto&& arg) -> sol::object {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::nullptr_t>) {
                return sol::make_object(lua, sol::lua_nil);
            } else {
                return sol::make_object(lua, arg);
            }
        },
        val
    );
}

std::map<std::string, Value> lua_table_to_value_map(const std::string& caller, const sol::table& t) {
    // Column order in the resulting map is alphabetical (std::map invariant),
    // which is fine because the C++ layer indexes by column name rather than
    // relying on positional order. Callers should not depend on insertion order.
    std::map<std::string, Value> result;
    for (auto& pair : t) {
        auto key = pair.first.as<std::string>();
        result[key] = lua_to_value(pair.second, caller, "column '" + key + "'");
    }
    return result;
}

// ========================================================================
// Time series data
// ========================================================================

sol::table read_time_series_group_lua(
    Database& db,
    const std::string& collection,
    const std::string& group,
    int64_t id,
    sol::this_state s
) {
    sol::state_view lua(s);
    auto rows = db.read_time_series_group(collection, group, id);
    // Column-oriented result: { name = {v1, v2, ...}, ... }
    auto t = lua.create_table();
    if (rows.empty()) {
        return t;
    }
    for (const auto& [name, _] : rows[0]) {
        auto column = lua.create_table();
        for (size_t i = 0; i < rows.size(); ++i) {
            column[i + 1] = value_to_lua_object(lua, rows[i].at(name));
        }
        t[name] = column;
    }
    return t;
}

sol::table read_time_series_row_lua(
    Database& db,
    const std::string& collection,
    const std::string& group,
    const std::string& attribute,
    const std::string& date_time,
    sol::this_state s
) {
    sol::state_view lua(s);
    auto values = db.read_time_series_row(collection, group, attribute, date_time);
    auto t = lua.create_table();
    for (size_t i = 0; i < values.size(); ++i) {
        t[i + 1] = value_to_lua_object(lua, values[i]);
    }
    return t;
}

// Column-oriented input: { name = {v1, v2, ...}, ... } transposed into the row maps the
// C++ API takes. The dimension column(s) are the row count authority -- PK members are
// implicitly NOT NULL in STRICT tables, so they must be present and dense. Value columns
// may be shorter, sparse, or empty: every cell missing at a dimension index is written as
// NULL, which round-trips the nil holes that read_time_series_group produces. An empty
// table (no columns) clears all rows; named columns whose dimension transposes to zero
// rows still throw instead of silently clearing the group.
// Takes `db` (unlike group_rows_from_lua) because the dimension column(s) come from metadata.
std::vector<std::map<std::string, Value>> time_series_rows_from_lua(
    Database& db,
    const std::string& caller,
    const std::string& collection,
    const std::string& group,
    const sol::table& columns
) {
    auto lua_columns = collect_group_columns(caller, columns);
    if (lua_columns.empty()) {
        return {};
    }

    // The date_ ordering column plus any extra PK dimensions (multi-dim groups, e.g.
    // (date_time, block)): get_time_series_metadata reports the extras as value_columns
    // with primary_key set.
    auto metadata = db.get_time_series_metadata(collection, group);
    std::vector<std::string> dimension_columns;
    dimension_columns.push_back(metadata.dimension_column);
    for (const auto& vc : metadata.value_columns) {
        if (vc.primary_key) {
            dimension_columns.push_back(vc.name);
        }
    }

    const auto find_column = [&](const std::string& name) -> const GroupColumn* {
        for (const auto& column : lua_columns) {
            if (column.name == name) {
                return &column;
            }
        }
        return nullptr;
    };

    for (const auto& dim : dimension_columns) {
        if (find_column(dim) == nullptr) {
            throw std::runtime_error("Cannot " + caller + ": missing dimension column '" + dim + "'");
        }
    }

    const size_t row_count = find_column(dimension_columns.front())->extent;
    for (const auto& dim : dimension_columns) {
        const auto* column = find_column(dim);
        if (column->extent != row_count) {
            throw std::runtime_error(
                "Cannot " + caller + ": column '" + dim + "' has length " + std::to_string(column->extent) +
                " but expected " + std::to_string(row_count)
            );
        }
        if (column->count != column->extent) {
            for (size_t i = 1; i <= column->extent; ++i) {
                if (!column->values[i].valid()) {
                    throw std::runtime_error(
                        "Cannot " + caller + ": dimension column '" + dim + "' has nil at index " + std::to_string(i)
                    );
                }
            }
        }
    }

    for (const auto& column : lua_columns) {
        if (column.extent > row_count) {
            throw std::runtime_error(
                "Cannot " + caller + ": column '" + column.name + "' has length " + std::to_string(column.extent) +
                " but expected " + std::to_string(row_count)
            );
        }
    }

    if (row_count == 0) {
        throw std::runtime_error(
            "Cannot " + caller + ": columns [" + join_column_names(lua_columns) +
            "] contain no rows; pass an empty table {} to clear the group"
        );
    }

    return columns_to_cpp_rows(caller, lua_columns, row_count);
}

// NOLINTBEGIN(performance-unnecessary-value-param) sol2 lambda bindings require pass-by-value for type
// deduction
void update_time_series_group_lua(
    Database& db,
    const std::string& collection,
    const std::string& group,
    int64_t id,
    sol::table columns
) {
    db.update_time_series_group(
        collection,
        group,
        id,
        time_series_rows_from_lua(db, "update_time_series_group", collection, group, columns)
    );
}

void update_time_series_group_by_label_lua(
    Database& db,
    const std::string& collection,
    const std::string& group,
    const std::string& label,
    sol::table columns
) {
    db.update_time_series_group_by_label(
        collection,
        group,
        label,
        time_series_rows_from_lua(db, "update_time_series_group_by_label", collection, group, columns)
    );
}

void upsert_time_series_row_lua(
    Database& db,
    const std::string& collection,
    const std::string& group,
    int64_t id,
    sol::table row
) {
    db.upsert_time_series_row(collection, group, id, lua_table_to_value_map("upsert_time_series_row", row));
}

void upsert_time_series_row_by_label_lua(
    Database& db,
    const std::string& collection,
    const std::string& group,
    const std::string& label,
    sol::table row
) {
    db.upsert_time_series_row_by_label(
        collection,
        group,
        label,
        lua_table_to_value_map("upsert_time_series_row_by_label", row)
    );
}

// ========================================================================
// Time series files
// ========================================================================

sol::table list_time_series_files_columns_lua(Database& db, const std::string& collection, sol::this_state s) {
    sol::state_view lua(s);
    return to_lua_table(lua, db.list_time_series_files_columns(collection));
}

sol::table read_time_series_files_lua(Database& db, const std::string& collection, sol::this_state s) {
    sol::state_view lua(s);
    auto files = db.read_time_series_files(collection);
    auto t = lua.create_table();
    for (const auto& [key, val] : files) {
        if (val.has_value()) {
            t[key] = *val;
        }
    }
    return t;
}

void update_time_series_files_lua(Database& db, const std::string& collection, const sol::table& paths) {
    std::map<std::string, std::optional<std::string>> cpp_paths;
    for (auto& pair : paths) {
        auto key = pair.first.as<std::string>();
        sol::object val = pair.second;
        if (val.is<sol::lua_nil_t>()) {
            cpp_paths[key] = std::nullopt;
        } else {
            cpp_paths[key] = lua_cell_as<std::string>(val, "update_time_series_files", "path '" + key + "'");
        }
    }
    db.update_time_series_files(collection, cpp_paths);
}

}  // namespace

void bind_time_series(sol::usertype<Database>& bind) {
    bind.set_function("has_time_series_files", [](Database& self, const std::string& collection) {
        return self.has_time_series_files(collection);
    });

    bind.set_function("read_time_series_group", &read_time_series_group_lua);
    bind.set_function("read_time_series_row", &read_time_series_row_lua);
    bind.set_function("read_time_series_files", &read_time_series_files_lua);

    bind.set_function("update_time_series_group", &update_time_series_group_lua);
    bind.set_function("update_time_series_group_by_label", &update_time_series_group_by_label_lua);
    bind.set_function("upsert_time_series_row", &upsert_time_series_row_lua);
    bind.set_function("upsert_time_series_row_by_label", &upsert_time_series_row_by_label_lua);
    bind.set_function("update_time_series_files", &update_time_series_files_lua);

    bind.set_function("list_time_series_files_columns", &list_time_series_files_columns_lua);
}
// NOLINTEND(performance-unnecessary-value-param)

namespace {

// Collect a nested option table's entries before any is checked (the same collect-then-validate rule), after
// checking the value really is a table.
std::vector<std::pair<sol::object, sol::object>> table_entries(
    const sol::object& value,
    const std::string& operation,
    const std::string& what
) {
    if (value.get_type() != sol::type::table) {
        throw std::runtime_error("Cannot " + operation + ": option '" + what + "' must be a table");
    }
    std::vector<std::pair<sol::object, sol::object>> entries;
    value.as<sol::table>().for_each([&](sol::object key, sol::object entry) {
        entries.emplace_back(std::move(key), std::move(entry));
    });
    return entries;
}

std::string string_key(const sol::object& key, const std::string& operation, const std::string& what) {
    if (key.get_type() != sol::type::string) {
        throw std::runtime_error("Cannot " + operation + ": keys of option '" + what + "' must be strings");
    }
    return key.as<std::string>();
}

// Strict decoder for db:export_csv / db:import_csv options, the same collect-then-validate walk
// as the read_csv/write_csv decoders: nil/missing means defaults; anything else must be a table
// with only known keys of the right types.
CSVOptions parse_csv_options(const sol::object& options, const std::string& operation) {
    CSVOptions result;
    if (!options.valid() || options.get_type() == sol::type::lua_nil) {
        return result;
    }
    if (options.get_type() != sol::type::table) {
        throw std::runtime_error("Cannot " + operation + ": options must be a table");
    }
    const auto found = csv_options_entries(options, operation, {"date_time_format", "enum_labels"});

    if (const auto& format = found[0]) {
        if (format->get_type() != sol::type::string) {
            throw std::runtime_error("Cannot " + operation + ": option 'date_time_format' must be a string");
        }
        result.date_time_format = format->as<std::string>();
    }
    if (const auto& enums = found[1]) {
        // attribute -> locale -> { label = code }: every level collected before it is checked.
        for (const auto& [attr_key, attr_value] : table_entries(*enums, operation, "enum_labels")) {
            const auto attr = string_key(attr_key, operation, "enum_labels");
            const auto attr_where = "enum_labels['" + attr + "']";
            auto& locales = result.enum_labels[attr];
            for (const auto& [locale_key, locale_value] : table_entries(attr_value, operation, attr_where)) {
                const auto locale = string_key(locale_key, operation, attr_where);
                const auto where = attr_where + "['" + locale + "']";
                auto& labels = locales[locale];
                for (const auto& [label_key, code] : table_entries(locale_value, operation, where)) {
                    const auto label = string_key(label_key, operation, where);
                    labels[label] = lua_cell_as<int64_t>(code, operation, "code for label '" + label + "'");
                }
            }
        }
    }
    return result;
}

std::vector<Value> lua_table_to_values(const std::string& caller, const sol::table& parameters) {
    std::vector<Value> values;
    for (size_t i = 1; i <= parameters.size(); ++i) {
        // A skipped parameter would shift every later placeholder, so anything unsupported throws.
        values.push_back(lua_to_value(parameters.get<sol::object>(i), caller, "parameter #" + std::to_string(i)));
    }
    return values;
}

sol::object query_string_lua(
    Database& db,
    const std::string& sql,
    sol::optional<sol::table> parameters,
    sol::this_state s
) {
    sol::state_view lua(s);
    auto values = parameters ? lua_table_to_values("query_string", *parameters) : std::vector<Value>{};
    auto result = db.query_string(sql, values);
    if (result.has_value()) {
        return sol::make_object(lua, *result);
    }
    return sol::make_object(lua, sol::lua_nil);
}

sol::object query_integer_lua(
    Database& db,
    const std::string& sql,
    sol::optional<sol::table> parameters,
    sol::this_state s
) {
    sol::state_view lua(s);
    auto values = parameters ? lua_table_to_values("query_integer", *parameters) : std::vector<Value>{};
    auto result = db.query_integer(sql, values);
    if (result.has_value()) {
        return sol::make_object(lua, *result);
    }
    return sol::make_object(lua, sol::lua_nil);
}

sol::object query_float_lua(
    Database& db,
    const std::string& sql,
    sol::optional<sol::table> parameters,
    sol::this_state s
) {
    sol::state_view lua(s);
    auto values = parameters ? lua_table_to_values("query_float", *parameters) : std::vector<Value>{};
    auto result = db.query_float(sql, values);
    if (result.has_value()) {
        return sol::make_object(lua, *result);
    }
    return sol::make_object(lua, sol::lua_nil);
}

}  // namespace

// NOLINTBEGIN(performance-unnecessary-value-param) sol2 lambda bindings require pass-by-value for type
// deduction
void bind_core(sol::usertype<Database>& bind) {
    bind.set_function("is_healthy", [](Database& self) { return self.is_healthy(); });
    bind.set_function("current_version", [](Database& self) { return self.current_version(); });
    bind.set_function("path", [](Database& self) -> const std::string& { return self.path(); });
    bind.set_function("begin_transaction", [](Database& self) { self.begin_transaction(); });
    bind.set_function("commit", [](Database& self) { self.commit(); });
    bind.set_function("rollback", [](Database& self) { self.rollback(); });
    bind.set_function("in_transaction", [](Database& self) { return self.in_transaction(); });
    bind.set_function("transaction", [](Database& self, sol::protected_function fn) -> sol::object {
        self.begin_transaction();
        auto result = fn(std::ref(self));
        if (!result.valid()) {
            sol::error err = result;
            try {
                self.rollback();
            } catch (...) {
            }
            throw std::runtime_error(err.what());
        }
        self.commit();
        if (result.return_count() > 0) {
            return result.get<sol::object>(0);
        }
        return sol::make_object(result.lua_state(), sol::lua_nil);
    });
    bind.set_function("begin_dry_run", [](Database& self) { self.begin_dry_run(); });
    bind.set_function("end_dry_run", [](Database& self) { self.end_dry_run(); });
    bind.set_function("in_dry_run", [](Database& self) { return self.in_dry_run(); });
    bind.set_function("dry_run", [](Database& self, sol::protected_function fn) -> sol::object {
        self.begin_dry_run();
        auto result = fn(std::ref(self));
        if (!result.valid()) {
            sol::error err = result;
            try {
                self.end_dry_run();
            } catch (...) {
            }
            throw std::runtime_error(err.what());
        }
        self.end_dry_run();
        if (result.return_count() > 0) {
            return result.get<sol::object>(0);
        }
        return sol::make_object(result.lua_state(), sol::lua_nil);
    });
    bind.set_function(
        "export_csv",
        [](Database& self,
           const std::string& collection,
           const std::string& group,
           const std::string& path,
           const sol::object& options) {
            // Sandbox checks before the options table, as in db:write_csv.
            const auto resolved = resolve_sandboxed_path(self, "export_csv", path);
            self.export_csv(collection, group, resolved, parse_csv_options(options, "export_csv"));
        }
    );
    bind.set_function(
        "import_csv",
        [](Database& self,
           const std::string& collection,
           const std::string& group,
           const std::string& path,
           const sol::object& options) {
            const auto resolved = resolve_sandboxed_path(self, "import_csv", path);
            self.import_csv(collection, group, resolved, parse_csv_options(options, "import_csv"));
        }
    );

    bind.set_function("number_of_elements", [](Database& self, const std::string& collection) {
        return self.number_of_elements(collection);
    });

    bind.set_function("describe", [](Database& self) { return self.describe(); });
    bind.set_function("describe_collection", [](Database& self, const std::string& collection) {
        return self.describe_collection(collection);
    });
    bind.set_function("summarize_collection", [](Database& self, const std::string& collection) {
        return self.summarize_collection(collection);
    });

    bind.set_function("query_string", &query_string_lua);
    bind.set_function("query_integer", &query_integer_lua);
    bind.set_function("query_float", &query_float_lua);

    // Migration round-trip validation — db-scoped and sandboxed like the file I/O below.
    bind.set_function("validate_migrations", [](Database& self, const std::string& path) {
        Database::validate_migrations(resolve_sandboxed_path(self, "validate_migrations", path));
    });
}
// NOLINTEND(performance-unnecessary-value-param)

namespace {

std::unordered_map<std::string, int64_t> lua_table_to_dim_map(const sol::table& t, const std::string& caller) {
    std::unordered_map<std::string, int64_t> dims;
    for (auto& pair : t) {
        auto key = pair.first.as<std::string>();
        dims[key] = lua_cell_as<int64_t>(pair.second, caller, "dimension '" + key + "'");
    }
    return dims;
}

// One quiver.metadata{...} field, as csv_options_entries returned it (absent = disengaged).
std::string metadata_string(const std::optional<sol::object>& value, const char* key, const std::string& fallback) {
    return value ? lua_cell_as<std::string>(*value, "metadata", std::string("field '") + key + "'") : fallback;
}

template <typename T>
std::vector<T> metadata_array(const std::optional<sol::object>& value, const char* key) {
    if (!value) {
        return {};
    }
    const auto field = std::string("field '") + key + "'";
    if (value->get_type() != sol::type::table) {
        throw std::runtime_error("Cannot metadata: " + field + " must be a table");
    }
    return lua_table_to_vector<T>(value->as<sol::table>(), "metadata: " + field);
}

// Build BinaryMetadata from a Lua kwargs table, mirroring the Julia Metadata(; ...) constructor:
// assemble an Element and delegate to from_element (which computes time-dimension initial values).
// Strict: a table (checked here, since sol2 does not check a table parameter in Release) with
// only these eight keys, each of the right type.
BinaryMetadata build_metadata_from_lua(const sol::object& t) {
    if (t.get_type() != sol::type::table) {
        throw std::runtime_error("Cannot metadata: options must be a table");
    }
    const auto found = csv_options_entries(
        t,
        "metadata",
        {"version",
         "initial_datetime",
         "unit",
         "labels",
         "dimensions",
         "dimension_sizes",
         "time_dimensions",
         "frequencies"}
    );
    Element el;
    el.set("version", metadata_string(found[0], "version", "1"));
    el.set("initial_datetime", metadata_string(found[1], "initial_datetime", ""));
    el.set("unit", metadata_string(found[2], "unit", ""));
    el.set("labels", metadata_array<std::string>(found[3], "labels"));
    el.set("dimensions", metadata_array<std::string>(found[4], "dimensions"));
    el.set("dimension_sizes", metadata_array<int64_t>(found[5], "dimension_sizes"));
    el.set("time_dimensions", metadata_array<std::string>(found[6], "time_dimensions"));
    el.set("frequencies", metadata_array<std::string>(found[7], "frequencies"));
    return BinaryMetadata::from_element(el);
}

sol::table dimension_to_lua(sol::state_view& lua, const Dimension& dim) {
    auto t = lua.create_table();
    t["name"] = dim.name;
    t["size"] = dim.size;
    t["is_time_dimension"] = dim.is_time_dimension();
    if (dim.is_time_dimension()) {
        t["frequency"] = frequency_to_string(dim.time->frequency);
        t["initial_value"] = dim.time->initial_value;
        t["parent_dimension_index"] = dim.time->parent_dimension_index;
    }
    return t;
}

// ------------------------------------------------------------------------
// Expression operator dispatch (shared by Expression and BinaryFile metamethods)
// ------------------------------------------------------------------------

enum class BinOp { Add, Subtract, Multiply, Divide, Gt, Lt, Gte, Lte, Eq, Neq, And, Or };

bool is_number(const sol::object& o) {
    return o.get_type() == sol::type::number;
}

// A Lua operand in arithmetic is either an Expression, a BinaryFile (auto-wrapped), or a number
// (handled by the scalar operator overloads). Numbers are rejected here on purpose.
Expression to_expression(const sol::object& o) {
    if (o.is<Expression>()) {
        return o.as<Expression>();
    }
    if (o.is<BinaryFile>()) {
        return Expression(o.as<BinaryFile&>());
    }
    throw std::runtime_error("Cannot build expression: operand must be an expression or a binary file");
}

// One body for all three operand combos (expr/expr, expr/double, double/expr); overload
// resolution on l/r picks the matching Expression operator per instantiation. double/double
// is never instantiated (binop_dispatch routes numbers through to_expression, which throws).
template <typename L, typename R>
Expression apply_binop(BinOp op, const L& l, const R& r) {
    switch (op) {
    case BinOp::Add:
        return l + r;
    case BinOp::Subtract:
        return l - r;
    case BinOp::Multiply:
        return l * r;
    case BinOp::Divide:
        return l / r;
    case BinOp::Gt:
        return l > r;
    case BinOp::Lt:
        return l < r;
    case BinOp::Gte:
        return l >= r;
    case BinOp::Lte:
        return l <= r;
    case BinOp::Eq:
        return l == r;
    case BinOp::Neq:
        return l != r;
    case BinOp::And:
        return l && r;
    case BinOp::Or:
        return l || r;
    }
    throw std::runtime_error("Cannot apply operator: unknown operation");
}

Expression binop_dispatch(BinOp op, const sol::object& lhs, const sol::object& rhs) {
    bool lnum = is_number(lhs);
    bool rnum = is_number(rhs);
    if (lnum && !rnum) {
        return apply_binop(op, lhs.as<double>(), to_expression(rhs));
    }
    if (!lnum && rnum) {
        return apply_binop(op, to_expression(lhs), rhs.as<double>());
    }
    return apply_binop(op, to_expression(lhs), to_expression(rhs));
}

// `caller` is the public method ("aggregate" / "aggregate_agents") named in the Pattern 1 message.
ExpressionAggregate::Operation parse_aggregate_op(const std::string& op, const std::string& caller) {
    if (op == "sum") {
        return ExpressionAggregate::Operation::Sum;
    }
    if (op == "mean") {
        return ExpressionAggregate::Operation::Mean;
    }
    if (op == "min") {
        return ExpressionAggregate::Operation::Min;
    }
    if (op == "max") {
        return ExpressionAggregate::Operation::Max;
    }
    if (op == "percentile") {
        return ExpressionAggregate::Operation::Percentile;
    }
    throw std::runtime_error("Cannot " + caller + ": unknown operation '" + op + "'");
}

// NOLINTBEGIN(performance-unnecessary-value-param) sol2 lambda bindings require pass-by-value for type
// deduction
// The arithmetic, unary-minus and logical metamethods shared by every usertype that behaves like an
// expression (BinaryFile auto-wraps to Expression). One table, so a new operator is added once.
template <typename T>
void bind_expression_operators(sol::usertype<T>& type) {
    type[sol::meta_function::addition] = [](sol::object a, sol::object b) { return binop_dispatch(BinOp::Add, a, b); };
    type[sol::meta_function::subtraction] = [](sol::object a, sol::object b) {
        return binop_dispatch(BinOp::Subtract, a, b);
    };
    type[sol::meta_function::multiplication] = [](sol::object a, sol::object b) {
        return binop_dispatch(BinOp::Multiply, a, b);
    };
    type[sol::meta_function::division] = [](sol::object a, sol::object b) {
        return binop_dispatch(BinOp::Divide, a, b);
    };
    type[sol::meta_function::unary_minus] = [](sol::object a, sol::object) { return -to_expression(a); };
    // Logical ops (nonzero = true, NaN propagates, unitless): `&` / `|` / `~`.
    type[sol::meta_function::bitwise_and] = [](sol::object a, sol::object b) {
        return binop_dispatch(BinOp::And, a, b);
    };
    type[sol::meta_function::bitwise_or] = [](sol::object a, sol::object b) { return binop_dispatch(BinOp::Or, a, b); };
    type[sol::meta_function::bitwise_not] = [](sol::object a, sol::object) { return !to_expression(a); };
}

}  // namespace

// ========================================================================
// Binary subsystem bindings (mirrors the Julia Binary.* surface)
// ========================================================================

void bind_binary(sol::state& lua, sol::usertype<Database>& bind, sol::table& ns, Database& db, RunHandles& handles) {
    // Binary subsystem file I/O — db-scoped and sandboxed: paths resolve against the directory
    // containing the database file and must stay inside it.
    bind.set_function(
        "open_file",
        [&handles](
            Database& self,
            const std::string& path,
            const std::string& mode,
            sol::optional<BinaryMetadata> metadata
        ) -> std::shared_ptr<BinaryFile> {
            if (mode.size() != 1 || (mode[0] != 'r' && mode[0] != 'w')) {
                throw std::runtime_error("Cannot open_file: mode must be \"r\" or \"w\"");
            }
            const auto resolved = resolve_sandboxed_path(self, "open_file", path);
            std::optional<BinaryMetadata> md = metadata ? std::optional<BinaryMetadata>(*metadata) : std::nullopt;
            auto file = std::make_shared<BinaryFile>(BinaryFile::open_file(resolved, mode[0], md));
            handles.open_binary_files.push_back(file);
            return file;
        }
    );
    bind.set_function("bin_to_csv", [](Database& self, const std::string& path, sol::optional<bool> aggregate) {
        CSVConverter::bin_to_csv(resolve_sandboxed_path(self, "bin_to_csv", path), aggregate.value_or(true));
    });
    bind.set_function("csv_to_bin", [](Database& self, const std::string& path) {
        CSVConverter::csv_to_bin(resolve_sandboxed_path(self, "csv_to_bin", path));
    });

    lua.new_usertype<BinaryMetadata>(
        "BinaryMetadata",
        sol::no_constructor,
        "get_unit",
        [](BinaryMetadata& self) -> std::string { return self.unit; },
        "get_version",
        [](BinaryMetadata& self) -> std::string { return self.version; },
        "get_initial_datetime",
        [](BinaryMetadata& self) -> std::string { return quiver::datetime::format_utc(self.initial_datetime); },
        "get_labels",
        [](BinaryMetadata& self, sol::this_state s) {
            sol::state_view lua(s);
            return to_lua_table(lua, self.labels);
        },
        "get_dimensions",
        [](BinaryMetadata& self, sol::this_state s) {
            sol::state_view lua(s);
            auto t = lua.create_table();
            for (size_t i = 0; i < self.dimensions.size(); ++i) {
                t[i + 1] = dimension_to_lua(lua, self.dimensions[i]);
            }
            return t;
        },
        "get_number_of_time_dimensions",
        [](BinaryMetadata& self) { return self.number_of_time_dimensions(); },
        "to_toml",
        [](BinaryMetadata& self) -> std::string { return self.to_toml(); }
    );

    auto binary_file_type = lua.new_usertype<BinaryFile>(
        "BinaryFile",
        sol::no_constructor,
        "read",
        [](BinaryFile& self, const sol::table& dims, sol::optional<bool> allow_nulls, sol::this_state s) {
            sol::state_view lua(s);
            auto data = self.read(lua_table_to_dim_map(dims, "read"), allow_nulls.value_or(false));
            return to_lua_table(lua, data);
        },
        "write",
        [](BinaryFile& self, const sol::table& data, const sol::table& dims) {
            self.write(lua_table_to_vector<double>(data, "write"), lua_table_to_dim_map(dims, "write"));
        },
        "close",
        [](BinaryFile& self) { self.close(); },
        "is_open",
        [](BinaryFile& self) { return self.is_open(); },
        "get_metadata",
        [](BinaryFile& self) -> BinaryMetadata { return self.get_metadata(); },
        "get_file_path",
        [](BinaryFile& self) -> std::string { return self.get_file_path(); }
    );
    // Arithmetic on files mirrors Julia: file_a + file_b, -file, file * 2.0 (auto-wrap to Expression)
    bind_expression_operators(binary_file_type);

    ns.set_function("metadata", [](const sol::object& t) { return build_metadata_from_lua(t); });
    ns.set_function("metadata_from_toml", [](const std::string& content) {
        return BinaryMetadata::from_toml_content(content);
    });
    ns.set_function("metadata_from_element", [](const sol::table& t) {
        return BinaryMetadata::from_element(table_to_element("metadata_from_element", t));
    });

    // Expression subsystem bindings (mirrors the Julia Expression surface)
    auto expression_type = lua.new_usertype<Expression>(
        "Expression",
        sol::no_constructor,
        "save",
        [&db](Expression& self, const std::string& path) { self.save(resolve_sandboxed_path(db, "save", path)); },
        "metadata",
        [](Expression& self) -> BinaryMetadata { return self.metadata(); },
        "aggregate",
        [](Expression& self, const std::string& dimension, const std::string& op, sol::optional<double> parameter) {
            return self.aggregate(
                dimension,
                parse_aggregate_op(op, "aggregate"),
                parameter ? std::optional<double>(*parameter) : std::nullopt
            );
        },
        "aggregate_agents",
        [](Expression& self, const std::string& op, sol::optional<double> parameter) {
            return self.aggregate_agents(
                parse_aggregate_op(op, "aggregate_agents"),
                parameter ? std::optional<double>(*parameter) : std::nullopt
            );
        },
        "select_agents",
        [](Expression& self, const sol::table& labels) {
            return self.select_agents(lua_table_to_vector<std::string>(labels, "select_agents"));
        },
        "rename_agents",
        [](Expression& self, const sol::object& mapping) {
            // sol2 does not check a table parameter in Release, so check it here. Then collect,
            // then check both halves: an unchecked as<std::string>() spelled a number
            // key as text and gave "" for a boolean in Release.
            if (mapping.get_type() != sol::type::table) {
                throw std::runtime_error("Cannot rename_agents: mapping must be a table");
            }
            std::vector<std::pair<sol::object, sol::object>> entries;
            mapping.as<sol::table>().for_each([&](sol::object key, sol::object value) {
                entries.emplace_back(std::move(key), std::move(value));
            });
            std::vector<std::pair<std::string, std::string>> pairs;
            for (const auto& [key, value] : entries) {
                auto old_name = lua_cell_as<std::string>(key, "rename_agents", "key");
                auto new_name = lua_cell_as<std::string>(value, "rename_agents", "value for '" + old_name + "'");
                pairs.emplace_back(std::move(old_name), std::move(new_name));
            }
            return self.rename_agents(pairs);
        }
    );
    bind_expression_operators(expression_type);

    ns.set_function("expression", [](sol::object o) { return to_expression(o); });
    ns.set_function("abs", [](sol::object o) { return quiver::abs(to_expression(o)); });
    ns.set_function("sqrt", [](sol::object o) { return quiver::sqrt(to_expression(o)); });
    ns.set_function("log", [](sol::object o) { return quiver::log(to_expression(o)); });
    ns.set_function("exp", [](sol::object o) { return quiver::exp(to_expression(o)); });
    ns.set_function("ifelse", [](sol::object c, sol::object t, sol::object e) {
        return quiver::ifelse(to_expression(c), to_expression(t), to_expression(e));
    });
    // Comparisons produce 1.0/0.0 per element (NaN operand -> NaN). Free functions because Lua
    // comparison metamethods are coerced to bool and cannot return an Expression.
    ns.set_function("gt", [](sol::object a, sol::object b) { return binop_dispatch(BinOp::Gt, a, b); });
    ns.set_function("lt", [](sol::object a, sol::object b) { return binop_dispatch(BinOp::Lt, a, b); });
    ns.set_function("gte", [](sol::object a, sol::object b) { return binop_dispatch(BinOp::Gte, a, b); });
    ns.set_function("lte", [](sol::object a, sol::object b) { return binop_dispatch(BinOp::Lte, a, b); });
    ns.set_function("eq", [](sol::object a, sol::object b) { return binop_dispatch(BinOp::Eq, a, b); });
    ns.set_function("neq", [](sol::object a, sol::object b) { return binop_dispatch(BinOp::Neq, a, b); });
    // Logical ops on boolean-valued expressions are the `&` / `|` / `~` metamethods bound on the
    // Expression and BinaryFile usertypes (`and`/`or`/`not` are Lua keywords, so no free functions).
}
// NOLINTEND(performance-unnecessary-value-param)

// Lua-visible handle behind db:write_csv. Wraps the internal writer; sol2 owns
// it via std::unique_ptr, registered with sol::no_constructor and no explicit finalizer.
// db:open_file's BinaryFile is sol2-owned through a std::shared_ptr instead, for the same reason
// `writer` below is one: RunHandles keeps a weak_ptr to it (open_binary_files).
struct CsvWriter {
    // shared_ptr, not a value: RunHandles keeps a weak_ptr to every writer it hands out so run()
    // can close the ones the script never closed, whether or not Lua can still reach them
    // (collect_garbage() alone only finalizes unreachable ones -- see close_open_writers).
    // It also means the Writer is constructed in place and never moved.
    std::shared_ptr<quiver::csv_write::Writer> writer;
    // 1-based ordinal of the NEXT data row to attempt, so the non-finite-number error
    // can name which w:write_row call failed (a row of forty cells with one bad value is
    // otherwise unfindable). Incremented only after a row is accepted -- a rejected row keeps
    // the ordinal unchanged, so a script that retries the same logical row after a pcall sees
    // the same number.
    std::int64_t next_row_index = 1;
    // Width of the header this writer was opened with (write_csv's decoded
    // csv_options.header.size()); 0 means no header was given and therefore no width
    // enforcement -- header = {} already means "no header row", so 0 is unambiguous.
    // Stored once at construction; the header vector itself is never read again.
    std::size_t header_width = 0;

    CsvWriter(std::shared_ptr<quiver::csv_write::Writer> w, std::size_t header_width_)
        : writer(std::move(w)), header_width(header_width_) {}
};

namespace {

// The MAXIMUM integer key of a Lua table, never sol::table::size()/lua_rawlen, plus
// the key rule and the width cap. Shared by csv_row_cells_from_lua and csv_header_from_lua so
// all three live in one place; `what` names the offending container in the messages ("row",
// "option 'header'").
std::int64_t csv_max_integer_key(const sol::table& t, const std::string& operation, const std::string& what) {
    std::int64_t max_index = 0;
    for (auto& pair : t) {
        if (!pair.first.is<std::int64_t>() || pair.first.as<std::int64_t>() < 1) {
            throw std::runtime_error("Cannot " + operation + ": " + what + " key must be a positive integer");
        }
        max_index = std::max(max_index, pair.first.as<std::int64_t>());
    }
    // Both callers materialize a dense vector up to max_index, so a single stray large key
    // ({ [1e9] = "x" }) would allocate that whole range -- tens of gigabytes, or a raw
    // std::bad_alloc/std::length_error reaching the script with no Pattern 1 prefix.
    // No real CSV record is this wide; reject it as a precondition failure instead.
    constexpr std::int64_t kMaxWidth = 1'000'000;
    if (max_index > kMaxWidth) {
        throw std::runtime_error(
            "Cannot " + operation + ": " + what + " key " + std::to_string(max_index) +
            " exceeds the maximum width of " + std::to_string(kMaxWidth)
        );
    }
    return max_index;
}

// One sol::object cell -> the std::string quiver::csv_write::Writer takes. Dispatch order is
// nil first (a hole from csv_row_cells_from_lua below is an empty cell), then the same
// order lua_to_value uses -- boolean, int64, double, string -- else a Pattern 1 rejection
// naming write_row and the 1-based cell index.
//
// This project compiles with SOL_SAFE_NUMERICS=1 (src/CMakeLists.txt), which is what makes
// is<>() safe here: under it a numeric-looking Lua STRING (e.g. "0012") answers false to
// is<std::int64_t>()/is<double>() and falls through to the string branch, written verbatim --
// it does NOT apply Lua's own string<->number coercion the way the as<>() family would.
// Verified by compiling and running this exact dispatch against this repo's own sol2/Lua
// build; do not add a get_type() guard here and do not reorder it.
std::string csv_cell_to_string(
    const sol::object& cell,
    const std::string& operation,
    std::int64_t index,
    std::int64_t row_index
) {
    if (!cell.valid() || cell.is<sol::lua_nil_t>()) {
        return {};
    }
    if (is_lua_boolean(cell)) {
        // The project-wide boolean-is-INTEGER-1/0 write policy: text "1" or "0", not
        // "true"/"false".
        return cell.as<bool>() ? "1" : "0";
    }
    if (cell.is<std::int64_t>()) {
        // The int64_t overload directly, never routed through double first, so a Lua
        // integer past double's 53-bit mantissa survives exactly.
        std::string out;
        quiver::utils::append_number(cell.as<std::int64_t>(), out);
        return out;
    }
    if (cell.is<double>()) {
        const double value = cell.as<double>();
        // Reject BEFORE append_number/to_chars is reached, so no platform-specific
        // non-finite spelling (MSVC's "-nan(ind)"/"nan"/"inf" vs. glibc's "nan"/"inf" --
        // both seen in practice) can ever reach a cell. Both the row and the cell are
        // named: a row of many cells with one bad value is otherwise unfindable.
        if (!std::isfinite(value)) {
            throw std::runtime_error(
                "Cannot " + operation + ": row " + std::to_string(row_index) + " cell #" + std::to_string(index) +
                " is not a finite number"
            );
        }
        // to_chars' shortest round-trip form, with no synthetic decimal point -- a whole
        // float and the equal integer produce identical text.
        std::string out;
        quiver::utils::append_number(value, out);
        return out;
    }
    if (cell.is<std::string>()) {
        return cell.as<std::string>();
    }
    throw std::runtime_error("Cannot " + operation + ": cell #" + std::to_string(index) + " has unsupported Lua type");
}

// Converts one Lua row table to the ordered std::vector<std::string> quiver::csv_write::Writer
// takes. Row width is the table's maximum integer key: an interior hole is exactly the
// shape a nullable db:read_csv result produces, and must become an empty cell rather than
// collapse the row. A missing key reads back as Lua nil, which csv_cell_to_string above turns
// into an empty cell -- so an interior nil and an absent key are structurally identical,
// exactly as they are in Lua itself.
std::vector<std::string> csv_row_cells_from_lua(
    const sol::table& row,
    const std::string& operation,
    std::int64_t row_index
) {
    const std::int64_t max_index = csv_max_integer_key(row, operation, "row");

    std::vector<std::string> cells(static_cast<std::size_t>(max_index));
    for (std::int64_t i = 1; i <= max_index; ++i) {
        cells[static_cast<std::size_t>(i - 1)] = csv_cell_to_string(row[i], operation, i, row_index);
    }
    return cells;
}

// Converts a `header` option table to an ordered list of column names, through the same
// csv_max_integer_key walk csv_row_cells_from_lua uses. Every present entry must be a
// string; an empty table (zero integer keys) yields an empty result, which csv_write::Options
// treats as no header row.
// A bad KEY gets csv_max_integer_key's own message ("option 'header' key must be a positive
// integer"), never the entry-type one below -- `{ header = { name = "a" } }` would otherwise
// be told to fix a value it already got right.
std::vector<std::string> csv_header_from_lua(const sol::table& header, const std::string& operation) {
    const std::int64_t max_index = csv_max_integer_key(header, operation, "option 'header'");

    std::vector<std::string> names(static_cast<std::size_t>(max_index));
    for (std::int64_t i = 1; i <= max_index; ++i) {
        const sol::object cell = header[i];
        if (!cell.is<std::string>()) {
            throw std::runtime_error("Cannot " + operation + ": option 'header' entry must be a string");
        }
        names[static_cast<std::size_t>(i - 1)] = cell.as<std::string>();
    }
    return names;
}

// The `separator` branch both decoders share, so the byte-vs-character rule and the messages
// the tests pin exist once.
char csv_separator_from_lua(const sol::object& value, const std::string& operation) {
    // Check the Lua type explicitly rather than routing through the checked-conversion
    // helper every other converter uses: that helper surfaces sol2's own stack-index
    // message, which is neither Pattern 1 nor stable across build types.
    if (value.get_type() != sol::type::string) {
        throw std::runtime_error("Cannot " + operation + ": option 'separator' must be a string");
    }
    const auto separator = value.as<std::string>();
    // Measured in BYTES -- a multi-byte UTF-8 character is rejected as multi-character.
    if (separator.size() != 1) {
        throw std::runtime_error("Cannot " + operation + ": option 'separator' must be a single character");
    }
    // A quote, a line terminator or a NUL is one byte but not a delimiter: csv-parser refuses
    // a delimiter that overlaps its quote character, and a CR/LF/NUL delimiter makes the
    // writer emit records this project's own reader can never put back together. Rejecting
    // here keeps db:write_csv from silently producing a file db:read_csv cannot parse.
    const char c = separator[0];
    if (c == '"' || c == '\r' || c == '\n' || c == '\0') {
        throw std::runtime_error(
            "Cannot " + operation + ": option 'separator' must not be a quote, carriage return, newline or NUL"
        );
    }
    return c;
}

// Strict decoder for db:write_csv's trailing options table, on the two shared helpers above.
csv_write::Options write_csv_options_from_lua(const sol::object& options, const std::string& operation) {
    csv_write::Options result;
    if (!options.valid() || options.get_type() == sol::type::lua_nil) {
        // Missing parameter or explicit nil -- same as an empty table, both valid.
        return result;
    }
    if (options.get_type() != sol::type::table) {
        throw std::runtime_error("Cannot " + operation + ": options must be a table");
    }

    const auto found = csv_options_entries(options, operation, {"separator", "header"});
    const auto& separator_value = found[0];
    const auto& header_value = found[1];

    if (separator_value) {
        result.separator = csv_separator_from_lua(*separator_value, operation);
    }

    if (header_value) {
        if (header_value->get_type() != sol::type::table) {
            throw std::runtime_error("Cannot " + operation + ": option 'header' must be a table");
        }
        result.header = csv_header_from_lua(header_value->as<sol::table>(), operation);
    }

    return result;
}

// Shared strict decoder for db:read_csv / db:read_csv_stream's trailing options table
// (one decoder so the two entry points cannot diverge on any
// option). `options` is `sol::object`, not `sol::optional<sol::table>`: sol2's optional checker
// never raises on a type mismatch on its own (see relation_target_from_lua, the pattern
// this copies), so a wrong type would otherwise silently fall through to defaults instead of
// throwing. `operation` is the
// caller's own method name ("read_csv" / "read_csv_stream"), so the same bad table reports
// whichever entry point the script actually called.
csv_read::Options read_csv_options_from_lua(const sol::object& options, const std::string& operation) {
    csv_read::Options result;
    if (!options.valid() || options.get_type() == sol::type::lua_nil) {
        // Missing parameter or explicit nil -- same as an empty table, both valid.
        return result;
    }
    if (options.get_type() != sol::type::table) {
        throw std::runtime_error("Cannot " + operation + ": options must be a table");
    }

    const auto found = csv_options_entries(options, operation, {"separator", "header_row"});
    const auto& separator_value = found[0];
    const auto& header_row_value = found[1];

    if (separator_value) {
        result.separator = csv_separator_from_lua(*separator_value, operation);
    }

    if (header_row_value) {
        // Same rationale as separator above: explicit get_type() rather than lua_cell_as
        // (stable Pattern 1 text). This also rules out a quoted "2", which Lua's own string->number
        // coercion would otherwise let through.
        if (header_row_value->get_type() != sol::type::number) {
            throw std::runtime_error("Cannot " + operation + ": option 'header_row' must be an integer");
        }
        // .is<int64_t>() rejects a fractional number (e.g. 2.5) -- the house idiom already
        // used for group-column indices (see collect_group_columns' cell.first.is<int64_t>()
        // check). SOL_SAFE_NUMERICS=1 is set unconditionally in src/CMakeLists.txt (not gated
        // on build type), so this precision check holds in Release too.
        if (!header_row_value->is<int64_t>()) {
            throw std::runtime_error("Cannot " + operation + ": option 'header_row' must be an integer");
        }
        const auto header_row = header_row_value->as<int64_t>();
        if (header_row < 0) {
            // A separate message from the type check above: "-1" IS an integer, so telling
            // the caller otherwise would be a lie.
            throw std::runtime_error("Cannot " + operation + ": option 'header_row' must not be negative");
        }
        result.header_row = header_row;
    }

    return result;
}

}  // namespace

// NOLINTBEGIN(performance-unnecessary-value-param) sol2 lambda bindings require pass-by-value for type
// deduction
void bind_csv(sol::state& lua, sol::usertype<Database>& bind, RunHandles& handles) {
    // CSV file reading/writing -- db-scoped and sandboxed like the file I/O above. The two
    // reading entry points below construct the same csv_read reader and drive it through
    // header()/for_each_row(), so they cannot diverge on any input. Writing
    // (db:write_csv) is streaming-only -- there is no whole-file counterpart, by decision.
    bind.set_function(
        "read_csv",
        [](Database& self, const std::string& path, sol::object options, sol::this_state s) -> sol::table {
            sol::state_view lua(s);
            // Evaluation order: sandbox checks (in-memory db, path escape) before the
            // options table, so a bad separator never masks an escaping path.
            const auto resolved = resolve_sandboxed_path(self, "read_csv", path);
            auto csv_options = read_csv_options_from_lua(options, "read_csv");
            csv_read::Reader reader(resolved, path, "read_csv", csv_options);

            // Each row is handed to Lua as it is parsed, exactly as read_csv_stream does, so
            // the file exists once (in Lua) rather than twice -- staging every row in a
            // std::vector first and converting afterwards held a complete C++ copy alongside
            // the complete Lua copy for the whole conversion.
            auto rows = lua.create_table();
            reader.for_each_row([&lua, &rows](std::vector<std::string>&& cells, int64_t index) {
                rows[index] = to_lua_table(lua, cells);
                return true;
            });

            auto result = lua.create_table();
            // `header` is absent (not an empty table) when the file has no header (header_row = 0):
            // {} is truthy in Lua and nil is falsy, so a script testing `if result.header` must see
            // nil. db:read_csv_stream passes nil to on_row for the same reason; the two forms must not
            // diverge.
            const auto& header = reader.header();
            if (!header.empty()) {
                result["header"] = to_lua_table(lua, header);
            }
            result["rows"] = rows;
            return result;
        }
    );
    bind.set_function(
        "read_csv_stream",
        [](Database& self, const std::string& path, sol::object on_row_arg, sol::object options, sol::this_state s)
            -> int64_t {
            sol::state_view lua(s);
            // sol::object plus an explicit type check, not a typed
            // sol::protected_function parameter: a typed one surfaces sol2's own
            // "stack index 3, expected function" text, which is neither Pattern 1
            // nor stable across build types -- the same reason `options`
            // is decoded by hand.
            if (on_row_arg.get_type() != sol::type::function) {
                throw std::runtime_error("Cannot read_csv_stream: on_row must be a function");
            }
            const sol::protected_function on_row = on_row_arg.as<sol::protected_function>();
            // Evaluation order: sandbox checks before the options table.
            const auto resolved = resolve_sandboxed_path(self, "read_csv_stream", path);
            auto csv_options = read_csv_options_from_lua(options, "read_csv_stream");
            csv_read::Reader reader(resolved, path, "read_csv_stream", csv_options);

            // Built once, before the loop, and passed by reference into every callback
            // invocation -- reachable during the stream so a script can find a column by
            // name before processing row 1.
            //
            // Nil, not an empty table, when there is no header -- exactly the guard
            // db:read_csv uses above for its `header` key. The two forms must not
            // diverge on the same input, and here that is behavioural rather
            // than cosmetic: `{}` is truthy in Lua and `nil` is falsy, so a script
            // written as `if header then ... end` would take opposite branches between
            // the whole-file and streaming forms of the same file under header_row = 0.
            const auto& header_names = reader.header();
            const sol::object header_table =
                header_names.empty() ? sol::object(sol::lua_nil) : sol::object(to_lua_table(lua, header_names));

            return reader.for_each_row([&](std::vector<std::string>&& cells, int64_t index) -> bool {
                const auto row_table = to_lua_table(lua, cells);
                auto result = on_row(row_table, index, header_table);
                if (!result.valid()) {
                    // Propagate the Lua error verbatim and unwrapped: the reader is a
                    // stack local and ~CSVReader() joins its scheduler during normal C++
                    // unwinding, so no manual cleanup is needed here.
                    sol::error err = result;
                    throw std::runtime_error(err.what());
                }
                // sol::optional<bool> is a strict LUA_TBOOLEAN check, Debug/Release-identical.
                // Only an exact `false` stops the read -- get<bool>() would be
                // lua_toboolean truthiness and misread a no-return callback's nil as "stop".
                if (result.return_count() > 0 && result.get<sol::optional<bool>>(0) == false) {
                    return false;
                }
                return true;
            });
        }
    );
    bind.set_function(
        "write_csv",
        [&handles](Database& self, const std::string& path, sol::object options) -> std::unique_ptr<CsvWriter> {
            // Evaluation order: sandbox checks (in-memory db, path escape) before
            // the options table, so a bad separator never masks an escaping path.
            const auto resolved = resolve_sandboxed_path(self, "write_csv", path);
            auto csv_options = write_csv_options_from_lua(options, "write_csv");
            if (handles.path_has_open_writer(resolved)) {
                throw std::runtime_error("Cannot write_csv: file is already open for writing: " + path);
            }
            const auto header_width = csv_options.header.size();
            auto writer = std::make_shared<quiver::csv_write::Writer>(resolved, path, "write_csv", csv_options);
            // Registered so close_open_writers() can flush it at run()'s exit even
            // when the script leaves it reachable (a global), which the GC cannot.
            handles.open_writers.emplace_back(resolved, writer);
            return std::make_unique<CsvWriter>(std::move(writer), header_width);
        }
    );

    // sol::no_constructor + std::unique_ptr return (above), no explicit finalizer.
    // BinaryFile is the same except for its holder, a std::shared_ptr (see db:open_file).
    lua.new_usertype<CsvWriter>(
        "CsvWriter",
        sol::no_constructor,
        "write_row",
        [](CsvWriter& self, const sol::object& row) {
            // sol2's table check for a `const sol::table&` parameter is a LOOSE one that also
            // accepts userdata, and iterating a userdata yields no keys -- so w:write_row(db)
            // silently appended an empty record instead of being rejected. Check the Lua type
            // first; this also turns sol2's raw "stack index 2, expected table" for a
            // string/number/nil argument into a Pattern 1 message.
            if (row.get_type() != sol::type::table) {
                throw std::runtime_error("Cannot write_row: row must be a table");
            }
            // Check the closed state BEFORE formatting a single cell -- cells were
            // previously formatted as csv_write::Writer::write_row's argument, evaluated before
            // the call, so a write after close on a bad row raised the wrong error. Delegating
            // to Writer with an empty vector reuses its own closed-writer message verbatim
            // (never reached: Writer checks closed_ before touching cells) instead of
            // duplicating the text here.
            if (self.writer->is_closed()) {
                self.writer->write_row({}, "write_row");
                return;
            }
            const auto row_index = self.next_row_index;
            auto cells = csv_row_cells_from_lua(row.as<sol::table>(), "write_row", row_index);
            // header_width == 0 means no header was given, so no enforcement applies.
            // A row wider than the header is never truncated -- it throws, naming the 1-based
            // data-row ordinal and both counts (pinned in src/csv/csv_write.cpp's
            // message catalogue comment -- reword both together). A short row is padded BEFORE
            // Writer::write_row ever sees it -- append_record is a pure function of the vector
            // it receives, so padding after the call would be too late.
            if (self.header_width != 0) {
                if (cells.size() > self.header_width) {
                    throw std::runtime_error(
                        "Cannot write_row: row " + std::to_string(row_index) + " has " + std::to_string(cells.size()) +
                        " cells but header declares " + std::to_string(self.header_width)
                    );
                }
                if (cells.size() < self.header_width) {
                    cells.resize(self.header_width);
                }
            }
            self.writer->write_row(cells, "write_row");
            ++self.next_row_index;
        },
        "close",
        [](CsvWriter& self) { self.writer->close("close"); }
    );
}
// NOLINTEND(performance-unnecessary-value-param)

// True while some writer this run handed out for `resolved_path` is alive and unclosed. Two
// writers on one path each open with ios::trunc and write from offset 0, so the second one
// silently discards everything the first buffered -- the same hazard db:open_file's
// process-global write registry (src/binary/binary_file.cpp) already refuses. Reopening a path
// whose previous writer was closed stays legal (it truncates).
bool RunHandles::path_has_open_writer(const std::string& resolved_path) const {
    for (const auto& [path, weak] : open_writers) {
        if (path != resolved_path) {
            continue;
        }
        if (const auto writer = weak.lock(); writer && !writer->is_closed()) {
            return true;
        }
    }
    return false;
}

// The run-exit flush mechanism, for every writer and binary file handle. A CsvWriter the script
// left reachable -- `w = db:write_csv(...)` without `local`, the Lua default -- is a GC root, so
// collect_garbage() never finalizes it and its buffered rows never reach disk. Closing through
// this registry instead makes the flush independent of reachability, which is what the documented
// guarantee ("the file is complete and re-readable even without w:close()") actually promises.
void RunHandles::close_open_writers() {
    for (const auto& [path, weak] : open_writers) {
        if (const auto writer = weak.lock()) {
            try {
                writer->close("close");
            } catch (const std::exception&) {
                // Runs at run()'s scope exit, including exception unwinding: there is no caller
                // to report a flush failure to. ~Writer swallowed it identically before.
            }
        }
    }
    open_writers.clear();
    for (const auto& weak : open_binary_files) {
        if (const auto file = weak.lock()) {
            try {
                file->close();
            } catch (const std::exception&) {
                // Same as the CSV loop above: nobody to report a flush failure to at scope exit.
            }
        }
    }
    open_binary_files.clear();
}

}  // namespace quiver::lua_internal

namespace quiver {

struct LuaRunner::Impl {
    Database& db;
    // Declared before `lua`: the state, and every closure that captured `handles`, is torn down first.
    lua_internal::RunHandles handles;
    sol::state lua;

    explicit Impl(Database& database) : db(database) {
        lua.open_libraries(
            sol::lib::base,
            sol::lib::string,
            sol::lib::table,
            sol::lib::math,
            sol::lib::coroutine,
            sol::lib::utf8
        );
        // Scripts may not load Lua source from disk; string-form load() stays available.
        lua["dofile"] = sol::lua_nil;
        lua["loadfile"] = sol::lua_nil;
        sol::table ns = lua.create_named_table("quiver");
        // The only Database usertype: registering it again would clear every method bound before.
        auto bind = lua.new_usertype<Database>("Database");
        lua_internal::bind_core(bind);
        lua_internal::bind_read(bind);
        lua_internal::bind_write(bind);
        lua_internal::bind_metadata(bind);
        lua_internal::bind_time_series(bind);
        lua_internal::bind_csv(lua, bind, handles);
        lua_internal::bind_binary(lua, bind, ns, db, handles);
        lua["db"] = &db;
    }
};

LuaRunner::LuaRunner(Database& db) : impl_(std::make_unique<Impl>(db)) {}

LuaRunner::~LuaRunner() = default;

LuaRunner::LuaRunner(LuaRunner&&) noexcept = default;

LuaRunner& LuaRunner::operator=(LuaRunner&&) noexcept = default;

std::string LuaRunner::run(const std::string& script) {
    // A writer (or any other unique_ptr + sol::no_constructor usertype, e.g. CsvWriter)
    // the script leaves unreachable at run()'s return is never collected on its own -- sol::state
    // is a long-lived member of Impl, so nothing forces a GC cycle between script executions.
    // GcGuard's destructor runs one full collection, unconditionally, on every exit path (normal
    // return, the empty-return, and exception unwinding alike), which synchronously finalizes any
    // such object and therefore flushes/closes its underlying resource (a probe
    // ran this exact one-call-suffices check against this repo's own vendored sol2/Lua build).
    // Declared BEFORE `result`: C++ destroys stack locals in reverse declaration order, so this
    // guard (declared first) is destroyed AFTER `result` (declared second) -- releasing
    // `result`'s Lua stack reference before the collection below runs. Declaring the guard after
    // `result` would collect while a live stack reference still anchors the script's userdata.
    // close_open_writers() runs first and does NOT depend on reachability: it closes CSV writers and
    // binary files. A writer the script assigned to a global (`w = db:write_csv(...)`, the Lua
    // default spelling) is a GC root, so collect_garbage() alone would leave its rows in the ofstream
    // buffer and the file at 0 bytes -- and a db:open_file writer's path in the write registry.
    // The collection still runs afterwards for every other unique_ptr usertype.
    struct GcGuard {
        Impl& impl;
        ~GcGuard() {
            impl.handles.close_open_writers();
            impl.lua.collect_garbage();
        }
    } gc_guard{*impl_};

    auto result = impl_->lua.safe_script(script, sol::script_pass_on_error);
    if (!result.valid()) {
        sol::error err = result;
        throw std::runtime_error(std::string("Failed to run Lua script: ") + err.what());
    }

    if (result.return_count() == 0) {
        return {};
    }

    return lua_internal::encode_return_json(result.get<sol::object>(0));
}

}  // namespace quiver
