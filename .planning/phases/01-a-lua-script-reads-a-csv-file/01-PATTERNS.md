# Phase 1: A Lua script reads a CSV file - Pattern Map

**Mapped:** 2026-09-15
**Files analyzed:** 8 (2 new source, 1 new test, 5 modified)
**Analogs found:** 8 / 8

This phase's own CONTEXT.md/RESEARCH.md already pin exact file:line citations for every pattern
(27-subagent adversarial research + a verification pass). This map restates them in
planner-consumable form and adds nothing beyond what was already verified against the live tree
this session.

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|---|---|---|---|---|
| `src/csv_read.h` | utility (internal header, no public counterpart) | transform | `src/utils/datetime.h` (header-only internal utility) — structurally; no prior internal `.cpp`+`.h` pair with a `.cpp` exists | role-match, new pattern |
| `src/csv_read.cpp` | service (parser wrapper) | streaming / batch | `src/database_csv_import.cpp` (rapidcsv usage + Pattern 1 errors) | role-match |
| `src/lua_runner.cpp` (2 new `bind.set_function` entries) | binding / controller | request-response + streaming | `bind.set_function("csv_to_bin"...)` / `"bin_to_csv"` at :441-446 (sandboxed file-op registration); `"transaction"` at :290-306 (protected_function callback idiom); `relation_target_from_lua` at :1006 (strict sol::object decode) | exact |
| `cmake/Dependencies.cmake` | config | batch (build-time) | `rapidcsv` FetchContent block (already present, same file) | exact |
| `src/CMakeLists.txt` | config | batch (build-time) | `QUIVER_SOURCES` list + `target_link_libraries(quiver PRIVATE ... rapidcsv)` (same file) | exact |
| `bindings/js/src/lua-api.ts` | config / doc | request-response (build-gate) | existing `LUA_DB_API_REFERENCE` sandbox bullet (:97-99) + existing `db:bin_to_csv`/`db:csv_to_bin` entries | exact |
| `tests/test_lua_runner_read_csv.cpp` (new) | test | request-response | `tests/test_lua_runner.h` (`LuaSandboxTest` fixture) + existing `class LuaRunner_ImportCSV : public LuaSandboxTest` sibling suite | exact |
| `tests/CMakeLists.txt` | config | batch | explicit source list (no glob) — add new test file by name | exact |

## Pattern Assignments

### `src/csv_read.h` + `src/csv_read.cpp` (service, streaming/batch)

**Analog:** `src/database_csv_import.cpp` (rapidcsv + Pattern 1 conventions) and
`src/lua_runner.cpp:771-805` (`resolve_sandboxed_path`, for the operation-name-threading idiom).

No direct analog exists for "internal `.cpp` with no public header" — D-09 flags this as a new
pattern (`src/CLAUDE.md` File Map needs a row saying so). Structure to follow:

**Error wrapping pattern** (Pattern 1, `CLAUDE.md` root — copy verbatim shape):
```cpp
throw std::runtime_error("Cannot " + operation + ": " + reason);
```
Applied per D-22's catalogue, e.g.:
```cpp
throw std::runtime_error("Cannot " + operation + ": file '" + original_path + "' is empty");
```
`operation` is threaded in from the caller (`"read_csv"` or `"read_csv_stream"`, per D-19) exactly
the way `resolve_sandboxed_path`'s own `operation` parameter is threaded — same idiom, same file.

**CSVFormat construction (one place only, D-12):**
```cpp
csv::CSVFormat format;
format.delimiter(separator);
format.variable_columns(csv::VariableColumnPolicy::KEEP_NON_EMPTY);
format.header_row(0);
// never format.guess_csv(); never format.chunk_size(...)
```

**Wrap CSVReader construction/iteration in try/catch** (D-20), translating any csv-parser
exception into the Pattern-1 wrapper (`Cannot <op>: cannot read file '<p>': <reason>`), quoting the
**caller's own path spelling** (`original_path`), not the resolved absolute path (D-21) —
`resolve_sandboxed_path`'s two-path design (candidate vs `path` param) is the precedent: it already
returns the resolved path while quoting `path` (the raw arg) in its own error message
(`src/lua_runner.cpp:788`: `"path '" + path + "' escapes..."`).

**Cell copy (D-04, Pitfall 4 — never retain `CSVRow`):**
```cpp
for (csv::CSVRow& row : reader) {
    std::vector<std::string> cells;
    cells.reserve(row.size());
    for (csv::CSVField& field : row) {
        cells.emplace_back(field.get<std::string_view>());
    }
    // use `cells`; let `row` die at end of this iteration
}
```

