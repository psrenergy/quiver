# Phase 3: Dedupe - Pattern Map

**Mapped:** 2026-10-03
**Files analyzed:** 11 (all existing; no new TU)
**Analogs found:** 11 / 11 (in-file analogs; the dedupe converges on shapes already in `internal.h` / the files themselves)

Note: the repo has **no** existing `template <auto ...>` member-pointer adapter and no `&Database::x`
registration (grep over `src/`: zero hits). M3/M4/M7/M8 introduce that shape; the target code is in
03-RESEARCH.md (compiled on MSVC/clang/GCC). The analogs below are the *current* code each helper
replaces, which fixes the signature that must stay byte-identical.

## File Classification

| File (lines now) | Items | Role | Data Flow | Closest Analog | Match |
|---|---|---|---|---|---|
| `src/lua_runner/internal.h` (225) | M4 adapters, M9 `option_entries`/`collect_entries`/`option_table`, M13 decls, M16 drop `join_column_names` decl | utility (templates/inline) | transform | `to_lua_table` (53-86), `csv_options_entries` (152-187) | exact |
| `src/lua_runner/db_core.cpp` (231) | M1, M3, M5, M9 caller, IN-01 | binder | request-response | own lambdas 137-218 | exact |
| `src/lua_runner/db_read.cpp` (236) | M4, M5 ternaries, M7 | binder | request-response | `read_scalar_strings_lua` 14-22, registrations 213-236 | exact |
| `src/lua_runner/db_metadata.cpp` (164) | M8 | binder | transform | own `scalar_metadata_lua`/`group_metadata_lua` 30-148 | exact |
| `src/lua_runner/db_write.cpp` (311) | M3 (288-296), M16 "contain no rows" into `columns_to_cpp_rows` (120) | binder | request-response | own | exact |
| `src/lua_runner/db_time_series.cpp` (289) | M3 (271-273), M4 (237-240), M16 `length_mismatch` | binder | request-response | own | exact |
| `src/lua_runner/csv.cpp` (446) | M9, M10, M11, M12, M13 caller, IN-02 | binder + usertype | file-I/O | `lua_to_value` (internal.h:120), usertype style binary.cpp:244 | exact |
| `src/lua_runner/binary.cpp` (377) | M9/M15 named slots, M14 functors, M13 caller | binder + usertype | transform | own `found[0..7]` 73-80, `BinOp` 101-150 | exact |
| `src/lua_runner/lua_runner.cpp` (144) | M13 `add_*`, rename `close_open_handles` | service (registry) | event-driven (run exit) | own `close_open_writers` 38-60 | exact |
| `src/csv/csv_write.cpp` | comment only (M10, lines 26-31) | doc | - | - | - |
| `src/AGENTS.md` | layout bullet 633-645, lines 145, 271, 698, 713, 819/829/833 | doc | - | - | - |

## Pattern Assignments

### `internal.h` (M4) — copy the parameter list of today's reader verbatim
Analog `db_read.cpp:14-22`; the template must keep this exact signature (Debug bad-argument text unchanged):
```cpp
sol::table read_scalar_strings_lua(
    Database& db,
    const std::string& collection,
    const std::string& attribute,
    sol::this_state s
) {
    sol::state_view lua(s);
    return to_lua_table(lua, db.read_scalar_strings(collection, attribute));
}
```
Target: `template <auto Read> sol::table bulk_read_lua(...)` body `return to_lua_table(lua, (db.*Read)(collection, attribute));`
plus 1-arg `collection_read_lua`. Place next to `to_lua_table` (internal.h:53-86). Registration line shape
(db_read.cpp:216-235) becomes `bind.set_function("read_scalar_strings", &bulk_read_lua<&Database::read_scalar_strings>);`
— registered name must equal member name (grep check).

### `internal.h` (M9) — generalize `csv_options_entries` (152-187)
Keep the collect-then-validate walk and both messages byte-identical:
```cpp
options.as<sol::table>().for_each([&](sol::object key, sol::object value) {
    entries.emplace_back(std::move(key), std::move(value));
});
...
if (entry.first.get_type() != sol::type::string) {
    throw std::runtime_error("Cannot " + operation + ": option key must be a string");
}
...
    throw std::runtime_error("Cannot " + operation + ": unknown option '" + name + "'");
```
Changes: extract the `for_each` into `collect_entries`; move the "options must be a table" check inside
(before collect); `template <std::size_t N>` with `const std::string_view (&allowed)[N]` returning
`std::array<std::optional<sol::object>, N>`. Nil early-return stays in each CSV caller; `quiver.metadata` has none.

### `db_core.cpp` (M1, M3, M5)
Analog: own registrations 140-166:
```cpp
bind.set_function("is_healthy", [](Database& self) { return self.is_healthy(); });
bind.set_function("path", [](Database& self) -> const std::string& { return self.path(); });
bind.set_function("transaction", [](Database& self, sol::protected_function fn) -> sol::object {
```
- M3: 17 forwarders -> `bind.set_function("is_healthy", &Database::is_healthy);` (list in RESEARCH §M3).
- M1: `run_in_scope` in anon namespace; keep both lambdas with the exact `(Database& self, sol::protected_function fn)` signature.
- M5: `query_*_lua` (90-133) return `std::optional<T>`, drop `sol::this_state`; move `NOLINTBEGIN` (137) above line 90 (IN-01).

