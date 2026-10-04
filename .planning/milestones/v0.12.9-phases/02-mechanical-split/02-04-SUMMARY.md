---
phase: 02-mechanical-split
plan: 04
subsystem: lua-runner
status: complete
tags: [lua, docs, agents-md, phase-gate, clang-tidy, split]
requires: [02-03 final 11-file src/lua_runner/ folder]
provides: [no stale citation of the pre-split core file, src/lua_runner/ file map and layout conventions in src/AGENTS.md, phase-gate evidence and PR notes]
affects: [phase 03]
tech-stack:
  added: []
  patterns: [AGENTS.md file map lists every src/lua_runner/ file with a one-line role]
key-files:
  created: []
  modified: [src/csv/csv_read.h, src/csv/csv_write.h, src/csv/csv_write.cpp, cmake/Platform.cmake, bindings/dart/hook/build.dart, bindings/js/src/lua-api.ts, tests/test_database_ui_metadata.cpp, tests/test_lua_runner_write_csv.cpp, AGENTS.md, src/AGENTS.md, tests/AGENTS.md, bindings/js/AGENTS.md, bindings/dart/AGENTS.md, bindings/julia/AGENTS.md, src/lua_runner/internal.h]
decisions:
  - "Citation check C2 keeps one hit by construction: the src/AGENTS.md file-map entry `    lua_runner.cpp  # ...` sits under `  lua_runner/` and names the new root TU; the RESEARCH block shape and C2's regex cannot both be satisfied literally"
  - "The 15 pre-existing tidy warnings on src/lua_runner/ are the accepted baseline; header warnings from include/quiver/*.h and src/utils/datetime.h are pre-existing findings in untouched headers, reproduced from untouched TUs"
metrics:
  duration: 45min
  completed: 2026-10-03
actuals:
  tokens: 9000
  tasks: 3
  commits: 3
---

# Phase 2 Plan 04: Citations, layout docs and phase gate Summary

Every comment and AGENTS.md citation of the old single `src/lua_runner.cpp` now names its file under `src/lua_runner/`. `src/AGENTS.md` carries the 11-line folder map and a "Layout" convention bullet. The phase gate is green: Debug and Release `Lua*` 444/12, C API 27, all six suites PASS on a fresh Dart hook build, tidy at the 15-warning baseline, clang-format a fixed point, and the sync test fails on a deleted registration.

## Commits

| Task | Commit | Files |
|------|--------|-------|
| 1 (tracer): code, CMake, hook, JS-header, test comments | 660f3ec | src/csv/{csv_read.h, csv_write.h, csv_write.cpp}, cmake/Platform.cmake, bindings/dart/hook/build.dart, bindings/js/src/lua-api.ts, tests/test_database_ui_metadata.cpp, tests/test_lua_runner_write_csv.cpp |
| 2: AGENTS.md citations + layout | 486561e | AGENTS.md, src/AGENTS.md, tests/AGENTS.md, bindings/{js,dart,julia}/AGENTS.md |
| carried-over note: `is_lua_boolean` comment | c63ad40 | src/lua_runner/internal.h (one comment line) |
| 3: phase gate | none | no fix was needed |

## Gate evidence

**Debug** (after every commit, `build/split-check/checks.sh`): `Lua*` 444 listed in 12 suites, 444 passed. `LuaRunnerCApiTest.*` 27 listed, 27 passed. Sync test 6 pass. Surface diff against `before.txt` empty.

**Release** (`cmake --build --preset release --target quiver_tests quiver_c_tests`): `Lua*` 444 listed in 12 suites (counted with `--gtest_list_tests`), all passed (18.8 s). `LuaRunnerCApiTest.*` 27 listed, all passed.

**Six suites:** `bindings/dart/.dart_tool/hooks_runner` and `bindings/dart/.dart_tool/lib` were deleted first (`ls` confirmed both gone; both were recreated by the run, so the hook rebuilt the native). `cmd //c 'scripts\test-all.bat'` exited 0:

```
  C++ tests:        PASS   (1410 tests)
  C API tests:      PASS   (543 tests)
  Julia tests:      PASS   (1574 / 1574)
  Dart tests:       PASS   (+444: All tests passed!)
  JavaScript tests: PASS   (241 pass, 0 fail)
  Python tests:     PASS   (350 passed)
```

`grep -Ec 'tests: +PASS'` over the captured output prints 6.

**clang-format 22.1.8:** `--dry-run --Werror src/lua_runner/*.cpp src/lua_runner/*.h` exits 0. A second `-i` run left `git diff -- src/lua_runner` empty (fixed point). The five C++ files from Task 1 and `internal.h` are also clean.

