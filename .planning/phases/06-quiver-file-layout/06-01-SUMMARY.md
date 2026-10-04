---
phase: 06-quiver-file-layout
plan: 01
subsystem: lua_runner
status: complete
tags: [lua, refactor, file-layout, sol2, harness]
requires: []
provides:
  - build/layout-check harness and base baselines (gitignored)
  - src/lua_runner/database_describe.cpp (bind_describe)
  - src/lua_runner/database_read.cpp, database_metadata.cpp, database_time_series.cpp (git mv)
  - shared list_metadata_lua / get_metadata_lua templates in internal.h
affects: [06-02, 06-03]
tech-stack:
  added: []
  patterns: [rename-only commit before content edits, per-commit behaviour gate against a frozen base]
key-files:
  created:
    - src/lua_runner/database_describe.cpp
  modified:
    - src/CMakeLists.txt
    - src/lua_runner/internal.h
    - src/lua_runner/lua_runner.cpp
    - src/lua_runner/db_core.cpp
    - src/lua_runner/database_read.cpp
    - src/lua_runner/database_metadata.cpp
    - src/lua_runner/database_time_series.cpp
decisions:
  - "Tidy file filter is 'src.lua_runner.' instead of 'src[\\\\/]lua_runner[\\\\/]': through uv on Git Bash the doubled backslash arrives single, so the old regex matched 0 files"
  - "Tracer feedback gate self-verified (gate.sh + golden release green on the committed tree), per human_verify_mode end-of-phase and the self-verify memory"
  - "Task 3 landed as one commit (number_of_elements and the time-series metadata pair together)"
metrics:
  duration: 20min
  completed: 2026-10-04
actuals:
  tokens: 4300
  tasks: 3
  commits: 3
---

# Phase 6 Plan 01: Layout harness, describe tracer and history-preserving renames Summary

Built the `build/layout-check` per-commit gate with a runtime metatable surface probe, froze every baseline at the base, then moved the describe trio into `database_describe.cpp`, renamed the read/metadata/time-series binders with a pure `git mv` commit, and moved `number_of_elements` and the time-series metadata pair beside their core files with the metadata templates shared through `internal.h`.

## Base

- BASE = `21ba6f89fdac3c18e814d24e7015c1bd4a9e8e7e` (sources identical to `da6f67b`: precondition `git diff --quiet da6f67b HEAD -- src include tests cmake CMakeLists.txt bindings` exited 0).
- Both trees rebuilt before capture (Debug `build/`, Release `build/release/`).
- Base counts, identical in Debug and Release: `Lua*` 477 in 12 suites, `SandboxedPathTest*` 11, `LuaRunnerCApiTest*` 27, full `quiver_tests` 1454, full `quiver_c_tests` 543.
- `gate.sh` on the unchanged tree: `GATE PASS lua=477 pairs=36`; `golden.sh release`: `GOLDEN release OK`.

## Baselines (build/layout-check, gitignored, never recaptured)

| File | Lines |
|---|---|
| `baseline/debug/*.txt` | 11 probes (10 copied + `surface.txt`, 4090 bytes, has `__lt:function`) + `baseline/debug_text.txt` |
| `names-base.txt` | 86 |
| `surface-base.txt` | 107 |
| `sandbox-base.txt` | 10 |
| `grep24-base.txt` | 24 |
| `expected-placement.txt` | 86 (covers exactly the base names) |
| `tidy-base.txt` | 1376 (10 files; 14 unique `src/lua_runner` warnings, 0 `clang-diagnostic-error`) |
| `tidy-pairs-base.txt` | 14 |

Scripts: `golden.sh`, `gate.sh`, `tidy_pairs.sh`, `prelude.lua`, `debug_text.lua`, `scripts/*.lua` (10 copied + `surface.lua`), `BASE`.

The golden baseline differs from the old `build/fixes-check/baseline` in core/csv/handles_run1 (that one predates the Phase 4/5 review fixes); Release matches the new Debug baseline byte for byte, which is the comparison that matters.

## The 14 tidy pairs (check, source line)

