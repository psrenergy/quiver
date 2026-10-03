# Phase 4: Fixes and Release Type Safety - Pattern Map

**Mapped:** 2026-10-03
**Files analyzed:** 17 (all modified; no new source or test files, per D-13)
**Analogs found:** 16 / 17 (only the CaptureStderr usage has no in-repo analog)

## File Classification

| Modified File | Role | Data Flow | Closest Analog | Match Quality |
|---|---|---|---|---|
| `src/lua_runner/internal.h` (new helpers `lua_type_name`, `lua_type_error`, `require_table`, `lua_string_key`, `optional_from_lua<T>`) | utility | transform/validation | `option_table` / `option_entries` in the same file (lines ~179-215) | exact |
| `src/lua_runner/db_core.cpp` (`run_in_scope` C4/C6, `query_*` optionals) | binding | request-response | `read_csv_stream` `on_row` check, `src/lua_runner/csv.cpp:346-357` | exact |
| `src/lua_runner/db_write.cpp` (`table_to_element`, `collect_group_columns`, C7 skip at `:61`) | binding/decoder | transform | `option_entries` key check (`internal.h`) | role-match |
| `src/lua_runner/db_time_series.cpp` (`lua_table_to_value_map`, `update_time_series_files_lua`, D1 delete) | binding/decoder | transform | same | role-match |
| `src/lua_runner/binary.cpp` (`lua_table_to_dim_map`, optionals, `binop` factory, `to_expression(o, caller)`, `rename_agents`, `metadata_array`) | binding | transform | `option_table` + RESEARCH inventory | role-match |
| `src/lua_runner/csv.cpp` (`write_row` reroute) | binding | file-I/O | `option_table` | exact |
| `src/lua_runner/db_metadata.cpp` (`lua_data_type_name` -> lowercase of `data_type_to_string`) | utility | transform | explicit-ASCII lowercase rule, `src/AGENTS.md:212` | role-match |
| `src/lua_runner/lua_runner.cpp` (text-only `load`) | config/sandbox | n/a | `dofile`/`loadfile` nil-out, `lua_runner.cpp:100-102` | exact |
| `src/CMakeLists.txt` (sol2 defines) | config | n/a | existing PRIVATE block `src/CMakeLists.txt:71-76` | exact |
| `tests/test_lua_runner_*.cpp`, `tests/test_lua_binary.cpp`, `tests/test_lua_expression.cpp` | test | n/a | `expect_lua_error` (`tests/test_lua_runner.h:48-56`), e.g. `test_lua_expression.cpp:315-327` | exact |
| `tests/test_lua_runner_errors.cpp` (stderr silence test, D-09) | test | n/a | none (no `CaptureStderr` in `tests/`); DB-open pattern from `tests/test_lua_runner_describe.cpp:5-11` | partial |
| `CHANGELOG.md` | docs | n/a | `## [0.12.8]` section (line 8) + link block (line 1297) | exact |
| `bindings/js/src/lua-api.ts` (`:308-309`, `:198`, `:105`) | docs | n/a | existing text | exact |
| `AGENTS.md`, `src/AGENTS.md`, `tests/AGENTS.md` | docs | n/a | existing sections cited in RESEARCH | exact |

## Pattern Assignments

### `src/lua_runner/internal.h` helpers

**Analog:** `option_table` (current):
```cpp
inline sol::table option_table(const sol::object& value, const std::string& operation, const std::string& what) {
    if (value.get_type() != sol::type::table) {
        throw std::runtime_error("Cannot " + operation + ": option '" + what + "' must be a table");
    }
    return value.as<sol::table>();
}
```
Key check to mirror in `lua_string_key` (`option_entries`):
```cpp
if (entry.first.get_type() != sol::type::string) {
    throw std::runtime_error("Cannot " + operation + ": option key must be a string");
}
const auto name = entry.first.as<std::string>();
```
Rule: guard with `get_type()`, never `is<sol::table>()` / `sol::optional<sol::table>` (loose; accepts userdata). Helper bodies: RESEARCH "Helper design". Then `option_table` becomes `require_table(value, operation, "option '" + what + "'")` and `option_entries`' table check becomes `require_table(options, operation, "options")`. Keep the `option key must be a string` text (pinned).

### `src/lua_runner/db_core.cpp` `run_in_scope` (C6 + C4)

**Analog:** `src/lua_runner/csv.cpp:346-357`:
```cpp
[](Database& self, const std::string& path, sol::object on_row_arg, sol::object options, sol::this_state s)
...
    if (on_row_arg.get_type() != sol::type::function) {
        throw std::runtime_error("Cannot read_csv_stream: on_row must be a function");
    }
    const sol::protected_function on_row = on_row_arg.as<sol::protected_function>();
```
Current `run_in_scope` (`db_core.cpp:70-92`): `finish()` sits outside any try (C4). Target shape: RESEARCH "Pattern: run_in_scope" (type check before `begin`, `finish` inside try, best-effort `abort` + rethrow). Error text for the Lua-error path must stay `std::runtime_error(err.what())` (golden `tx_err`/`dry_err`).

