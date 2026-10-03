---
phase: 03-dedupe
plan: 01
subsystem: lua-binding
tags: [sol2, cpp20, lua_runner, refactor, member-pointers, templates]

requires:
  - phase: 02-split
    provides: "src/lua_runner/ split into per-area TUs, bind.set_function everywhere, sync test hardened"
provides:
  - "17 plain forwarders bound as &Database:: member pointers"
  - "run_in_scope: one scoped-block helper behind db:transaction / db:dry_run"
  - "bulk_read_lua / collection_read_lua adapter templates in internal.h (11 bulk readers)"
  - "std::optional returns from query_*_lua and read_scalars_by_id cells"
  - "read_groups_by_id template behind read_vectors_by_id / read_sets_by_id"
  - "metadata_to_lua overloads + list_metadata_lua / get_metadata_lua templates"
  - "columns_to_cpp_rows owns the no-rows rejection; length_mismatch owns the length message"
  - "gitignored golden-output harness and per-commit / per-wave gates in build/dedupe-check/"
affects: [03-02, 03-03, 04-fixes]

actuals:
  tokens: 9508
  tasks: 3
  commits: 7

tech-stack:
  added: []
  patterns:
    - "template <auto MemberPtr> adapters registered as bind.set_function(\"x\", &adapter<&Database::x>)"
    - "registered name == member name, checked mechanically on the flattened source"

key-files:
  created:
    - build/dedupe-check/ (gitignored harness, not committed)
  modified:
    - src/lua_runner/internal.h
    - src/lua_runner/db_core.cpp
    - src/lua_runner/db_read.cpp
    - src/lua_runner/db_metadata.cpp
    - src/lua_runner/db_write.cpp
    - src/lua_runner/db_time_series.cpp
    - src/AGENTS.md

key-decisions:
  - "db:transaction / db:dry_run stay two lambdas over run_in_scope so their Debug bad-argument text is unchanged"
  - "query_*_lua stay three named functions (not a template) so Phase 4's optional_from_lua lands at exactly three sites"
  - "Debug-only sol2 text changes accepted for the 17 forwarders and the three query_* methods; Release unchanged"

patterns-established:
  - "Golden harness: Lua probe scripts through quiver_cli, Debug and Release, byte-diffed against base-commit output"
  - "Mutation check per item, run after the commit and reverted with git checkout"

requirements-completed: [DEDUP-01, DEDUP-03]

duration: 35min
completed: 2026-10-03
status: complete
---

# Phase 3 Plan 01: Registration and Adapter Dedupe Summary

**The `db_*.cpp` binders now bind 17 forwarders as `&Database::` member pointers and route 11 bulk readers and 8 metadata wrappers through `template <auto>` adapters. One `run_in_scope` helper, one by-id template, `std::optional` returns and single throw sites for the two group-decoder messages replace the duplicates. A golden harness shows Debug and Release output byte-identical to the base.**

## Performance

- **Duration:** 35 min
- **Started:** 2026-10-03T05:54:15Z
- **Completed:** 2026-10-03T06:29:30Z
- **Tasks:** 3 (tracer + 2 auto)
- **Files modified:** 7

## Base and gate evidence

Base SHA: `3c13209344228420ddef06bd2693ddd5a20dcc8f` (`build/dedupe-check/BASE`). The parent of the first refactor commit has the same `src tests bindings` as the base (`git diff --quiet BASE HEAD~1` at the M3 commit: equal).

At the base, before any edit:
- `GATE PASS pairs=0`
- `WAVE GATE PASS tidy=15`

| Commit | Item | Gate line |
|--------|------|-----------|
| `e46e681` | M3 forwarders (confirms M2) | `GATE PASS pairs=17` (run with `DEBUG_TEXT_CHANGE=1`: 7 forwarder probes changed, 0 `control_`) |
| `f248e16` | M1 `run_in_scope` | `GATE PASS pairs=17` |
| `8ab8f4b` | M4 bulk-read adapters | `GATE PASS pairs=28` |
| `ec864cc` | M5 `std::optional` + IN-01 | `GATE PASS pairs=28` (run with `DEBUG_TEXT_CHANGE=1`: 3 `badarg_query_*` probes changed, 0 `control_`) |
| `31a0d0f` | M7 `read_groups_by_id` | `GATE PASS pairs=28` |
| `997c1b3` | M8 metadata wrappers | `GATE PASS pairs=36` |
| `31b5a07` | M16 merged messages | `GATE PASS pairs=36` |

