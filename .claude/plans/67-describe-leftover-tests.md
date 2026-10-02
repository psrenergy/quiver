# 67 — Delete the leftover "describe does not throw" tests; move the C++ describe-content tests

**Batch** 7 · **Severity** low · **Breaking** no · **Size** S · **Layers** tests only (C++, C API, Julia, Dart, Python, JS)
**Depends on** none · **Overlaps with** 66 (other edits in the same lifecycle test files), 30 (plan 30 also deletes the Python `test_describe_runs_without_error`; skip that deletion here if 30 already made it)

## Why

`describe()` used to print to stdout. It now returns a string. Every binding has a dedicated
describe suite that asserts the string: `test_database_describe.jl`, `describe_test.dart`,
`test_database_metadata.py::TestDescribe` and `database-describe.test.ts`. Each binding also keeps an
older copy in its lifecycle file that asserts nothing useful:

- Julia `bindings/julia/test/test_database_lifecycle.jl` (~L114-123), `@testset "Describe"`:
  `# Just verify describe runs without error` / `Quiver.describe(db)` / `@test true`.
- Dart `bindings/dart/test/database_lifecycle_test.dart` (~L220-229), `group('Database describe', ...)`
  / `test('describe does not throw', ...)`.
- Python `bindings/python/tests/test_database_lifecycle.py` (~L91-92),
  `test_describe_runs_without_error`, with a comment that is now wrong:
  `db.describe()  # Should not raise; output goes to stdout`.
- JS `bindings/js/test/introspection.test.ts` (~L40-47), `test("describe runs without error", ...)`.

In C++, `tests/test_database_lifecycle.cpp` still has
`static std::string capture_describe(const quiver::Database& db) { return db.describe(); }`
(~L392). It is an identity wrapper whose name only made sense for the old ostream API. It is used by
five describe-content tests that live under an unrelated `TempFileFixture` (they open `:memory:`).
`DescribeDoesNotThrow` (~L384) duplicates `DatabaseDescribe.WholeDatabaseReport` in
`tests/test_database_describe.cpp`. The C API has the same pattern:
`TempFileFixture, DescribeDoesNotFail` in `tests/test_c_api_database_lifecycle.cpp`. Its describe
coverage already lives in `test_c_api_database_metadata.cpp`, ~L175-215.

Principle: delete duplicated and vacuous tests.

## Constraints and decisions

- Delete only the duplicates. The dedicated describe suites stay as they are.
- Move the five C++ content tests, don't delete them. They check real report structure (headers
  printed once, column order).
- Keep `TempFileFixture` in `test_database_lifecycle.cpp`, because genuine file-backed lifecycle
  tests still use it.

## Changes (tests only)

1. **Julia**: delete `@testset "Describe" ... end` from `test_database_lifecycle.jl`.
2. **Dart**: delete `group('Database describe', () { ... });` from `database_lifecycle_test.dart`.
3. **Python**: delete `test_describe_runs_without_error` from `test_database_lifecycle.py`, unless
   plan 30 already removed it.
4. **JS**: delete `test("describe runs without error", ...)` from `introspection.test.ts`.
5. **C++** `tests/test_database_lifecycle.cpp`:
   - Delete `DescribeDoesNotThrow`, `capture_describe`, the `// Describe tests` banner and
     `#include <sstream>` if nothing else uses it (`grep -n "stringstream\|ostringstream" tests/test_database_lifecycle.cpp`).
   - Move the five describe-content tests into `tests/test_database_describe.cpp` as plain
     `TEST(DatabaseDescribe, ...)`. Use that file's existing `open(VALID_SCHEMA(...))` helper and call
     `db.describe()` directly. Rename them without "Printed", e.g. `VectorsHeaderAppearsOnce`,
     `SetsHeaderAppearsOnce`, `TimeSeriesHeaderAppearsOnceWithBracketedDimension`,
     `ScalarOrderMatchesSchema`, `NoCategoryHeaderWhenEmpty`. Map each old name to its new one in
     the commit message. Find them with `grep -n "capture_describe" tests/test_database_lifecycle.cpp`.
6. **C API** `tests/test_c_api_database_lifecycle.cpp`: delete `TEST_F(TempFileFixture, DescribeDoesNotFail)`.
   Confirm first that `test_c_api_database_metadata.cpp` asserts `quiver_database_describe` returns
   a non-empty string (`grep -n "quiver_database_describe(" tests/test_c_api_database_metadata.cpp`).

