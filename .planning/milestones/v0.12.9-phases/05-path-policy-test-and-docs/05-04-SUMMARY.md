---
phase: 05-path-policy-test-and-docs
plan: 04
subsystem: docs
tags: [lua-runner, sandbox, changelog, agents-md, phase-gate]
status: complete
requires:
  - "05-03: DOC-01 closed (ids.sh IDS=0 FILES=0)"
  - "05-01 harness: build/phase5-check/{gate.sh,linux.sh,ids.sh,PHASE_BASE,golden-before}"
provides:
  - "LUA_DB_API_REFERENCE 'What the sandbox does not limit' bullet and the full read_csv options message"
  - "CHANGELOG [0.13.0] empty-array BREAKING entry covers quiver.metadata_from_element"
  - "Eight AGENTS.md files describe the empty-array rule, safety flags, pre-shape type errors and text-only run where each is true"
  - "Milestone final counts recorded in STATE.md"
affects: [milestone-ship]
tech-stack:
  added: []
  patterns: []
key-files:
  created: []
  modified:
    - bindings/js/src/lua-api.ts
    - CHANGELOG.md
    - AGENTS.md
    - src/AGENTS.md
    - tests/AGENTS.md
    - src/c/AGENTS.md
    - bindings/julia/AGENTS.md
    - bindings/dart/AGENTS.md
    - bindings/python/AGENTS.md
    - bindings/js/AGENTS.md
    - .planning/STATE.md
decisions:
  - "The reference's new limits bullet gets no CHANGELOG entry: it documents behaviour that already existed"
  - "The type errors without the got-<type> suffix are documented in src/AGENTS.md as pinned texts; no message changed"
metrics:
  duration: "35 min"
  completed: 2026-10-03
actuals:
  tokens: 1900
  tasks: 3
  commits: 4
---

# Phase 5 Plan 04: Docs and Phase Gate Summary

The agent-facing Lua reference now states what the sandbox does not bound: CPU, memory, wall-clock time, and globals that persist across `run()`. The CHANGELOG `[0.13.0]` records the `quiver.metadata_from_element` error-text change, with every message quoted from a probe. All eight AGENTS.md files are accurate for their areas. The full phase gate passed at `a3d57e4`: Debug, Release, all six suites, Linux GCC 13 and Clang 18/libc++, format, IDs and version. The milestone is ready to ship.

## Phase PR notes

### Link strategy (05-01)
`quiver_tests` compiles its own copy of `src/lua_runner/path_policy.cpp` and reaches `resolve_sandboxed_path` through a new sol2-free `src/lua_runner/path_policy.h`. Why this approach:
- libquiver uses hidden visibility, so the test cannot link the library's copy.
- The function only needs `Database::path()`, and `Database` is already exported (`QUIVER_API`), so nothing new goes on the ABI.
- Header-inline would have deleted `path_policy.cpp` and broken its citations. Exporting the function would have put an internal symbol on the public ABI.

The one constraint: `path_policy.cpp` must stay a one-function file. A static link would otherwise define a symbol twice. This is stated in `tests/CMakeLists.txt` and `tests/AGENTS.md`.

### SandboxedPathTest per platform

| Platform | Listed | Result |
|----------|--------|--------|
| Windows MSVC Debug | 11 | 11 passed, no skips (the symlink case ran with Developer Mode on) |
| Windows MSVC Release | 11 | 11 passed |
| Linux GCC 13.3.0 | 10 | 10 passed (the device-name case is `_WIN32`-only) |
| Linux Clang 18.1.3 / libc++ | 10 | 10 passed |

Mutations (05-01):
- Forcing the `".."` comparison false fails `DotDotEscapeIsRejected`, `NormalisedEscapeIsRejected`, `AbsolutePathOutsideIsRejected` and `SymlinkPointingOutsideIsRejected`.
- Dropping the `rel == "."` term fails `RootItselfIsRejected`.

### Planning-ID sweep (05-02, 05-03)
- **Scope:** 256 lines in 19 files under the corrected PCRE gate. Gate trail: 256/19 -> 180/6 -> 0/0. This covers older-milestone section labels (`CSV-`, `OPT-`, `CAPI-`, `QUERY-`, `JSCSV-` ...) as well as this milestone's IDs.
- **`.gitattributes`:** its reference was a real planning ID. It now names the two `LuaRunner_ReadCsv` regression tests that read `fixtures/` byte for byte.
- **Diagnostic strings:** 19 failure-diagnostic strings lost only their ID prefix or their "not yet implemented" note (42 lines). One trailing comment changed (2 lines), for `RESIDUAL=44` in total. No assert condition, compared value or test name changed: `gate.sh` list identity confirms it.
- **Flush comments:** corrected in the write_csv tests. The close-at-exit flush runs when `run()` returns, and that is after the same script's `db:read_csv`. That is why the explicit `w:close()` in those tests is load-bearing.