Final, at `31b5a07`: `WAVE GATE PASS tidy=14`. A re-run of `gate.sh` at the same HEAD printed `GATE PASS pairs=36`.

Each gate run checked the following:
- Debug `Lua*` lists 444 tests in 12 suites, and all pass.
- `LuaRunnerCApiTest` lists 27 tests, and all pass.
- The sync test passes: 6 pass, 0 fail.
- The surface equals the base: 71 `db` + 15 `quiver` names plus the four usertypes.
- `.set_function(` appears 86 times. `new_usertype<Database>` and `open_libraries(` each appear once.
- Every registered name equals its member name.
- clang-format 22.1.8 is clean.
- Every file is at most 450 lines.
- No csv-parser include and no planning ID appear under `src/lua_runner`.
- The Debug golden output is byte-identical to the base.

The wave gate added these checks:
- Release `Lua*` 444/12 and the C API 27 pass.
- The Release golden output is byte-identical to the base.
- The folder tidy reports 14 unique warnings, with 0 `performance-unnecessary-value-param` and 0 `clang-diagnostic-error`.
- The Docker `gcc:14` `-fsyntax-only -Wall -Wextra` pass over every `src/lua_runner/*.cpp` is clean, with empty output.

### Tidy warnings (wave gate) vs the Phase 2 table

| File | Check | Phase 2 | Now |
|------|-------|---------|-----|
| binary.cpp | modernize-return-braced-init-list | 1 | 1 |
| binary.cpp | modernize-raw-string-literal | 1 | 1 |
| binary.cpp | bugprone-unchecked-optional-access | 3 | 3 |
| csv.cpp | readability-identifier-naming | 2 | 2 |
| db_core.cpp | bugprone-empty-catch | 2 (lines 154, 174) | 1 (line 99, in `run_in_scope`) |
| lua_runner.cpp | bugprone-empty-catch | 2 | 2 |
| return_json.cpp | readability-identifier-naming | 3 | 3 |
| return_json.cpp | bugprone-implicit-widening-of-multiplication-result | 1 | 1 |
| **Total** | | **15** | **14** |

### Line counts (`wc -l src/lua_runner/*`)

| File | Lines |
|------|-------|
| binary.cpp | 377 |
| csv.cpp | 446 |
| db_core.cpp | 197 (was 231) |
| db_metadata.cpp | 98 (was 164) |
| db_read.cpp | 129 (was 236) |
| db_time_series.cpp | 286 (was 289) |
| db_write.cpp | 307 (was 311) |
| internal.h | 240 (was 225) |
| lua_runner.cpp | 144 |
| path_policy.cpp | 63 |
| return_json.cpp | 225 |

## Accomplishments

- **M3 (DEDUP-02, db part):** 17 forwarders are registered as `bind.set_function("<name>", &Database::<name>)`.
  - `db_core.cpp`: 14 of them. `db_write.cpp`: `delete_element` and `delete_element_by_label`. `db_time_series.cpp`: `has_time_series_files`.
  - M2 held: the only Database usertype call is the single-argument one in `lua_runner.cpp`.
  - `src/AGENTS.md`'s forwarder bullet now names the member-pointer group. Its planning-ID count is still 15.
- **M1 (DEDUP-01):** `run_in_scope(self, fn, begin, finish, abort)` sits in `db_core.cpp`'s anonymous namespace. Both registrations keep the exact `[](Database& self, sol::protected_function fn) -> sol::object` lambda, so neither registers a template instantiation directly.
- **M4 (DEDUP-03):** `bulk_read_lua<auto Read>` and `collection_read_lua<auto Read>` live in `internal.h`.
  - They replace the ten `read_*_lua` functions and `list_time_series_files_columns_lua`.
  - The empty "Bulk set reads" banner is gone.
- **M5 + IN-01 (DEDUP-03):**
  - `query_string/integer/float_lua` return `std::optional<std::string|int64_t|double>` and no longer take `sol::this_state`.
  - `read_scalars_by_id` assigns each optional straight into the table, so a NULL key stays absent.
  - The single NOLINTBEGIN now sits directly above `query_string_lua`.