## Tests

This plan only moves and deletes tests. After it, run every suite. The moved tests must pass under
their new names.

## Docs and changelog

- `tests/AGENTS.md`: if it lists the describe tests' location
  (`grep -n "describe" tests/AGENTS.md`), point it at `test_database_describe.cpp`. Plan 75 owns the
  rest of that file.
- No CHANGELOG entry.

## Verification

From the repo root:
1. `cmake --build build --config Debug`
2. `./build/bin/quiver_tests.exe --gtest_filter=DatabaseDescribe*:*Lifecycle*`
3. `./build/bin/quiver_c_tests.exe`
4. `scripts/test-all.bat`

## Acceptance criteria

- [x] No "describe runs/does not throw" test remains in any lifecycle file.
- [x] `capture_describe` is gone, and the five content tests pass in `test_database_describe.cpp`.
- [x] All suites green.

## Pitfalls

- The moved tests may rely on `TempFileFixture` members. Replace them with the describe file's
  `open(...)` helper, which opens `:memory:`.

## Out of scope

- Adding new describe assertions.

## Implementation notes

- **Landed as written, with small drift.** `rs/plan67` was already level with `master` (merge
  was a no-op). Every excerpt matched, give or take a line.
- **Python step skipped.** Plan 30 had already deleted `test_describe_runs_without_error`
  (`636ccb9`), as the header allowed.
- **Drift fixed.**
  - The plan's filter `*Lifecycle*` matches no C++ lifecycle test, because they are all
    `TempFileFixture.*`. I ran `--gtest_filter=DatabaseDescribe*:TempFileFixture*` instead.
  - The C API `// Describe tests` banner went with `DescribeDoesNotFail`, since it would have
    been orphaned.
  - `#include <sstream>` had no user in `test_database_lifecycle.cpp`, so it is gone.
- **Nothing was lost by the C API deletion.** `DescribeDoesNotFail` checked the
  `Database: :memory:` header. That line is still pinned in the core by
  `DatabaseDescribe.WholeDatabaseReport`, and `DatabaseCApiMetadata.DescribeReturnsText` still
  covers the C ABI marshalling.
- **The moved tests are unchanged.** Only the setup differs: `open(VALID_SCHEMA(...)).describe()`
  replaces the fixture plus `capture_describe`. Renames:
  - `DescribeVectorsHeaderPrintedOnce` → `VectorsHeaderAppearsOnce`
  - `DescribeSetsHeaderPrintedOnce` → `SetsHeaderAppearsOnce`
  - `DescribeTimeSeriesWithDimensionColumn` → `TimeSeriesHeaderAppearsOnceWithBracketedDimension`
  - `DescribeColumnOrderMatchesSchema` → `ScalarOrderMatchesSchema`
  - `DescribeNoCategoryHeaderWhenEmpty` → `NoCategoryHeaderWhenEmpty`
- **No CHANGELOG, AGENTS.md or FFI change.** `tests/AGENTS.md` never placed describe tests in the
  lifecycle file. Its line listing `test_database_describe.cpp` is already right.
- **Verification.**
  - Filtered C++ run: 30/30 passed.
  - `scripts/test-all.bat`: all six suites PASS.
    - C++: 1401. That is 1402 at plan 64, minus `DescribeDoesNotThrow`.
    - C API: 571. That is 572 minus `DescribeDoesNotFail`.
    - Julia: 1574. Dart: 444. JS: 241. Python: 350.
  - `scripts/format.bat` exited 0.
  - The acceptance grep prints nothing.
- **For later plans.**
  - Plan 66 edits the same lifecycle files, so expect a trivial merge conflict there.
  - Biome again rewrote 42 untouched CRLF JS files to LF (no content diff). I reverted them.
  - The Python step of `format.bat` (`uv sync`) rebuilt the wheel, which took 18 minutes while
    other sessions ran.
  - The first `test-all.bat` run was killed by Claude Code under memory pressure from the
    parallel sessions. The re-run passed.
  - `scripts/test-all.bat` now runs only the six suites, with no CLI smoke step. Plan 65's premise
    (step 7 runs the deleted `example1.lua`) may already be stale, so check before executing it.
