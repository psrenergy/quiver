# 70 — Replace four wrong-reason update tests with one real NULL string-cell test

**Batch** 7 · **Severity** medium (the tests claim coverage that does not exist) · **Breaking** no · **Size** S · **Layers** C API tests only
**Depends on** none · **Overlaps with** 71 (deletes other leftovers in the same file and explicitly leaves `UpdateVectorIntegersNullAttribute` / `UpdateSetStringsNullAttribute` to this plan), 69 (leak fixes in the same file)

## Why

Four tests in `tests/test_c_api_database_update.cpp` are named after a NULL-handling path they
never reach:
- `UpdateVectorIntegersNullAttribute` (~L614)
- `UpdateSetStringsNullAttribute` (~L776)
- `UpdateVectorStringsNullElement` (~L922)
- `UpdateSetStringsNullElement` (~L940)

They are leftovers from the per-type update API removed in #100. NULL cells became legal later (root
design decision "Element arrays accept NULL cells": a NULL `char*` entry in a string array is SQL
NULL). Read each test body. They pass today for a different reason than their names suggest:
- the setter rejects a null *name*, which leaves an empty element; or
- the element they update does not exist.

So the actual behaviour, "a NULL `char*` entry in a string array written through `update_element`
is stored as SQL NULL", is not asserted anywhere through the element path. The group-writer path is
covered by `UpdateGroupNullStringEntryIsNull` (~L1815).

Principle: tests must test what they claim.

## Constraints and decisions

- Delete all four and add one test that makes the real claim. Base it on
  `UpdateGroupNullStringEntryIsNull` and put it next to that test, so the element-array path and the
  group-writer path for a NULL `char*` sit side by side.
- The null-*name* guard stays covered by `ElementCApi.ArrayNullErrors` (`tests/test_c_api_element.cpp`).
  Check that it still exists with `grep -n "ArrayNullErrors" tests/test_c_api_element.cpp` before
  deleting.

## Changes (tests only) — `tests/test_c_api_database_update.cpp`

1. Delete the four tests listed above. Read each first to confirm it matches the description. If
   one does assert a real NULL-cell outcome, keep it and delete only the other three.
2. Add, right after `UpdateGroupNullStringEntryIsNull`:
```cpp
TEST(DatabaseCApi, UpdateElementNullStringArrayEntryIsNull) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);

    // Collection needs a Configuration row only if the schema's triggers require it; copy the
    // neighbouring test's setup exactly.
    quiver_element_t* item = nullptr;
    ASSERT_EQ(quiver_element_create(&item), QUIVER_OK);
    ASSERT_EQ(quiver_element_set_string(item, "label", "Item 1"), QUIVER_OK);
    int64_t id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Collection", item, &id), QUIVER_OK) << quiver_get_last_error();
    EXPECT_EQ(quiver_element_destroy(item), QUIVER_OK);

    quiver_element_t* update = nullptr;
    ASSERT_EQ(quiver_element_create(&update), QUIVER_OK);
    const char* tags[] = {"a", nullptr, "c"};
    ASSERT_EQ(quiver_element_set_array_string(update, "tag", tags, 3, nullptr), QUIVER_OK);  // dense mask
    ASSERT_EQ(quiver_database_update_element(db, "Collection", id, update), QUIVER_OK) << quiver_get_last_error();
    EXPECT_EQ(quiver_element_destroy(update), QUIVER_OK);

    int64_t nulls = -1;
    int has_value = 0;
    ASSERT_EQ(quiver_database_query_integer(db, "SELECT COUNT(*) FROM Collection_set_tags WHERE tag IS NULL",
                                            &nulls, &has_value),
              QUIVER_OK);
    EXPECT_EQ(nulls, 1);

    quiver_database_close(db);
}
```
Adapt every call to the current signatures. Copy them from `UpdateGroupNullStringEntryIsNull` and a
query test:
- `quiver_database_query_integer`'s out-params (and the `_params` form, if plan 22 has not yet
  merged them);