---

### `src/lua_runner.cpp` — `db:read_csv` / `db:read_csv_stream` bindings

**Analog 1 — sandboxed file-op registration site** (`src/lua_runner.cpp:441-446`):
```cpp
bind.set_function("bin_to_csv", [](Database& self, const std::string& path, sol::optional<bool> aggregate) {
    CSVConverter::bin_to_csv(resolve_sandboxed_path(self, "bin_to_csv", path), aggregate.value_or(true));
});
bind.set_function("csv_to_bin", [](Database& self, const std::string& path) {
    CSVConverter::csv_to_bin(resolve_sandboxed_path(self, "csv_to_bin", path));
});
// db:read_csv / db:read_csv_stream land here — no NOLINT wrapper, this region has none
```
This block is a plain (non-`NOLINTBEGIN`-wrapped) sequence of `bind.set_function` calls sitting
right after the binary-subsystem file ops (`open_file`, `bin_to_csv`, `csv_to_bin`). New bindings
are two more entries in the same sequence.

**Analog 2 — `resolve_sandboxed_path`** (`src/lua_runner.cpp:771-805`) — reuse verbatim for LUA-04:
```cpp
static std::string
resolve_sandboxed_path(const Database& db, const std::string& operation, const std::string& path) {
    namespace fs = std::filesystem;
    const std::string& db_path = db.path();
    if (db_path == ":memory:") {
        throw std::runtime_error("Cannot " + operation +
                                 ": database is in-memory, file operations are unavailable");
    }
    auto root = fs::path(db_path).parent_path();
    if (root.empty()) { root = fs::current_path(); }
    root = fs::weakly_canonical(root);
    auto candidate = fs::path(path);
    if (candidate.is_relative()) { candidate = root / candidate; }
    candidate = fs::weakly_canonical(candidate);
    const auto rel = candidate.lexically_relative(root);
    if (rel.empty() || rel == "." || rel.begin()->string() == "..") {
        throw std::runtime_error("Cannot " + operation + ": path '" + path + "' escapes the database directory '" +
                                 root.string() + "'");
    }
    return candidate.string();
}
```
Does **not** check existence — "file not found"/"path is a directory" (D-22 entries 7-8) must be
checked separately, after this call, inside the new csv helper.

**Analog 3 — strict `sol::object` option decoding** (`src/lua_runner.cpp:1006-1016`,
`relation_target_from_lua`) — the pattern to copy for D-15/D-16/D-17 (do **not** copy
`parse_csv_options` at `:807`, which silently ignores unknown keys and uses the wrong
`sol::optional<sol::table>` parameter type):
```cpp
static std::optional<std::string> relation_target_from_lua(const sol::object& target_label,
                                                           const std::string& caller) {
    if (!target_label.valid() || target_label.get_type() == sol::type::lua_nil) {
        return std::nullopt;
    }
    if (target_label.get_type() != sol::type::string) {
        throw std::runtime_error("Cannot " + caller + ": target_label has unsupported Lua type");
    }
    return target_label.as<std::string>();
}
```
Apply the same shape to the new shared options decoder: `sol::object`, explicit `get_type()`
checks, unknown-key-throws, single-char string check for `separator`.

**Analog 4 — `sol::protected_function` callback idiom** (`src/lua_runner.cpp:290-306`,
`db:transaction`) — copy verbatim for D-08's stream-callback error propagation:
```cpp
[](Database& self, sol::protected_function fn) -> sol::object {
    self.begin_transaction();
    auto result = fn(std::ref(self));
    if (!result.valid()) {
        sol::error err = result;
        try { self.rollback(); } catch (...) {}
        throw std::runtime_error(err.what());
    }
    self.commit();
    if (result.return_count() > 0) { return result.get<sol::object>(0); }
    return sol::make_object(result.lua_state(), sol::lua_nil);
}
```
For the stream callback: call `fn(row_table, index, header_table)`, check `result.valid()` the
same way, and additionally check the early-stop:
```cpp
if (result.return_count() > 0 && result.get<sol::optional<bool>>(0) == false) {
    break; // stop the read; never use get<bool>() here (see D-06)
}
```