**clang-tidy** (`run-clang-tidy -p build -quiet 'src.lua_runner.'`, 10 files; output in `build/split-check/tidy.txt`): 0 `performance-unnecessary-value-param`, 0 `clang-diagnostic-error`. Warnings on `src/lua_runner/` files, matched against the baseline:

| File | Check | Count | Baseline |
|------|-------|-------|----------|
| return_json.cpp | readability-identifier-naming (`kMaxReturnDepth`, `kMaxReturnBytes`, `kHex`) | 3 | 3 |
| return_json.cpp | bugprone-implicit-widening-of-multiplication-result | 1 | 1 |
| csv.cpp | readability-identifier-naming (`header_width_`, `kMaxWidth`) | 2 | 2 |
| lua_runner.cpp | bugprone-empty-catch (`close_open_writers`) | 2 | 2 |
| db_core.cpp | bugprone-empty-catch (transaction, dry_run) | 2 | 2 |
| binary.cpp | bugprone-unchecked-optional-access | 3 | 3 |
| binary.cpp | modernize-raw-string-literal | 1 | 1 |
| binary.cpp | modernize-return-braced-init-list | 1 | 1 |
| internal.h, path_policy.cpp, db_read.cpp, db_metadata.cpp, db_time_series.cpp, db_write.cpp | none | 0 | 0 |
| **total** | | **15** | **15** |

The header filter also reports warnings in headers outside the folder: `bugprone-exception-escape` in `include/quiver/element.h`, `options.h`, `expression/expression_node.h`, `binary/csv_converter.h`, and two `modernize-return-braced-init-list` plus one `readability-identifier-naming` in `src/utils/datetime.h`. None of these headers changed since `bab557e`. A control tidy run over untouched TUs (`database_csv_export.cpp`, `database_create.cpp`, `expression/expression.cpp`, `build/split-check/tidy-control.txt`) reports the same header warnings, so they are pre-existing and not on the new files. They repeat once per including TU, which is why the split raises their raw count.

**Sync-test mutation:** I deleted line 212 of `db_core.cpp` (`bind.set_function("describe", ...)`) and did not build. `bun test test/lua-api-sync.test.ts` then gave 5 pass, 1 fail. The failing test was `lua-api reference stays in sync with src/lua_runner/ > no documented db:/quiver. name has been removed from the binding`, with `db:describe` received as stale. After `git checkout -- src/lua_runner/db_core.cpp` it gave 6 pass, 0 fail, and `git diff --exit-code -- src/lua_runner` exited 0.

**Citation checks** (repo-wide, after 486561e):
- C1 (`src/lua_runner\.cpp`): prints nothing.
- C3 (stale symbols): prints nothing.
- C2 (bare name): one hit, `src/AGENTS.md:47:    lua_runner.cpp        # LuaRunner::Impl ...`. This is the root TU's line in the new file map, indented under `  lua_runner/`. See Deviations.
- Every remaining `git grep -n 'lua_runner\.cpp'` hit outside `.planning/` and `CHANGELOG.md`: `AGENTS.md:896` and `bindings/julia/AGENTS.md:158` (`src/lua_runner/lua_runner.cpp`), `src/AGENTS.md:47` (file map), `src/AGENTS.md:828` (`src/lua_runner/lua_runner.cpp`), `src/CMakeLists.txt:25` (`lua_runner/lua_runner.cpp`), `src/CMakeLists.txt:141` (`c/lua_runner.cpp`), `src/c/AGENTS.md:38` (C API listing), `tests/AGENTS.md:96,136` and `tests/CMakeLists.txt:93` (`test_c_api_lua_runner.cpp`). Neither C API file was renamed.

**Static checks:**
- `wc -l src/lua_runner/*`: max 446 (`csv.cpp`), none over 450.
- `git grep -n 'SOL_' -- '*CMakeLists.txt' cmake`: only `src/CMakeLists.txt:73-75`, inside `target_compile_definitions(quiver PRIVATE`.
- The planning-ID regex over `src/lua_runner` prints nothing. No file this plan edited gained an ID match against `bab557e`. The per-file counts are equal: src/csv 2/7/10, the test files 31/58, the AGENTS.md files 1/15/7/0/0/0, and the rest 0.
- `git diff --stat bab557e` over the test trees lists exactly `bindings/js/test/lua-api-sync.test.ts`, `tests/AGENTS.md`, `tests/test_database_ui_metadata.cpp`, `tests/test_lua_runner_write_csv.cpp`.
- `CHANGELOG.md`, `CMakeLists.txt` and the four binding manifests are unchanged since `bab557e`.
- `new_usertype<Database>` / `open_libraries(` counts in `AGENTS.md` and `src/AGENTS.md`: 0/0, the same as at `bab557e`.

## Build time (before)

