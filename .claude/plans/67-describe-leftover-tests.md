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

- `tests/CLAUDE.md`: if it lists the describe tests' location
  (`grep -n "describe" tests/CLAUDE.md`), point it at `test_database_describe.cpp`. Plan 75 owns the
  rest of that file.
- No CHANGELOG entry.

## Verification

From the repo root:
1. `cmake --build build --config Debug`
2. `./build/bin/quiver_tests.exe --gtest_filter=DatabaseDescribe*:*Lifecycle*`
3. `./build/bin/quiver_c_tests.exe`
4. `scripts/test-all.bat`

## Acceptance criteria

- [ ] No "describe runs/does not throw" test remains in any lifecycle file.
- [ ] `capture_describe` is gone, and the five content tests pass in `test_database_describe.cpp`.
- [ ] All suites green.

## Pitfalls

- The moved tests may rely on `TempFileFixture` members. Replace them with the describe file's
  `open(...)` helper, which opens `:memory:`.

## Out of scope

- Adding new describe assertions.
