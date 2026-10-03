---
phase: 03-dedupe
plan: 03
subsystem: lua-binding
tags: [sol2, cpp20, lua_runner, refactor, functors, raii]

requires:
  - phase: 03-dedupe
    provides: "03-01/03-02 shared helpers and the build/dedupe-check/ golden harness and gates"
provides:
  - "RunHandles::add_writer / add_binary_file: the only way into the run-handle registries (prune expired, then append)"
  - "RunHandles::close_open_handles (renamed from the writers-only name; closes CSV writers and binary files)"
  - "template <typename Op> Expression binop(const sol::object&, const sol::object&) with transparent functors behind all twelve binary Expression operators"
  - "src/AGENTS.md **Shared helpers** bullet naming every helper of the phase"
affects: [04-fixes]

actuals:
  tokens: 5469
  tasks: 3
  commits: 3

tech-stack:
  added: []
  patterns:
    - "binary operators bound as function pointers to one functor-parameterised template: &binop<std::greater_equal<>>"
    - "registry insert = std::erase_if(expired) + append, behind one RunHandles member per registry"

key-files:
  created: []
  modified:
    - src/lua_runner/internal.h
    - src/lua_runner/lua_runner.cpp
    - src/lua_runner/csv.cpp
    - src/lua_runner/binary.cpp
    - src/AGENTS.md

key-decisions:
  - "Registries prune only expired() entries on insert; a closed-but-alive writer stays, and erase_if keeps survivor order so the exit close order is unchanged"
  - "The two close loops in close_open_handles stay separate (different element types and close calls)"
  - "gt/lt/gte/lte/eq/neq stay six literal ns.set_function calls (the sync test reads one literal call per name)"

patterns-established:
  - "Mutation check per item, run after the commit and reverted with git checkout"

requirements-completed: [DEDUP-02, DEDUP-04, DEDUP-05, DEDUP-06]

duration: 22min
completed: 2026-10-03
status: complete
---

# Phase 3 Plan 03: Registry, Operators and Docs Summary

**Both run-handle registries now take inserts only through `RunHandles::add_writer` / `add_binary_file`, which prune expired entries first. The close-at-exit function is `close_open_handles`. All twelve binary Expression operators go through one `binop<Op>` template with transparent functors. `src/AGENTS.md` names every shared helper of the phase. The phase gate is green in Debug, Release, all six suites, and Linux GCC 13 and Clang 18/libc++.**

## Performance

- **Duration:** 22 min
- **Started:** 2026-10-03T06:54:30Z
- **Completed:** 2026-10-03T07:16:00Z
- **Tasks:** 3 (a tracer, an auto task, and the docs plus phase-gate task)
- **Files modified:** 5

## Gate evidence

| Commit | Item | Gate line |
|--------|------|-----------|
| `dd3a11c` | Registry prune on insert, `close_open_handles` (M13) | `GATE PASS pairs=36` (debug text unchanged, `GOLDEN debug OK`) |
| `9c917d1` | `binop<Op>` functors, `bind_binary(sol::state& state, ...)` (M14 + IN-02) | `GATE PASS pairs=36` (debug text unchanged) |
| `0c1ae56` | `src/AGENTS.md` **Shared helpers** bullet | `GATE PASS pairs=36` (debug text unchanged) |

Each gate run covered the same checks as in 03-01/03-02:
- Debug `Lua*`: 444/12, all pass. C API: 27, all pass. Sync test: 6 pass.
- The surface equals the base. `.set_function(` count is 86. Every registered name equals its member name.
- clang-format 22.1.8 is clean. Every file is at most 450 lines. No planning ID appears under `src/lua_runner`.
- The Debug golden output matches the base.

Logs: `build/dedupe-check/m13_gate.txt`, `m14_gate.txt`, `docs_gate.txt`.

### Task-level verify checks

- **Task 1:**
  - `close_open_writers` is absent repo-wide outside `.planning/`.
  - Each of these appears exactly once: `handles.add_writer(resolved, writer);` in csv.cpp, `handles.add_binary_file(file);` in binary.cpp, and `impl.handles.close_open_handles();` in lua_runner.cpp.
  - `std::erase_if(` appears 2 times. Each of the two `expired()` predicates appears once.
  - The declaration grep in internal.h gives 3, and the definition grep in lua_runner.cpp gives 3.
  - `open_writers.emplace_back|open_binary_files.push_back` appears 0 times in csv.cpp and 0 in binary.cpp.
  - Line order in write_csv: 395 `path_has_open_writer(` < 399 `make_shared<...Writer>` < 402 `handles.add_writer(` < 403 `make_unique<CsvWriter>`.
  - `close_open_handles` appears 3 times in `src/AGENTS.md`.