**Analog 5 — `to_lua_table` marshaler** (`src/lua_runner.cpp` — flat + nested overloads shown
above, e.g. flat `std::vector<T>` overload) — reuse for both `header` (flat `vector<string>`) and
each row / the whole `rows` (nested `vector<vector<string>>`); **do not** write a new
`to_lua_row`-shaped helper (`src/CLAUDE.md` names `to_lua_table<T>` the only vector→table
marshaler).

---

### `cmake/Dependencies.cmake` (config, batch)

**Analog:** the existing `rapidcsv` FetchContent block (same file, immediately above where the new
block is added) and the `lua`-dependency forced-cache-variable pattern (`LUA_TESTS`,
`LUA_LINE_EDITOR`) for the "FORCE cache vars before `FetchContent_MakeAvailable`" idiom:
```cmake
# rapidcsv for CSV reading/writing (header-only)
FetchContent_Declare(rapidcsv
    GIT_REPOSITORY https://github.com/d99kris/rapidcsv.git
    GIT_TAG v8.92
)
FetchContent_MakeAvailable(rapidcsv)
```
New block (per D-11, add directly after this):
```cmake
FetchContent_Declare(csv_parser
    GIT_REPOSITORY https://github.com/vincentlaucsb/csv-parser.git
    GIT_TAG 5.3.0
)
set(CSV_ENABLE_THREADS OFF CACHE BOOL "" FORCE)
set(CSV_NO_SIMD ON CACHE BOOL "" FORCE)
set(CSV_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(CSV_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(csv_parser)
set_target_properties(csv_no_simd PROPERTIES EXCLUDE_FROM_ALL YES)
```
Do **not** copy the `lua-cmake` `EXCLUDE_FROM_ALL`-is-unsafe warning comment by analogy — that
warning is specific to `lua-cmake`'s `install()` rules; csv-parser declares none, so
`EXCLUDE_FROM_ALL` on `csv_no_simd` is safe here (verified against csv-parser's own
`CMakeLists.txt` this session).

---

### `src/CMakeLists.txt` (config, batch)

**Analog:** the existing `QUIVER_SOURCES` list and PRIVATE link block (same file, lines ~1-25 and
~73-84):
```cmake
set(QUIVER_SOURCES
    ...
    database_csv_export.cpp
    database_csv_import.cpp
    database_describe.cpp
    ...
)
...
target_link_libraries(quiver
    PUBLIC
        SQLite::SQLite3
    PRIVATE
        quiver_compiler_options
        tomlplusplus::tomlplusplus
        spdlog::spdlog
        lua_library
        sol2
        rapidcsv
        ${CMAKE_DL_LIBS}
)
```
Add `csv_read.cpp` to `QUIVER_SOURCES` (after `database_csv_import.cpp`, per Integration Points)
and `csv` to the PRIVATE link list (after `rapidcsv`). Note the existing `/bigobj` handling on
`lua_runner.cpp` (same file) — irrelevant to `csv_read.cpp` since csv-parser headers never enter
that TU (D-10), but confirms MSVC-specific per-source `COMPILE_OPTIONS` is the established idiom
if `csv_read.cpp` ever needs one.

---

### `bindings/js/src/lua-api.ts` (config/doc, build-gate)

**Analog:** the existing sandbox bullet (`:97-99`, exact text captured this session):
```
- **Filesystem sandbox.** Every file-touching operation (`db:export_csv`, `db:import_csv`,
  `db:open_file`, `db:bin_to_csv`, `db:csv_to_bin`, `db:validate_migrations`, `expr:save`) resolves
  relative paths against the directory containing the database file and rejects anything outside it
  ...
```
Add `db:read_csv` and `db:read_csv_stream` to this parenthetical list, and add two new full
entries in `LUA_DB_API_REFERENCE` in the same style as the existing `db:bin_to_csv`/`db:csv_to_bin`
entries (signature + one-line semantics + traps, per DOC-03 house style — the callback-arity trap,
the `index`-counts-read-not-kept trap, and the `return false` truncation trap all belong here per
CONTEXT.md's Specific Ideas). Both literal tokens `db:read_csv` and `db:read_csv_stream` **must**
appear verbatim — `lua-api-sync.test.ts`'s regex (`\b(bind|ns)\.set_function\(\s*"([a-z_][a-z0-9_]*)"`
paired with a `${token}(?![a-z0-9_])` doc check) fails the JS suite otherwise.

---

### `tests/test_lua_runner_read_csv.cpp` (new test)

**Analog:** `tests/test_lua_runner.h` (fixture, whole file) + the sibling suite pattern
`class LuaRunner_ImportCSV : public LuaSandboxTest`.

