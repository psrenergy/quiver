---
phase: 02-mechanical-split
plan: 01
subsystem: lua-runner
status: complete
tags: [lua, sol2, cmake, refactor, sync-test]
requires: [phase-01 lifecycle pins]
provides: [src/lua_runner/ folder, target-wide /bigobj, folder-reading sync test, planning-ID-free lua_runner.cpp]
affects: [02-02, 02-03]
tech-stack:
  added: []
  patterns: [sync test parses every .cpp/.h under src/lua_runner/ in sorted order, usertype reset per file]
key-files:
  created: [src/lua_runner/lua_runner.cpp]
  modified: [src/CMakeLists.txt, bindings/js/test/lua-api-sync.test.ts]
decisions:
  - "/bigobj (MSVC) and -Wa,-mbig-obj (other WIN32) are target-wide on quiver, so no new sol2 TU can miss them"
  - "NOLINT pairs renamed to performance-unnecessary-value-param in the comment-only commit; the old name suppressed nothing"
metrics:
  duration: 20min
  completed: 2026-10-03
actuals:
  tokens: 9600
  tasks: 2
  commits: 2
---

# Phase 2 Plan 01: Move lua_runner.cpp into src/lua_runner/ Summary

`src/lua_runner.cpp` is now `src/lua_runner/lua_runner.cpp` (100% rename, history intact). CMake sets `/bigobj` for the whole `quiver` target, the JS sync test parses the whole folder, and the moved file's comments carry no planning IDs. Its four NOLINT pairs now name a check clang-tidy knows.

## Commits

| Task | Commit | Files |
|------|--------|-------|
| 1 (tracer): git mv + CMake path + target-wide /bigobj + folder-reading sync test | 32ebdcc | src/lua_runner.cpp -> src/lua_runner/lua_runner.cpp, src/CMakeLists.txt, bindings/js/test/lua-api-sync.test.ts |
| 2: comment-only planning-ID strip + NOLINT check name | cfb37d4 | src/lua_runner/lua_runner.cpp |

## Build time (before)

Pre-split clean build of the `quiver` target (Debug, Ninja, `build/` tree, measured with the RESEARCH protocol): **70 s wall clock**. Slowest TU: `lua_runner.cpp.obj` at **60.87 s**, which is the critical path. Next: `database_update.cpp.obj` 24.04 s, `csv/csv_read.cpp.obj` 23.84 s.

## Surface baseline

`build/split-check/before.txt` (from `bab557e:src/lua_runner.cpp`) has 107 entries: 71 `db`, 15 `quiver`, 21 usertype methods. After both commits, `surface.ts src/lua_runner` gives the identical list (empty diff).

## Per-commit checks (observed)

| After | Lua* listed / suites / passed | LuaRunnerCApiTest | sync test | surface diff |
|-------|-------------------------------|-------------------|-----------|--------------|
| 32ebdcc | 444 / 12 / 444 passed | 27 passed | 6 pass | empty |
| cfb37d4 | 444 / 12 / 444 passed | 27 passed | 6 pass | empty |

## Tracer gate

Commit 1 was verified by me instead of a human checkpoint (per the user's self-verify preference). I mutation-checked the folder parse. A stray `zz.txt` holding `bind.set_function("bogus_x", ...)` was ignored and the test still passed 6/6. The same line in `zz.h` was parsed and failed "no documented db:/quiver. name has been removed" (5 pass, 1 fail). I removed the file afterwards. A missing folder throws ENOENT from `readdirSync`.

## Task 2 details

- 59 matching comment lines. 53 were rewritten in place: the token was dropped where the sentence already gives the reason, or replaced with the reason. The 6 `// Group N:` dividers were deleted. The file went from 2539 to 2533 lines.
- The `db:read_csv` header comment said the empty-header guard was "forward-looking". That is stale now that `header_row = 0` exists. It now says why the key is absent rather than `{}` (truthiness), and that the stream form must agree.
- One rewrite was wrapped by clang-format: a test name at the end of the int64 cell comment, which is CSV code. I dropped the test name to keep the CSV code line-neutral (RESEARCH Pitfall 2).
- The diff gate printed nothing (no non-comment line changed). `clang-format 22.1.8 --dry-run --Werror` is clean. `new_usertype<` = 5 and `open_libraries(` = 1, both unchanged.

## Deviations from Plan

None. The plan ran as written. The `git show -M --summary` output reads `rename src/{ => lua_runner}/lua_runner.cpp (100%)`, which is git's spelling of the same rename. The plan's grep literal spelled it differently, so I checked the 100% similarity by eye.

## Known Stubs

None.

## Self-Check: PASSED

- FOUND: src/lua_runner/lua_runner.cpp; src/lua_runner.cpp absent
- FOUND: 32ebdcc, cfb37d4
- `git log --follow` on the new path reaches e7c6358
