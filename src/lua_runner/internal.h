#ifndef QUIVER_SRC_LUA_RUNNER_INTERNAL_H
#define QUIVER_SRC_LUA_RUNNER_INTERNAL_H

#include "lua_runner/path_policy.h"
#include "quiver/database.h"
#include "quiver/element.h"
#include "quiver/value.h"

#include <sol/sol.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
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

class AbstractExpression;
class BinaryFile;
class Expression;

}  // namespace quiver

// sol2 inheritance as compile-time traits, not the runtime base-classes tag: the tag swaps each derived
// metatable's __index table for a C closure that every f:read / f:write pays. They are explicit
// specializations, so every TU that uses sol2 with these types must see them: every src/lua_runner TU that
// includes sol2 includes this header first.
SOL_BASE_CLASSES(quiver::BinaryFile, quiver::AbstractExpression);
SOL_BASE_CLASSES(quiver::Expression, quiver::AbstractExpression);
SOL_DERIVED_CLASSES(quiver::AbstractExpression, quiver::BinaryFile, quiver::Expression);

namespace quiver {

namespace csv_write {

class Writer;

}  // namespace csv_write

namespace lua_internal {

struct RunHandles {
    // Every writer db:write_csv has handed out during the current run(), by resolved path. Weak,
    // so a writer the script did drop (and the GC did collect) simply expires; expired entries are
    // pruned on every insert, and the list is cleared at run()'s exit.
    std::vector<std::pair<std::string, std::weak_ptr<quiver::csv_write::Writer>>> open_writers;

    // Every BinaryFile db:open_file handed out during the current run(), readers and writers
    // alike, so close_open_handles() can close it at run()'s exit even when the script keeps it in
    // a global (a GC root). A writer left open would otherwise keep its path in the process-wide
    // write registry until the LuaRunner is destroyed. Pruned and cleared like open_writers.
    std::vector<std::weak_ptr<BinaryFile>> open_binary_files;