**Fixture (copy verbatim):**
```cpp
class LuaSandboxTest : public ::testing::Test {
protected:
    void SetUp() override {
        const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
        sandbox = std::filesystem::temp_directory_path() /
                  (std::string("quiver_lua_") + info->test_suite_name() + "_" + info->name());
        std::filesystem::remove_all(sandbox);
        std::filesystem::create_directories(sandbox);
    }
    void TearDown() override { std::filesystem::remove_all(sandbox); }
    std::string db_path() const { return (sandbox / "test.db").string(); }
    std::filesystem::path sandbox;
};

inline void expect_lua_error(quiver::LuaRunner& lua, const std::string& script, const std::string& substring) {
    try {
        lua.run(script);
        FAIL() << "expected script to throw: " << script;
    } catch (const std::exception& e) {
        EXPECT_NE(std::string(e.what()).find(substring), std::string::npos) << e.what();
    }
}
```
New suite: `class LuaRunner_ReadCsv : public LuaSandboxTest`, an explicit `Database::from_schema`
+ a file-local `write_lua_csv_file`-style helper (mirror whatever the import-CSV suite uses to
drop a fixture file into `sandbox`), a runner named `lua`. Register the new file by name in
`tests/CMakeLists.txt` (explicit source list, no glob).

**Required assertions (from CONTEXT.md's Specific Ideas, all cheap and Phase-1-scoped):**
- 5 TEST-03 sandbox negatives via `expect_lua_error`, pinning D-22's exact message substrings.
- `ragged.csv` (3-col header, one 2-col row, one 4-col row) → all 3 rows arrive, `#rows[1] == 2`.
- `preamble.csv` (one-cell title line above the real header) → nothing eaten.
- One `EXPECT_EQ` on the full JSON string from `lua.run(...)` for a small file — pins
  `{"header":[...],"rows":[[...]]}` shape, nil-not-`{}`, and D-01's contract in one assertion.
- After a callback error mid-stream, `std::filesystem::remove(csv_path)` succeeds (catches a
  `sol::function`-instead-of-`sol::protected_function` regression on Windows).

## Shared Patterns

### Pattern 1 error convention (root `CLAUDE.md`)
**Source:** every `throw std::runtime_error(...)` in `src/csv_read.cpp` and the two new
`lua_runner.cpp` bindings.
**Apply to:** all 10 error-catalogue entries in D-22, in evaluation order.
```cpp
throw std::runtime_error("Cannot " + operation + ": " + reason);
```
`operation` is `"read_csv"` or `"read_csv_stream"` per D-19 — never a shared literal.

### Sandboxing
**Source:** `resolve_sandboxed_path` (`src/lua_runner.cpp:771-805`).
**Apply to:** both new bindings, reused verbatim, no modification.

### Protected-function callback propagation
**Source:** `db:transaction` / `db:dry_run` (`src/lua_runner.cpp:290-323`).
**Apply to:** `db:read_csv_stream`'s per-row callback invocation.

### Strict Lua-object decoding
**Source:** `relation_target_from_lua` (`src/lua_runner.cpp:1006-1016`).
**Apply to:** the shared options-table decoder used by both new bindings.

### Vector→table marshaling
**Source:** `to_lua_table<T>` overloads (`src/lua_runner.cpp`).
**Apply to:** `header` and `rows` construction in `db:read_csv`; the per-row `row`/`header` tables
passed into the `db:read_csv_stream` callback.

## No Analog Found

| File | Role | Data Flow | Reason |
|------|------|-----------|--------|
| `src/csv_read.h` (as a `.h` paired with a `.cpp`, no public header) | utility | transform | D-09 flags this as the first internal-only `.cpp` in `src/` with no `include/quiver/` counterpart — every existing internal helper is header-only inline. No structural analog; follow the shape RESEARCH.md's Architecture Patterns lays out and note the new-pattern status in `src/CLAUDE.md`'s File Map. |

## Metadata

**Analog search scope:** `src/lua_runner.cpp`, `src/database_csv_import.cpp`,
`src/database_csv_export.cpp`, `cmake/Dependencies.cmake`, `src/CMakeLists.txt`,
`bindings/js/src/lua-api.ts`, `bindings/js/test/lua-api-sync.test.ts`, `tests/test_lua_runner.h`,
`tests/CMakeLists.txt`.
**Files scanned:** 9 (all read in full or targeted ranges this session, per the phase's own
RESEARCH.md verification pass).
**Pattern extraction date:** 2026-09-15