- **Task 2:**
  - `BinOp|apply_binop|binop_dispatch` appears 0 times. `&binop<std::` appears 12 times.
  - Each of the six `ns.set_function("<name>", &binop<std::...<>>);` lines appears once. Each of the six metamethod `= &binop<std::...<>>` lines appears once.
  - `meta_function::(unary_minus|bitwise_not)\] = \[\]` appears 2 times.
  - `void bind_binary(sol::state& state` appears once in binary.cpp and once in internal.h. `state.new_usertype<` appears 3 times.
  - `sol::state& lua` appears nowhere under `src/lua_runner`. `#include <functional>` appears once.
- **Task 3:**
  - The last commit's files are only `src/AGENTS.md`. `**Shared helpers**` appears once.
  - All 19 helper names in the verify loop are present. The planning-ID count is 15.

## Mutation results

Each mutation was run after its commit, then reverted with `git checkout -- <file>`. `git diff --exit-code -- src/lua_runner` was clean after every revert.

| Item | Mutation | Result |
|------|----------|--------|
| M13 (a) | `add_writer`'s predicate erases every entry (`return true;`) | `Lua*`: 444/444 pass, so **no test sees it**. `golden.sh debug` exits nonzero on `csv_lifecycle`: `p_again` changes from refused (`Cannot write_csv: file is already open for writing: p.csv`) to accepted (userdata), and `p_after_close` changes from accepted to refused. |
| M13 (b) | Remove the append from `add_writer` | Both `LuaRunner_WriteCsv.UnclosedWriterHeldInAGlobalIsAlsoFlushedWhenRunReturns` and `LuaRunner_WriteCsvErrors.SecondWriterOnAnAlreadyOpenPathIsRefused` fail (0/2 pass) |
| M13 (c) | Remove the append from `add_binary_file` | Both `LuaBinaryTest.WriterHeldInAGlobalIsClosedWhenRunReturns` and `LuaBinaryTest.HandleFromAnEarlierRunIsClosed` fail (0/2 pass) |
| M14 | Register `gte` as `&binop<std::greater<>>` | `Lua*`: 444/444 pass, so **no test covers gte**. `golden.sh debug` fails on `operators`: `gte_fn` and `gte_nf` cell r1c2 changes from `float:1` to `float:0`. |

No test and no in-run probe can see over-pruning of the binary list. The process-wide binary write registry lives in `BinaryFile`, and the runner's own list matters only at `run()`'s exit. That predicate is `[](const auto& weak) { return weak.expired(); }`, verified **by inspection** and by grep.

## Tracer gate (delegated self-verification)

`auto_advance` is false, so the tracer's human-verify checkpoint applied. Following the user's standing preference (memory: self-verify-checkpoints), I verified it adversarially myself:
- `GATE PASS pairs=36`, with the debug text unchanged.
- All the grep checks above pass.
- Mutations (a), (b) and (c) were each caught, (a) by the golden harness only.

Verdict: pass (delegated), so execution continued.

## Phase gate

### 1. Gates

- `bash build/dedupe-check/gate.sh` at `0c1ae56`: `GATE PASS pairs=36`.
- `bash build/dedupe-check/wave_gate.sh` at `0c1ae56`: `WAVE GATE PASS tidy=14` (log `build/dedupe-check/wave_0303.txt`).
  - Release `Lua*`: 444/12 pass. Release C API: 27 pass.
  - `GOLDEN release OK`.
  - The GCC 14 `-fsyntax-only -Wall -Wextra` pass is empty.
  - `performance-unnecessary-value-param`: 0. `clang-diagnostic-error`: 0.

| File | Check | Phase base | 03-02 end | Now |
|------|-------|-----------|-----------|-----|
| binary.cpp | modernize-return-braced-init-list | 1 | 1 | 1 |
| binary.cpp | modernize-raw-string-literal | 1 | 1 | 1 |
| binary.cpp | bugprone-unchecked-optional-access | 3 | 3 | 3 |
| csv.cpp | readability-identifier-naming (`header_width_`, `kMaxWidth`) | 2 | 2 | 2 |
| db_core.cpp | bugprone-empty-catch | 2 | 1 | 1 |
| lua_runner.cpp | bugprone-empty-catch (the two close loops) | 2 | 2 | 2 |
| return_json.cpp | readability-identifier-naming | 3 | 3 | 3 |
| return_json.cpp | bugprone-implicit-widening-of-multiplication-result | 1 | 1 | 1 |
| **Total** | | **15** | **14** | **14** |

