# Requirements: Quiver Lua Runner Refactor

**Defined:** 2026-10-02
**Core Value:** Every file in the Lua scripting layer is small and single-purpose enough for an agent to change safely, and every existing script behaves exactly as before, apart from the deliberate, test-pinned fixes listed below.

Evidence for each item (line numbers at `bdf9087`) is in `.planning/research/LUA-RUNNER-MAP.md`; research is in `.planning/research/SUMMARY.md`.

## v1 Requirements

### Behaviour pins (tests before code moves)

- [x] **PIN-01**: Tests pin the 1,000,000 key-width cap for `w:write_row` rows (`{[1000001]='x'}`) and `db:write_csv` headers (`{[2e6]='a'}`), asserting the full Pattern 1 message.
- [x] **PIN-02**: Tests pin check order where it decides which error a call reports: `db:open_file` validates `mode` before the path; containment is checked before options decode; `w:write_row` checks the argument type before closed state.
- [x] **PIN-03**: The JS sync test asserts at least one parsed method for each of `BinaryFile`, `BinaryMetadata`, `Expression` and `CsvWriter`, and exactly one `open_libraries(` call, so a missed file or usertype fails instead of passing vacuously.
- [x] **PIN-04**: A test moves a runner (move-construct and move-assign) after `db:write_csv`/`db:open_file` have registered handles, then runs again. This shows that captured state survives a pimpl move.
- [x] **PIN-05**: Pins cover only behaviour that is defined in both Debug and Release. No test asserts today's Release-UB paths (C1/C2/C5); those are written red-then-green with their fixes.

### Mechanical split (zero behaviour change)

- [x] **SPLIT-01**: `src/lua_runner.cpp` is replaced by `src/lua_runner/`: `lua_runner.cpp`, `internal.h`, `return_json.cpp`, `path_policy.cpp`, `db_core.cpp`, `db_read.cpp`, `db_write.cpp`, `db_metadata.cpp`, `db_time_series.cpp`, `csv.cpp`, `binary.cpp`. Each binder registers and implements its own slice, and no file is over ~450 lines.
- [x] **SPLIT-02**: There is exactly one `new_usertype<Database>` in the folder (grep count is 1). Binders receive `sol::usertype<Database>& bind` and the `quiver` table as `ns`. The ctor order is preserved: `open_libraries` → nil `dofile`/`loadfile` → `quiver` table → binders → `lua["db"] = &db`.
- [x] **SPLIT-03**: Instance state lives in a `RunHandles` (writer registry, binary-file registry, `path_has_open_writer`, close-at-exit). It is held by the heap-allocated `Impl` and declared before `lua`. The three `[this]` captures become `[&handles]` / `[&db]`. `GcGuard` is still declared before `result`, with close then exactly one `collect_garbage()`.
- [x] **SPLIT-04**: The JS sync test reads every source file under `src/lua_runner/` in sorted order, parses each one separately (resetting `current` at each file boundary), and extracts the same method set as before the split.
- [x] **SPLIT-05**: `/bigobj` (MSVC) and `-Wa,-mbig-obj` (MinGW) apply to the whole target. All sol2 TUs are in the `quiver` target with identical PRIVATE defines, listed explicitly (no glob). Each file whose functions take sol2 arguments by value has its own NOLINT pair (check name `performance-unnecessary-value-param`). clang-format 22.1.8 is clean on the new files, and `scripts/tidy.bat` reports nothing beyond the 15 pre-existing warnings recorded as the baseline.
- [x] **SPLIT-06**: No test expectation changes. All C++ Lua tests (`--gtest_filter=Lua*`: 428 at `bdf9087` plus the Phase 1 pins), all 27 C API tests, and the Julia, Dart, Python and JS suites pass unmodified.
- [x] **SPLIT-07**: Every citation of `src/lua_runner.cpp` is updated to the new paths: the AGENTS.md files, `src/csv/*` comments, `cmake/Platform.cmake`, `bindings/dart/hook/build.dart`, the tests, and the `lua-api.ts` maintainer header. Re-derive the list with `git grep`. The C API translation unit `src/c/lua_runner.cpp` and `test_c_api_lua_runner.cpp` keep their names.

