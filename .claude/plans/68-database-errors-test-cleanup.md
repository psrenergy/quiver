# 68 — `test_database_errors.cpp`: stale segfault notes, wrong comments, a duplicate test

**Batch** 7 · **Severity** low · **Breaking** no · **Size** S · **Layers** C++ tests only
**Depends on** none · **Overlaps with** 71 (deletes the `UpdateVector*/UpdateSet* CollectionNotFound` family at ~L182-233 of the same file — if 71 lands first, the notes above those sections are already gone with them; do whichever is left)

## Why

`tests/test_database_errors.cpp` carries comments that are no longer true, plus one duplicated
success test:

1. Three section notes (~L134-139, ~L164-169, ~L182-187) say, in some form:
   `read_vector_* methods without schema cause segfault (null pointer dereference) because
   impl_->schema->find_vector_table() is called without null check. These tests are skipped until
   the library adds proper null checks.` That is false now. Every reader enters through
   `Impl::require_collection`, which calls `require_schema()` (`src/database_impl.h` ~L85-96). On a
   non-quiver database that runs `load_schema_metadata`, and the `SchemaValidator` throws. There is
   no segfault. A fourth note (~L216-219) mentions `find_set_table()` the same way.
2. The comments at ~L118, ~L247 and ~L360 say the no-schema throw comes from "executing SQL ...
   missing table". It now comes from the lazy schema load (L118, L360) or from `require_column`
   rejecting the attribute before any SQL runs (L247).
3. `DatabaseErrors.CreateElementEmptyArraySkipsSilently` (~L39-54) is not an error case, and it is
   a weaker copy of `Database.CreateElementWithEmptyArraySkipsSilently`
   (`tests/test_database_create.cpp` ~L333-355): same schema, same element, but only
   `EXPECT_GT(id, 0)`, where the original also checks the label and the empty vector.

Principle: comments must describe the code, and duplicated tests should be deleted.

## Constraints and decisions

- Do not add new no-schema vector/set tests. The scalar no-schema tests already exercise the
  shared `require_collection` → `require_schema` path, and nothing asks for more.
- Keep the section banners (the titles). Delete only the notes under them.

## Changes (tests only) — `tests/test_database_errors.cpp`

1. Delete the three segfault notes and the `find_set_table` note. Keep each section's title line.
2. At ~L118 and ~L360, reword the comment to: "the lazy schema load rejects a database with no quiver
   schema (no Configuration table)". At ~L247, reword it to: "require_column rejects the unknown
   attribute before any SQL runs". Or delete the three comments if the test names already say this.
3. Delete `TEST(DatabaseErrors, CreateElementEmptyArraySkipsSilently)` (~L39-54).

Read each comment in context before editing: `sed -n 110,125p`, `sed -n 240,250p`,
`sed -n 355,365p tests/test_database_errors.cpp`.

## Tests

Test edits only. The remaining tests must pass:
`./build/bin/quiver_tests.exe --gtest_filter=DatabaseErrors*:Database.CreateElementWithEmptyArraySkipsSilently`.

## Docs and changelog

None.

## Verification

1. `cmake --build build --config Debug`
2. `./build/bin/quiver_tests.exe --gtest_filter=DatabaseErrors*`
3. `grep -n "segfault\|null check\|find_vector_table()\|find_set_table()" tests/test_database_errors.cpp`
   must print nothing.

## Acceptance criteria

- [ ] No stale segfault or null-check notes remain. The three comments are accurate.
- [ ] The duplicate test is gone and the suite is green.

## Pitfalls

- If plan 71 already deleted a section, its note is gone too. Don't recreate it.

## Out of scope

- Adding vector/set no-schema tests.