### `src/lua_runner/lua_runner.cpp` text-only `load`

**Analog:** `lua_runner.cpp:100-102`:
```cpp
// Scripts may not load Lua source from disk; string-form load() stays available.
lua["dofile"] = sol::lua_nil;
lua["loadfile"] = sol::lua_nil;
```
Insert the `lua.safe_script(...)` wrapper (RESEARCH "Pattern: text-only load") directly after these lines, before `create_named_table("quiver")`. Never `lua.set_function("load", ...)` (breaks `lua-api-sync.test.ts:73` and the `.set_function(` = 86 gate).

### `src/CMakeLists.txt`

**Analog:** current block (lines ~71-76):
```cmake
# sol2 safety settings
target_compile_definitions(quiver PRIVATE
    SOL_SAFE_NUMERICS=1
    SOL_SAFE_FUNCTION=1
    SOL_NO_NIL=1
)
```
Edit in place (last commit): add `SOL_ALL_SAFETIES_ON=1`, `SOL_PRINT_ERRORS=0`; delete `SOL_SAFE_FUNCTION=1`; keep PRIVATE.

### Tests (all per-domain files)

**Analog helper:** `tests/test_lua_runner.h:48-56`:
```cpp
inline void expect_lua_error(quiver::LuaRunner& lua, const std::string& script, const std::string& substring) {
    try {
        lua.run(script);
        FAIL() << "expected script to throw: " << script;
    } catch (const std::exception& e) {
        EXPECT_NE(std::string(e.what()).find(substring), std::string::npos) << e.what();
    }
}
```
Existing usage to copy (`tests/test_lua_expression.cpp:315-327`): `expect_lua_error(lua, "...", "Cannot rename_agents: mapping must be a table");` — substring still matches after `, got <type>` is appended (D-03). New tests assert the full `..., got number` / `got userdata` / `got nil` text.

Placement: C1/C2 element+group -> `test_lua_runner_create.cpp` / `_update.cpp` / `_time_series.cpp`; query optionals -> `_query`; binary/expr -> `test_lua_binary.cpp` / `test_lua_expression.cpp`; transaction C4/C6 -> `_transaction`; `load`, stderr, dot-call -> `test_lua_runner_errors.cpp`.

### stderr-silence test (D-09) — no analog

Use `testing::internal::CaptureStderr()` / `GetCapturedStderr()` around `lua.run(...)` only. DB must be opened quiet, copy from `tests/test_lua_runner_describe.cpp:5-11`:
```cpp
quiver::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"),
    {.read_only = false, .console_level = quiver::LogLevel::Off});
```
Two scripts: `pcall(function() db:commit() end)` and uncaught `db:commit()` (wrapped in try/catch in the test). Both `EXPECT_EQ(captured, "")`.

### `CHANGELOG.md`

Insert above `## [0.12.8] — 2026-10-01` (line 8):
```markdown
## [0.13.0] — unreleased

### Changed
- **BREAKING** ...

### Fixed
- ...
```
Link block (line ~1297) — prepend: `[0.13.0]: https://github.com/psrenergy/quiver/compare/v0.12.9...v0.13.0`. Note: the file has no `[0.12.9]` section/link today; the planner should confirm `v0.12.9` exists via `git tag` (memory note) before using it as the compare base.

## Shared Patterns

### Pattern 1 type error (all binding files)
Single builder `lua_type_error(operation, what, expected, got)` -> `Cannot <op>: <what> must be <expected>, got <lua type>`. `<op>` is the public Lua method name (expression metamethods: `add/sub/mul/div/unm/band/bor/bnot`, D-17).

### Check order (Pitfall 8)
Checks live in the shared decoder that first walks the argument; never above `resolve_sandboxed_path`. Hoist decodes out of single-call argument lists (`bin_to_csv`, `file:read`, `file:write`, `aggregate*`) into ordered locals.

### Converter messages untouched (D-04)
`lua_to_value` / `lua_cell_as` "has unsupported Lua type" texts stay byte-identical; D-14 userdata check in `table_to_element` goes before dispatch.

## No Analog Found

| File | Role | Reason |
|---|---|---|
| `tests/test_lua_runner_errors.cpp` (stderr test) | test | No `CaptureStderr` usage in `tests/`; use gtest's API per RESEARCH |

## Metadata

**Search scope:** `src/lua_runner/`, `src/CMakeLists.txt`, `tests/`, `CHANGELOG.md`
**Pattern extraction date:** 2026-10-03