### Docs changes in this plan
- **`bindings/js/src/lua-api.ts`:**
  - New "What the sandbox does not limit" bullet, right after "Filesystem sandbox". It says there is no instruction-count limit, no memory cap and no wall-clock timeout, that globals persist across `run()` (use `local`), and that hosts must impose those limits themselves.
  - The `read_csv` options paragraph now quotes `Cannot read_csv: options must be a table, got string`.
  - The maintainer header points to the "Filesystem sandbox" bullet, which lists all ten operations, instead of carrying its own five-operation list.
  - The `## CSV file writing` worked example is untouched.
- **`CHANGELOG.md`:** one sentence was added to the empty-array BREAKING entry (see below). Nothing else changed: no 0.12.9 backfill, the compare link still uses `v0.12.9`, and there is no rename entry.
- **`AGENTS.md`:**
  - The CRUD bullet states the empty-array rule, with its core pins `Database.UpdateElementEmptyArrayClearsRows` and `Database.CreateElementWithEmptyArraySkipsSilently`, and notes that Lua passes an empty array through since 0.13.0.
  - The sol2 dependency entry names `SOL_ALL_SAFETIES_ON` / `SOL_PRINT_ERRORS=0` and says the checked getter and the stack check are off for cost.
  - The sandbox design decision is re-wrapped to at most 100 columns (byte count), with its words unchanged (the `tr` diff is empty).
- **`src/AGENTS.md`:** lists the value checks that predate the `got <type>` shape and keep their pinned texts, each confirmed by `git grep -F`:
  - `on_row must be a function`: `src/lua_runner/csv.cpp:351`
  - `option 'header' entry must be a string`: `csv.cpp:159`; `option 'separator' must be a string`: `csv.cpp:173`; `option 'header_row' must be an integer`: `csv.cpp:241`
  - `keys of option '<what>' must be strings`: `db_core.cpp:21`; `option 'date_time_format' must be a string`: `db_core.cpp:39`
  - `option key must be a string`: `internal.h:271`
  - `<what> has unsupported Lua type`: `internal.h:131`, `internal.h:155`; `target_label has unsupported Lua type`: `db_write.cpp:189`
- **`tests/AGENTS.md`:** `_update` pins the empty-array rule (`UpdateElementEmptyArrayClearsGroup`, `UpdateElementEmptyArrayErrors`, `UpdateElementEmptyArrayClearsEveryGroupSharingTheColumn`), and `_create` pins the skip (`CreateElementSkipsEmptyArray`). The mixed-array bullet already said the stack check is off (fixed in 05-02), so it needed no change.
- **`src/c/AGENTS.md`:** the `lua_runner.cpp` entry now says `run` passes the script through unchanged, so a bytecode chunk fails with `Failed to run Lua script: ... attempt to load a binary chunk (mode is 't')`, reported through `quiver_get_last_error`.
- **`bindings/{julia,dart,python,js}/AGENTS.md`:** each `run` bullet now says the script must be Lua source text, and that a bytecode chunk is rejected with `Failed to run Lua script: ...` like any other script error.

### CHANGELOG addition and its probe
Probe: `build/phase5-check/probe-in03.txt`, produced by `quiver_cli` at `b5f245f`. Each case sets every other required field:
```
{"baseline":"OK","both_empty":"Number of dimensions must be positive, got 0","dimension_sizes_empty":"Cannot from_element: dimension_sizes count (0) does not match dimensions count (1)","dimensions_empty":"Cannot from_element: dimension_sizes count (1) does not match dimensions count (0)","frequencies_empty":"OK","labels_empty":"Number of labels must be positive, got 0","read_csv_positional":"Cannot read_csv: options must be a table, got string","time_dimensions_empty":"OK"}
```
The sentence added to the empty-array BREAKING entry says:
- An empty `dimensions`, `dimension_sizes` or `labels` no longer reports `Cannot from_element: missing array '<name>'`. It now reports the validation error, and each of those texts is quoted verbatim from the probe.
- An empty `time_dimensions` or `frequencies` still means the same as leaving it out.