### Dedupe (zero behaviour change)

- [x] **DEDUP-01**: `db:transaction` and `db:dry_run` share one helper (M1).
- [x] **DEDUP-02**: Every `db:` method is registered with `bind.set_function`, and plain forwarders become member pointers (M2/M3). The Debug-only dot-call error text change is noted in the PR.
- [x] **DEDUP-03**: The bulk readers go through two adapter templates (M4). `query_*` and the by-id composites return `std::optional` directly (M5). `read_vectors_by_id` and `read_sets_by_id` become one template (M7).
- [x] **DEDUP-04**: The metadata wrappers collapse into one helper (M8). The option decoders are consolidated, with nil handling kept per caller (M9). `csv_cell_to_string` routes through `lua_to_value` (M10). `CsvWriter` behaviour becomes members (M11). `read_csv` and `read_csv_stream` share one header rule (M12).
- [x] **DEDUP-05**: Expired entries are pruned on insert and the close-at-exit function is renamed to cover binary files (M13). `BinOp`/`apply_binop` are replaced by functors (M14). `build_metadata_from_lua` uses named option slots (M15). Duplicate messages are merged (M16).
- [x] **DEDUP-06**: Each dedupe PR states "reordered checks: none", and every suite passes with no expectation changes.

### Release type safety

- [x] **SAFE-01**: Every table parameter is taken as `sol::object` and checked with `require_table` (`get_type() == sol::type::table`, never `is<sol::table>()`). Value-level sites are included (`table_to_element` cells, `collect_group_columns`). A wrong type raises a Pattern 1 error naming the operation and argument (C1).
- [x] **SAFE-02**: Map keys are type-checked by `lua_string_key` before they become column, dimension, attribute or files-column names: `lua_table_to_value_map`, `table_to_element`, `lua_table_to_dim_map` and `update_time_series_files` (C2).
- [x] **SAFE-03**: Optional arguments go through `optional_from_lua` at all 8 sites. nil or missing means absent; any other wrong type raises Pattern 1 (C5).
- [x] **SAFE-04**: `db:transaction` / `db:dry_run` reject a non-function argument with Pattern 1 before any side effect (C6).
- [x] **SAFE-05**: Every type error from these checks ends with a consistent "got <lua type>" suffix.
- [x] **SAFE-06**: `SOL_ALL_SAFETIES_ON=1` and `SOL_PRINT_ERRORS=0` are PRIVATE on `quiver`, landing after SAFE-01..04. The no-op `SOL_SAFE_FUNCTION=1` define and its AGENTS.md claim are deleted. The Release cost is measured once by hand and reported in the PR, with no committed perf scripts.
- [x] **SAFE-07**: Lua `load` accepts text chunks only. A binary (bytecode) chunk raises an error, string-form `load` keeps working, and both cases are tested.

### Behaviour fixes

- [x] **FIX-01**: If COMMIT fails inside `db:transaction`, it does a best-effort rollback and rethrows, matching Julia, Python and Dart (C4).
- [x] **FIX-02**: An empty array in Lua `create_element`/`update_element` reaches the core as it does in C++/Python/JS. `update_element` with `{col = {}}` clears the group, and a misspelled empty column throws there. `create_element` still skips it. Tests and the `lua-api.ts` text are updated (C7).
- [x] **FIX-03**: The expression helper errors name the public operation that was called (C8). Unreachable branches are removed: the nil branch in `update_time_series_files` (D1), `lua_data_type_name`'s `default:`, and `apply_binop`'s throw.

### Tests

- [x] **TEST-01**: `resolve_sandboxed_path` is unit-tested directly through a sol2-free header, covering containment, escape rejection, the root itself, `:memory:`, and the device-name prefix. These tests are in addition to the existing Lua-level tests, and their suite name sits outside the `Lua*` gtest filter.