### 2. Six suites

I deleted `bindings/dart/.dart_tool/hooks_runner` and `bindings/dart/.dart_tool/lib` before the run. A listing taken right after the delete showed neither directory, and the run recreated both. Then I ran `cmd //c 'scripts\test-all.bat'` (output in `build/dedupe-check/test-all.txt`, exit 0):

```
  C++ tests:        PASS
  C API tests:      PASS
  Julia tests:      PASS
  Dart tests:       PASS
  JavaScript tests: PASS
  Python tests:     PASS

All tests PASSED
```

`grep -Ec 'tests: +PASS'` prints 6.

### 3. Linux

I built `git archive HEAD` at `0c1ae56` from scratch in two `ubuntu:24.04` containers: Debug, `-G Ninja`, tests and C API on. The script is `build/dedupe-check/linux.sh`, and the logs are `linux_gcc.txt` and `linux_clang.txt`.

| Toolchain | Build | `Lua*` | `LuaRunnerCApiTest` |
|-----------|-------|--------|---------------------|
| GCC 13.3.0 (`c++ (Ubuntu 13.3.0-6ubuntu2~24.04.1)`) | 0 errors; the only warnings are in the untouched `src/binary/time_properties.cpp` (`-Wreturn-type`, as in Phase 2) | `LISTED=442`, `[==========] 442 tests from 12 test suites ran.`, `[  PASSED  ] 441 tests.`, `[  SKIPPED ] 1 test.` | `[==========] 27 tests from 1 test suite ran.`, `[  PASSED  ] 27 tests.` |
| Clang 18.1.3 `-stdlib=libc++` (`CMAKE_CXX_FLAGS:STRING=-stdlib=libc++`) | 0 errors; the only warning is in the untouched `tests/test_migrations.cpp` (`-Wself-assign-overloaded`, as in Phase 2) | `LISTED=442`, `[==========] 442 tests from 12 test suites ran.`, `[  PASSED  ] 441 tests.`, `[  SKIPPED ] 1 test.` | `[==========] 27 tests from 1 test suite ran.`, `[  PASSED  ] 27 tests.` |

These match the Phase 2 baseline. The two `DeviceNamePathIsReportedWithPrefix` tests are `_WIN32`-only, and the chmod-000 test self-skips as root.

### 4. Static checks against `BASE=3c13209`

- `git diff --stat $BASE HEAD -- tests bindings` prints nothing.
- `git diff --quiet $BASE HEAD -- CHANGELOG.md CMakeLists.txt` plus the four binding manifests, `src/lua_runner/path_policy.cpp` and `src/lua_runner/return_json.cpp` exits 0.
- Planning-ID regex counts: 0 under `src/lua_runner`, 15 in `src/AGENTS.md` (15 at base) and 10 in `src/csv/csv_write.cpp` (10 at base).
- Every file has at most 450 lines (table in the PR notes below).

### 5. Duplicate-message scan

Command: `cat src/lua_runner/*.cpp src/lua_runner/*.h | grep -oE '"[^"]{12,}"' | sort | uniq -cd`

None of the merged messages appears as a repeated literal. Single-site counts under `src/lua_runner`:

| Message | Count |
|---------|-------|
| `contain no rows` | 1 |
| `has length` | 1 |
| `: options must be a table` | 1 |
| `' must be a table` | 1 |
| `option 'header_row' must be an integer` | 1 |

`unknown data type` has 3 sites, and the base had 4 (db_read.cpp 3 → 2):
- `lua_data_type_name`, kept for Phase 4.
- `read_scalars_by_id`.
- The single `read_groups_by_id` site shared by both by-id composites.

Every remaining repeated literal is listed below. The `Ph4` column flags the literals a Phase 4 message change would have to edit twice.

