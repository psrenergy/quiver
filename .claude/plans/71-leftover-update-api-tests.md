# 71 — Delete the duplicate tests left from the removed per-type update API

**Batch** 7 · **Severity** medium · **Breaking** no · **Size** S · **Layers** C++ and C API tests only
**Depends on** none · **Overlaps with** 69 (leak fixes in three of the tests deleted here — whichever lands second skips them), 70 (deletes `UpdateVectorIntegersNullAttribute`/`UpdateSetStringsNullAttribute` and two others — **not** deleted here), 68 (same `test_database_errors.cpp` file)

## Why

The per-type update functions (`update_vector_integers`, `update_set_strings`, ...) were deleted in
#100 in favour of `update_element` and the group writers. The tests written for them stayed. Most
now duplicate other tests or test nothing specific.

`tests/test_c_api_database_update.cpp`:
- 12 `...NullDb` / `...NullCollection` tests (~L586-774): one per former per-type function, all now
  calling `quiver_database_update_element` with a NULL db/collection. One NULL-db and one
  NULL-collection test cover that path.
- `UpdateScalarInteger`, `UpdateScalarFloat`, `UpdateScalarString` (~L15-104): subsets of the
  multi-scalar update tests. They also leak (see plan 69).
- `UpdateElementNoFkColumnsUnchanged` (~L1353).
- `tests/test_c_api_database_lifecycle.cpp` ~L310-501: update/read copies placed in the lifecycle
  file. `CreateElementInNonExistentCollection` (~L268) is outside that range and is the only C API
  test for a missing collection on create, so keep it.

`tests/test_database_update.cpp`:
- `UpdateElementNoFkColumnsUnchanged` (~L888).
- `UpdateVectorIntegersInvalidColumnThrows` (~L628): covered by `UpdateElementInvalidArrayAttribute` (~L459).
- `UpdateVectorInvalidCollection` (~L524), `UpdateSetInvalidCollection` (~L537): covered by
  `DatabaseErrors.UpdateElementCollectionNotFound` (`tests/test_database_errors.cpp` ~L69).

`tests/test_database_errors.cpp` ~L182-233: `UpdateVectorIntegersCollectionNotFound`,
`UpdateVectorFloatsCollectionNotFound`, `UpdateSetStringsCollectionNotFound`, and their stale
section comments. `UpdateElementCollectionNotFound` covers them.

Principle: delete duplicated tests. They slow the suite and mislead about coverage.

## Constraints and decisions

- **Keep** `UpdateVectorIntegersNullAttribute` and `UpdateSetStringsNullAttribute` (~L614, ~L776).
  They go through a different path (the setter rejects a null name) and belong to plan 70's
  replacement.
- Before deleting each test, confirm the claimed covering test exists and asserts the same outcome
  (`grep -n "<covering test name>" tests/*.cpp`). If one does not, keep the leftover and rename it
  to what it tests.
- Keep one NULL-db and one NULL-collection test for `quiver_database_update_element`. If none of the
  12 is named that way, keep the first of each kind and rename it
  `UpdateElementNullDb` / `UpdateElementNullCollection`.

## Changes (tests only)

1. `tests/test_c_api_database_update.cpp`: delete the 12 NullDb/NullCollection tests (~L586-774),
   keeping one of each kind renamed as above and keeping the two NullAttribute tests. Delete
   `UpdateScalarInteger/Float/String` (~L15-104) and `UpdateElementNoFkColumnsUnchanged` (~L1353).
2. `tests/test_c_api_database_lifecycle.cpp`: delete ~L310-501. First list the test names in that
   range (`sed -n 300,505p ... | grep "TEST"`) and check that each is duplicated in the matching
   `test_c_api_database_{update,read_*}.cpp` file.
3. `tests/test_database_update.cpp`: delete `UpdateElementNoFkColumnsUnchanged`,
   `UpdateVectorIntegersInvalidColumnThrows`, `UpdateVectorInvalidCollection` and
   `UpdateSetInvalidCollection`.
4. `tests/test_database_errors.cpp`: delete the ~L182-233 family and its section comments, unless
   plan 68 already trimmed the comments.

## Tests

Deleting tests is the change. Verify coverage is intact with the covering tests named above, and
run the full suites.

## Docs and changelog

- `tests/AGENTS.md`: if it counts tests or describes these files' contents in a way that changes,
  update it. Plan 75 owns the other edits there.
- No CHANGELOG entry.

## Verification

1. `cmake --build build --config Debug`
2. `./build/bin/quiver_tests.exe` and `./build/bin/quiver_c_tests.exe`
3. Compare the test counts before and after (the gtest summary line) and write the delta in the
   commit message.

## Acceptance criteria

- [ ] The listed leftovers are gone, and every covering test still exists and passes.
- [ ] The two NullAttribute tests remain for plan 70.
- [ ] Both suites are green.

## Pitfalls

- Line numbers drift as tests are deleted. Delete bottom-up within a file, or anchor on test names.

## Out of scope

- Replacing the NullAttribute/NullElement tests (plan 70).
- Leak fixes in tests that survive (plan 69).
