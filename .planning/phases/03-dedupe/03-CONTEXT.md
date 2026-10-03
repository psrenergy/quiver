# Phase 3: Dedupe - Context

**Gathered:** 2026-10-03
**Status:** Ready for planning
**Mode:** Smart discuss, infrastructure skip. This is a behaviour-neutral refactor with only technical success criteria. The ROADMAP, REQUIREMENTS (DEDUP-01..06) and `.planning/research/LUA-RUNNER-MAP.md` (M1–M16) already specify each dedupe, so no grey areas were put to the user. The decisions below are carried forward from upstream documents and Phase 2's results, not newly made.

<domain>
## Phase Boundary

Every repeated pattern in `src/lua_runner/` exists exactly once, so each Phase 4 fix lands in a single place, and no observable behaviour changes.

In scope: M1, M3 (M2 already landed in Phase 2: confirm no variadic `Database` pair came back), M4, M5, M7, M8, M9, M10, M11, M12, M13, M14, M15, M16, plus the `src/AGENTS.md` description of the shared helpers. The requirements are DEDUP-01..06.

Out of scope: any behaviour fix (C1–C8, `require_table`, `SOL_ALL_SAFETIES_ON`, `load` text-only, empty-array rule). These are Phase 4. Also out: the path-policy unit test and the CHANGELOG/docs sweep (Phase 5), and M6 (see Deferred).

</domain>

<decisions>
## Implementation Decisions

### Behaviour neutrality (locked: PROJECT.md Constraints, DEDUP-06)
- Zero behaviour change. Every check order that decides which error a call reports stays byte-for-byte. Each plan's SUMMARY states "reordered checks: none" (the phase PR repeats it).
- No test expectation changes. Gate counts: Windows `quiver_tests --gtest_filter=Lua*` = 444 / 12 suites in Debug and Release, and C API `LuaRunnerCApiTest` = 27. Linux GCC/Clang `Lua*` = 442 (2 `_WIN32`-only tests) plus 1 root skip. The six suites (`scripts/test-all.bat`) must pass.
- The only allowed observable text changes are Debug-only sol2 diagnostics. These are the dot-call error, plus the C++ signature that sol2's Debug "bad argument" message prints. That signature changes for the 17 forwarders that become member pointers (M3) and for the `query_*` functions that return `std::optional` and drop `sol::this_state` (M5). The exact before/after strings (03-RESEARCH.md) go in the SUMMARY and the PR, and no test may pin them. Release builds print none of this. *Amended after research (orchestrator decision, flagged to the user): the roadmap's member-pointer allowance is extended to the same mechanism in M5.* M1 keeps two lambdas over one helper so `db:transaction(5)`'s text does not change.
- Error message wording stays identical, including where M16 merges two throw sites into one.

### Registration shape (locked: sync-test contract)
- Every `db:` method stays registered as `bind.set_function("name", ...)` on the parameter named `bind`. Every `quiver.*` function stays registered as `ns.set_function("name", ...)` with a literal name, one per call (M14 keeps literal `ns.set_function("gt", …)` calls).
- The sync test (hardened in Phase 2, e5b00b7) must still extract the same method set: 71 `db:` + 15 `quiver.*`, with the `.set_function(` count equal to the parsed count. `CsvWriter` member-pointer lines (M11) must fit the 120-column limit as one method per line in the variadic usertype.
- Template adapters keep the line shape `bind.set_function("read_scalar_strings", &bulk_read_lua<&Database::read_scalar_strings>)` (M4).

### File layout and build cost (from Phase 2 results)
- No new `src/lua_runner/*.cpp` TU unless a helper has nowhere else to live. Phase 2 measured sol2 CPU time across the Lua files rising from 61 s to 223 s, because each TU parses sol2 separately. Shared helpers go into `internal.h` (templates/inline) or stay in the TU that uses them.
- Every `src/lua_runner/` file stays at about 450 lines or less (`csv.cpp` is at 446 now, so M10/M11/M12 should shrink it).
- csv-parser headers are never included from `src/lua_runner/`. clang-format 22.1.8 must be clean. `scripts/tidy.bat` may report fewer warnings than the Phase 2 baseline of 15, never more.