- **M7 (DEDUP-03):** the `read_groups_by_id<List, ReadIntegers, ReadFloats, ReadStrings>` template holds the dispatch. The two named wrappers keep their exact signatures, and `read_element_by_id_lua` still calls both.
- **M8 (DEDUP-04, first item):**
  - `scalar_metadata_lua` and `group_metadata_lua` became two `metadata_to_lua` overloads.
  - Four list functions and four get functions became `list_metadata_lua<auto List>` and `get_metadata_lua<auto Get>`.
  - `lua_data_type_name` and its `default:` are untouched, for Phase 4.
- **M16 (DEDUP-05, its item):**
  - `columns_to_cpp_rows` throws the "contain no rows" text as its first statement. That literal now appears once under `src/lua_runner`.
  - `join_column_names` is anonymous-namespace in `db_write.cpp`, and its declaration is gone from `internal.h`.
  - `length_mismatch(...)` builds the "has length" text, which now appears once. It is used at both sites, and their `!=` / `>` predicates are unchanged.

## Check-order ledger

reordered checks: none

- **run_in_scope** has the same order for both callers (transaction: `begin_transaction`, `commit`, `rollback`; dry_run: `begin_dry_run`, `end_dry_run`, `end_dry_run`):
  1. begin
  2. call `fn(std::ref(self))`
  3. on an invalid result, capture `sol::error`, then abort inside an empty `catch (...)`, then rethrow `std::runtime_error(err.what())`
  4. finish
  5. return the first value, or nil
- **group_rows_from_lua** (vector/set writers):
  1. collect (`collect_group_columns`: key type, array shape, positive integer keys)
  2. empty-return (no columns clears)
  3. extent (row count = max extent)
  4. no-rows: now the first statement of `columns_to_cpp_rows`, which this function calls immediately after computing the extent, as before
  5. cells (`lua_to_value` per cell)
- **time_series_rows_from_lua:**
  1. collect
  2. empty-return
  3. metadata (`get_time_series_metadata`)
  4. missing dimension
  5. dimension length (`length_mismatch`, `!=`)
  6. dimension holes
  7. value too long (`length_mismatch`, `>`)
  8. no-rows: first statement of `columns_to_cpp_rows`, still after every check above
  9. cells
- **Other changes:**
  - The M4/M7/M8 adapters make the same core calls in the same order.
  - M8's list functions now always create the result table before calling `db` (`list_scalar_attributes` used to call `db` first). `create_table` has no effect a script can see, and the golden `metadata` probes, error paths included, are unchanged.

## Debug-only text changes (Release: none)

Every probe below shares the same `[string "-- schema: collections.sql..."]:<line>: ` prefix before and after; it is omitted here. `std::basic_string<char,std::char_traits<char>,std::allocator<char> >` is abbreviated `std::string`. The full verbatim strings are in `build/dedupe-check/baseline/debug_text.base.pretty.txt` (base) and `debug_text.pretty.txt` (now). No `control_` probe changed: the diff over `control_` lines is 0, covering `control_transaction`, `control_dry_run`, the bulk readers, the by-id composites, the metadata functions, `read_csv` and `w:write_row(5)`. The `dot_writer_*` probes are unchanged in this plan.

