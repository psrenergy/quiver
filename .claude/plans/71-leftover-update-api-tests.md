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

- [x] The listed leftovers are gone, and every covering test still exists and passes.
- [x] The two NullAttribute tests remain for plan 70.
- [x] Both suites are green.

## Pitfalls

- Line numbers drift as tests are deleted. Delete bottom-up within a file, or anchor on test names.

## Out of scope

- Replacing the NullAttribute/NullElement tests (plan 70).
- Leak fixes in tests that survive (plan 69).

## Implementation notes

Implemented on `rs/plan71`. Merging `origin/master` fast-forwarded from `6fb45ba` (plan 68) to
`01948d8`, which brought in plans 65, 66, 67 and 69. Plan 70 had not landed. Tests only, in four
files: `tests/test_c_api_database_{update,lifecycle}.cpp` and
`tests/test_database_{update,errors}.cpp`.

**Drift:**
- Line numbers moved by about 20 to 25. I re-anchored every test by name. All the test names and
  their bodies matched the plan.
- Plan 69 had already fixed the leaks and `delete[]` frees in `UpdateScalarInteger/Float/String`,
  `UpdateElementNoFkColumnsUnchanged` (C API) and the two lifecycle `if (ids != nullptr)` guards.
  Its header expected this: "if 69 lands first, 71 deletes the fixed tests". I deleted the fixed
  versions.
- Plan 67 had already removed the lifecycle file's `Describe tests` section. The deleted lifecycle
  range therefore runs from the `Element ID operations` banner to the end of the file.
- **NULL-db / NULL-collection:** the plan says to keep one of the 12 of each kind and rename it
  `UpdateElementNullDb` / `UpdateElementNullCollection`. That coverage already exists in
  `DatabaseCApi.UpdateElementNullArguments`, which checks null db, null collection and null
  element. `QUIVER_REQUIRE(db, collection, element)` is the first statement of
  `quiver_database_update_element`, so what the element holds never matters. I deleted all 12 and
  renamed none; a renamed copy would have duplicated `UpdateElementNullArguments`. The lifecycle
  tests the plan deletes were already named `UpdateElementNullDb/NullCollection/NullElement`.
- `test_database_update.cpp`: deleting `UpdateVectorIntegersInvalidColumnThrows` left the
  `// Identifier validation tests` banner over only the two whitespace-trimming tests. I renamed
  it `// Whitespace trimming tests`, the banner the C API file uses for the same two tests.

**Deleted (33), each with the test that covers it:**
- C API update (16):
  - `UpdateScalarInteger/Float/String` and their banner: covered by `UpdateElementSingleScalar` /
    `UpdateElementMultipleScalars`.
  - The 12 `Update{Vector,Set}{Integers,Floats,Strings}Null{Db,Collection}`: covered by
    `UpdateElementNullArguments`.
  - `UpdateElementNoFkColumnsUnchanged`: covered by `UpdateElementMultipleScalars`.
- C API lifecycle (10):
  - `ReadElementIdsNullDb/NullCollection/NullOutput`: same-named tests in
    `test_c_api_database_read_scalar.cpp`.
  - `ReadElementIdsValid`: covered by `ReadElementIds`.
  - `DeleteElementNullDb/NullCollection`: covered by `DeleteElementByIdNullArguments`.
  - `DeleteElementValid`: covered by `DeleteElementById`.
  - `UpdateElementNullDb/NullCollection/NullElement`: covered by `UpdateElementNullArguments`.
- C++ update (4):
  - `UpdateVectorInvalidCollection` and `UpdateSetInvalidCollection`: covered by
    `DatabaseErrors.UpdateElementCollectionNotFound`. `Database::update_element` calls
    `require_collection` on its first line, before it reads the element.
  - `UpdateVectorIntegersInvalidColumnThrows`: covered by `UpdateElementInvalidArrayAttribute`.
  - `UpdateElementNoFkColumnsUnchanged`: covered by `UpdateElementMultipleScalars`.
- C++ errors (3): `Update{VectorIntegers,VectorFloats,SetStrings}CollectionNotFound` and the two
  `Update vector/set error tests` banners, covered by `UpdateElementCollectionNotFound`.

**Kept for plan 70:** `UpdateVectorIntegersNullAttribute` and `UpdateSetStringsNullAttribute`,
with their `Update vector/set null pointer tests` banners. I left those banners alone so the merge
stays small. Once plan 70 deletes the two tests, delete the two banners too, or they will be empty.

**Results:**
- `quiver_tests.exe`: 1401 → 1394 (−7), all passing.
- `quiver_c_tests.exe`: 572 → 546 (−26), all passing.
- The filtered runs of every covering test passed (12 C API, 3 C++).
- A read-only adversarial pass (three agents, one per file group, each told to show that a deleted
  test asserted something no remaining test checks) found nothing uncovered and no seam problems.
- No other file names any deleted test, apart from the plan files. `tests/AGENTS.md` neither
  counts tests nor describes these files' contents, so it is unchanged. No CHANGELOG entry.

**For plan 70:** this branch deletes the 12 NullDb/NullCollection tests on both sides of the two
NullAttribute tests, so the merge will conflict there. Resolve it by keeping both deletions; both
plans want those regions gone.