```
bugprone-empty-catch                                 } catch (...) {
bugprone-empty-catch                                 } catch (const std::exception&) {
bugprone-empty-catch                                 } catch (const std::exception&) {
bugprone-implicit-widening-of-multiplication-result  constexpr std::size_t kMaxReturnBytes = 64UL * 1024UL * 1024UL;
bugprone-unchecked-optional-access                   t["frequency"] = frequency_to_string(dim.time->frequency);
bugprone-unchecked-optional-access                   t["initial_value"] = dim.time->initial_value;
bugprone-unchecked-optional-access                   t["parent_dimension_index"] = dim.time->parent_dimension_index;
modernize-raw-string-literal                         throw std::runtime_error("Cannot open_file: mode must be \"r\" or \"w\"");
modernize-return-braced-init-list                    return Expression(o.as<BinaryFile&>());
readability-identifier-naming                        CsvWriter(std::shared_ptr<quiver::csv_write::Writer> w, std::size_t header_width_)
readability-identifier-naming                        constexpr int kMaxReturnDepth = 32;
readability-identifier-naming                        constexpr std::int64_t kMaxWidth = 1'000'000;
readability-identifier-naming                        constexpr std::size_t kMaxReturnBytes = 64UL * 1024UL * 1024UL;
readability-identifier-naming                        static constexpr char kHex[] = "0123456789abcdef";
```

## Commits

| Task | Commit | Gate |
|---|---|---|
| 1 (tracer) describe trio -> `database_describe.cpp` | `8fb84cc` | `GATE PASS lua=477 pairs=36`, novel lines 2; Release rebuilt, `GOLDEN release OK`, Release `Lua*` 477 pass |
| 2 rename-only (`git mv` x3 + 3 CMake lines) | `818eb95` | `GATE PASS lua=477 pairs=36`; 3 renames at 100%, 4 files changed; `git log --follow` reaches `da6f67b` for all three |
| 3 `number_of_elements` -> `database_read.cpp`; time-series metadata pair -> `database_time_series.cpp`; templates -> `internal.h` | `e0b4b1d` | `GATE PASS lua=477 pairs=36`, novel lines 10; GCC 14 `-fsyntax-only` (docker) clean on database_{metadata,time_series,read,describe}.cpp |

Registrations per file after the plan: `database_describe.cpp` 3, `database_read.cpp` 15, `database_metadata.cpp` 6, `database_time_series.cpp` 12, `db_core.cpp` 18. Totals unchanged: 86 = 71 `bind` + 15 `ns`.

Novel lines (informational; added lines matching no removed line) at e0b4b1d: 10. They are the new TU's includes/namespace/binder header, the `bind_describe` declaration and call, and the two `metadata_to_lua` declarations. None is logic.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Tidy file regex matched 0 files**
- **Found during:** Task 1, base tidy capture
- **Issue:** `run-clang-tidy ... 'src[\\/]lua_runner[\\/]'` through `uv run python` on Git Bash reported "0 files out of 198": the argument arrived as `src[\/]lua_runner[\/]` (one backslash), so the class matched only `/` while the compile-database paths are absolutized with `\`.
- **Fix:** used the file regex `src.lua_runner.` (same 10 files; does not match `src/c/lua_runner.cpp` or the tests). Plan 06-03's tidy step should use the same regex.
- **Files modified:** none in the repo (`build/layout-check/tidy-base.txt`)

**2. Tracer feedback gate self-verified instead of returning a checkpoint**
- auto_advance is false, but `human_verify_mode` is `end-of-phase`, the tracer's verify is fully automated, and the user's standing instruction is to self-verify gates. The tracer verify (gate.sh + golden release) was green on the exact committed tree before expansion.

**3. Requirements left Pending.** `requirements.mark-complete` was run and reverted: LAYOUT-01..04 span all three plans (8 of 14 binder files, docs and the phase gate are still to come), so they are marked when 06-03 closes the phase.

No source deviation: every move is verbatim, no test, baseline or pinned count changed, no GOLDEN_CHANGE/DEBUG_TEXT_CHANGE run.

## Self-Check: PASSED

- FOUND: src/lua_runner/database_describe.cpp, database_read.cpp, database_metadata.cpp, database_time_series.cpp
- FOUND: build/layout-check/gate.sh, tidy_pairs.sh, scripts/surface.lua, all baselines
- FOUND commits: 8fb84cc, 818eb95, e0b4b1d