| Probe | Base text | New text |
|-------|-----------|----------|
| `dot_describe` (M3) | `stack index 1, expected userdata, received no value: value is not a valid userdata (bad argument into 'std::string(quiver::Database&)')` | `sol: received nil for 'self' argument (use ':' for accessing member functions, make sure member variables are preceeded by the actual object with '.' syntax)` |
| `dot_is_healthy` (M3) | `stack index 1, expected userdata, received no value: value is not a valid userdata (bad argument into 'bool(quiver::Database&)')` | `sol: received nil for 'self' argument (use ':' for accessing member functions, make sure member variables are preceeded by the actual object with '.' syntax)` |
| `dot_delete_element` (M3) | `stack index 1, expected userdata, received string: value is not a valid userdata (bad argument into 'void(quiver::Database&, const std::string&, __int64)')` | `sol: received nil for 'self' argument (use ':' for accessing member functions, make sure member variables are preceeded by the actual object with '.' syntax)` |
| `badarg_delete_element` (M3) | `stack index 3, expected number, received string: not a numeric type that fits exactly an integer (number maybe has significant decimals) (bad argument into 'void(quiver::Database&, const std::string&, __int64)')` | `stack index 3, expected number, received string: not a numeric type that fits exactly an integer (number maybe has significant decimals) (bad argument into 'void(const std::string&, __int64)')` |
| `badarg_number_of_elements` (M3) | `stack index 2, expected string, received table: (bad argument into '__int64(quiver::Database&, const std::string&)')` | `stack index 2, expected string, received table: (bad argument into '__int64(const std::string&)')` |
| `badarg_describe_collection` (M3) | `stack index 2, expected string, received table: (bad argument into 'std::string(quiver::Database&, const std::string&)')` | `stack index 2, expected string, received table: (bad argument into 'std::string(const std::string&)')` |
| `badarg_has_time_series_files` (M3) | `stack index 2, expected string, received table: (bad argument into 'bool(quiver::Database&, const std::string&)')` | `stack index 2, expected string, received table: (bad argument into 'bool(const std::string&)')` |
| `badarg_query_string` (M5) | `stack index 2, expected string, received table: (bad argument into 'sol::basic_object<sol::basic_reference<0> >(quiver::Database&, const std::string&, sol::optional<sol::basic_table_core<0,sol::basic_reference<0> > >, sol::this_state)')` | `stack index 2, expected string, received table: (bad argument into 'std::optional<std::string >(quiver::Database&, const std::string&, sol::optional<sol::basic_table_core<0,sol::basic_reference<0> > >)')` |
| `badarg_query_integer` (M5) | `stack index 2, expected string, received table: (bad argument into 'sol::basic_object<sol::basic_reference<0> >(quiver::Database&, const std::string&, sol::optional<sol::basic_table_core<0,sol::basic_reference<0> > >, sol::this_state)')` | `stack index 2, expected string, received table: (bad argument into 'std::optional<__int64>(quiver::Database&, const std::string&, sol::optional<sol::basic_table_core<0,sol::basic_reference<0> > >)')` |
| `badarg_query_float` (M5) | `stack index 2, expected string, received table: (bad argument into 'sol::basic_object<sol::basic_reference<0> >(quiver::Database&, const std::string&, sol::optional<sol::basic_table_core<0,sol::basic_reference<0> > >, sol::this_state)')` | `stack index 2, expected string, received table: (bad argument into 'std::optional<double>(quiver::Database&, const std::string&, sol::optional<sol::basic_table_core<0,sol::basic_reference<0> > >)')` |

No test pins any of these texts.

## Mutation results (each run after its commit and reverted; `git diff --exit-code -- src/lua_runner` was clean afterwards)

| Item | Mutation | Result |
|------|----------|--------|
| M3 | `commit` registered as `&Database::rollback` | the name==member check printed `MISMATCH commit rollback` and exited 1 |
| M1 | `&Database::commit` and `&Database::rollback` swapped in the transaction lambda, then rebuilt | `LuaRunnerTest.TransactionBlockAutoCommit` failed (`labels.size()` was 0, expected 1), then the run aborted on an MSVC debug vector assertion |
| M4 | `read_scalar_floats` registered as `&bulk_read_lua<&Database::read_scalar_integers>` | `MISMATCH read_scalar_floats read_scalar_integers`, exit 1 |
| M8 | `get_scalar_metadata` registered as `&get_metadata_lua<&Database::get_set_metadata>` | `MISMATCH get_scalar_metadata get_set_metadata`, exit 1 |
| M16 | the moved no-rows check deleted, then rebuilt | all four named tests failed: `UpdateGroupErrors`, `UpdateVectorGroupByLabelErrors`, `UpdateSetGroupByLabelErrors`, `UpdateTimeSeriesGroupAllEmptyColumnsThrows` |

The M3 mutation was first run before the commit. Its `git checkout` reverted the uncommitted forwarder edit as well. The edit was reapplied byte-for-byte (same 34-line diff), the gate was re-run (`GATE PASS pairs=17`), the change was committed, and the mutation was then repeated after the commit with the same result.

## Tracer gate (delegated self-verification)

The run was interactive (`auto_advance` false). Following the user's standing preference, the tracer's human-verify checkpoint was verified by the executor rather than handed back. The evidence:
- The full gate passed at the base and after M3.
- The surface diff was empty.
- Exactly 17 name==member pairs matched.
- The debug-text diff touched only the 7 forwarder probes (`dot_describe` among them) and 0 `control_` probes.
- The commit/rollback mutation was caught.

