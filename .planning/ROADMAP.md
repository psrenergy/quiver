# Roadmap: Quiver Lua Runner Refactor

## Overview

The Lua scripting layer starts as one 2,539-line `src/lua_runner.cpp` and ends as a `src/lua_runner/` folder
of small per-domain files. The `LuaRunner` class and its names in every layer stay as they are. The order is
strict, because each phase protects the next. First, tests pin the behaviour the split could silently break.
Then the file is split mechanically, and the split is provably behaviour-neutral against those pins. Next the
duplicated boilerplate is collapsed, so each fix lands in exactly one place. After that the C1-C8 bugs are
fixed and Release builds gain argument type checks, with the `SOL_ALL_SAFETIES_ON` backstop last in that
phase. The path-policy unit test and the docs go last, so that the AGENTS.md files, reference text and
CHANGELOG entries are finished once, against the final code.
Each phase is one PR into master and is green on its own. Everything lands in `0.13.0` with no version
bump. Every phase keeps the AGENTS.md nearest its change current (Self-Updating rule). No phase writes a
planning ID (legacy or this milestone's) into a code or test comment; Phase 5 gates on it. After Phases 2
and 4, the Dart suite runs only after deleting `bindings/dart/.dart_tool/hooks_runner/` and
`.dart_tool/lib/`, because the hook's cache does not notice source-list or define changes.

## Phases

**Phase Numbering:**

- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [x] **Phase 1: Behaviour Pins** - Tests pin every behaviour the split could break, before any code moves (completed 2026-10-02)
- [x] **Phase 2: Mechanical Split** - `src/lua_runner.cpp` becomes `src/lua_runner/` per-domain files with zero behaviour change (completed 2026-10-03)
- [ ] **Phase 3: Dedupe** - Repeated boilerplate (M1, M3-M5, M7-M16) collapses into shared helpers with zero behaviour change
- [ ] **Phase 4: Fixes and Release Type Safety** - Wrong-type script arguments raise Pattern 1 errors in Release, and C1-C8 are fixed (backstop flag last)
- [ ] **Phase 5: Path-Policy Test and Docs** - The path-policy unit test, the repo-wide planning-ID sweep, and the finished docs for 0.13.0

## Phase Details

### Phase 1: Behaviour Pins

**Goal**: Every existing behaviour that moving the code could silently break is pinned by a test that is defined, and green, in both Debug and Release, before any production source changes.
**Depends on**: Nothing (first phase)
**Requirements**: PIN-01, PIN-02, PIN-03, PIN-04, PIN-05
**Success Criteria** (what must be TRUE):

  1. New C++ tests assert the full Pattern 1 message for the 1,000,000 key-width cap on `w:write_row` (`{[1000001]='x'}`) and on the `db:write_csv` header (`{[2e6]='a'}`). Separate check-order tests each pass two bad arguments and assert which error is reported: `db:open_file` checks `mode` before the path, containment comes before options decoding, and `w:write_row` checks the argument type before closed state.
  2. A test runs a script that registers handles through `db:write_csv` and `db:open_file`, then move-constructs and move-assigns the runner. The moved-to runner runs a second script that calls `db:write_csv` and `db:open_file` again, so the `[this]` captures are exercised through the moved `Impl`, and the handles that second script opens are closed at its `run()` exit. (Handles from the first run were already closed when it returned, as `HandleFromAnEarlierRunIsClosed` pins, so they prove nothing about the move.)
  3. The JS sync test fails if any of `BinaryFile`, `BinaryMetadata`, `Expression` or `CsvWriter` parses to zero methods, or if the count of `open_libraries(` is not exactly 1. A mutation check confirms this: deleting one usertype's registrations makes the test fail, and the deletion is then reverted.
  4. Every new test passes under both the `dev` (Debug) and `release` presets. None of them exercises a Release-UB path: no non-table argument (C1), no non-string key at the four C2 map-key sites, and no wrong-type optional (C5). The numeric-key cap pins in criterion 1 are defined behaviour and stay. The phase diff touches only `tests/`, `bindings/js/test/` and the AGENTS.md files nearest them (`tests/AGENTS.md`, `bindings/js/AGENTS.md`).
  5. All six suites are green (`scripts/test-all.bat`). The resulting Lua gtest count (`--gtest_filter=Lua*`: 428 at `bdf9087` across 11 suites, plus the new pins) and the C API count (27) are recorded as the baseline that Phases 2 and 3 must reproduce exactly.

**Plans**: 2/2 plans executed

Plans:

- [x] 01-01-PLAN.md — C++ behaviour pins: lifecycle move tests, check-order pins, write_csv cap and closed-writer pins
- [x] 01-02-PLAN.md — JS sync-test guards, AGENTS.md updates and the phase gate (baseline in STATE.md)

### Phase 2: Mechanical Split

**Goal**: `src/lua_runner.cpp` is replaced by `src/lua_runner/`. Each file there registers and implements its own slice of the Lua surface, so a later change touches one small file, and no observable behaviour changes.
**Depends on**: Phase 1
**Requirements**: SPLIT-01, SPLIT-02, SPLIT-03, SPLIT-04, SPLIT-05, SPLIT-06, SPLIT-07
**Success Criteria** (what must be TRUE):

  1. `src/lua_runner.cpp` no longer exists. `src/lua_runner/` holds `lua_runner.cpp`, `internal.h`, `return_json.cpp`, `path_policy.cpp`, `db_core.cpp`, `db_read.cpp`, `db_write.cpp`, `db_metadata.cpp`, `db_time_series.cpp`, `csv.cpp` and `binary.cpp`, each listed explicitly in the `quiver` target's sources with no GLOB. `wc -l` shows no file over about 450 lines.
  2. `new_usertype<Database>` appears exactly once across `src/lua_runner/`, and so does `open_libraries(`. The ctor runs `open_libraries`, then nils `dofile`/`loadfile`, creates the `quiver` table, calls the binders (`bind`/`ns`), and sets `lua["db"] = &db` last. The 17 variadic name/function pairs on the `Database` usertype become `bind.set_function` calls here, because per-domain binders cannot share one variadic call (behaviour-neutral per the sol2 source). The four non-`Database` usertypes stay variadic, one name per line. `RunHandles` is declared before `lua` in the heap-allocated `Impl`. No `[this]` capture remains. `GcGuard` is still declared before `result` and closes writers before exactly one `collect_garbage()`.
  3. The JS sync test reads every file under `src/lua_runner/` in sorted order and resets `current` at each file boundary. It extracts exactly the same method set as before the split (the before/after set diff is empty), and deleting one `bind.set_function` line makes it fail.
  4. No test expectation changes. The Lua gtest count equals the Phase 1 baseline, the 27 C API tests pass, and the Julia, Dart, Python and JS suites pass unmodified (Dart after deleting `bindings/dart/.dart_tool/hooks_runner/` and `.dart_tool/lib/`). `/bigobj` (MSVC) and `-Wa,-mbig-obj` (MinGW) are set target-wide. All `SOL_*` defines stay PRIVATE on `quiver`. clang-format 22.1.8 is clean on `src/lua_runner/`, and `scripts/tidy.bat` reports no warning there beyond the 15 pre-existing ones recorded as the baseline (none of them `performance-unnecessary-value-param`). Each TU whose functions take sol2 arguments by value has its own NOLINT pair, with the check name corrected to `performance-unnecessary-value-param`. The PR notes the clean-build time of the `quiver` target before and after the split (recorded, not gated).
  5. The old monolith path is gone: `git grep -n 'src/lua_runner\.cpp'` returns nothing outside `.planning/` and `CHANGELOG.md`, and every `git grep -n 'lua_runner\.cpp'` hit left there names a file under `src/lua_runner/`, the C API translation unit (`src/c/lua_runner.cpp`, the `c/lua_runner.cpp` entry in `src/CMakeLists.txt`, the `src/c/AGENTS.md` file listing) or `test_c_api_lua_runner.cpp`. The C API translation unit and its test keep their names; this milestone renames neither. The citations of the core file (including bare `lua_runner.cpp` meaning it) in the AGENTS.md files, `src/csv/*`, `cmake/Platform.cmake`, `bindings/dart/hook/build.dart`, the test comments and the `lua-api.ts` maintainer header point to the new paths. The root and `src/` AGENTS.md describe the folder layout, and the code moved into `src/lua_runner/` carries no planning-ID comments (59 lines in `src/lua_runner.cpp` at `bab557e`).

**Plans**: 4/4 plans executed

Plans:
**Wave 1**

- [x] 02-01-PLAN.md — Tracer: git mv into src/lua_runner/, CMake path + target-wide /bigobj, folder-reading sync test; comment-only planning-ID strip + NOLINT name fix

**Wave 2** *(blocked on Wave 1 completion)*

- [x] 02-02-PLAN.md — De-class Impl in place (RunHandles, seven binders, one Database usertype, 17 pairs to bind.set_function); extract internal.h, return_json.cpp, path_policy.cpp

**Wave 3** *(blocked on Wave 2 completion)*

- [x] 02-03-PLAN.md — Extract the per-domain binders: db_metadata, db_read, db_write, db_time_series, db_core, csv, binary

**Wave 4** *(blocked on Wave 3 completion)*

- [x] 02-04-PLAN.md — Re-point every citation, describe the layout in AGENTS.md, and the phase gate (Release, six suites, tidy, format, mutation, build time)

### Phase 3: Dedupe

**Goal**: Every repeated pattern in `src/lua_runner/` exists exactly once, so each Phase 4 fix lands in a single place, and no observable behaviour changes.
**Depends on**: Phase 2
**Requirements**: DEDUP-01, DEDUP-02, DEDUP-03, DEDUP-04, DEDUP-05, DEDUP-06
**Success Criteria** (what must be TRUE):

  1. `db:transaction` and `db:dry_run` share one helper, and the plain forwarders are member pointers. No variadic name/function pair has come back on the `Database` usertype since Phase 2 converted them, so every `db:` method is still registered with `bind.set_function`.
  2. One implementation remains for each of these:
     - the bulk-read adapters (two templates);
     - `std::optional` returns from `query_*` and the by-id composites;
     - a single `read_vectors_by_id`/`read_sets_by_id` template;
     - the metadata wrappers;
     - the option decoders (nil handling stays per caller);
     - the CSV cell conversion, through `lua_to_value`;
     - `CsvWriter` behaviour, now members;
     - the `read_csv`/`read_csv_stream` header rule;
     - the binary operators, as functors replacing `BinOp`/`apply_binop`;
     - `build_metadata_from_lua`'s option slots, now named;
     - each message that used to be duplicated.

     Expired registry entries are pruned on insert, and the close-at-exit function's name covers binary files.

  3. Each dedupe PR states "reordered checks: none". The six suites pass with zero test-expectation changes, and the Lua gtest count equals the Phase 1 baseline. The one known text change, the Debug-only dot-call error from the move to member pointers, is noted in the PR.
  4. The JS sync test still extracts the same method set, including the `CsvWriter` member-pointer lines near the 120-column limit. No `src/lua_runner/` file is over about 450 lines. csv-parser headers are not included from `src/lua_runner/`. clang-format is clean, and `scripts/tidy.bat` adds no warning beyond the Phase 2 baseline.
  5. `src/AGENTS.md` describes the shared helpers where the per-method boilerplate used to be documented.

**Plans**: TBD

### Phase 4: Fixes and Release Type Safety

**Goal**: In Release builds, a wrong-type argument from an untrusted script raises a Pattern 1 error instead of undefined behaviour. The C4/C6/C7/C8 bugs are fixed, `load` is text-only, and each fix lands red-then-green with its own test and CHANGELOG line.
**Depends on**: Phase 3
**Requirements**: SAFE-01, SAFE-02, SAFE-03, SAFE-04, SAFE-05, SAFE-06, SAFE-07, FIX-01, FIX-02, FIX-03
**Success Criteria** (what must be TRUE):

  1. These cases now raise a Pattern 1 error that names the operation and argument and ends in "got <lua type>":
     - a number, string or userdata passed to any table parameter, including the `table_to_element` cells and `collect_group_columns`;
     - a non-string key at the four map-key sites;
     - a wrong-typed value at the 8 optional-argument sites.

     The tests for these cases are new (red before the fix, green after), cover every site, and pass in Debug and in the CI Release matrix.

  2. `db:transaction` and `db:dry_run` reject a non-function argument with Pattern 1 before any side effect, so `db:in_transaction()` stays false. A COMMIT failure inside `db:transaction` rolls back and rethrows, and an `end_dry_run` failure inside `db:dry_run` still surfaces. `load` of a bytecode chunk raises an error, while string-form `load` still works.
  3. `update_element` with `{col = {}}` clears that group, and a misspelled empty column throws "does not match any vector, set, or time series table". `create_element` still skips an empty array. The `lua-api.ts` empty-array text says this, and the sync test is green. Expression-helper errors name the public operation, and the D1 nil branch, `lua_data_type_name`'s `default:` and `apply_binop`'s throw are gone.
  4. The phase's last commit sets `SOL_ALL_SAFETIES_ON=1` and `SOL_PRINT_ERRORS=0` PRIVATE on `quiver`, and deletes the no-op `SOL_SAFE_FUNCTION=1` define and its AGENTS.md claim (decision recorded in PROJECT.md). A test shows that a caught script error writes nothing to stderr. The Release cost of the flag is measured once by hand, before and after, on a bulk read and on `file:read`, and reported in the PR; no perf script is committed. The budget is 5%. If it is exceeded, add `SOL_SAFE_GETTER=0` and `SOL_SAFE_STACK_CHECK=0` and re-measure; `SOL_SAFE_FUNCTION_CALLS` and `SOL_SAFE_USERTYPE` are never disabled.
  5. `CHANGELOG.md` has `## [0.13.0] — unreleased` and its compare link in the existing link block (`[0.13.0]: https://github.com/psrenergy/quiver/compare/v0.12.9...v0.13.0`). BREAKING entries, each saying what a script author must change, cover wrong-type arguments now throwing (C1/C5), the empty-array change (C7) and text-only `load`. Entries also record that a Release dot-call (`db.method()`) goes from undefined behaviour to an error, with the backstop's raw sol2 text (`sol: received nil for 'self' argument…`). `### Fixed` entries cover C2, C4, C6 and C8. Entries describe behaviour and carry no planning IDs, and all manifests stay at 0.13.0. The AGENTS.md nearest each change is updated, including the root sandbox decision, which now says `load` accepts text chunks only. All six suites are green (Dart, the only local Release run, after deleting `bindings/dart/.dart_tool/hooks_runner/` and `.dart_tool/lib/`), and the resulting Lua gtest count (`--gtest_filter=Lua*`) and C API count are recorded as the baseline that Phase 5's runs must reproduce.

**Plans**: TBD

### Phase 5: Path-Policy Test and Docs

**Goal**: The path-containment gate `resolve_sandboxed_path` has its own unit test, no planning-ID comment remains in the repo, and the docs, the shipped Lua reference and the `[0.13.0]` CHANGELOG fully describe the finished milestone.
**Depends on**: Phase 4
**Requirements**: TEST-01, DOC-01, DOC-02, DOC-03, DOC-04
**Success Criteria** (what must be TRUE):

  1. New `SandboxedPathTest` unit tests call `resolve_sandboxed_path` through a sol2-free header and cover containment, escape rejection, the root itself, `:memory:` and the device-name prefix. Tests do not include `src/` internals today, and `quiver` is a shared library with hidden visibility (`cmake/Platform.cmake:40-42`), so the plan picks how they link: a header-inline definition, an exported symbol, or compiling `path_policy.cpp` into `quiver_tests`. The new suite sits outside the `Lua*` filter, so `quiver_tests --gtest_filter=Lua*` still matches exactly the Lua-layer count recorded at the end of Phase 4 (428 at `bdf9087`, plus the Phase 1 pins and the Phase 4 tests), and the C API suite runs the count recorded at the end of Phase 4 (27 at `bdf9087`).
  2. A repo-wide `git grep` for planning-ID comments outside `.planning/` and `CHANGELOG.md` returns 0. It covers the legacy IDs (`D-xx`, `LUA-xx`, `WRITE-xx`, `FMT-xx`, `TEST-xx`, references such as `NN-NN-PLAN.md`; about 224 lines in 13 files at `0a32506`) and this milestone's own (`C1`-`C8`, `M1`-`M16`, and the `PIN-NN`, `SPLIT-NN`, `DEDUP-NN`, `SAFE-NN`, `FIX-NN`, `DOC-NN` and `TEST-NN` requirement IDs).
  3. The root, `src/`, `src/c/`, `tests/` and four binding AGENTS.md files describe the `src/lua_runner/` layout, the C7 rule, text-only `load` and the safety flags.
  4. CHANGELOG `[0.13.0] — unreleased` is complete: it holds Phase 4's BREAKING and `### Fixed` entries and the compare link, has no rename entry (nothing is renamed), and no entry carries a planning ID. `LUA_DB_API_REFERENCE` states the empty-array rule and what the sandbox does not limit (instructions, memory, wall time, globals persisting across `run()`). The sync test and all six suites are green, and the version is still 0.13.0.

**Plans**: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1 → 2 → 3 → 4 → 5

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Behaviour Pins | 2/2 | Complete    | 2026-10-02 |
| 2. Mechanical Split | 4/4 | Complete    | 2026-10-03 |
| 3. Dedupe | 0/TBD | Not started | - |
| 4. Fixes and Release Type Safety | 0/TBD | Not started | - |
| 5. Path-Policy Test and Docs | 0/TBD | Not started | - |