- `quiver_element_set_array_string`'s `count` type (`int32_t` at HEAD);
- whether `collections.sql` needs a `Configuration` row first.

Check that `collections.sql` has `Collection_set_tags(id, tag TEXT, UNIQUE(id, tag))` and that
`tag` is nullable. It is at HEAD. SQLite's UNIQUE treats each NULL as distinct, so one NULL row is
fine.

## Tests

The new test is the deliverable. Before the fix there is nothing to fix; it pins existing correct
behaviour that no test covered.

## Docs and changelog

None.

## Verification

1. `cmake --build build --config Debug`
2. `./build/bin/quiver_c_tests.exe --gtest_filter=*UpdateElementNullStringArrayEntryIsNull*:*UpdateGroupNullStringEntryIsNull*:ElementCApi.ArrayNullErrors`
3. `./build/bin/quiver_c_tests.exe` (full)

## Acceptance criteria

- [x] The four misnamed tests are gone, and the new test passes.
- [x] `ElementCApi.ArrayNullErrors` still covers the null-name guard.

## Pitfalls

- A dense (NULL) mask plus a NULL `char*` entry is the documented way to spell a NULL string cell.
  Do not also pass a `has_value` array.

## Out of scope

- Other leftover tests (plan 71) and leaks (plan 69).

## Implementation notes

- **Done as planned.** All four tests matched the description and passed for the wrong reason
  (confirmed by running them before deletion: 4/4 OK). The two `NullAttribute` tests ignored the
  setter's null-name `QUIVER_ERROR` and updated an empty element. The two `NullElement` tests'
  `{"a", nullptr, "c"}` was accepted by the setter, and the update then failed on a missing id 1.
  All four were deleted. The empty `// Null string element tests` banner went with them.
- **New test** `DatabaseCApi.UpdateElementNullStringArrayEntryIsNull` sits directly after
  `UpdateGroupNullStringEntryIsNull`. It asserts `COUNT(*) ... WHERE tag IS NULL == 1`, which does
  not depend on row order (set row order is deliberately not a contract). It passed first time. No
  fix was needed: it pins existing behaviour. To show the test is not vacuous, I temporarily set
  `tags[]` to `{"a", "b", "c"}`. The test then failed with `nulls` 0 vs 1. The change was reverted
  before commit.
- **Verification:** the filtered run (new test, `UpdateGroupNullStringEntryIsNull`,
  `ElementCApi.ArrayNullErrors`) passed 3/3, and the full `quiver_c_tests.exe` passed 569/569. Both
  ran after integrating master at `6fb45ba` (plan 68). `scripts/format.bat` exited 0 and left no
  content diff. clang-format, Julia, Dart and ruff changed nothing. Biome rewrote 43 JS files from
  CRLF to LF with no content change, so `git checkout -- bindings/js` restored them. Its Python step
  `uv sync` rebuilds the quiverdb wheel from source, so expect ~15 min on a fresh checkout.
- **Drift fixed:**
  - Plan 22 has landed, so `quiver_database_query_integer` now takes
    `(db, sql, param_types, param_values, param_count, &out, &has_value)`. It is called with
    `nullptr, nullptr, 0`.
  - The line numbers had moved: the four tests were at L622/L784/L930/L948 and the anchor at L1859.
  - `collections.sql` has no triggers, so no `Configuration` row is created. The plan's
    "copy the neighbouring setup" comment was dropped from the test.
- **`ElementCApi.ArrayNullErrors`** (`tests/test_c_api_element.cpp`) still covers the null-name
  guard for all three array setters.
- **For 69/71:** none of the four deleted tests had a leak or a `delete[]`, so plan 69's sites are
  unaffected. Plan 71's step "**Keep** `UpdateVectorIntegersNullAttribute` and
  `UpdateSetStringsNullAttribute`" and its acceptance criterion "The two NullAttribute tests remain
  for plan 70" no longer apply: this plan deleted both tests. 71 should skip that item and tick that
  criterion as handled by 70.