### Gate lines (all at `a3d57e4`, the commit holding the binding clauses)
- `bash build/phase5-check/gate.sh` -> `P5 GATE PASS lua=477 sandboxed=11 capi=27 tests=1454 c=543`. No SKIPPED line in `SandboxedPathTest`, and the test-list identity holds.
- `bash build/fixes-check/wave_gate.sh` -> `WAVE GATE PASS lua=477 tidy=14`:
  - Release `Lua*` 477 and C API 27 passed, and the output includes `GOLDEN release OK`.
  - Tidy reports the same 14 baseline warnings.
  - The GCC 14 syntax pass is clean.
- `build/release/bin/quiver_tests.exe --gtest_filter='SandboxedPathTest.*'` -> `[  PASSED  ] 11 tests.`
- `bash build/fixes-check/golden.sh debug` -> `GOLDEN debug OK`, and `diff -r build/phase5-check/golden-before build/fixes-check/out/debug` is empty.
- Six suites: `bindings/dart/.dart_tool/hooks_runner` and `.dart_tool/lib` were deleted and confirmed absent before the run. `test-all.bat` then exited 0 with 6 `tests: PASS` lines (C++, C API, Julia, Dart, JavaScript, Python).
- `bun test test/lua-api-sync.test.ts`: 6 pass. `bunx biome check src/lua-api.ts`: clean.
- clang-format 22.1.8 `--dry-run --Werror` passes on all 17 `.cpp`/`.h` files changed since `PHASE_BASE` (`737b6af`).
- `ids.sh` -> `IDS=0 FILES=0`, and the `[0.13.0]` CHANGELOG section has 0 `ids.regex` matches.
- `uv run python scripts/assert_version.py` -> `All project files at 0.13.0`. The five manifests are unchanged since `PHASE_BASE`.
- `git diff PHASE_BASE HEAD -- src/lua_runner/path_policy.cpp` changes 2 lines: the include swap, plus the blank line clang-format requires.
- Linux (`git archive HEAD` at `a3d57e4`):
  - GCC 13.3.0 and Clang 18.1.3/libc++ both show `LISTED=475`, `[  PASSED  ] 474 tests.` with `[  SKIPPED ] 1 test.`, `SANDBOXED=10` with `[  PASSED  ] 10 tests.`, C API `[  PASSED  ] 27 tests.`, and `exit=0`.
  - The build warnings in those logs are outside this phase's scope: `-Wreturn-type` in `src/binary/time_properties.cpp` on GCC, and `-Wself-assign-overloaded` in `tests/test_migrations.cpp` on Clang. Neither file was touched in Phase 5.

### Final counts
At `a3d57e4`:

| Count | Value |
|-------|-------|
| `Lua*` (Windows Debug and Release) | 477 tests / 12 suites |
| `SandboxedPathTest` | 11 on Windows (no skips); 10 on Linux GCC 13 and Clang 18/libc++ |
| Linux `Lua*` | 475 run (474 pass + 1 root skip) |
| `LuaRunnerCApiTest` | 27 |
| Full `quiver_tests` / `quiver_c_tests` (Windows Debug) | 1454 / 543 |
| Six suites | green |
| Planning-ID gate | 0 |
| Version | 0.13.0 |

## Commits

| Task | Commit | Files |
|------|--------|-------|
| 1 (tracer) | `b4ac79a` | `bindings/js/src/lua-api.ts`, `CHANGELOG.md` |
| 2 | `71706bc` | `AGENTS.md`, `src/AGENTS.md`, `tests/AGENTS.md`, `src/c/AGENTS.md` |
| 3 (binding clauses) | `a3d57e4` | the four `bindings/*/AGENTS.md` |
| 3 (counts) | `676c838` | `.planning/STATE.md` |

Tracer gate: auto mode is off. Following the user's standing rule (verify gates yourself rather than asking), I re-ran Task 1's full verify after its commit and it passed, so the plan continued.

## Deviations from Plan

- **Wrap measured in bytes.** The verify's `awk length` counts bytes under MSYS, so each em dash counts as 3. The re-wrap therefore targets at most 100 bytes per line, which comes out slightly under 100 characters on lines with an em dash. The words are unchanged.
- **One read_csv line re-wrapped.** The longer quoted message pushed a line in the `read_csv` options paragraph to about 110 columns, so that sentence was re-wrapped within its paragraph. No other text moved.

Otherwise the plan ran as written.

## Known Stubs

None.

## Self-Check: PASSED

- FOUND: build/phase5-check/probe-in03.txt, test-all.txt, linux_gcc.txt, linux_clang.txt, wave-gate-final.txt
- FOUND commits: b4ac79a, 71706bc, a3d57e4, 676c838
- `git status --porcelain` lists only ` M .planning/config.json` and `?? .gsd/`. No path under either appears in `git log --name-only PHASE_BASE..HEAD`.
