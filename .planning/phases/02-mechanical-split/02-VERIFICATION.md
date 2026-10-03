---
phase: 02-mechanical-split
verified: 2026-10-03T04:35:00Z
status: passed
score: 5/5 roadmap success criteria verified (7/7 requirements satisfied); backstop truth delegated-verified on Linux GCC 13 + Clang 18/libc++ (Docker), Apple Clang left to PR CI
behavior_unverified: 0
overrides_applied: 0
human_verification:
  - test: "Open the phase PR and let CI build and test on Linux (GCC) and macOS (Apple Clang)"
    expected: "The quiver target compiles with no missing-include or duplicate-symbol error, and quiver_tests Lua* (444), quiver_c_tests LuaRunnerCApiTest (27) and the binding suites pass on both platforms"
    why_human: "Only MSVC is installed locally (no g++ and no Apple toolchain). 02-04-PLAN marks this truth `verification: backstop`. The local evidence is a clang-tidy run (clang frontend, MSVC STL) with no clang-diagnostic-error, and the reviewer's `clang++ -fsyntax-only` per TU against the MSVC STL. Neither proves the libstdc++/libc++ include set or the GCC linker"
---

# Phase 2: Mechanical Split Verification Report

**Phase Goal:** `src/lua_runner.cpp` is replaced by `src/lua_runner/`. Each file there registers and implements its own slice of the Lua surface, so a later change touches one small file, and no observable behaviour changes.
**Verified:** 2026-10-03
**Status:** human_needed (all code checks pass; GCC/Apple Clang is confirmed only by the PR's CI)
**Re-verification:** No, this is the initial verification

## Goal Achievement

### Observable Truths (ROADMAP success criteria)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | `src/lua_runner.cpp` is gone. The folder holds the 11 named files, each listed explicitly in `QUIVER_SOURCES` with no GLOB, and no file is over about 450 lines | ✓ VERIFIED | `ls src/lua_runner.cpp` gives ENOENT. `ls src/lua_runner/` shows exactly the 11 files. `src/CMakeLists.txt:17-27` lists them one per line, and `grep -i glob` finds no match. `wc -l`: largest is csv.cpp at 446, then binary.cpp at 377 (2711 lines total) |
| 2 | Exactly one `new_usertype<Database>` and one `open_libraries(`. Ctor order is preserved, the 17 variadic pairs are now `bind.set_function`, the other four usertypes stay variadic, `RunHandles` comes before `lua`, no `[this]` remains, and `GcGuard` comes before `result` and runs close and then one `collect_garbage()` | ✓ VERIFIED | Each grep count is 1, both in lua_runner.cpp. Ctor in `lua_runner.cpp:70-96`: open_libraries (the same 6 libs as bab557e:243-250), nil dofile/loadfile, `ns = create_named_table("quiver")`, then `bind`, the 7 binders, and `lua["db"]` last. The multiline extraction of `bind.set_function` names gives 71 in the new folder and 71 in the old file (variadic plus set_function), with an empty diff and no duplicates. The `ns.set_function` set is the same 15. The 4 other usertypes are variadic (binary.cpp:244,273,307, csv.cpp:395). `Impl` declares db, handles, lua in that order. `[this]` has 0 hits, and captures are `[&handles]` (binary.cpp:221, csv.cpp:376) and `[&db]` (binary.cpp:311). `GcGuard` (lua_runner.cpp:117-123) is declared before `result`. sol2 `new_usertype(key)` with no args uses the same `constant_automagic_enrollments<>` as the old variadic call (`table.hpp:36-81`), so the enrollments are unchanged |
| 3 | The sync test reads every file in sorted order and resets `current` per file. The method set is unchanged, and deleting a `bind.set_function` line makes it fail | ✓ VERIFIED | `lua-api-sync.test.ts:10-16` uses `readdirSync`, a `.cpp/.h` filter and `.sort()`. Line 37 sets `let current = ""` inside the per-file loop. `build/split-check/before.txt` and `after.txt` are identical. I re-ran the mutation myself: deleting `db_core.cpp:212` (describe) gave 5 pass and 1 fail (`db:describe` stale). After `git checkout`, `git diff --exit-code` is clean, and the restored suite gives 6 pass |
| 4 | No test expectation changes, and Lua* = baseline, C API 27, the 4 binding suites pass. `/bigobj` is target-wide, `SOL_*` stay PRIVATE, clang-format is clean, tidy is at the 15-warning baseline, NOLINT pairs carry the corrected name, and build time is recorded | ✓ VERIFIED (GCC/Apple Clang: see human item) | Re-run here: Debug `Lua*` lists 444 in 12 suites and all pass, and C API lists 27 and all pass. Release (`build/release`, built 01:06, after the last source commit) also gives 444 and 27, all passing. `build/split-check/test-all.txt` reports all six suites PASS. Since bab557e the only changes under `tests/` and `bindings/*/test` are the sync test's file-reading code and one comment line in each of the two C++ test files. `CMakeLists.txt:78-84` uses `target_compile_options` for `/bigobj` and `-Wa,-mbig-obj`, and the define block is identical to bab557e. `uvx clang-format==22.1.8 --dry-run --Werror` is clean. tidy.txt covers all 10 TUs, with 0 `performance-unnecessary-value-param` and 0 `clang-diagnostic-error`. Its 15 lua_runner warnings match RESEARCH Pitfall 1's baseline item for item (5 naming, 1 widening, 4 empty-catch, 3 unchecked-optional, 1 raw-string, 1 braced-init). There is one NOLINT pair, named `performance-unnecessary-value-param`, in each of db_core, db_write, db_time_series, csv and binary. db_read, db_metadata, return_json, path_policy and lua_runner.cpp have no by-value sol2 parameters. Build time went from 70 s to 46 s wall clock (02-01 and 02-04 SUMMARY) |
| 5 | No full-path citation of the old file remains. Every remaining `lua_runner.cpp` hit names the new folder, the C API TU or its test. The listed sites are re-pointed, AGENTS.md describes the layout, and the moved code has no planning IDs | ✓ VERIFIED | `git grep 'src/lua_runner\.cpp'` outside `.planning/` and CHANGELOG returns nothing. A backslash spelling also returns nothing. Every remaining `lua_runner\.cpp` hit is one of: `src/lua_runner/lua_runner.cpp` (root AGENTS:896, julia AGENTS:158, src AGENTS:828), the file-map entry under `lua_runner/` (src AGENTS:47), `lua_runner/lua_runner.cpp` (CMake:25), `c/lua_runner.cpp` (CMake:141, src/c AGENTS:38), or `test_c_api_lua_runner.cpp`. Stale symbols (`bind_database`, `Impl::CsvWriter` and the like) return nothing. Platform.cmake, build.dart, csv_read.h, csv_write.h/.cpp and the lua-api.ts header all name the new files, and the diffs are comment-only. The `to_chars` citations in Platform.cmake and root AGENTS name return_json.cpp and csv.cpp, which matches `grep append_number\|to_chars`. `src/AGENTS.md:46-57` carries the 11-file map and `:642` the Layout bullet. A planning-ID regex over `src/lua_runner/` finds 0 |

**Score:** 5/5 roadmap truths verified (0 present-but-behavior-unverified)

The behaviour-dependent pieces are the move semantics of `RunHandles`, the `[&handles]` captures surviving a runner move, and `GcGuard`'s close-then-collect. Each has a passing pin inside the 444, in Debug and in Release: the 4 `LuaRunner_Lifecycle` move pins, `HandleFromAnEarlierRunIsClosed`, and the global-writer flush tests. `static_assert(sizeof(LuaRunner) == sizeof(void*))` is still in `tests/test_lua_runner_lifecycle.cpp:8`.

### Behaviour-neutrality cross-check (independent of SUMMARY claims)

I took a multiset diff of the normalised code lines (whitespace stripped, comment and blank lines dropped) between `bab557e:src/lua_runner.cpp` and the whole new folder. Every difference is one of these:

- a `static` member became a free function or a function in an anonymous namespace;
- `[this]` became `[&handles]` or `[&db]`;
- `close_open_writers` and `path_has_open_writer` gained the `handles.` qualifier;
- the 17 variadic pairs became `bind.set_function`;
- `append_json(result...)` moved behind `encode_return_json(value)`;
- includes, namespace wrappers and binder signatures;
- clang-format re-joined lines inside messages and the operator lambdas, with the concatenated text unchanged.

No logic line or message string changed. `#include <chrono>` was dropped. The build is green, so it was unused.

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/lua_runner/lua_runner.cpp` | Impl, ctor, RunHandles bodies, run()/GcGuard | ✓ VERIFIED | 144 lines, no `bind.set_function`, no binder definitions |
| `src/lua_runner/internal.h` | guard, RunHandles, binder declarations, templates and inlines only | ✓ VERIFIED | `QUIVER_SRC_LUA_RUNNER_INTERNAL_H`. No `static` or anonymous namespace, and no csv-parser include |
| `src/lua_runner/return_json.cpp` | `encode_return_json` | ✓ VERIFIED | called from `run()` |
| `src/lua_runner/path_policy.cpp` | single `resolve_sandboxed_path` definition | ✓ VERIFIED | the only definition |
| `db_core/read/write/metadata/time_series.cpp` | per-domain binders 22/14/11/8/10 | ✓ VERIFIED | counts match exactly |
| `csv.cpp`, `binary.cpp` | 3 db methods each + CsvWriter / 15 quiver fns + 3 usertypes | ✓ VERIFIED | `CsvWriter` is defined once in named `quiver::lua_internal` (csv.cpp:24) |
| `src/CMakeLists.txt` | 11 explicit entries, target-wide bigobj, defines unchanged | ✓ VERIFIED | |
| `bindings/js/test/lua-api-sync.test.ts` | folder reader, per-file reset | ✓ VERIFIED | |
| `src/AGENTS.md`, `AGENTS.md` | file map, layout conventions, pointers | ✓ VERIFIED | |

### Key Link Verification

| From | To | Via | Status |
|------|----|-----|--------|
| `Impl` ctor | 7 binders in `internal.h` | `lua_internal::bind_*(...)` calls (lua_runner.cpp:87-93) | ✓ WIRED |
| `LuaRunner::run` | `return_json.cpp` | `lua_internal::encode_return_json` | ✓ WIRED |
| open_file / write_csv closures | `RunHandles` registries | `handles.open_binary_files.push_back` / `handles.open_writers.emplace_back` | ✓ WIRED |
| db_time_series.cpp / binary.cpp | db_write.cpp | `collect_group_columns`, `columns_to_cpp_rows`, `table_to_element` declared in internal.h | ✓ WIRED (links; tests exercise them) |
| sync test | `src/lua_runner/` | `SRC_DIR` + sorted `readdirSync` | ✓ WIRED |

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| Debug build current | `cmake --build build --config Debug` | `ninja: no work to do` | ✓ PASS |
| Debug Lua* | `quiver_tests --gtest_filter=Lua*` (list + run) | 444 / 12 suites, all PASSED | ✓ PASS |
| Debug C API | `quiver_c_tests --gtest_filter=LuaRunnerCApiTest.*` | 27 PASSED | ✓ PASS |
| Release Lua* + C API | `build/release/bin` same filters | 444 PASSED, 27 PASSED | ✓ PASS |
| Sync test | `bun test test/lua-api-sync.test.ts` | 6 pass | ✓ PASS |
| Sync-test mutation | delete describe line, test, restore | 1 fail (stale `db:describe`), then clean restore | ✓ PASS |
| clang-format pin | `uvx --from clang-format==22.1.8 clang-format --dry-run --Werror` on folder + touched C++ | no output | ✓ PASS |
| Six suites | `build/split-check/test-all.txt` (executor run, Dart hook cache cleared) | all PASS | ✓ PASS (artifact; not re-run) |

### Probe Execution

Step 7c: SKIPPED. No `scripts/*/tests/probe-*.sh` exists, and no plan declares one.

### Requirements Coverage

| Requirement | Source Plan | Status | Evidence |
|-------------|-------------|--------|----------|
| SPLIT-01 | 02-01, 02-02, 02-03 | ✓ SATISFIED | Truth 1 |
| SPLIT-02 | 02-02, 02-03 | ✓ SATISFIED | Truth 2 |
| SPLIT-03 | 02-02 | ✓ SATISFIED | Truth 2 (RunHandles order, captures, GcGuard) plus the lifecycle pins |
| SPLIT-04 | 02-01, 02-04 | ✓ SATISFIED | Truth 3 |
| SPLIT-05 | 02-01, 02-02, 02-03, 02-04 | ✓ SATISFIED | Truth 4 (bigobj, defines, NOLINT, format, tidy baseline) |
| SPLIT-06 | 02-01 through 02-04 | ✓ SATISFIED (MSVC); GCC/Clang via CI | Truth 4 |
| SPLIT-07 | 02-01, 02-04 | ✓ SATISFIED | Truth 5 |

No requirement is orphaned. REQUIREMENTS.md maps exactly SPLIT-01 to SPLIT-07 to Phase 2, and the plans claim every one of them.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| `src/lua_runner/db_core.cpp` | 224 | "sandboxed like the file I/O below": that file I/O now lives in binary.cpp (review WR-01) | ⚠️ Warning | A misleading comment only. It is not a citation of the old file, so SC5 still holds |
| `src/lua_runner/csv.cpp` | 284 | "sandboxed like the file I/O above": same issue (WR-01) | ⚠️ Warning | Same as above |
| `bindings/js/test/lua-api-sync.test.ts` | 12-26 | Pass 1 sees only the `bind`/`ns` receiver names, and `readdirSync` is not recursive (WR-02) | ⚠️ Warning | Under a different receiver name, an undocumented new method would go unnoticed. SC3 holds as written, and the gap matters for Phase 3 edits |
| `src/lua_runner/csv.cpp`, `binary.cpp` | various | Lambda-local `sol::state_view lua` shadows the binder's `sol::state& lua` parameter (IN-02) | ℹ️ Info | No bug today |
| — | — | `CsvWriter`'s sol2 demangled name changed (`LuaRunner::Impl::CsvWriter` became `quiver::lua_internal::CsvWriter`) | ℹ️ Info | It shows up only in Debug-build sol2 panic text, never in a bound name. No test asserts it |

There are no TBD/FIXME/XXX/TODO/HACK markers in any file this phase modified.

### Human Verification Required

#### 1. GCC (Linux) and Apple Clang (macOS) build + suites

**Test:** Open the phase PR and let the CI matrix run.
**Expected:** The `quiver` target compiles and links on GCC and Apple Clang, and every suite passes.
**Why human:** Only MSVC exists locally. The plan declares this truth `backstop`, to be confirmed by CI. The local evidence is a clang frontend running against the MSVC STL, which cannot prove that libstdc++/libc++ get the include set they need. `std::visit` comes in transitively through `quiver/value.h`, and `size_t` through `<cstddef>`; both look fine on reading.

### Gaps Summary

No blocking gaps. Every roadmap success criterion and all 7 requirements are met in the codebase, and I re-ran the evidence independently:

- the method-set diff;
- the line-multiset diff against bab557e;
- the Debug and Release test counts;
- the sync test and its mutation;
- clang-format;
- the citation greps.

The status is `human_needed` only because the GCC/Apple Clang compile can be confirmed only by the PR's CI run. That fits the plan's own backstop classification, and it is what decides between merging and holding the PR.

On the 02-04 plan's C2 contradiction: the only bare `lua_runner.cpp` hit is the new root file's line in the `src/AGENTS.md` file map (`src/AGENTS.md:47`, indented under `lua_runner/`). It names a file under `src/lua_runner/`, so roadmap SC5 holds. The plan-internal acceptance grep was stricter than the roadmap contract, and the deviation is correct.

Two warnings are worth fixing cheaply before or in Phase 3: WR-01 (two directional comments) and WR-02 (add `expect(setFns.length).toBe(CPP.match(/\.set_function\(/g)?.length)` and a no-subdirectory assertion). Neither breaks this phase's goal.

---

_Verified: 2026-10-03T04:35:00Z_
_Verifier: Claude (gsd-verifier)_

## Human Verification Resolution

Delegated to Claude per the maintainer's standing "you check that" instruction (memory: self-verify-checkpoints), 2026-10-03T04:42:10Z.
Backstop truth: GCC/Clang compile + suites. Evidence: `git archive 8fbb066` built from scratch in ubuntu:24.04 containers
(Debug, tests + C API on), once with GCC 13.3/libstdc++ and once with Clang 18.1.3 `-stdlib=libc++`:

| Toolchain | Build | Lua* | LuaRunnerCApiTest | quiver_tests | quiver_c_tests |
|-----------|-------|------|-------------------|--------------|----------------|
| GCC 13 / libstdc++ | 0 errors; warnings only in untouched `src/binary/time_properties.cpp` | 441 pass + 1 skip / 442 | 27/27 | 1404 + 3 skip / 1407 | 543/543 |
| Clang 18 / libc++ | 0 errors; one warning in untouched `tests/test_migrations.cpp` | 441 pass + 1 skip / 442 | 27/27 | 1404 + 3 skip / 1407 | 543/543 |

442 vs the Windows 444 is fully accounted for: `LuaRunner_ReadCsv.DeviceNamePathIsReportedWithPrefix` and
`LuaBinaryTest.DeviceNamePathIsReportedWithPrefix` are `#ifdef _WIN32` by design. The skip is the chmod-000 test,
which self-skips as root. Commits after 8fbb066 (8700b62, e5b00b7) change C++ comments only.
Residual: Apple Clang itself (Apple libc++ availability macros, deployment target) and the binding suites on
Linux/macOS are left to the PR's CI matrix. The split adds no new library calls, so the existing 13.3 floor in
`cmake/Platform.cmake` still covers it.
