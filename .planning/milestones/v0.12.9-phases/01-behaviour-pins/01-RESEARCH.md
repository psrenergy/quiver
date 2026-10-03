# Phase 1: Behaviour Pins - Research

**Researched:** 2026-10-02
**Domain:** C++ gtest pins for the sol2 Lua binding layer (`src/lua_runner.cpp`) + one Bun test guard
**Confidence:** HIGH. Every message string below was observed at runtime in both a Debug and a Release build of the current HEAD. The move tests were prototyped and passed against both builds.

## Summary

The CONTEXT.md decisions hold up against the code at HEAD (`0b6448b`). `src/`, `include/` and `tests/` have
not changed since `bdf9087` (`git diff --stat bdf9087 HEAD -- src tests include` is empty), so every line
number in CONTEXT.md is still correct. I ran every proposed two-bad-argument call through
`quiver_cli.exe` against `build/dev` (Debug) and `build/release` (Release). The winning error was identical
in both builds, and each matches the decision text. Every chosen bad value goes through a `sol::object`
parameter with an explicit `get_type()` check, or never reaches a conversion at all. None is passed to an
unchecked `sol::table`, a non-string map key, or a wrong-typed `sol::optional`, so all of them are
defined behaviour in Release.

`LuaRunner`'s move operations are `= default` + `noexcept` over a `std::unique_ptr<Impl>`. The heap `Impl`
never moves, so the `[this]` captures (761, 876, 1032) and `lua["db"] = &db` stay valid. I compiled the
move-construct and move-assign shapes from D-09 as a scratch exe against both `quiver.lib` builds. Each one
opens handles, moves, opens again, then checks closure in a third run, and both pass in Debug and Release.
Today the JS sync test parses `CsvWriter`=2, `BinaryMetadata`=7, `BinaryFile`=6 and `Expression`=6
methods, and finds exactly one `open_libraries(`. Only `Expression` has a floor so far.

**Primary recommendation:** write one `TEST_F` per pinned edge (13 new tests → 441 `Lua*` tests across 12
suites). Assert with `expect_lua_error` and the exact sentences quoted below. Keep the moved-from runner
alive for the whole move test.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Key-width cap, check-order pins | C++ test layer (`tests/test_lua_*.cpp`) | — | The behaviour lives in `src/lua_runner.cpp`. Only a Lua script driven by `LuaRunner::run` reaches it |
| Move-survival pins | C++ test layer (new `tests/test_lua_runner_lifecycle.cpp`) | — | The move ops are C++ API (`include/quiver/lua_runner.h`), so they can't be reached from Lua |
| Usertype / `open_libraries` guards | JS test (`bindings/js/test/lua-api-sync.test.ts`) | — | This test is the existing parser of `src/lua_runner.cpp`. It needs no native lib |
| Baseline counts | `.planning/STATE.md` + phase SUMMARY | — | D-12: not AGENTS.md (counts there go stale) |

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