### Plan granularity
- One phase PR into master (roadmap decision), split into small commits: one commit per M-item or per tight cluster, each green on its own (build + `Lua*` + sync test), so the series bisects.
- Suggested waves: (1) the registration/forwarder/adapter cluster M1, M3, M4, M5, M7, M8 in `db_*.cpp`; (2) the CSV cluster M9, M10, M11, M12 in `csv.cpp`; (3) the binary/registry cluster M13, M14, M15 in `binary.cpp`/`lua_runner.cpp`, plus M16 and the AGENTS.md update. The planner may regroup by file overlap.

### Claude's Discretion
- The exact helper names and signatures (e.g. `bulk_read_lua`, `group_list_lua`, `header_object`, the M1 transaction helper), as long as the registration shapes above hold.
- Whether to fold in Phase 2 review items IN-01 (move the `db_core.cpp` NOLINT region up to cover the `query_*_lua` helpers) and IN-02 (rename the lambda-local `lua` that shadows the `bind_csv`/`bind_binary` parameter). Both are behaviour-neutral cleanups in files this phase already touches. IN-03 (key type checks) is Phase 4.
- The M13 rename target for the close-at-exit function (it must cover binary files too).

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- `src/lua_runner/internal.h` (225 lines): the shared declarations, templates and inline helpers (`to_lua_table`, `lua_to_value`, `is_lua_boolean`, `RunHandles`, the binder declarations) live here. This is the natural home for the new adapter templates.
- `lua_to_value` already has the dispatch order that `csv_cell_to_string` duplicates (M10).
- sol2 pushes `std::optional` `nullopt` as nil, which M5 relies on.

### Established Patterns
- Seven binders: `bind_core`, `bind_read`, `bind_write`, `bind_metadata`, `bind_time_series` (on the `Database` usertype `bind`), `bind_csv(lua, bind, handles)` and `bind_binary(lua, bind, ns, db, handles)`. They are called in that order from the `Impl` constructor (`lua_runner.cpp:73-94`), after `open_libraries`, the `dofile`/`loadfile` nil-out and the `quiver` table.
- Closures capture `handles`/`db` by reference, never the `Impl` pointer. `sizeof(LuaRunner)==sizeof(void*)` is static-asserted in the tests.
- Each file has one `NOLINTBEGIN/END(performance-unnecessary-value-param)` pair around the sol2 lambdas.
- Line ranges in LUA-RUNNER-MAP.md refer to the pre-split `src/lua_runner.cpp` (`bab557e`). Find the code by symbol name in `src/lua_runner/`, not by line number.

### Integration Points
- `bindings/js/test/lua-api-sync.test.ts`: parses the registrations. It is the first thing to break if a registration shape changes.
- `src/AGENTS.md`: the `src/lua_runner/` layout bullet and the per-method boilerplate notes (`csv_cell_to_string` writes a boolean as 1/0; `CsvWriter` wrapper wording) must be updated for M10/M11 and the new shared helpers (success criterion 5).
- Phase 1 pins: the check-order pins in `LuaBinaryTest`, `LuaRunner_ReadCsv`, `LuaRunner_ExportCSV`, `LuaRunner_ImportCSV` and `LuaRunner_WriteCsv*`, the lifecycle move pins and the key-width cap pins. These are the safety net for M9/M11/M12/M13.

</code_context>

<specifics>
## Specific Ideas

- M1 exists so that C4 (failed COMMIT does not roll back) and C6 (non-function argument) are fixed once in Phase 4. The helper only sequences public `Database` calls, which keeps the "Dry runs live on Database" decision intact.
- M14: use transparent functors (`std::plus<>`, …, `std::logical_and<>`/`std::logical_or<>`, which call the overloaded `&&`/`||`) in place of `BinOp`/`apply_binop`.
- M15: the `found[0..7]` slots get names, so two swapped `metadata_array<std::string>` slots can no longer compile silently.
- M12 also fixes the stale "forward-looking" comment about `header_row = 0`. Phase 2's 02-01 already rewrote one such comment; check that none remains.

</specifics>

<deferred>
## Deferred Ideas

- M6 (`value_to_lua_object` duplicates sol2's `std::variant` pusher): optional per the research map, and only after a macOS CI run, because platform-default sol2 macros already caused trouble once (the `SOL_NO_NIL` story). Not in DEDUP-01..06; it stays deferred.
- Precompiled headers / reducing the sol2 parse cost: compile time is explicitly out of scope (PROJECT.md). Note it only.

</deferred>
