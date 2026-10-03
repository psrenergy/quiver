#ifndef QUIVER_SRC_LUA_RUNNER_INTERNAL_H
#define QUIVER_SRC_LUA_RUNNER_INTERNAL_H

#include "quiver/database.h"
#include "quiver/element.h"
#include "quiver/value.h"

#include <sol/sol.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace quiver {

class BinaryFile;

namespace csv_write {

class Writer;

}  // namespace csv_write

namespace lua_internal {

struct RunHandles {
    // Every writer db:write_csv has handed out during the current run(), by resolved path. Weak,
    // so a writer the script did drop (and the GC did collect) simply expires; cleared at each
    // run()'s exit.
    std::vector<std::pair<std::string, std::weak_ptr<quiver::csv_write::Writer>>> open_writers;

    // Every BinaryFile db:open_file handed out during the current run(), readers and writers
    // alike, so close_open_writers() can close it at run()'s exit even when the script keeps it in
    // a global (a GC root). A writer left open would otherwise keep its path in the process-wide
    // write registry until the LuaRunner is destroyed.
    std::vector<std::weak_ptr<BinaryFile>> open_binary_files;

    bool path_has_open_writer(const std::string& resolved_path) const;
    void close_open_writers();
};

template <typename T>
sol::table to_lua_table(sol::state_view& lua, const std::vector<T>& values) {
    auto t = lua.create_table();
    for (size_t i = 0; i < values.size(); ++i) {
        t[i + 1] = values[i];
    }
    return t;
}

// Nullable reads (scalar bulk, vector/set cells): a SQL NULL becomes a nil hole, preserving
// positional index. #t over holes is unreliable — for scalars read_element_ids is the
// count/position authority; a vector/set inner list has none, so trailing NULLs are invisible.
template <typename T>
sol::table to_lua_table(sol::state_view& lua, const std::vector<std::optional<T>>& values) {
    auto t = lua.create_table();
    for (size_t i = 0; i < values.size(); ++i) {
        if (values[i]) {
            t[i + 1] = *values[i];
        }
        // else: leave index i + 1 absent (nil).
    }
    return t;
}

template <typename T>
sol::table to_lua_table(sol::state_view& lua, const std::vector<std::vector<T>>& values) {
    auto outer = lua.create_table();
    for (size_t i = 0; i < values.size(); ++i) {
        outer[i + 1] = to_lua_table(lua, values[i]);
    }
    return outer;
}

// The bulk readers, bound straight to the Database member they read: `Read` is the member pointer
// (read_{scalar,vector,set}_{integers,floats,strings} for bulk_read_lua; read_element_ids and
// list_time_series_files_columns for collection_read_lua). The parameter lists are the ones sol2
// sees, so each registration keeps its argument checks.
template <auto Read>
sol::table bulk_read_lua(Database& db, const std::string& collection, const std::string& attribute, sol::this_state s) {
    sol::state_view lua(s);
    return to_lua_table(lua, (db.*Read)(collection, attribute));
}

template <auto Read>
sol::table collection_read_lua(Database& db, const std::string& collection, sol::this_state s) {
    sol::state_view lua(s);
    return to_lua_table(lua, (db.*Read)(collection));
}

// Every boolean test in src/lua_runner/ goes through this one predicate, so the rule lives in one
// place rather than in a comment repeated at each site. The Value mapping itself lives in
// lua_to_value (scalars, row upserts, query parameters, group cells) and lua_cell_as (the typed
// paths, e.g. arrays).
inline bool is_lua_boolean(const sol::object& v) {
    return v.get_type() == sol::type::boolean;
}

// The checked Lua-value→T conversion for the typed paths (arrays, dimensions, file paths,
// quiver.metadata fields, rename_agents names, enum_labels codes); lua_to_value below is its
// Value-typed sibling. Load-bearing:
// sol2's plain `get<T>` is unchecked whenever SOL_SAFE_GETTER is off — which is every release
// build (`src/CMakeLists.txt` sets SOL_SAFE_NUMERICS and SOL_SAFE_FUNCTION, not
// SOL_SAFE_GETTER), where a mismatched value silently yields 0 / 0.0 / "" while a Debug build
// aborts on a sol2 panic, so the two disagree on the same script. A boolean is INTEGER 1/0 for
// a numeric `T`, matching the scalar policy; anything that does not fit `T` is a Pattern 1
// rejection. `what` names the offending slot ("cell #3", "dimension 'stage'").
template <typename T>
T lua_cell_as(const sol::object& cell, const std::string& caller, const std::string& what) {
    if constexpr (std::is_arithmetic_v<T>) {
        if (is_lua_boolean(cell)) {
            return static_cast<T>(cell.as<bool>() ? 1 : 0);
        }
    }
    auto value = cell.as<sol::optional<T>>();
    if (!value) {
        throw std::runtime_error("Cannot " + caller + ": " + what + " has unsupported Lua type");
    }
    return std::move(*value);
}

// The one Lua-value -> Value conversion: nil -> NULL, boolean -> INTEGER 1/0 (the cross-layer
// write policy), then int64, double, string; anything else is Pattern 1 naming the slot. The
// Value-typed sibling of lua_cell_as<T>, with the same arguments and message shape.
inline Value lua_to_value(const sol::object& v, const std::string& caller, const std::string& what) {
    if (v.is<sol::lua_nil_t>()) {
        return nullptr;
    }
    if (is_lua_boolean(v)) {
        return v.as<bool>() ? int64_t{1} : int64_t{0};
    }
    if (v.is<int64_t>()) {
        return v.as<int64_t>();
    }
    if (v.is<double>()) {
        return v.as<double>();
    }
    if (v.is<std::string>()) {
        return v.as<std::string>();
    }
    throw std::runtime_error("Cannot " + caller + ": " + what + " has unsupported Lua type");
}

// The only table→vector converter. Every cell goes through `lua_cell_as`, which
// `table_to_element`'s array dispatch depends on: it picks the element type from cell 1 alone.
template <typename T>
std::vector<T> lua_table_to_vector(const sol::table& t, const std::string& caller) {
    const size_t count = t.size();
    std::vector<T> result;
    result.reserve(count);
    for (size_t i = 1; i <= count; ++i) {
        result.push_back(lua_cell_as<T>(t.get<sol::object>(i), caller, "cell #" + std::to_string(i)));
    }
    return result;
}

// The collect-then-validate walk every strict options decoder shares: read_csv,
// write_csv, export_csv/import_csv and quiver.metadata. Every entry is collected before any of
// them is validated, so a mid-traversal throw cannot abandon sol2's traversal state. `allowed`
// is the option names this caller accepts, in precedence order; an unknown key is the first
// thing rejected, and the returned vector is parallel to `allowed` (an absent option is a
// disengaged optional).
inline std::vector<std::optional<sol::object>> csv_options_entries(
    const sol::object& options,
    const std::string& operation,
    std::initializer_list<std::string_view> allowed
) {
    std::vector<std::optional<sol::object>> found(allowed.size());

    std::vector<std::pair<sol::object, sol::object>> entries;
    options.as<sol::table>().for_each([&](sol::object key, sol::object value) {
        entries.emplace_back(std::move(key), std::move(value));
    });

    for (auto& entry : entries) {
        // Check the key's Lua type before converting it: sol2's std::string getter is
        // lua_tolstring, which answers nullptr for a boolean/table/function key -- unchecked
        // in Release (SOL_SAFE_GETTER is off there) and a raw sol2 panic in Debug, so a
        // `{ [true] = 1 }` options table would reach the script as a bare Lua value rather
        // than a Pattern 1 message.
        if (entry.first.get_type() != sol::type::string) {
            throw std::runtime_error("Cannot " + operation + ": option key must be a string");
        }
        const auto name = entry.first.as<std::string>();
        const auto it = std::find(allowed.begin(), allowed.end(), name);
        if (it == allowed.end()) {
            throw std::runtime_error("Cannot " + operation + ": unknown option '" + name + "'");
        }
        found[static_cast<std::size_t>(std::distance(allowed.begin(), it))] = entry.second;
    }
    return found;
}

// One named column of the column-oriented update payload: its Lua table plus the extent
// (largest 1-based integer key; 0 when empty) and the number of non-nil cells. nil cells
// never appear during table iteration, so count < extent means the column has holes.
// Computed via iteration instead of sol::table::size() because lua_rawlen on a table with
// holes returns an arbitrary border.
struct GroupColumn {
    std::string name;
    sol::table values;
    size_t extent = 0;
    size_t count = 0;
};

void bind_core(sol::usertype<Database>& bind);
void bind_read(sol::usertype<Database>& bind);
void bind_write(sol::usertype<Database>& bind);
void bind_metadata(sol::usertype<Database>& bind);
void bind_time_series(sol::usertype<Database>& bind);
void bind_csv(sol::state& lua, sol::usertype<Database>& bind, RunHandles& handles);
void bind_binary(sol::state& lua, sol::usertype<Database>& bind, sol::table& ns, Database& db, RunHandles& handles);

std::string resolve_sandboxed_path(const Database& db, const std::string& operation, const std::string& path);
std::string encode_return_json(const sol::object& value);

Element table_to_element(const std::string& caller, const sol::table& values);
std::vector<GroupColumn> collect_group_columns(const std::string& caller, const sol::table& columns);
std::vector<std::map<std::string, Value>> columns_to_cpp_rows(
    const std::string& caller,
    const std::vector<GroupColumn>& lua_columns,
    size_t row_count
);
std::string join_column_names(const std::vector<GroupColumn>& lua_columns);

}  // namespace lua_internal

}  // namespace quiver

#endif  // QUIVER_SRC_LUA_RUNNER_INTERNAL_H