### Docs

- [x] **DOC-01**: Planning-ID comments (`D-xx`, `LUA-xx`, `WRITE-xx`, `FMT-xx`, `TEST-xx`, and references to deleted `.planning` files) are replaced repo-wide with their one-line reason or the test that pins them.
- [x] **DOC-02**: The root, `src/`, `src/c/`, `tests/` and four binding AGENTS.md files describe the new layout and changed decisions: the `src/lua_runner/` layout, the C7 rule, text-only `load`, and the safety flags. (Deleting the `SOL_SAFE_FUNCTION` claim is owned by SAFE-06.)
- [x] **DOC-03**: CHANGELOG gets a `## [0.13.0] — unreleased` section. BREAKING entries cover wrong-type arguments now throwing (C1/C5), the empty-array change (C7) and text-only `load`. `### Fixed` entries cover C2, C4, C6 and C8.
- [x] **DOC-04**: The shipped `LUA_DB_API_REFERENCE` text matches the code: the empty-array rule, and a statement of what the sandbox does not limit (instructions, memory, wall time, and globals persisting across `run()`). The sync test stays green.

## v2 Requirements

### Hardening

- **HARD-01**: Resource limits for scripts (instruction-count hook, allocator cap, wall-clock deadline)
- **HARD-02**: `__close` metamethods on CSV writer and binary-file handles
- **HARD-03**: A fresh `_ENV` per `run()`, so globals do not persist between runs

### Cleanup

- **CLEAN-01**: Replace `value_to_lua_object` with sol2's `std::variant` pusher (M6), after a macOS CI run confirms the defaults
- **CLEAN-02**: Move the sol2-free half of the JSON encoder to `src/json/` once a second JSON consumer exists

## Out of Scope

