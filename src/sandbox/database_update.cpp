#include "sandbox/internal.h"
#include "quiver/database.h"
#include "quiver/element.h"
#include "quiver/value.h"

#include <sol/sol.hpp>

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace quiver::lua_internal {

namespace {

// Join the column names for the empty-group rejection in columns_to_cpp_rows.
std::string join_column_names(const std::vector<GroupColumn>& lua_columns) {
    std::string joined;
    for (const auto& column : lua_columns) {
        joined += (joined.empty() ? "" : ", ") + column.name;
    }
    return joined;
}

}  // namespace

std::vector<GroupColumn> collect_group_columns(const std::string& caller, const sol::object& columns) {
    std::vector<GroupColumn> result;
    for (auto& pair : require_table(columns, caller, "columns")) {
        // Check the key's type before converting it. sol2's string getter is unchecked in every
        // build (SOL_SAFE_GETTER=0): key 1 would become column "1" and a boolean key column "",
        // so an array of row tables would get a misleading error instead of this one.
        if (pair.first.get_type() != sol::type::string) {
            throw std::runtime_error(
                "Cannot " + caller +
                ": column names must be strings; pass { column = { values... } }, "
                "not an array of row tables"
            );
        }
        auto name = pair.first.as<std::string>();
        GroupColumn column{name, require_table(pair.second, caller, "column '" + name + "'", "an array of values")};
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
// It also owns the one rejection of named columns that transpose to zero rows, so neither group
// decoder can turn such a payload into a silent clear (only an empty table {} clears). Both
// decoders call it last, after every check of their own.
std::vector<std::map<std::string, Value>> columns_to_cpp_rows(
    const std::string& caller,
    const std::vector<GroupColumn>& lua_columns,
    size_t row_count
) {
    if (row_count == 0) {
        throw std::runtime_error(
            "Cannot " + caller + ": columns [" + join_column_names(lua_columns) +
            "] contain no rows; pass an empty table {} to clear the group"
        );
    }
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

namespace {

void update_element_lua(Database& db, const std::string& collection, int64_t id, const sol::object& values) {
    auto element = table_to_element("update_element", values);
    db.update_element(collection, id, element);
}

void update_element_by_label_lua(
    Database& db,
    const std::string& collection,
    const std::string& label,
    const sol::object& values
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
// writer's treatment of value columns. Named columns that reach no index at all throw (in
// columns_to_cpp_rows) instead of silently clearing the group; an empty table {} clears.
std::vector<std::map<std::string, Value>> group_rows_from_lua(const std::string& caller, const sol::object& columns) {
    auto lua_columns = collect_group_columns(caller, columns);
    if (lua_columns.empty()) {
        return {};
    }

    size_t row_count = 0;
    for (const auto& column : lua_columns) {
        row_count = std::max(row_count, column.extent);
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
    const sol::object& columns
) {
    db.update_vector_group(collection, group, id, group_rows_from_lua("update_vector_group", columns));
}

void update_vector_group_by_label_lua(
    Database& db,
    const std::string& collection,
    const std::string& group,
    const std::string& label,
    const sol::object& columns
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
    const sol::object& columns
) {
    db.update_set_group(collection, group, id, group_rows_from_lua("update_set_group", columns));
}

void update_set_group_by_label_lua(
    Database& db,
    const std::string& collection,
    const std::string& group,
    const std::string& label,
    const sol::object& columns
) {
    db.update_set_group_by_label(collection, group, label, group_rows_from_lua("update_set_group_by_label", columns));
}

}  // namespace

void bind_update(sol::usertype<Database>& bind) {
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

}  // namespace quiver::lua_internal
