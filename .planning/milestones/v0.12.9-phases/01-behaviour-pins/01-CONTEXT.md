# Phase 1: Behaviour Pins - Context

**Gathered:** 2026-10-02
**Status:** Ready for planning

<domain>
## Phase Boundary

Phase 1 is tests only. They pin every behaviour that the Phase 2 split of `src/lua_runner.cpp` could
silently break, before any production source changes:
- the 1,000,000 key-width cap;
- the check orders that decide which error a call reports;
- the JS sync test's usertype and `open_libraries` guards;
- a `LuaRunner` move after handles have been registered.

The diff touches only `tests/`, `bindings/js/test/`, `tests/AGENTS.md` and `bindings/js/AGENTS.md`.
Every new test is defined behaviour, green in both the `dev` (Debug) and `release` presets.

</domain>

<decisions>
## Implementation Decisions

### Milestone scope change (decided during this discussion, applies beyond Phase 1)
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

### Check-order pins (PIN-02)
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

### Key-width cap (PIN-01; message text taken from the code)
- Row: `w:write_row({[1000001]='x'})` →
  `Cannot write_row: row key 1000001 exceeds the maximum width of 1000000`.
- Header: `db:write_csv(path, { header = {[2e6]='a'} })` →
  `Cannot write_csv: option 'header' key 2000000 exceeds the maximum width of 1000000`. Lua 5.4
  normalizes the integral float key `2e6` to the integer `2000000`. The planner verifies this text
  against `csv_max_integer_key` (`src/lua_runner.cpp:348-372`, header caller `:460`).

### Sync-test guards (PIN-03)
- **D-06:** Floors only. Each of `BinaryFile`, `BinaryMetadata`, `Expression` and `CsvWriter` must
  parse to more than 0 methods, and `open_libraries(` must appear exactly once in the parsed
  source. There are no exact per-usertype counts and no committed snapshot. Phase 2 does its
  before/after method-set diff once, by hand. The existing `dbMethods > 40`, `quiverFns > 10` floors
  stay. The mutation check (delete one usertype's registrations, see the test fail, revert) is
  done by hand and not committed (locked by the roadmap).

### Test placement
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

### Message matching
- **D-10:** Use the existing `expect_lua_error` (`tests/test_lua_runner.h`, substring match) with the
  **full Pattern 1 sentence**. There is no new exact-match helper.
- **D-11:** The check-order pins assert the **full winning sentence**, including the operation name
  (e.g. `Cannot open_file: mode must be "r" or "w"`), so a helper that misattributes the operation
  also fails. The containment message ends with the resolved root
  (`... escapes the database directory '<root>'`, `src/lua_runner.cpp:1352`), and that root depends
  on the temp directory and canonicalization. Those pins assert the sentence up to and including
  `escapes the database directory`, as the existing escape tests do.

### Baseline (PIN success criterion 5)
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

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Milestone scope
- `.planning/ROADMAP.md` § Phase 1: goal and the five success criteria. Phase 5's rename items are
  superseded by D-01 here.
- `.planning/REQUIREMENTS.md` § Behaviour pins: PIN-01..PIN-05.
- `.planning/PROJECT.md` § Constraints: the order-sensitive code, and the zero-behaviour rule.

### Evidence and pitfalls
- `.planning/research/LUA-RUNNER-MAP.md`:
  - §T1: the untested 1,000,000 cap;
  - the named-pins list (~line 281);
  - "Critic pass" corrections (they take precedence).
- `.planning/research/PITFALLS.md`:
  - Pitfall 1: no Release-UB pins;
  - Pitfall 8: check order, two-bad-argument tests;
  - the lifetime/move pitfall (~line 222): `[this]` captures across a move.

### Project rules
- `AGENTS.md` (root): the Lua file-operations decision, the CSV decision, and the three error-message
  patterns.
- `tests/AGENTS.md`: test layout, the file-touching Lua tests rule (must use `LuaSandboxTest`), and
  the suite list to update.
- `bindings/js/AGENTS.md`: sync-test contract and the `LUA_DB_API_REFERENCE` rules.
- `src/AGENTS.md`: LuaRunner implementation notes (GcGuard, writer registry).

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `expect_lua_error(lua, script, substring)`: `tests/test_lua_runner.h`. The assertion for every
  new pin.
- `LuaSandboxTest`: `tests/test_lua_runner.h`. A per-test temp directory, `db_path()` and
  `sandbox`. Base class for every file-backed fixture.
- `lp()` path helper, which converts to forward slashes for embedding in Lua literals:
  `tests/test_lua_runner_write_csv.cpp:14`, mirrored in `LuaBinaryTest::lp` and the read_csv file.
- `md1()` metadata snippet: `tests/test_lua_binary.cpp:33`, for `db:open_file(..., 'w', md)`.
- `expect_prefixed_error`: file-local copies in the read_csv and write_csv test files. Not needed,
  per D-10.

### Established Patterns
- Shape of a check-order pin: `EscapingPathTakesPrecedenceOverInvalidSeparator`
  (`tests/test_lua_runner_write_csv.cpp:1171`).
- Shape of a run-exit closure test: `HandleFromAnEarlierRunIsClosed`
  (`tests/test_lua_binary.cpp:149`), `UnclosedWriterHeldInAGlobalIsAlsoFlushedWhenRunReturns`
  (`tests/test_lua_runner_write_csv.cpp:1508`).
- Move tests: `tests/test_database_lifecycle.cpp:49-62` (`MoveConstructor` / `MoveAssignment`).
- `run()` wraps every script error as `"Failed to run Lua script: " + err.what()`
  (`src/lua_runner.cpp:2527`).

### Code Sites Being Pinned (`src/lua_runner.cpp`, as of this commit)
- `csv_max_integer_key`, the cap: 348-372. Row caller at 385, header caller at 460.
- `open_file`, mode check then `resolve_sandboxed_path`: 759-776.
- `read_csv`: 790-796. `read_csv_stream`, on_row check → resolve → options: 822-838.
- `write_csv`, resolve → options → same-path guard: 874-891.
- `CsvWriter:write_row`, type → closed → cells: 896-940.
- `export_csv` / `import_csv`, resolve → `parse_csv_options`: 664-683.
- Escape message: ~1352. `LuaRunner` move ops: 2495-2497.
- Options decoders, all `sol::object` with explicit type checks: 562, 1362, 1406.

### Integration Points
- `tests/CMakeLists.txt` (~line 31-49): add `test_lua_runner_lifecycle.cpp` to the `quiver_tests`
  sources.
- `bindings/js/test/lua-api-sync.test.ts`: add the guards to the
  "parse found the binding surface" test, or beside it.
- Release run: `cmake --preset release && cmake --build --preset release`, then
  `build/release/bin/quiver_tests.exe --gtest_filter=Lua*`. Run the JS sync test with `bun test`
  directly, because quoted filters break through `cmd //c`.

</code_context>

<specifics>
## Specific Ideas

- No new test or code comment may carry a planning ID (`D-xx`, `PIN-xx`, `C1`, `LUA-xx`, ...). This
  file's decision numbers are for planning only. Comments state the reason or name the pinning test.
- No test may exercise a Release-UB path:
  - no non-table passed to an unchecked `sol::table` parameter (C1);
  - no non-string key at the four C2 map-key sites;
  - no wrong-type optional (C5).

</specifics>

<deferred>
## Deferred Ideas

- None for Phase 1 beyond the milestone change in D-01. The ROADMAP, REQUIREMENTS and PROJECT edits
  that apply it are a separate step after this context is committed.

</deferred>

---

*Phase: 01-behaviour-pins*
*Context gathered: 2026-10-02*