#### Milestone scope change (decided during this discussion, applies beyond Phase 1)
- **D-01:** The `LuaRunner` → `quiver::Sandbox` rename is **dropped from this milestone** ("to keep it
  simple"). The class, `include/quiver/lua_runner.h`, the C API `quiver_lua_runner_*`, every
  binding's `LuaRunner`, all file names, `LuaSandboxTest`, `resolve_sandboxed_path`, `tests/sandbox`
  / `quiver_sandbox`, and the closed/disposed messages keep their current names.
  - Effect on Phase 1: the names it introduces (`test_lua_runner_lifecycle.cpp`, `LuaRunner_*`
    fixtures) are permanent. No Phase 5 `Sandbox*` filter count needs to be fed.
  - Effect on later phases (to apply in ROADMAP/REQUIREMENTS/PROJECT):
    - Phase 2's folder is `src/lua_runner/`, not `src/sandbox/`.
    - REN-01..07 are dropped. The one exception is the non-rename docs sentence from REN-05: the Lua
      reference states what the sandbox does not limit (instructions, memory, wall time, globals
      across `run()`).
    - Phase 5 stays as a slim phase: TEST-01, written against `resolve_sandboxed_path`, plus
      DOC-01..04 with the rename parts removed. DOC-03 has no rename BREAKING entry, and DOC-04 has
      no "three meanings of sandbox" item.

#### Check-order pins (PIN-02)
- **D-02:** Sweep every op whose order is not pinned yet, rather than only the three orders the
  roadmap names. Each test passes two bad arguments and asserts which error wins:
  - `db:open_file`: a bad mode (e.g. `'z'`) and an escaping path (`'../escape'`) → the mode error.
  - `db:read_csv`, `db:export_csv`, `db:import_csv`: an escaping path and non-table options → the
    containment error.
  - `db:read_csv_stream`: its order is on_row type → containment → options, so it gets two
    pins:
    - a non-function `on_row` and an escaping path → `Cannot read_csv_stream: on_row must be a
      function`;
    - a valid function, an escaping path and non-table options → the containment error.
  - `db:write_csv`: one new pin with an escaping path and non-table options. Its two existing pins
    (`test_lua_runner_write_csv.cpp:1171` and the LUA-10 one near `:1400`) use the in-table form, so
    they cannot catch a misplaced `require_table`.
- **D-03:** The bad options value is a **non-table**, e.g. `5`, which today reports
  `Cannot <op>: options must be a table`. Every options decoder takes `sol::object` and checks the
  type itself, so this is defined in both builds. It catches the Phase 4 risk named in Pitfall 8:
  a `require_table` placed at the top of the lambda would report the table error first.
- **D-04:** Use an escaping path only, with no `:memory:` variants. Both failure modes live in the
  same `resolve_sandboxed_path` call, so all pins use file-backed fixtures derived from
  `LuaSandboxTest`.
- **D-05:** Pin **both** `w:write_row` ordering edges:
  - a non-table row (e.g. a number) on a closed writer → `Cannot write_row: row must be a table`
    (type before closed);
  - a table with an unsupported cell (e.g. `{ print }`) on a closed writer →
    `Cannot write_row: writer for '<path>' is already closed` (closed before cell formatting).

  Neither is pinned today.

#### Key-width cap (PIN-01; message text taken from the code)
- Row: `w:write_row({[1000001]='x'})` →
  `Cannot write_row: row key 1000001 exceeds the maximum width of 1000000`.
- Header: `db:write_csv(path, { header = {[2e6]='a'} })` →
  `Cannot write_csv: option 'header' key 2000000 exceeds the maximum width of 1000000`. Lua 5.4
  normalizes the integral float key `2e6` to the integer `2000000`. The planner verifies this text
  against `csv_max_integer_key` (`src/lua_runner.cpp:348-372`, header caller `:460`).

#### Sync-test guards (PIN-03)
- **D-06:** Floors only. Each of `BinaryFile`, `BinaryMetadata`, `Expression` and `CsvWriter` must
  parse to more than 0 methods, and `open_libraries(` must appear exactly once in the parsed
  source. There are no exact per-usertype counts and no committed snapshot. Phase 2 does its
  before/after method-set diff once, by hand. The existing `dbMethods > 40`, `quiverFns > 10` floors
  stay. The mutation check (delete one usertype's registrations, see the test fail, revert) is
  done by hand and not committed (locked by the roadmap).

#### Test placement
- **D-07:** Pins go in the **matching area files** with their existing fixtures:
  - `db:open_file` → `tests/test_lua_binary.cpp` (`LuaBinaryTest`);
  - `db:read_csv` / `db:read_csv_stream` → `tests/test_lua_runner_read_csv.cpp` (`LuaRunner_ReadCsv`);
  - `db:export_csv` → `tests/test_lua_runner_csv_export.cpp` (`LuaRunner_ExportCSV`);
  - `db:import_csv` → `tests/test_lua_runner_csv_import.cpp` (`LuaRunner_ImportCSV`);
  - the cap, `w:write_row` and the new `db:write_csv` pin → `tests/test_lua_runner_write_csv.cpp`
    (`LuaRunner_WriteCsv` / `LuaRunner_WriteCsvErrors`).

  There is no dedicated "pins" file.
- **D-08:** The move tests go in a **new `tests/test_lua_runner_lifecycle.cpp`**, mirroring
  `tests/test_database_lifecycle.cpp`. Its fixture derives from `LuaSandboxTest` and is named in the
  `Lua*` family (e.g. `LuaRunner_Lifecycle`), so the `Lua*` filter counts it. The file is added to
  `quiver_tests` in `tests/CMakeLists.txt` and listed in `tests/AGENTS.md`.
- **D-09:** There are **two** move tests, `MoveConstructor` and `MoveAssignment`, mirroring the core's
  `test_database_lifecycle.cpp:49-62`. Each one:
  1. registers handles through `db:write_csv` and `db:open_file` in a first run;
  2. moves the runner;
  3. runs a second script on the moved-to runner that calls `db:write_csv` and `db:open_file` again,
     which exercises the `[this]` captures (`src/lua_runner.cpp:761, 876, 1032`) through the moved
     `Impl`;
  4. asserts that the handles the second script opened were closed at its `run()` exit.

  In `MoveAssignment`, the target runner first runs a script of its own, so its old `Impl` is
  destroyed with live state.

#### Message matching
- **D-10:** Use the existing `expect_lua_error` (`tests/test_lua_runner.h`, substring match) with the
  **full Pattern 1 sentence**. There is no new exact-match helper.
- **D-11:** The check-order pins assert the **full winning sentence**, including the operation name
  (e.g. `Cannot open_file: mode must be "r" or "w"`), so a helper that misattributes the operation
  also fails. The containment message ends with the resolved root
  (`... escapes the database directory '<root>'`, `src/lua_runner.cpp:1352`), and that root depends
  on the temp directory and canonicalization. Those pins assert the sentence up to and including
  `escapes the database directory`, as the existing escape tests do.

#### Baseline (PIN success criterion 5)
- **D-12:** The baseline counts go into **STATE.md and the phase SUMMARY**, not AGENTS.md, because
  counts written there go stale. The Lua count uses `--gtest_filter=Lua*`: 428 at `bdf9087` across
  11 suites, plus the new pins. The C API count is the 27 tests in `tests/test_c_api_lua_runner.cpp`.
  Phases 2 and 3 must reproduce both exactly.

### Claude's Discretion
- Exact test and fixture names (behaviour-describing PascalCase, no planning IDs).
- Which collection/group strings the `export_csv` / `import_csv` pins pass. The path is resolved
  before the collection is validated.
- How the move tests observe "closed at exit": a third run asserting `not g:is_open()` as
  `HandleFromAnEarlierRunIsClosed` does, and/or reading the CSV back.
- Which non-table value the `write_row` type pin passes. It must be defined behaviour, i.e. not a
  value passed into an unchecked `sol::table` parameter.

### Specific Ideas (from CONTEXT.md)
- No new test or code comment may carry a planning ID (`D-xx`, `PIN-xx`, `C1`, `LUA-xx`, ...). This
  file's decision numbers are for planning only. Comments state the reason or name the pinning test.
- No test may exercise a Release-UB path:
  - no non-table passed to an unchecked `sol::table` parameter (C1);
  - no non-string key at the four C2 map-key sites;
  - no wrong-type optional (C5).

### Deferred Ideas (OUT OF SCOPE)
- None for Phase 1 beyond the milestone change in D-01. The ROADMAP, REQUIREMENTS and PROJECT edits
  that apply it are a separate step after this context is committed.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| PIN-01 | Cap pins for `w:write_row` `{[1000001]='x'}` and the `db:write_csv` header `{[2e6]='a'}`, full Pattern 1 message | §Verified Messages rows 1-2. `2e6` becomes integer key `2000000` (seen at runtime, Lua manual §2.1) |
| PIN-02 | Check-order pins: open_file mode before path; containment before options; write_row type before closed | §Verified Messages rows 3-11 (7 call sites, 9 tests) |
| PIN-03 | JS sync test: >0 methods for 4 usertypes, exactly 1 `open_libraries(` | §Pattern 3, today's parse counts, mutation procedure |
| PIN-04 | Move-construct / move-assign after handles registered, then run again | §Pattern 2. Prototype passed in Debug + Release |
| PIN-05 | Defined behaviour only, green in Debug + Release | §Release-safety audit; both builds observed |
</phase_requirements>

## Project Constraints (from AGENTS.md; there is no CLAUDE.md)

- Error messages are Pattern 1 `"Cannot {operation}: {reason}"`. Tests assert them, never craft them [VERIFIED: AGENTS.md "C++ Error Message Patterns"].
- **Every file-touching Lua test uses `LuaSandboxTest`**: a file-backed db in a per-test temp dir [VERIFIED: tests/AGENTS.md:44-46].
- Keep the nearest AGENTS.md current (Self-Updating rule): `tests/AGENTS.md` and `bindings/js/AGENTS.md` here.
- clang-format is pinned to 22.1.8. CI runs `find include src tests -name '*.cpp' -o -name '*.h' | xargs uvx --from clang-format==22.1.8 clang-format --dry-run --Werror` [VERIFIED: .github/workflows/ci.yml:204], so new test files must be formatted with that exact version. The VS-bundled `clang-format` on PATH is not the pinned one.
- Do not drive-by fix lint debt in untouched JS files. Format only `lua-api-sync.test.ts` (`bunx biome check test/lua-api-sync.test.ts` is clean today).
- Do not "clean up" `tests/sandbox`.
- No planning IDs in new comments (CONTEXT Specific Ideas). Many neighbouring tests carry `LUA-10:`, `FMT-08`, `WRITE-05`-style comments. **Do not copy that style.**
- Run Python via `uv`, and call bun/pytest/dart directly for quoted filters (memory note: quoted filters break through `cmd //c`).

## Verified Messages (the exact text each pin asserts)

All rows were observed by running the call inside `pcall` through `quiver_cli.exe` against
`build/dev/bin` (Debug) **and** `build/release/bin` (Release), with a file-backed `basic.sql` db.
The output was byte-identical across builds [VERIFIED: runtime probe this session]. `run()` prefixes
`"Failed to run Lua script: "` (`src/lua_runner.cpp:2527`), so the substring match in
`expect_lua_error` still finds the sentence.

| # | Call (two bad args where applicable) | Winning message to assert (substring) | Source |
|---|---|---|---|
| 1 | `w:write_row({[1000001]='x'})` | `Cannot write_row: row key 1000001 exceeds the maximum width of 1000000` | `src/lua_runner.cpp:364-369`, row caller `:385` |
| 2 | `db:write_csv(p, { header = {[2e6]='a'} })` | `Cannot write_csv: option 'header' key 2000000 exceeds the maximum width of 1000000` | `:364-369`, header caller `:460` |
| 3 | `db:open_file('../escape', 'z')` | `Cannot open_file: mode must be "r" or "w"` | `:767-770` |
| 4 | `db:read_csv('../escape.csv', 5)` | `Cannot read_csv: path '../escape.csv' escapes the database directory` | `:794-795`, `:1352` |
| 5 | `db:read_csv_stream('../escape.csv', 5)` | `Cannot read_csv_stream: on_row must be a function` | `:831-836` |
| 6 | `db:read_csv_stream('../escape.csv', function() end, 5)` | `Cannot read_csv_stream: path '../escape.csv' escapes the database directory` | `:836-837` |
| 7 | `db:write_csv('../escape.csv', 5)` | `Cannot write_csv: path '../escape.csv' escapes the database directory` | `:879-880` |
| 8 | `db:export_csv('Items', '', '../x.csv', 5)` | `Cannot export_csv: path '../x.csv' escapes the database directory` | `:671-672` |
| 9 | `db:import_csv('Items', '', '../x.csv', 5)` | `Cannot import_csv: path '../x.csv' escapes the database directory` | `:681-682` |
| 10 | closed writer, `w:write_row(5)` | `Cannot write_row: row must be a table` | `:905-906` before `:914` |
| 11 | closed writer, `w:write_row({ print })` | `Cannot write_row: writer for '<path as passed to db:write_csv>' is already closed` | `:914-915` → `src/csv/csv_write.cpp:143` |

The probe used collection `Configuration` for rows 8-9. Use the fixture's own `Items` (`csv_export.sql`):
the path is resolved before the collection is looked at, so either works.

**Controls (each second bad argument alone also errors, so every pin is meaningful)** [VERIFIED: runtime probe, both builds]:
- `db:open_file('../escape','r')` gives `Cannot open_file: path '../escape' escapes the database directory '…'`
- `db:read_csv('f.csv',5)`, `db:read_csv_stream('f.csv',function() end,5)`, `db:write_csv('f.csv',5)`,
  `db:export_csv(...,'x.csv',5)` and `db:import_csv(...,'x.csv',5)` each give `Cannot <op>: options must be a table`
- `db:read_csv_stream('f.csv', 5)` gives `Cannot read_csv_stream: on_row must be a function`
- open writer, `w:write_row({ print })` gives `Cannot write_row: cell #1 has unsupported Lua type`. The existing
  `FunctionCellThrowsNamingWriteRowAndCellIndex` already pins this one.
- A Lua table `{[2e6]='a'}` iterates its key as `math.type(k) == "integer"`, `2000000` [VERIFIED: runtime probe].
  The manual says: "any float used as a key that is equal to an integer is converted to that integer"
  [CITED: lua.org/manual/5.4/manual.html §2.1].

### Source quotes (verbatim, read this session)

`src/lua_runner.cpp:364-370` (cap):
```cpp
        constexpr std::int64_t kMaxWidth = 1'000'000;
        if (max_index > kMaxWidth) {
            throw std::runtime_error(
                "Cannot " + operation + ": " + what + " key " + std::to_string(max_index) +
                " exceeds the maximum width of " + std::to_string(kMaxWidth)
            );
        }
```
`:385` `csv_max_integer_key(row, operation, "row");` · `:460` `csv_max_integer_key(header, operation, "option 'header'");`

`:767-770` (open_file):
```cpp
                if (mode.size() != 1 || (mode[0] != 'r' && mode[0] != 'w')) {
                    throw std::runtime_error("Cannot open_file: mode must be \"r\" or \"w\"");
                }
                const auto resolved = resolve_sandboxed_path(self, "open_file", path);
```
`:831-837` (read_csv_stream):
```cpp
                if (on_row_arg.get_type() != sol::type::function) {
                    throw std::runtime_error("Cannot read_csv_stream: on_row must be a function");
                }
                const sol::protected_function on_row = on_row_arg.as<sol::protected_function>();
                // D-22 evaluation order: sandbox checks before the options table.
                const auto resolved = resolve_sandboxed_path(self, "read_csv_stream", path);
                auto csv_options = read_csv_options_from_lua(options, "read_csv_stream");
```
`:905-916` (write_row):
```cpp
                if (row.get_type() != sol::type::table) {
                    throw std::runtime_error("Cannot write_row: row must be a table");
                }
                ...
                if (self.writer->is_closed()) {
                    self.writer->write_row({}, "write_row");
                    return;
                }
```
`src/csv/csv_write.cpp:143`: `throw std::runtime_error("Cannot " + operation + ": writer for '" + original_path_ + "' is already closed");`
`original_path_` is the **script's** path argument, not the resolved one. With `db:write_csv("closed.csv")` the message is `writer for 'closed.csv' is already closed` [VERIFIED: runtime probe].

`:1352`: `"Cannot " + operation + ": path '" + path + "' escapes the database directory '" + root.string() + "'"`

`:2495-2497`: `LuaRunner::LuaRunner(LuaRunner&&) noexcept = default;` / `LuaRunner& LuaRunner::operator=(LuaRunner&&) noexcept = default;`
`include/quiver/lua_runner.h`: `struct Impl; std::unique_ptr<Impl> impl_;`

## Release-Safety Audit (PIN-05)

| Pin | Parameter it lands in | Why it is defined in Release |
|---|---|---|
| cap row `{[1000001]='x'}` | `write_row(CsvWriter&, const sol::object& row)` (`:899`) | `get_type()==table` checked. The key walk uses `pair.first.is<std::int64_t>()` (a checked query, `SOL_SAFE_NUMERICS=1` at `src/CMakeLists.txt:63`). It throws before allocating |
| cap header `{[2e6]='a'}` | `write_csv(..., sol::object options)` → `header_value->get_type() != sol::type::table` check (`:581`) | Explicit type checks. The key is an integer, not a C2 non-string map key |
| `open_file('../escape','z')` | `const std::string& path, const std::string& mode, sol::optional<BinaryMetadata>` | Both are real strings. The optional is **absent** (nil → nullopt), not wrong-typed, so it is not C5 |
| options `5` (rows 4, 6-9) | `sol::object options` / `const sol::object& options` (`:790`, `:823`, `:876`, `:669`, `:680`) | Every decoder checks `options.get_type() != sol::type::table` (`:569`, `:1368`, `:1413`) |
| on_row `5` (row 5) | `sol::object on_row_arg` (`:823`) | `get_type() != function` is checked before any `.as<>` |
| `w:write_row(5)` closed | `const sol::object& row` | Same `get_type()` check, and it is the first statement |
| `w:write_row({ print })` closed | table row | The closed check returns before any cell is converted, so the function value is never touched |

The `collection`/`group`/`path` arguments are always real Lua strings, passed into `const std::string&`.
Nothing in this phase passes a number/table/userdata where sol2 would do an unchecked `lua_tolstring` or
`sol::table` conversion. **Do not** add "extra coverage" variants such as `w:write_row(db)` (userdata: OK
today, but it gains nothing), `options = db`, or a typed-optional wrong value (`db:bin_to_csv(p, 5)`, which
**is C5**).

## Architecture Patterns

### Pattern 1: Check-order pin (shape of `EscapingPathTakesPrecedenceOverInvalidSeparator`, `test_lua_runner_write_csv.cpp:1171`)

```cpp
// Source: tests/test_lua_runner_write_csv.cpp:1171-1177 (shape); message text from the table above
// The path is resolved before the options are decoded, so a non-table options argument cannot mask
// an escaping path.
TEST_F(LuaRunner_ReadCsv, EscapingPathIsReportedBeforeNonTableOptions) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    expect_lua_error(
        lua,
        R"(db:read_csv("../escape.csv", 5))",
        "Cannot read_csv: path '../escape.csv' escapes the database directory"
    );
}
```
The same body works for the rest with the call/message swapped, using the fixture's own schema per file:
`LuaBinaryTest` uses `schema` (collections.sql), ExportCSV/ImportCSV use `VALID_SCHEMA("csv_export.sql")`,
and the WriteCsv suites use `basic.sql`. The open_file one needs no `md1()`: the mode check fires before
metadata matters. Its message carries double quotes, so use `R"(Cannot open_file: mode must be "r" or "w")"`.

### Pattern 2: Move survival (new `tests/test_lua_runner_lifecycle.cpp`)

I compiled this exact Lua and move sequence as a scratch exe against `build/dev/lib/quiver.lib` (`/MDd`)
and `build/release/lib/quiver.lib` (`/MD`). Both runs passed, including the `expr:save` output file
[VERIFIED: prototype run this session].

```cpp
#include "test_lua_runner.h"

#include <filesystem>
#include <string>
#include <utility>

// A move hands the heap Impl over whole, so the moved-to runner's bindings still reach the registries
// that close every CSV writer and binary file at run() exit.
class LuaRunner_Lifecycle : public LuaSandboxTest {
protected:
    void SetUp() override {
        LuaSandboxTest::SetUp();
        schema = VALID_SCHEMA("collections.sql");
    }

    // Leaves a CSV writer and a binary writer open in globals, so only run()'s exit can close them.
    static std::string open_handles(const std::string& name) {
        return "local NAME = '" + name + "'\n" + R"(
            local md = quiver.metadata{ initial_datetime = '2025-01-01T00:00:00', unit = 'x',
                labels = {'v'}, dimensions = {'row'}, dimension_sizes = {1} }
            w = db:write_csv(NAME .. '.csv')
            w:write_row({ 'x' })
            g = db:open_file(NAME, 'w', md)
            g:write({ 1.0 }, { row = 1 })
        )";
    }

    // Run after open_handles(name): both handles were closed (and flushed) when that run returned.
    static std::string expect_handles_closed(const std::string& name) {
        return "local NAME = '" + name + "'\n" + R"(
            assert(not g:is_open(), 'binary handle outlived its run()')
            local csv = db:read_csv(NAME .. '.csv', { header_row = 0 })
            assert(#csv.rows == 1 and csv.rows[1][1] == 'x', 'csv writer was not closed at run() exit')
            local r = db:open_file(NAME, 'r')
            assert(r:read({ row = 1 })[1] == 1.0, 'binary writer was not flushed')
            local doubled = quiver.expression(r) * 2.0
            doubled:save(NAME .. '_doubled')
            r:close()
        )";
    }

    std::string schema;
};

TEST_F(LuaRunner_Lifecycle, MoveConstructor) {
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner source(db);
    source.run(open_handles("first"));

    // `source` stays alive to the end: run state left behind in it would then fail the checks below
    // instead of dangling.
    quiver::LuaRunner moved = std::move(source);
    moved.run(open_handles("second"));
    moved.run(expect_handles_closed("second"));
    EXPECT_TRUE(std::filesystem::exists(sandbox / "second_doubled.qvr"));
}

TEST_F(LuaRunner_Lifecycle, MoveAssignment) {
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner source(db);
    source.run(open_handles("first"));
    quiver::LuaRunner target(db);
    target.run(open_handles("target"));  // destroyed with live globals by the assignment below

    target = std::move(source);
    target.run(open_handles("second"));
    target.run(expect_handles_closed("second"));
    EXPECT_TRUE(std::filesystem::exists(sandbox / "second_doubled.qvr"));
}
```
Notes:
- `doubled:save(...)` exercises the third `[this]` capture (`:1032`, `expr:save` uses `Impl::db`).
  Without it only 761 and 876 run, which is all SC-2 demands. It is cheap, so keep it. Keep it as a
  `local` binding: a statement that *starts* with `(` right after `assert(...)` parses as a call on
  assert's result (Pitfall 1).
- Why `source` must stay in scope: Phase 2 (SPLIT-03) moves the registries into `RunHandles` and the
  captures to `[&handles]`. If `RunHandles` landed outside the heap `Impl`, the moved-to runner's lambdas
  would register into the moved-from object. If that object is alive, closure at exit silently misses
  them, so `not g:is_open()` fails deterministically. If it is dead, the result is UB and may pass by
  accident [ASSUMED: reasoning over Pitfall 10, not executed as a mutation].
- Two runners on one `Database` is legal (`LuaRunner` borrows `Database&`). Distinct file names per
  script (`first`/`target`/`second`) avoid the process-global binary write registry
  (`src/binary/binary_file.cpp:67-68`: `"Cannot open_file: file is already open for writing: "`).
- Never call `run()` on the moved-from runner (`impl_` is null).

### Pattern 3: Sync-test guards (`bindings/js/test/lua-api-sync.test.ts:53-60`)

Today's meta-guard (verbatim):
```ts
    expect(dbMethods.size).toBeGreaterThan(40);
    expect(quiverFns.size).toBeGreaterThan(10);
    expect(usertypeMethods.get("Expression")?.size).toBeGreaterThan(0);
```
Replace the third line with this. It uses the file's own "list the offenders, compare to `[]`" idiom, so
a failure names the usertype:
```ts
    // A usertype that parses to nothing would let the ":<name>(" check below pass vacuously.
    const unparsed = ["BinaryFile", "BinaryMetadata", "Expression", "CsvWriter"].filter(
      (type) => !usertypeMethods.get(type)?.size,
    );
    expect(unparsed).toEqual([]);
    // The stdlib check below reads only the first call; a second one would go unchecked.
    expect(CPP.match(/open_libraries\(/g)?.length ?? 0).toBe(1);
```
Current parse at HEAD [VERIFIED: bun probe of the test's own parser]: `Database 17, CsvWriter 2,
BinaryMetadata 7, BinaryFile 6, Expression 6`, `bind` 54, `ns` 15, `open_libraries(` 1 (the only
occurrence of `open_libraries` in the file, at `:243`). Phase 2 changes `CPP` to a per-file read. Keep the
guard independent of how `CPP` is built, so that phase can reuse it unchanged.

**Mutation check (by hand, not committed):**
1. Delete the `"write_row",` and `"close",` name lines inside `lua.new_usertype<CsvWriter>(` (`src/lua_runner.cpp:898`/`:941`).
2. `cd bindings/js && bun test test/lua-api-sync.test.ts` must fail on "parse found the binding surface" with `["CsvWriter"]`.
   Today, without the guard, the same mutation passes: the `:<name>(` loop just iterates nothing.
3. `git checkout -- src/lua_runner.cpp`, then `git diff --exit-code -- src/` must be clean. **Do not build while it is mutated.**
4. Optional second mutation: duplicate the `lua.open_libraries(` line. The test must fail with count 2. Revert the same way.

### Anti-Patterns to Avoid
- **Asserting the containment root** (`... '<root>'`): it depends on the temp dir and canonicalization (D-11). Stop at `escapes the database directory`.
- **Using `:memory:` for the order pins** (D-04): the in-memory error fires earlier in the same function and would hide the ordering.
- **One test asserting several orderings**: one `TEST_F` per edge keeps the failure name diagnostic. The existing file style is also one call per test for these.
- **Putting `options` *inside* a table** (`{ separator = ";;" }`) for the new pins: that is what the two existing write_csv pins do, and it cannot catch a hoisted `require_table` (D-02).
- **`expect_prefixed_error`** for the new pins: it checks a prefix only, so stay with `expect_lua_error` (D-10).

## Recommended placement and count

| File | Fixture | New tests | Suggested name |
|---|---|---|---|
| `tests/test_lua_binary.cpp` (after `OpenFileInvalidModeThrows`, :349) | `LuaBinaryTest` | 1 | `OpenFileReportsInvalidModeBeforeEscapingPath` |
| `tests/test_lua_runner_read_csv.cpp` (after the escape tests, before `InMemoryDatabaseThrowsForReadCsv` :1152) | `LuaRunner_ReadCsv` | 3 | `EscapingPathIsReportedBeforeNonTableOptions`, `StreamReportsNonFunctionOnRowBeforeEscapingPath`, `StreamReportsEscapingPathBeforeNonTableOptions` |
| `tests/test_lua_runner_csv_export.cpp` (after `EscapeThrows`, :184) | `LuaRunner_ExportCSV` | 1 | `EscapeIsReportedBeforeNonTableOptions` |
| `tests/test_lua_runner_csv_import.cpp` (after `EscapeThrows`, :260) | `LuaRunner_ImportCSV` | 1 | `EscapeIsReportedBeforeNonTableOptions` |
| `tests/test_lua_runner_write_csv.cpp` (near `SubOneIntegerRowKeyThrows` :384 and the closed/escape tests :1118-1177) | `LuaRunner_WriteCsv` | 5 | `RowKeyPastMaximumWidthThrows`, `HeaderKeyPastMaximumWidthThrows`, `EscapingPathIsReportedBeforeNonTableOptions`, `NonTableRowOnClosedWriterReportsTheType`, `UnsupportedCellOnClosedWriterReportsClosed` |
| `tests/test_lua_runner_lifecycle.cpp` (new) | `LuaRunner_Lifecycle` | 2 | `MoveConstructor`, `MoveAssignment` |

**Expected baseline: 428 + 13 = 441 `Lua*` tests across 12 suites**, plus C API `LuaRunnerCApiTest` = 27
(unchanged). Today's per-suite counts at HEAD [VERIFIED: `--gtest_list_tests`, identical in build/, build/dev, build/release]:
LuaBinaryTest 26, LuaExpressionTest 24, LuaRunnerAllTypesTest 5, LuaRunnerTest 204, LuaRunner_ExportCSV 9,
LuaRunner_ImportCSV 12, LuaRunnerFkTest 18, LuaRunner_Migrations 4, LuaRunner_ReadCsv 69,
LuaRunner_WriteCsv 48, LuaRunner_WriteCsvErrors 9 (sum 428). Whole binaries: quiver_tests 1394, quiver_c_tests 543.
Record the count that actually results, whatever it is. 441 is the number if the table is followed exactly.

**`tests/CMakeLists.txt`**: insert `test_lua_runner_lifecycle.cpp` between `test_lua_runner_fk.cpp` (line 38)
and `test_lua_runner_migrations.cpp` (line 39). Ninja re-runs CMake by itself on the next build.

**AGENTS.md edits (minimal):**
- `tests/AGENTS.md:35-37`: add `_lifecycle` (runner move: handles registered after a move still close at `run()` exit) to the Lua per-area list.
- `tests/AGENTS.md:172-176` and `bindings/js/AGENTS.md:36-39`: one clause each saying the sync test also fails if any of the four usertypes parses to zero methods or `open_libraries(` does not appear exactly once.
- Optional, same file: `tests/AGENTS.md:142-145` tells people to run `--gtest_filter='LuaRunner*'` in Release, which misses `LuaBinaryTest`/`LuaExpressionTest`. The baseline filter is `Lua*`. Fixing that one filter string is in scope, because this phase relies on that instruction. Leave the "291/291" history as it is.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---|---|---|---|
| Error assertion | a new exact-match helper | `expect_lua_error` (`tests/test_lua_runner.h:49-56`) | D-10. It also catches the "attempt to call a nil value" vacuous pass |
| Temp dir / db path | ad-hoc `temp_directory_path()` fixtures | `LuaSandboxTest` (`db_path()`, `sandbox`) | tests/AGENTS.md rule. Per-test dir named suite+test |
| Path into Lua literal | backslash escaping | relative paths, or the file-local `lp()` | Relative paths avoid the issue entirely in the new pins |
| Usertype method parsing | a second parser | the test's existing `usertypeMethods` map | Phase 2 rewrites that parser once |

## Common Pitfalls

### Pitfall 1: Lua statement starting with `(`
**What goes wrong:** `assert(x)\n(quiver.expression(r) * 2):save(p)` parses as `assert(x)(...)`, calling the result of `assert`.
**How to avoid:** `local e = quiver.expression(r) * 2.0` then `e:save(...)` (the form the prototype used).

### Pitfall 2: Moved-from runner destroyed too early
**What goes wrong:** putting `source` in an inner scope turns a future misplaced-registry bug into UB instead of a deterministic failure.
**How to avoid:** declare `source` at test scope. Never call `source.run()` after the move.

### Pitfall 3: Wrong build for the Release check
**What goes wrong:** `scripts/test-all.bat` runs `build/bin` (plain Debug configure), which is neither preset.
**How to avoid:** build all three: `cmake --build build --config Debug` (test-all + bindings), `cmake --build --preset dev`, `cmake --build --preset release`. Then run `build/dev/bin/quiver_tests.exe --gtest_filter='Lua*'` and `build/release/bin/quiver_tests.exe --gtest_filter='Lua*'`. All three dirs are already configured with MSVC 14.51 + Ninja [VERIFIED: CMakeCache.txt].

### Pitfall 4: Debug-only stderr noise misread as failure
**What goes wrong:** Debug prints `[sol2] An exception occurred: ...` to stderr for every caught error (sol2's default `SOL_PRINT_ERRORS` in Debug). Release prints nothing [VERIFIED: probe output].
**How to avoid:** ignore it. It is Phase 4's (SAFE-06) concern, not a pin.

### Pitfall 5: Closed-writer message embeds the script's path spelling
**What goes wrong:** building the expected text from the resolved/canonical path fails, because `original_path_` is what the script passed.
**How to avoid:** pass a relative name (`"closed.csv"`) and assert `Cannot write_row: writer for 'closed.csv' is already closed`, or build the expectation from the same `lp(...)` string the script embeds.

### Pitfall 6: Formatter drift
**What goes wrong:** the CI clang-format job fails on the new test file.
**How to avoid:** `uvx --from clang-format==22.1.8 clang-format -i <changed .cpp files>` before committing. Run `bunx biome check test/lua-api-sync.test.ts` for the TS file.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|---|---|---|---|---|
| MSVC cl | C++ builds | ✓ | 14.51.36231 (VS 18) | — |
| CMake / Ninja | presets, build/ | ✓ | 4.3.1-msvc1 / 1.13.2 | — |
| bun | JS sync test | ✓ | 1.3.14 | — |
| uv / uvx | clang-format 22.1.8, Python suite | ✓ | 0.12.3 | — |
| julia | test-all step 3 | ✓ | 1.11.9 | — |
| dart | test-all step 4 | ✓ | 3.13.4 | — |
| build/, build/dev, build/release | all runs | ✓ configured, binaries at HEAD source | Debug/Debug/Release | — |

Nothing is missing.

## Package Legitimacy Audit

No external packages are installed in this phase (tests only, existing gtest/bun). **Packages removed:** none. **Flagged:** none.

## Security Domain

This phase adds tests only and changes no production code. The relevant control is the existing path-containment gate.

| ASVS Category | Applies | Standard Control |
|---|---|---|
| V5 Input Validation | yes | Explicit `get_type()` checks in the Lua bindings. The pins lock their *order* |
| V12 Files and Resources | yes | `resolve_sandboxed_path` strict containment (`:1301-1356`). The pins make sure no options error can mask an escape |
| V2/V3/V4/V6 | no | — |

| Pattern | STRIDE | Mitigation pinned |
|---|---|---|
| Path traversal (`../escape`) | Tampering / Info disclosure | containment error wins over every options error (rows 4, 6-9) |
| Unbounded allocation from a sparse key (`{[1e9]='x'}`) | DoS | 1,000,000 width cap (rows 1-2) |

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|---|---|---|
| A1 | The move test fails deterministically (not via UB) if Phase 2 puts `RunHandles` outside the heap `Impl` *while the moved-from runner is alive* | Pattern 2 notes | Low. The test still pins the defined behaviour today. It may just be a weaker guard for Phase 2 |
| A2 | 441 is the resulting `Lua*` count | Placement | None. The executor records the actual number |

## Open Questions

1. **Boundary pin at exactly 1,000,000?** `kMaxWidth` uses `>`, so a 1,000,000-key row is legal. Pinning it would allocate about 1M strings and write a row about 1 MB wide.
   Recommendation: skip it. The requirement pins 1,000,001, and a `>`→`>=` slip during dedupe would need a deliberate edit.
2. **Should the second-script globals assert they came from `source`'s state** (e.g. `assert(g ~= nil)` from the first run before reassigning)? That would make the "moved state is the source's" claim explicit. Recommendation: optional. One line, at the planner's discretion.

## Sources

### Primary (HIGH confidence)
- `src/lua_runner.cpp` (read this session: 238-372, 385, 440-470, 540-700, 740-960, 1020-1040, 1300-1420, 2480-2539)
- `src/csv/csv_write.cpp:1-40, 100-175`; `include/quiver/lua_runner.h`; `tests/test_lua_runner.h`
- `tests/test_lua_binary.cpp`, `tests/test_lua_runner_{write_csv,read_csv,csv_export,csv_import}.cpp`, `tests/test_database_lifecycle.cpp`, `tests/CMakeLists.txt`
- `bindings/js/test/lua-api-sync.test.ts` (full), `bindings/js/AGENTS.md:30-45`, `tests/AGENTS.md` (full)
- Runtime probes: `quiver_cli.exe` (Debug + Release) for every message; a scratch move-test exe linked to both `quiver.lib` builds; the bun parser probe; `--gtest_list_tests` counts in all three build dirs
- Lua 5.4 Reference Manual §2.1 (float-key normalization), lua.org

### Secondary
- `.planning/research/PITFALLS.md` Pitfall 10 (lifetime/move); `.planning/research/LUA-RUNNER-MAP.md` named pins + critic pass

## Metadata

**Confidence breakdown:**
- Messages and check order: HIGH. Observed at runtime in both builds.
- Release safety: HIGH. Each parameter type was read and the calls ran in Release.
- Move tests: HIGH for today's behaviour (prototype passed in both builds). MEDIUM for their strength as a Phase 2 guard (A1).
- Sync guard: HIGH. The parser output was reproduced with bun.

**Research date:** 2026-10-02
**Valid until:** the first commit that touches `src/lua_runner.cpp` (all line numbers are pinned to `bdf9087`/`0b6448b`)
