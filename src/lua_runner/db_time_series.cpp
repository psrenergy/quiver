#include "lua_runner/internal.h"
#include "quiver/database.h"
#include "quiver/value.h"

#include <sol/sol.hpp>

#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace quiver::lua_internal {

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
    bind.set_function("has_time_series_files", &Database::has_time_series_files);

    bind.set_function("read_time_series_group", &read_time_series_group_lua);
    bind.set_function("read_time_series_row", &read_time_series_row_lua);
    bind.set_function("read_time_series_files", &read_time_series_files_lua);

    bind.set_function("update_time_series_group", &update_time_series_group_lua);
    bind.set_function("update_time_series_group_by_label", &update_time_series_group_by_label_lua);
    bind.set_function("upsert_time_series_row", &upsert_time_series_row_lua);
    bind.set_function("upsert_time_series_row_by_label", &upsert_time_series_row_by_label_lua);
    bind.set_function("update_time_series_files", &update_time_series_files_lua);

    bind.set_function(
        "list_time_series_files_columns",
        &collection_read_lua<&Database::list_time_series_files_columns>
    );
}
// NOLINTEND(performance-unnecessary-value-param)

}  // namespace quiver::lua_internal