### `db_read.cpp` (M5, M7)
- M5: `read_scalars_by_id_lua` (79-108) assigns optional directly: `result[attribute.name] = db.read_scalar_integer_by_id(collection, attribute.name, id);`
- M7: `read_vectors_by_id_lua` (110-135) / `read_sets_by_id_lua` (137-162) bodies become `read_groups_by_id<List, I, F, S>`; keep both named wrappers (used by `read_element_by_id_lua` 164-177) and the `default:` throw text.

### `db_metadata.cpp` (M8)
Rename `scalar_metadata_lua`/`group_metadata_lua` to overloaded `metadata_to_lua`; 4 list + 4 get fns -> `list_metadata_lua<auto List>` / `get_metadata_lua<auto Get>` with the same signature as today's functions. `lua_data_type_name` `default:` stays (Phase 4).

### `csv.cpp` (M10, M11, M12)
- M10 analog `internal.h:120-137` (`lua_to_value`, order nil -> bool -> int64 -> double -> string -> throw). `csv_cell_to_string` = `std::visit` over `lua_to_value(cell, operation, "cell #" + std::to_string(index))`; keep finite-number message. Do not reorder/guard `lua_to_value`.
- M11 analog: usertype one-arg-per-line style, `binary.cpp:244-247`:
```cpp
lua.new_usertype<BinaryMetadata>(
    "BinaryMetadata",
    sol::no_constructor,
    "get_unit",
    [](BinaryMetadata& self) -> std::string { return self.unit; },
```
  Target replaces lambdas with `"write_row", &CsvWriter::write_row, "close", &CsvWriter::close`; members defined after the anon namespace. Rename `bind_csv` param `lua` -> `state` (IN-02, also internal.h:206).
- M12: `header_object(lua, header)` returns `sol::object(sol::lua_nil)` when empty; used at 308-316 and 350-352.

### `binary.cpp` (M14, M15)
- M15 replaces `found[0..7]` (73-80) with structured bindings over `option_entries`; keep `build_metadata_from_lua` **above** the first `new_usertype` (sync-test Pass 2 hazard).
- M14 removes `enum class BinOp` (101) / `apply_binop` (123-150); `template <typename Op> Expression binop(const sol::object&, const sol::object&)`; mapping table in RESEARCH §M14 must be exact; `ns.set_function("gt", &binop<std::greater<>>);` one literal per call. Add `#include <functional>`. `unary_minus`/`bitwise_not` lambdas stay.

### `lua_runner.cpp` + `internal.h:37-51` (M13)
Add `add_writer(const std::string&, const std::shared_ptr<csv_write::Writer>&)` / `add_binary_file(const std::shared_ptr<BinaryFile>&)` with `std::erase_if(..., expired())` then append; rename `close_open_writers` -> `close_open_handles` (internal.h:44,50; lua_runner.cpp:38,118,126; csv.cpp:27,386; src/AGENTS.md:819,829,833). Prune expired only.

### `db_write.cpp` / `db_time_series.cpp` (M16)
Move "contain no rows" throw to the top of `columns_to_cpp_rows` (db_write.cpp:120), delete both caller copies (db_time_series.cpp:166-171, db_write.cpp:229-234); make `join_column_names` anon-namespace and drop internal.h:219 decl. One `length_mismatch(...)` for db_time_series.cpp:141-144 and 159-162, predicates unchanged.

## Shared Patterns

- **Registration:** every `db:` method is `bind.set_function("name", ...)`; every `quiver.*` is `ns.set_function("name", ...)`. Sync test: 71 + 15.
- **Error text:** Pattern 1 `"Cannot " + operation + ": ..."`, byte-identical; no reordered checks.
- **Lua state access:** `sol::this_state s` param + `sol::state_view lua(s);` then `to_lua_table(lua, ...)`.
- **Lint:** one `NOLINTBEGIN/END(performance-unnecessary-value-param)` pair per TU; new helpers take `const&`.
- **Placement:** templates/inline in `internal.h`; TU-local helpers in anon namespace inside `quiver::lua_internal`; usertype types stay in the named namespace. No new `.cpp`.

## No Analog Found

| Shape | Use instead |
|---|---|
| `template <auto MemberPtr>` adapters (M4/M7/M8) and `&Database::x` registration (M3) | 03-RESEARCH.md §M3/M4/M7/M8 (compiled shapes) |
| Transparent-functor `binop<Op>` (M14) | 03-RESEARCH.md §M14 |

## Metadata
**Search scope:** `src/lua_runner/`, `src/` (member-pointer / `template <auto` grep)
**Pattern extraction date:** 2026-10-03