| Count | Literal | Reason | Ph4 |
|-------|---------|--------|-----|
| 12 | `" + caller + "` | Concatenation fragment of the `Cannot <caller>: ...` pattern, not a message | |
| 19 | `" + operation + "` | Same fragment, for the `operation` parameter | |
| 2 | `" has unsupported Lua type"` | `lua_cell_as<T>` (internal.h:130) and `lua_to_value` (internal.h:154): two converters with one message shape by design | **yes**: a wording change edits both throw sites |
| 2 | `"' must be an array of values"` | `collect_group_columns` (db_write.cpp:109, 114): a non-table column and a table with a non-integer key | **yes**: two throw sites in one function |
| 2 | `"option 'header'"` | A csv.cpp comment plus the one `csv_max_integer_key` call | |
| 3 | `"dimension_sizes"`, `"initial_datetime"`, `"time_dimensions"` | `build_metadata_from_lua`: the `option_entries` key list, then `el.set(key, ...)` and the field label passed to `metadata_array` / `metadata_string` | |
| 3 | `"aggregate_agents"`, `"rename_agents"` | Usertype method name, operation name passed to its decoder, and one comment | |
| 5 | `"read_csv_stream"` | Registration name, operation name for sandbox/options/reader, and one comment | |
| 2 | `"metadata_from_element"`, `"select_agents"`, `"validate_migrations"`, `"update_time_series_files"`, `"read_vectors_by_id"`, `"read_sets_by_id"`, `"query_string"`, `"query_integer"`, `"create_element"`, `"update_element"`, `"update_element_by_label"`, `"update_relation"`, `"update_relation_by_label"`, `"update_vector_group"`, `"update_vector_group_by_label"`, `"update_set_group"`, `"update_set_group_by_label"`, `"update_time_series_group"`, `"update_time_series_group_by_label"`, `"upsert_time_series_row"`, `"upsert_time_series_row_by_label"` | Registration name plus the same operation name passed to a decoder (Pattern 1 `Cannot <op>`) | |
| 2-10 | `"lua_runner/internal.h"`, `"quiver/database.h"`, `"quiver/element.h"`, `"quiver/value.h"`, `"quiver/binary/binary_file.h"`, `"csv/csv_write.h"`, `"utils/number.h"` | `#include` lines across translation units | |

**Note for the Phase 4 planner:** the two-site literals above are the shared unsupported-type suffix of `lua_cell_as` and `lua_to_value`, and the column array message in `collect_group_columns`. A message change to either must edit both of its sites.

## PR notes

reordered checks: none