70 s wall clock, Debug, Ninja, `build/` tree, clean `quiver` target (02-01). Slowest TU: `lua_runner.cpp.obj` at 60.87 s.

## Build time (after)

**46 s wall clock**, same protocol, tree and machine. Slowest TU: `lua_runner/binary.cpp.obj` at 42.09 s. Then `csv.cpp` 28.62 s, `lua_runner.cpp` 23.80 s, `path_policy.cpp` 23.30 s and `db_time_series.cpp` 22.68 s. The sum over the 10 `quiver.dir/lua_runner/*` objects is 222.7 s of CPU, against 60.87 s for the single TU before. Each TU parses sol2 on its own, so total CPU went up about 3.7x while the wall clock (the critical path) went down 24 s.

## PR notes

- **Build time:** 70 s before, 46 s after (clean `quiver`, Debug, Ninja). The critical path went from one 60.9 s TU to `binary.cpp` at 42.1 s. Total CPU across the Lua TUs rose from 60.9 s to 222.7 s.
- **Reordered checks:** none.
- **NOLINT check-name fix:** the old pairs read `performance-unnecessary-value-parameter`, which is not a check name, so they suppressed nothing (50 warnings). They now read `performance-unnecessary-value-param`, with one pair per TU that has by-value sol2 parameters (binary, csv, db_core, db_time_series, db_write).
- **Accepted pre-existing tidy warnings (15):** `return_json.cpp` 4 (3 identifier-naming, 1 implicit-widening), `csv.cpp` 2 (identifier-naming), `lua_runner.cpp` 2 (empty-catch), `db_core.cpp` 2 (empty-catch), `binary.cpp` 5 (3 unchecked-optional-access, 1 raw-string-literal, 1 return-braced-init-list).
- **`CsvWriter` sol2 registry key:** the type moved from `LuaRunner::Impl::CsvWriter` to `quiver::lua_internal::CsvWriter`, so sol2's demangled usertype name changed. This shows up only in Debug-build error text, never in a bound name.
- **Review hints:** for the de-class commit, `git show -w --color-moved=zebra --color-moved-ws=allow-indentation-change c898a63`. For the extraction commits (`53eae34`, `e80cba0`, `85b5ec1`, `1dae9f8`), `git show --color-moved=zebra`. For history across the re-indent, `git blame -w -C -C`.
- **GCC (Linux) and Apple Clang (macOS)** are verified by the PR's CI. Locally, only MSVC was built and run, plus clang-tidy's clang frontend, which raised no `clang-diagnostic-error`.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Stale "this file" in the `is_lua_boolean` comment**
- **Found during:** carried over from 02-03.
- **Issue:** `src/lua_runner/internal.h` said "Every boolean test in this file", which meant the old single file.
- **Fix:** changed to "in src/lua_runner/". This is a one-line, comment-only edit. Format was clean and the Debug checks plus the surface diff were re-run.
- **Commit:** c63ad40. This is a separate commit because neither Task 1's nor Task 2's `<files>` lists `internal.h`.

**2. [Rule 1 - Bug] Other stale single-file wording in src/AGENTS.md**
- **Found during:** Task 2.
- **Issue:** the "Implementation conventions" bullets for the JSON encoder and `SOL_SAFE_NUMERICS` still said "the whole file", "file-wide", "at the top of the file" and "moved out of this file". C1-C3 do not catch these, but they meant the monolith.
- **Fix:** they now read "every `src/lua_runner/` TU", "folder-wide", "at the top of `return_json.cpp`" and "moved out of the Lua binding". In the same pass, the `RunHandles` bodies citation was written as `src/lua_runner/lua_runner.cpp` so it does not trip C2.
- **Commit:** 486561e.

**3. [Plan conflict] C2 cannot be empty while the file map has RESEARCH's shape**
- **Found during:** Task 2 verify.
- **Issue:** Task 2's verify requires both of these. C2 must print nothing, and `grep -cE '^ {4}(lua_runner\.cpp|...) +#'` must count 11. The file-map line `    lua_runner.cpp        # ...` matches both, because a space before the name satisfies C2's `[^/_a-z]`.
- **Resolution:** I kept the RESEARCH file-map shape. The one C2 hit names the new `src/lua_runner/lua_runner.cpp` by its indentation under `  lua_runner/`, so truth 2 holds. Every hit names a file under `src/lua_runner/`, the C API TU, or its test. The verify clause for C2 fails on this line alone.

## Known Stubs

None.

## Self-Check: PASSED

- FOUND: 660f3ec, 486561e, c63ad40
- FOUND: build/split-check/tidy.txt, build/split-check/test-all.txt
- FOUND: src/AGENTS.md 11-line map, `Implementation conventions in \`src/lua_runner/\``
