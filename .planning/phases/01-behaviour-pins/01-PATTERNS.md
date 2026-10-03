# Phase 1: Behaviour Pins - Pattern Map

**Mapped:** 2026-10-02
**Files analyzed:** 10
**Analogs found:** 10 / 10

The exact messages, Release-safety audit and full test bodies are in 01-RESEARCH.md (§Verified Messages, Patterns 1-3). This file gives the insertion points and the shapes to copy.

## File Classification

| File | Role | Data Flow | Closest Analog | Match |
|---|---|---|---|---|
| `tests/test_lua_binary.cpp` (modify, +1) | test | request-response (error pin) | `OpenFileInvalidModeThrows` (same file, ~:349) | exact |
| `tests/test_lua_runner_read_csv.cpp` (modify, +3) | test | error pin | read_csv_stream escape test just before `InMemoryDatabaseThrowsForReadCsv` (~:1140-1152) | exact |
| `tests/test_lua_runner_csv_export.cpp` (modify, +1) | test | error pin | `LuaRunner_ExportCSV.EscapeThrows` (~:184) | exact |
| `tests/test_lua_runner_csv_import.cpp` (modify, +1) | test | error pin | `LuaRunner_ImportCSV.EscapeThrows` (~:260) | exact |
| `tests/test_lua_runner_write_csv.cpp` (modify, +5) | test | error pin | `EscapingPathTakesPrecedenceOverInvalidSeparator` (:1171), `SubOneIntegerRowKeyThrows` (:384), `WriteRowAfterCloseThrowsNamingWriteRow` (:1118) | exact |
| `tests/test_lua_runner_lifecycle.cpp` (new) | test | lifecycle / move | `tests/test_database_lifecycle.cpp:49-62` + `LuaBinaryTest.HandleFromAnEarlierRunIsClosed` (test_lua_binary.cpp:149) | role-match |
| `tests/CMakeLists.txt` | config | — | existing source list (:31-45) | exact |
| `tests/AGENTS.md` | docs | — | Lua per-area list (:35-37), sync-test paragraph (:172-176) | exact |
| `bindings/js/test/lua-api-sync.test.ts` | test | parse/transform | "parse found the binding surface" (:53-60) | exact |
| `bindings/js/AGENTS.md` | docs | — | sync-test paragraph (:36-40) | exact |

## Pattern Assignments

### Check-order pins (all five test files)

**Analog:** `tests/test_lua_runner_write_csv.cpp:1169-1177`
```cpp
// LUA-10: a path escaping the database directory takes precedence over an invalid separator --
// the sandbox resolves before the options table is decoded, so the path error is the one raised.
TEST_F(LuaRunner_WriteCsv, EscapingPathTakesPrecedenceOverInvalidSeparator) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    expect_lua_error(lua, R"(db:write_csv("../escape.csv", { separator = ";;" }))", "escapes the database directory");
}
```
Copy the body shape, but: (a) drop the `LUA-10:` planning-ID prefix (no planning IDs in comments); (b) pass a non-table `5` as options, not an in-table bad value; (c) assert the full sentence with the operation name, e.g. `"Cannot write_csv: path '../escape.csv' escapes the database directory"` (stop before the root).

Per-file schema / fixture setup to reuse:
- `test_lua_binary.cpp`: fixture member `schema` (`LuaBinaryTest`), `quiver::Database::from_schema(db_path(), schema)`. Analog at ~:349:
  ```cpp
  TEST_F(LuaBinaryTest, OpenFileInvalidModeThrows) {
      auto db = quiver::Database::from_schema(db_path(), schema);
      quiver::LuaRunner lua(db);
      expect_lua_error(lua, "db:open_file('bin_a', 'x')\n", "Cannot open_file: mode must be");
  ```
  New pin: `db:open_file('../escape', 'z')` → `R"(Cannot open_file: mode must be "r" or "w")"`. No `md1()` needed. Insert right after this test.
- `test_lua_runner_read_csv.cpp`: `VALID_SCHEMA("basic.sql")`, multi-line `expect_lua_error(lua, "...", "Cannot read_csv_stream: path '../outside.csv' escapes the database directory")` style (~:1140). Insert 3 tests before `InMemoryDatabaseThrowsForReadCsv` (~:1152).
- `test_lua_runner_csv_export.cpp` / `_csv_import.cpp`: `auto csv_schema = VALID_SCHEMA("csv_export.sql");` then
  ```cpp
  expect_lua_error(
      lua,
      R"(db:export_csv("Items", "", "../x.csv"))",
      "Cannot export_csv: path '../x.csv' escapes the database directory"
  ```
  New pin adds `, 5` as the fourth argument. Insert after each file's `EscapeThrows`.

### Cap + write_row pins (`tests/test_lua_runner_write_csv.cpp`)

