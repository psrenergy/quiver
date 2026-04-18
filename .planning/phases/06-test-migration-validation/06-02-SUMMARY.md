---
phase: 06-test-migration-validation
plan: 02
subsystem: js-bindings-tests
tags: [deno, test-migration, vitest-removal, filesystem-apis]
dependency_graph:
  requires: [06-01]
  provides: [migrated-test-suite-6-files]
  affects: [bindings/js/test/]
tech_stack:
  added: ["jsr:@std/assert", "jsr:@std/path"]
  patterns: [Deno.test/t.step, sanitizeResources-false, import.meta.dirname]
key_files:
  created: []
  modified:
    - bindings/js/test/database.test.ts
    - bindings/js/test/csv.test.ts
    - bindings/js/test/metadata.test.ts
    - bindings/js/test/time-series.test.ts
    - bindings/js/test/composites.test.ts
    - bindings/js/test/lua-runner.test.ts
    - bindings/js/deno.lock
decisions:
  - "Used sanitizeResources: false on all Deno.test blocks because Deno.dlopen singleton is loaded once and never closed"
  - "Replaced assertInstanceOf(db, Database) with assert(db instanceof Database) because Database has private constructor incompatible with @std/assert type constraint"
  - "Used Deno.UnsafePointerView.getCString() for quiver_version pointer return in Library loading test"
metrics:
  duration: 11m
  completed: 2026-04-18
  tasks: 3
  files: 7
---

# Phase 06 Plan 02: Migrate Remaining 6 Test Files to Deno.test Summary

Migrated database.test.ts, csv.test.ts (both with filesystem operations), and 4 standard test files (metadata, time-series, composites, lua-runner) from vitest to Deno.test with @std/assert assertions.

## What Changed

### Task 1: database.test.ts and csv.test.ts (filesystem-heavy)
- Replaced `vitest` imports with `jsr:@std/assert` (assert, assertEquals, assertGreater, assertInstanceOf, assertThrows, assertStringIncludes, assertFalse)
- Replaced `node:fs` APIs: `existsSync` -> helper using `Deno.statSync`, `mkdtempSync` -> `Deno.makeTempDirSync`, `rmSync` -> `Deno.removeSync`, `readFileSync` -> `Deno.readTextFileSync`, `unlinkSync` -> `Deno.removeSync`
- Replaced `node:os` `tmpdir()` with `Deno.makeTempDirSync()`
- Replaced `node:path` with `jsr:@std/path`
- Replaced `dirname(fileURLToPath(import.meta.url))` with `import.meta.dirname!`
- Converted `describe/test/afterEach` to `Deno.test/t.step` with cleanup in finally blocks
- Fixed `.ts` extensions on `../src/index.ts` and `../src/loader.ts` imports

### Task 2: metadata.test.ts, time-series.test.ts, composites.test.ts, lua-runner.test.ts
- Applied standard vitest-to-Deno.test transformation
- `toBeCloseTo` -> `assertAlmostEquals` (composites)
- `toContain` on arrays -> `assert(array.includes())` (lua-runner)
- `toHaveLength` -> `assertEquals(arr.length, n)`
- `.not.toThrow()` -> direct call (lua-runner empty script, close idempotency)

### Task 3: deno.lock update
- Lock file updated with `jsr:@std/assert`, `jsr:@std/path`, `jsr:@std/internal` entries
- All 6 files validated: 17 test groups, 56 steps, 0 failures

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] DLL not found in worktree**
- **Found during:** Task 1
- **Issue:** Worktree is in `.claude/worktrees/` directory, loader.ts walks up from `__dirname` looking for `build/bin/` but never reaches main repo's build directory
- **Fix:** Copied libquiver.dll and libquiver_c.dll to worktree's `build/bin/` (gitignored)
- **Files modified:** None (build/ is gitignored)

**2. [Rule 1 - Bug] Deno resource sanitizer rejects FFI singleton**
- **Found during:** Task 1
- **Issue:** Deno.test detects dynamic library loaded but never closed; the library is intentionally a singleton
- **Fix:** Added `sanitizeResources: false` to all `Deno.test` blocks across all 6 files
- **Files modified:** All 6 test files

**3. [Rule 1 - Bug] assertInstanceOf type error with private constructor**
- **Found during:** Task 1
- **Issue:** `assertInstanceOf(db, Database)` fails type checking because Database has a private constructor, incompatible with `@std/assert`'s `abstract new (...args) => any` constraint
- **Fix:** Replaced with `assert(db instanceof Database)` which performs the same runtime check
- **Files modified:** bindings/js/test/database.test.ts

**4. [Rule 1 - Bug] quiver_version returns pointer in Deno FFI**
- **Found during:** Task 1
- **Issue:** In koffi, `quiver_version()` returned a string directly. In Deno FFI, it returns a `Deno.PointerValue` that must be read with `UnsafePointerView.getCString()`
- **Fix:** Added pointer-to-string conversion in the Library loading test
- **Files modified:** bindings/js/test/database.test.ts

## Decisions Made

| Decision | Rationale |
|----------|-----------|
| `sanitizeResources: false` on all tests | FFI library is loaded as singleton, never closed; this is correct behavior |
| `assert(db instanceof Database)` over `assertInstanceOf` | TypeScript type system limitation with private constructors |
| Direct `Deno.UnsafePointerView.getCString()` in version test | Test exercises raw FFI; higher-level tests go through Database class |

## Commits

| Task | Commit | Description |
|------|--------|-------------|
| 1 | 7714fe0 | Migrate database.test.ts and csv.test.ts to Deno.test |
| 2 | ec8eec2 | Migrate metadata, time-series, composites, lua-runner tests |
| 3 | 4ab5ab1 | Update deno.lock with @std/assert dependency |

## Self-Check: PASSED

All 7 files verified present. All 3 commits verified in git log.