There are three check-order ledgers: [03-01](03-01-SUMMARY.md#check-order-ledger), [03-02](03-02-SUMMARY.md#check-order-ledger), and this plan.
- **This plan's ledger:**
  - `db:write_csv` still runs: sandbox, then options, then `path_has_open_writer`, then Writer construction, then registration, then `CsvWriter`.
  - `db:open_file` still runs: mode, then sandbox, then `BinaryFile::open_file`, then registration.
  - `binop<Op>` tests operands in the same order as the old dispatcher: number on the left only, then number on the right only, then both through `to_expression` (number∘number throws there with today's text).
  - Prune-on-insert happens after every check, at the old append point.

### Debug-only text changes (Release prints none of them)

Every probe shares the `[string "-- schema: collections.sql..."]:<line>: ` prefix before and after, which is left out of the table. In the 03-01 rows, `std::string` abbreviates `std::basic_string<char,std::char_traits<char>,std::allocator<char> >`. **No test pins any of these texts.** This plan changed none: all three of its gates ran without `DEBUG_TEXT_CHANGE`, and the debug text matched.

| Plan | Probe | Base text | New text |
|------|-------|-----------|----------|
| 03-01 | `dot_describe` | `stack index 1, expected userdata, received no value: value is not a valid userdata (bad argument into 'std::string(quiver::Database&)')` | `sol: received nil for 'self' argument (use ':' for accessing member functions, make sure member variables are preceeded by the actual object with '.' syntax)` |
| 03-01 | `dot_is_healthy` | `stack index 1, expected userdata, received no value: value is not a valid userdata (bad argument into 'bool(quiver::Database&)')` | `sol: received nil for 'self' argument (use ':' for accessing member functions, make sure member variables are preceeded by the actual object with '.' syntax)` |
| 03-01 | `dot_delete_element` | `stack index 1, expected userdata, received string: value is not a valid userdata (bad argument into 'void(quiver::Database&, const std::string&, __int64)')` | `sol: received nil for 'self' argument (use ':' for accessing member functions, make sure member variables are preceeded by the actual object with '.' syntax)` |
| 03-01 | `badarg_delete_element` | `stack index 3, expected number, received string: not a numeric type that fits exactly an integer (number maybe has significant decimals) (bad argument into 'void(quiver::Database&, const std::string&, __int64)')` | `stack index 3, expected number, received string: not a numeric type that fits exactly an integer (number maybe has significant decimals) (bad argument into 'void(const std::string&, __int64)')` |
| 03-01 | `badarg_number_of_elements` | `stack index 2, expected string, received table: (bad argument into '__int64(quiver::Database&, const std::string&)')` | `stack index 2, expected string, received table: (bad argument into '__int64(const std::string&)')` |
| 03-01 | `badarg_describe_collection` | `stack index 2, expected string, received table: (bad argument into 'std::string(quiver::Database&, const std::string&)')` | `stack index 2, expected string, received table: (bad argument into 'std::string(const std::string&)')` |
| 03-01 | `badarg_has_time_series_files` | `stack index 2, expected string, received table: (bad argument into 'bool(quiver::Database&, const std::string&)')` | `stack index 2, expected string, received table: (bad argument into 'bool(const std::string&)')` |
| 03-01 | `badarg_query_string` | `stack index 2, expected string, received table: (bad argument into 'sol::basic_object<sol::basic_reference<0> >(quiver::Database&, const std::string&, sol::optional<sol::basic_table_core<0,sol::basic_reference<0> > >, sol::this_state)')` | `stack index 2, expected string, received table: (bad argument into 'std::optional<std::string >(quiver::Database&, const std::string&, sol::optional<sol::basic_table_core<0,sol::basic_reference<0> > >)')` |
| 03-01 | `badarg_query_integer` | `stack index 2, expected string, received table: (bad argument into 'sol::basic_object<sol::basic_reference<0> >(quiver::Database&, const std::string&, sol::optional<sol::basic_table_core<0,sol::basic_reference<0> > >, sol::this_state)')` | `stack index 2, expected string, received table: (bad argument into 'std::optional<__int64>(quiver::Database&, const std::string&, sol::optional<sol::basic_table_core<0,sol::basic_reference<0> > >)')` |
| 03-01 | `badarg_query_float` | `stack index 2, expected string, received table: (bad argument into 'sol::basic_object<sol::basic_reference<0> >(quiver::Database&, const std::string&, sol::optional<sol::basic_table_core<0,sol::basic_reference<0> > >, sol::this_state)')` | `stack index 2, expected string, received table: (bad argument into 'std::optional<double>(quiver::Database&, const std::string&, sol::optional<sol::basic_table_core<0,sol::basic_reference<0> > >)')` |
| 03-02 | `dot_writer_write_row` | `stack index 1, expected userdata, received table: value is not a valid userdata (bad argument into 'void(quiver::lua_internal::CsvWriter&, const sol::basic_object<sol::basic_reference<0> >&)')` | `sol: received nil for 'self' argument (use ':' for accessing member functions, make sure member variables are preceeded by the actual object with '.' syntax)` |
| 03-02 | `dot_writer_close` | `stack index 1, expected userdata, received no value: value is not a valid userdata (bad argument into 'void(quiver::lua_internal::CsvWriter&)')` | `sol: received nil for 'self' argument (use ':' for accessing member functions, make sure member variables are preceeded by the actual object with '.' syntax)` |

### Commits (one per item, so the series bisects)

| SHA | Commit |
|-----|--------|
| `e46e681` | refactor(03-01): bind the 17 plain forwarders as Database member pointers |
| `f248e16` | refactor(03-01): run db:transaction and db:dry_run through one run_in_scope helper |
| `8ab8f4b` | refactor(03-01): route the 11 bulk readers through two adapter templates |
| `ec864cc` | refactor(03-01): return std::optional from query_* and read_scalars_by_id cells |
| `31a0d0f` | refactor(03-01): read vectors and sets by id through one read_groups_by_id template |
| `997c1b3` | refactor(03-01): fold the metadata wrappers into metadata_to_lua and two templates |
| `31b5a07` | refactor(03-01): give each group-decoder message a single throw site |
| `fe45e17` | refactor(03-02): one option walk with named slots for every strict decoder |
| `dd2bf9c` | refactor(03-02): convert CSV cells through lua_to_value |
| `e10068a` | refactor(03-02): make CsvWriter's write_row and close members |
| `4df3e92` | refactor(03-02): build the read_csv and read_csv_stream header in one place |
| `dd3a11c` | refactor(03-03): prune expired run handles on insert and rename the close to close_open_handles |
| `9c917d1` | refactor(03-03): bind every binary Expression operator through binop<Op> with transparent functors |
| `0c1ae56` | docs(03-03): describe the shared Lua binding helpers |

Each was preceded by `GATE PASS`. The `.planning/` docs commits are interleaved and touch no source.

### Tidy and line counts

Tidy went from 15 to 14. The warning removed is one `bugprone-empty-catch` in db_core.cpp: the two scoped-block catches became the one in `run_in_scope`.

| File | Base | Now |
|------|------|-----|
| binary.cpp | 377 | 336 |
| csv.cpp | 446 | 420 |
| db_core.cpp | 231 | 181 |
| db_metadata.cpp | 164 | 98 |
| db_read.cpp | 236 | 129 |
| db_time_series.cpp | 289 | 286 |
| db_write.cpp | 311 | 307 |
| internal.h | 225 | 258 |
| lua_runner.cpp | 144 | 162 |
| path_policy.cpp | 63 | 63 |
| return_json.cpp | 225 | 225 |
| **Total** | **2711** | **2465** |

### Coverage only the golden harness gives

No test would catch a regression in these:
- `quiver.gte` / `lt` / `eq` / `neq`. The gte→greater mutation passes all 444 `Lua*` tests.
- A number on the left of every operator except `*`.
- The `quiver.metadata` slot mapping for a swap of two same-typed, compatible slots. Incompatible swaps are caught by `BinaryMetadata` validation (03-02).
- The full option-key error message.
- Over-pruning the CSV writer registry. The erase-everything mutation passes all `Lua*` tests and fails golden `csv_lifecycle`.

Covered **by inspection only**: over-pruning the binary-file registry. Neither tests nor probes can observe it. The predicate is `weak.expired()`.

### Phase 4 notes

- The old operator switch's unreachable trailing `throw` ("Cannot apply operator: unknown operation") is **gone**: the enum, switch and dispatcher were deleted. Phase 4's list of unreachable branches to remove no longer includes it.
- `lua_data_type_name`'s `default:` is untouched (03-01).
- The two two-site messages are flagged above (unsupported-type suffix and `must be an array of values`).

### Platforms

- Windows MSVC: Debug and Release.
- Linux: GCC 13.3 and Clang 18.1.3/libc++. Both list 442 `Lua*` tests (441 pass, 1 skip as root) and pass 27 `LuaRunnerCApiTest` tests.
- Apple Clang (macOS) is left to the PR's CI matrix. The phase adds no library call beyond `std::erase_if` (C++20 `<vector>`) and the `<functional>` transparent functors. Both are header-only and are not gated by libc++ availability macros.

### CHANGELOG

There is no CHANGELOG entry and no version bump. Release behaviour is unchanged: the Release golden output is byte-identical to the base, and the only text changes are Debug-only sol2 bad-argument messages that no test pins.

## Decisions Made

- `add_writer` takes `const std::shared_ptr<...>&`, not a by-value parameter. `write_csv` still moves `writer` into the `CsvWriter` afterwards, and a by-value `shared_ptr` would trip `performance-unnecessary-value-param`.
- The `src/AGENTS.md` run-exit paragraph gains one sentence on prune-on-insert. The **Shared helpers** bullet sits right after Layout and points to the existing forwarder bullet rather than repeating its list.

## Deviations from Plan

None. The plan executed as written.

**Total deviations:** 0.

## Issues Encountered

None. Every gate passed on its first run.

## Next Phase Readiness

- Phase 3 is complete. DEDUP-02, DEDUP-04, DEDUP-05 and DEDUP-06 have their acceptance checks passing and are marked complete. 03-01 marked DEDUP-01 and DEDUP-03.
- Phase 4 inherits the notes above.

## Self-Check: PASSED

- Commits `dd3a11c`, `9c917d1` and `0c1ae56` are present in `git log`. None deletes a file.
- All 5 modified files exist.
- The evidence logs exist under `build/dedupe-check/`: `m13_gate.txt`, `m14_gate.txt`, `docs_gate.txt`, `wave_0303.txt`, `test-all.txt`, `linux_gcc.txt`, `linux_clang.txt`, `tidy.txt`.