**Analog:** `SubOneIntegerRowKeyThrows` (:384) and `WriteRowAfterCloseThrowsNamingWriteRow` (:1118). Both use the file-local `lp()` (:14) for an absolute sandbox path:
```cpp
const auto path = lp((sandbox / "bad_key_zero.csv").string());
expect_lua_error(lua, ..., "Cannot write_row:");
```
- Cap tests go next to `SubOneIntegerRowKeyThrows`. Messages: `Cannot write_row: row key 1000001 exceeds the maximum width of 1000000` and `Cannot write_csv: option 'header' key 2000000 exceeds the maximum width of 1000000`.
- Closed-writer tests go next to `WriteRowAfterCloseThrowsNamingWriteRow`. Use a relative name (`"closed.csv"`) so the expected text is `Cannot write_row: writer for 'closed.csv' is already closed` (the message embeds the script's path spelling, not the resolved one). Or build the expectation from the same `lp(...)` string.
- Escape + non-table options pin goes right after `EscapingPathTakesPrecedenceOverInvalidSeparator`.

### `tests/test_lua_runner_lifecycle.cpp` (new)

**Analogs:**
- `tests/test_database_lifecycle.cpp:49-62` (test names and the move shape):
  ```cpp
  TEST_F(TempFileFixture, MoveConstructor) {
      quiver::Database db1(path);
      quiver::Database db2 = std::move(db1);
      ...
  TEST_F(TempFileFixture, MoveAssignment) {
      quiver::Database db1(path);
      quiver::Database db2(":memory:");
      db2 = std::move(db1);
  ```
- `tests/test_lua_binary.cpp:149-158` (closure observed in a later run):
  ```cpp
  lua.run(R"(... g = db:open_file('bin_reuse', 'r') )");
  lua.run(R"(assert(not g:is_open(), 'a handle must not outlive its run()'))");
  ```
- Fixture base: `LuaSandboxTest` (`tests/test_lua_runner.h:26-43`, provides `sandbox` and `db_path()` = `sandbox / "test.db"`). Include only `"test_lua_runner.h"` plus `<filesystem> <string> <utility>`.

The full verified body (fixture `LuaRunner_Lifecycle`, helpers `open_handles` / `expect_handles_closed`, both tests) is in 01-RESEARCH.md Pattern 2. Copy it as-is. Keep `source` at test scope and never `run()` it after the move. Use distinct file names per script.

### `tests/CMakeLists.txt`

Insert `test_lua_runner_lifecycle.cpp` alphabetically between `test_lua_runner_fk.cpp` and `test_lua_runner_migrations.cpp` (~:40-41) in the `quiver_tests` list.

### `bindings/js/test/lua-api-sync.test.ts`

**Analog:** the same test (:53-60):
```ts
  test("parse found the binding surface", () => {
    expect(dbMethods.size).toBeGreaterThan(40);
    expect(quiverFns.size).toBeGreaterThan(10);
    expect(usertypeMethods.get("Expression")?.size).toBeGreaterThan(0);
  });
```
Replace the `Expression` line with the `unparsed` filter over `["BinaryFile","BinaryMetadata","Expression","CsvWriter"]` → `toEqual([])`, plus `expect(CPP.match(/open_libraries\(/g)?.length ?? 0).toBe(1);` (RESEARCH Pattern 3). Reuse `usertypeMethods` and `CPP`. Do not add a second parser.

### `tests/AGENTS.md` / `bindings/js/AGENTS.md`

- `tests/AGENTS.md:35-37`: add `_lifecycle` to the parenthesized list `(_create, _read, ..., _fk, _migrations)`, with a short gloss in the style of the `_return` / `_transaction` sentences that follow.
- `tests/AGENTS.md:172-176` ("only JS test file that needs neither a database...") and `bindings/js/AGENTS.md:37-40` ("fails if a `db:`/`quiver.*` name is undocumented, ..."): add one clause each, saying the test also fails if any of the four usertypes parses to zero methods or `open_libraries(` does not appear exactly once.
- Optional: in `tests/AGENTS.md` ~:142-145, change the Release filter `'LuaRunner*'` to `'Lua*'`.

## Shared Patterns

### Error assertion
**Source:** `tests/test_lua_runner.h:47-55`. **Apply to:** every new pin.
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
Do not use `expect_prefixed_error`, and do not add a new helper.

### Comment style
Neighbouring tests use `LUA-10:` / `WRITE-05:` prefixes. Do **not** copy them: comments state the reason only.

### Formatting
Format the C++ files with `uvx --from clang-format==22.1.8 clang-format -i`. For the TS file, run `bunx biome check test/lua-api-sync.test.ts`.

## No Analog Found

None.

## Metadata

**Analog search scope:** `tests/`, `bindings/js/test/`, `tests/AGENTS.md`, `bindings/js/AGENTS.md`
**Files scanned:** 12
**Pattern extraction date:** 2026-10-02