| Feature | Reason |
|---------|--------|
| Sparse-extent cap on `update_vector_group`/`update_set_group` | User decision: the host limits scripts; "sparse columns write NULL up to the largest index" stands |
| Committed perf scripts / Lua micro-benchmark | User decision: the safety-flag cost is measured once, by hand |
| New `db:` methods or Lua features | This milestone restructures and fixes; it does not extend |
| Compile-time optimisation, PCH | Not a goal; only `/bigobj` has to keep working |
| `LUA_USE_APICHECK`, Lua compiled as C++ | APICHECK aborts the host in Debug and does nothing in Release; C++ Lua changes exception semantics everywhere |
| Global sol2 error-rewriting handler | Pattern 1 messages come from explicit checks at the call site |
| SAVEPOINTs for nested writes | Rejected design decision (no-op TransactionGuard) |
| Lua whole-group / boolean readers, binary/expression in Dart/Python/JS | Documented design decisions |
| Relocating `LUA_DB_API_REFERENCE` | Do-Not-Fix; only its text changes |
| Renaming the `"Failed to run Lua script:"` prefix | Asserted across layers; not a naming concern |
| Backfilling a CHANGELOG `[0.12.9]` section | Maintainer's release ritual, not part of this milestone |
| Fixing the documented `update_element` array fan-out | Separate open item in AGENTS.md; a breaking core change, not part of the Lua layer |
| REN-01: the C++ class becomes `quiver::Sandbox` (`include/quiver/sandbox.h`) | Rename dropped from this milestone by user decision (keep it simple), 2026-10-02 |
| REN-02: the C API becomes `quiver_sandbox_t` / `quiver_sandbox_new/free/run/free_string` | Rename dropped from this milestone by user decision (keep it simple), 2026-10-02 |
| REN-03: every binding exposes `Sandbox`, with its FFI declarations, source, test and header files renamed (Python module path `quiverdb.sandbox`) | Rename dropped from this milestone by user decision (keep it simple), 2026-10-02 |
| REN-04: the closed, disposed and not-closed messages in Python, Dart and JS say "Sandbox" | Rename dropped from this milestone by user decision (keep it simple), 2026-10-02 |
| REN-05 (rename part): `resolve_sandboxed_path` becomes `resolve_contained_path`, and the file rule is called "directory containment" | Rename dropped from this milestone by user decision (keep it simple), 2026-10-02. Its non-rename sentence (what the sandbox does not limit) moved to DOC-04 |
| REN-06: one `Sandbox*` gtest prefix for all Lua suites (`LuaSandboxTest` becomes `SandboxFileTest`, files `test_sandbox_*`) | Rename dropped from this milestone by user decision (keep it simple), 2026-10-02. The Lua count filter stays `Lua*` |
| REN-07: `tests/sandbox` becomes `tests/scratch` / `quiver_scratch` | Rename dropped from this milestone by user decision (keep it simple), 2026-10-02 |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| PIN-01 | Phase 1 | Complete |
| PIN-02 | Phase 1 | Complete |
| PIN-03 | Phase 1 | Complete |
| PIN-04 | Phase 1 | Complete |
| PIN-05 | Phase 1 | Complete |
| SPLIT-01 | Phase 2 | Complete |
| SPLIT-02 | Phase 2 | Complete |
| SPLIT-03 | Phase 2 | Complete |
| SPLIT-04 | Phase 2 | Complete |
| SPLIT-05 | Phase 2 | Complete |
| SPLIT-06 | Phase 2 | Complete |
| SPLIT-07 | Phase 2 | Complete |
| DEDUP-01 | Phase 3 | Complete |
| DEDUP-02 | Phase 3 | Complete |
| DEDUP-03 | Phase 3 | Complete |
| DEDUP-04 | Phase 3 | Complete |
| DEDUP-05 | Phase 3 | Complete |
| DEDUP-06 | Phase 3 | Complete |
| SAFE-01 | Phase 4 | Complete |
| SAFE-02 | Phase 4 | Complete |
| SAFE-03 | Phase 4 | Complete |
| SAFE-04 | Phase 4 | Complete |
| SAFE-05 | Phase 4 | Complete |
| SAFE-06 | Phase 4 | Complete |
| SAFE-07 | Phase 4 | Complete |
| FIX-01 | Phase 4 | Complete |
| FIX-02 | Phase 4 | Complete |
| FIX-03 | Phase 4 | Complete |
| TEST-01 | Phase 5 | Complete |
| DOC-01 | Phase 5 | Complete |
| DOC-02 | Phase 5 | Complete |
| DOC-03 | Phase 5 | Complete |
| DOC-04 | Phase 5 | Complete |

Partial deliveries (each requirement is still owned by the one phase above; earlier phases only advance it):

- DEDUP-02: the M2 half (the 17 variadic `Database` pairs become `bind.set_function`) is forced by Phase 2's per-domain binders and lands there. Phase 3 adds the member-pointer forwarders (M3) and confirms no variadic pair came back.
- DOC-01: Phase 2 clears the `src/lua_runner.cpp` share (59 lines at `bab557e`) as the code moves, and Phases 1-4 add no new planning IDs. Phase 5 clears the rest and gates repo-wide.
- DOC-02: every phase updates the AGENTS.md nearest its change (Phase 2 the folder layout, Phase 4 the C7 rule, text-only `load` and the safety flags). Phase 5 does the final check.
- DOC-03: Phase 4 opens `[0.13.0] — unreleased` with its compare link and writes every entry. Phase 5 adds no rename entry; it checks the section is complete against the merged phases.
- DOC-04: Phase 4 rewrites the empty-array text with FIX-02. Phase 5 adds the statement of what the sandbox does not limit.

**Coverage:**

- v1 requirements: 33 total
- Mapped to phases: 33
- Unmapped: 0 ✓

---
*Requirements defined: 2026-10-02*
*Last updated: 2026-10-02 after scope change (LuaRunner rename dropped: REN-01..07 moved to Out of Scope, REN-05's scope sentence folded into DOC-04, split folder is `src/lua_runner/`)*