    bool path_has_open_writer(const std::string& resolved_path) const;
    void add_writer(const std::string& resolved_path, const std::shared_ptr<quiver::csv_write::Writer>& writer);
    void add_binary_file(const std::shared_ptr<BinaryFile>& file);
    void close_open_handles();
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

sol::table metadata_to_lua(sol::state_view& lua, const ScalarMetadata& attribute);
sol::table metadata_to_lua(sol::state_view& lua, const GroupMetadata& metadata);

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

// Every boolean test in src/lua_runner/ goes through this one predicate, so the rule lives in one
// place rather than in a comment repeated at each site. The Value mapping itself lives in
// lua_to_value (scalars, row upserts, query parameters, group cells, CSV cells) and lua_cell_as
// (the typed paths, e.g. arrays).
inline bool is_lua_boolean(const sol::object& v) {
    return v.get_type() == sol::type::boolean;
}

// The checked Lua-value→T conversion for the typed paths (arrays, dimensions, file paths,
// quiver.metadata fields, rename_agents names, enum_labels codes); lua_to_value below is its
// Value-typed sibling. Load-bearing:
// sol2's plain `get<T>` is unchecked in every build (`src/CMakeLists.txt` sets SOL_SAFE_GETTER=0
// to keep the bulk readers inside their cost budget), so a mismatched value silently yields
// 0 / 0.0 / ""; and a checked getter would only raise sol2's raw text through a luaL_error
// longjmp, never a Pattern 1 message. A boolean is INTEGER 1/0 for a numeric `T`, matching the
// scalar policy; anything that does not fit `T` is a Pattern 1 rejection. `what` names the
// offending slot ("cell #3", "dimension 'stage'").
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

// Every entry of `t`, collected before any of them is validated, so a mid-traversal throw cannot
// abandon sol2's traversal state. The one place a decoder walks a Lua table it then checks.
inline std::vector<std::pair<sol::object, sol::object>> collect_entries(const sol::table& t) {
    std::vector<std::pair<sol::object, sol::object>> entries;
    t.for_each([&](sol::object key, sol::object value) { entries.emplace_back(std::move(key), std::move(value)); });
    return entries;
}

// Lua's own type() name for a value; a missing argument reads as nil, as in Lua itself. A usertype
// is "userdata".
inline std::string lua_type_name(const sol::object& o) {
    const auto t = o.get_type();
    return t == sol::type::none ? "nil" : sol::type_name(o.lua_state(), t);
}

// The one shape of an argument type error: "Cannot <op>: <what> must be <expected>, got <type>".
inline std::runtime_error lua_type_error(
    const std::string& operation,
    const std::string& what,
    const char* expected,
    const sol::object& got
) {
    return std::runtime_error(
        "Cannot " + operation + ": " + what + " must be " + expected + ", got " + lua_type_name(got)
    );
}

// The one table check. It tests get_type(), never is<sol::table>(): sol2's table check is loose
// and accepts a userdata, which lua_next would then walk. Called by the decoder that first walks
// the argument, so the check fires where sol2's own up-front check used to.
inline sol::table require_table(
    const sol::object& o,
    const std::string& operation,
    const std::string& what,
    const char* expected = "a table"
) {
    if (o.get_type() != sol::type::table) {
        throw lua_type_error(operation, what, expected, o);
    }
    return o.as<sol::table>();
}

// A map key that names something (an attribute, a column, a dimension). Checked before it is
// converted: sol2's string getter spells a number key as text and has no text for a boolean.
inline std::string lua_string_key(const sol::object& key, const std::string& operation, const std::string& what) {
    if (key.get_type() != sol::type::string) {
        throw lua_type_error(operation, what, "a string", key);
    }
    return key.as<std::string>();
}

// An optional argument, with luaL_opt semantics: nil or a missing argument is absent, and any
// other value must be a T (checked strictly: a boolean, a number, a BinaryMetadata, or a table
// through require_table) or it is a type error naming `what`.
template <typename T>
std::optional<T> optional_from_lua(
    const sol::object& o,
    const std::string& operation,
    const std::string& what,
    const char* expected
) {
    if (!o.valid() || o.get_type() == sol::type::lua_nil) {
        return std::nullopt;
    }
    if constexpr (std::is_same_v<T, sol::table>) {
        return require_table(o, operation, what, expected);
    } else {
        if (auto value = o.as<sol::optional<T>>()) {
            return std::move(*value);
        }
        throw lua_type_error(operation, what, expected, o);
    }
}

// A nested option value that must be a table (an options table's `header`, the levels of
// `enum_labels`); `what` names it in the message.
inline sol::table option_table(const sol::object& value, const std::string& operation, const std::string& what) {
    return require_table(value, operation, "option '" + what + "'");
}

// The walk every strict options decoder shares: read_csv, write_csv, export_csv/import_csv and
// quiver.metadata. It owns the table check; what nil means stays with each caller (the CSV
// decoders return defaults before calling it, quiver.metadata lets nil reach the check). Then
// every entry is collected (collect_entries) before any is validated. `allowed` is the option
// names this caller accepts, in precedence order; an unknown key is the first thing rejected,
// and the result is parallel to `allowed` (an absent option is a disengaged optional), so a
// caller binds it by name: `const auto& [a, b] = option_entries(options, op, {"a", "b"});`.
template <std::size_t N>
std::array<std::optional<sol::object>, N> option_entries(
    const sol::object& options,
    const std::string& operation,
    const std::string_view (&allowed)[N]
) {
    std::array<std::optional<sol::object>, N> found;
    for (auto& entry : collect_entries(require_table(options, operation, "options"))) {
        // Check the key's Lua type before converting it: sol2's std::string getter is
        // lua_tolstring, which answers nullptr for a boolean/table/function key and is
        // unchecked in every build (SOL_SAFE_GETTER=0), so a `{ [true] = 1 }` options table
        // would reach the script as a bare Lua value rather than a Pattern 1 message.
        if (entry.first.get_type() != sol::type::string) {
            throw std::runtime_error("Cannot " + operation + ": option key must be a string");
        }
        const auto name = entry.first.as<std::string>();
        const auto it = std::find(std::begin(allowed), std::end(allowed), name);
        if (it == std::end(allowed)) {
            throw std::runtime_error("Cannot " + operation + ": unknown option '" + name + "'");
        }
        found[static_cast<std::size_t>(std::distance(std::begin(allowed), it))] = entry.second;
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

void bind_database(sol::usertype<Database>& bind);
void bind_create(sol::usertype<Database>& bind);
void bind_read(sol::usertype<Database>& bind);
void bind_update(sol::usertype<Database>& bind);
void bind_delete(sol::usertype<Database>& bind);
void bind_describe(sol::usertype<Database>& bind);
void bind_metadata(sol::usertype<Database>& bind);
void bind_query(sol::usertype<Database>& bind);
void bind_time_series(sol::usertype<Database>& bind);
void bind_csv_export(sol::usertype<Database>& bind);
void bind_csv_import(sol::usertype<Database>& bind);
void bind_csv(sol::state& state, sol::usertype<Database>& bind, RunHandles& handles);
sol::usertype<BinaryFile> bind_binary(
    sol::state& state,
    sol::usertype<Database>& bind,
    sol::table& ns,
    RunHandles& handles
);
void bind_expression(sol::state& state, sol::table& ns, sol::usertype<BinaryFile>& binary_file_type, Database& db);

std::string encode_return_json(const sol::object& value);

Element table_to_element(const std::string& caller, const sol::object& values);
std::vector<GroupColumn> collect_group_columns(const std::string& caller, const sol::object& columns);
std::vector<std::map<std::string, Value>> columns_to_cpp_rows(
    const std::string& caller,
    const std::vector<GroupColumn>& lua_columns,
    size_t row_count
);
CSVOptions parse_csv_options(const sol::object& options, const std::string& operation);

}  // namespace lua_internal

}  // namespace quiver

#endif  // QUIVER_SRC_LUA_RUNNER_INTERNAL_H