Verdict: pass, so execution continued.

## Golden harness (gitignored, `build/dedupe-check/`)

- **Contents:**
  - `BASE`, `surface-base.txt` (equal to `build/split-check/before.txt`) and `prelude.lua`, the shared `call`/`enc`/`walk` helpers spliced in after each schema line.
  - 10 probe scripts in `scripts/`: `core`, `read_all_types`, `read_collections`, `write`, `write_multi_dim`, `options`, `csv`, `handles_run1`, `handles_run2` and `binary`. Every first line matches `-- schema: <file>.sql`.
  - `debug_text.lua` (Debug only), with 13 `control_` and 12 `dot_`/`badarg_` probes.
  - The scripts `golden.sh`, `gate.sh` and `wave_gate.sh`, plus `baseline/{debug,release}` and the `debug_text{,.base}{,.pretty}.txt` files.
- **Coverage:** all 15 required top-level keys appear in both the Debug and Release baselines.
- **Determinism:** each build was captured, then re-run twice, with identical output. The Debug and Release outputs are identical to each other.
- **Excluded probes:** three were dropped because their output depends on Lua's per-process string-hash seed, which makes `pairs` order vary between runs: an error raised with a table value (its address), and two multi-column no-rows messages (the joined name order). Every remaining probe names at most one offending column.

## Decisions Made

- I accepted the Debug-only bad-argument signature changes for the 17 forwarders (M3) and the three `query_*` methods (M5). CONTEXT allows them as amended after research.
- I kept `query_*_lua` as three named functions, which leaves Phase 4 three sites for `optional_from_lua`.
- I did not change the `src/AGENTS.md` time-series transpose bullet in M16. It still says correctly that named columns with a zero-length dimension throw. The shared-helpers paragraph that will name `columns_to_cpp_rows` as the owner belongs to plan 03-03.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Tidy file filter quoting in wave_gate.sh**
- **Found during:** Task 1 (base wave gate)
- **Issue:** the plan's `"src[\\/]lua_runner[\\/]"` in bash double quotes reaches Python as `[\/]`. That matches no Windows backslash path, so tidy ran on 0 files and the gate exited silently.
- **Fix:** single-quoted `'src[\\/]lua_runner[\\/]'`, plus a guard that fails if tidy matched no files.
- **Files modified:** `build/dedupe-check/wave_gate.sh` (gitignored)

**2. [Rule 1 - Bug] CRLF written by a Python edit**
- **Found during:** Task 1 Part B
- **Issue:** `pathlib.write_text` on Windows wrote CRLF into three sources.
- **Fix:** converted the files back to LF before any commit. Every later edit used `read_bytes`/`write_bytes`. No CRLF reached a commit (`.gitattributes` would normalize it anyway).

### Noted shortfalls

- The acceptance item "`ls build/dedupe-check/scripts/*.lua | wc -l` prints at least 11" gives 10. The plan's own script list names exactly these 10 files, and `debug_text.lua` (the 11th probe file) lives in `build/dedupe-check/` as the plan specifies. Every required probe key is covered. I added no padding script, because a new script's baseline would have to come from base binaries that no longer exist.

**Total deviations:** 2 auto-fixed (harness tooling only), 1 noted count shortfall. **Impact:** none on the shipped code.

## Issues Encountered

None in the code. Every commit's gate passed on the first run.

## Next Phase Readiness

- Plans 03-02 (options/CSV) and 03-03 (binary/registry/docs) can reuse `build/dedupe-check/{gate.sh,wave_gate.sh,golden.sh}` unchanged.
- In 03-02, M11 will change the `dot_writer_write_row` and `dot_writer_close` Debug probes, which are expected to change. Run that gate with `DEBUG_TEXT_CHANGE=1` and promote `out/debug_text*.txt`.
- DEDUP-02, DEDUP-04, DEDUP-05 and DEDUP-06 are partially delivered here. 03-02 and 03-03 finish them.

## Self-Check: PASSED

- All 7 commits were found (`e46e681`, `f248e16`, `8ab8f4b`, `ec864cc`, `31a0d0f`, `997c1b3`, `31b5a07`). No commit deletes a file.
- All 7 modified source and doc files exist, and the harness scripts exist.
- `git diff --stat BASE HEAD -- tests bindings CHANGELOG.md CMakeLists.txt` is empty.
