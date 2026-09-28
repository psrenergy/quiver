# 19 — C API test for quiver_database_read_set_group_by_id

**Batch** 3 · **Severity** medium · **Breaking** no. Test-only; no public surface changes · **Size** S · **Layers** C API tests only (`tests/test_c_api_database_read_set.cpp`)
**Depends on** none · **Overlaps with**
- **18** (whole-group readers: Julia/Python call the native C API; JS gains them) makes Julia, Python and JS call this entry point. That plan owns the binding-level tests. This plan adds only the C-layer test, so those bindings will sit on an entry point that is tested at its own layer.
- **23** (C API group marshaller: stop narrowing REAL cells into INTEGER columns) edits the `QUIVER_DATA_TYPE_INTEGER` branch of `marshal_group_rows_to_c` (`src/c/database_helpers.h`). The test added here reads a STRING column and a FLOAT column, so it goes through the STRING and FLOAT branches only and is unaffected by 23. If 23 has already landed, this test must still pass without edits.
- **69** (C API tests: fix handle leaks and raw `delete[]` frees) later sweeps the `test_c_api_*` files. The test added here frees every handle it creates: each element is destroyed, the read result goes through `quiver_database_free_time_series_data`, and the database is closed. Plan 69 then has nothing to fix in it.
- **57** (one `require_group_table` lookup; Pattern 2 for every miss) may reword the not-found message of `get_set_metadata`. This test asserts no error message, so it is unaffected.

## Why

`quiver_database_read_set_group_by_id` is the only group-read C entry point with no C API test.

`src/c/database_read.cpp`, `quiver_database_read_set_group_by_id` (currently ~L437-473):
```cpp
    try {
        auto metadata = db->db.get_set_metadata(collection, group);
        auto rows = db->db.read_set_group_by_id(collection, group, id);

        std::vector<std::pair<std::string, int>> columns;
        for (const auto& vc : metadata.value_columns) {
            columns.push_back({vc.name, to_c_data_type(vc.data_type)});
        }

        marshal_group_rows_to_c("read_set_group_by_id",
                                columns,
                                rows,
                                ...
```

What I verified against HEAD 58dfe7a:
- `grep -rn "read_set_group_by_id" tests/` matches only C++ core tests: `test_database_create.cpp` (~L442), `test_database_read_set.cpp` (~L117, ~L152) and `test_database_update.cpp` (~L1381). No `test_c_api_*` file calls it.
- The vector sibling is covered by `DatabaseCApi.ReadVectorGroupByIdPreservesNullCells` in `tests/test_c_api_database_read_vector.cpp` (currently ~L591-643). That is the only C API test for either whole-group reader.
- The only FFI consumer today is Dart's native `readSetGroupById` (`bindings/dart/lib/src/database_read.dart`, tested only in `bindings/dart/test/metadata_test.dart` ~L462, on a dense single-column group with no NULLs). Julia (`bindings/julia/src/database_read.jl`) and Python (`bindings/python/src/quiverdb/database.py`) still build the result from per-column reads until plan 18.
- Nothing in any AGENTS.md records this gap as an accepted exception.

This breaks the rule that tests must exist in every layer where the behaviour is visible. The function's own logic is small. It shares `marshal_group_rows_to_c` with the vector reader and `read_time_series_group`. The set-specific parts are the `get_set_metadata` call and the `read_set_group_by_id` call. Nothing at the C layer checks the following:
1. A per-cell NULL in a **set** group comes back as `column_has_value[c][r] == 0`.
2. Two value columns of different types come back with the right names and `quiver_data_type_t` tags, in metadata order.
3. An element with no rows returns `QUIVER_OK` with every out-array NULL and both counts 0.

The current behaviour is correct, and this plan changes no code. Nothing reproduces as wrong output. The problem is that a regression here would reach Dart, and after plan 18 Julia, Python and JS too, before any C-layer test failed. For example, the entry point could be switched back to `get_vector_metadata` by a copy-paste. That throws `Vector group not found`. No C test would catch it, and the only binding test is a single-column dense Dart case.

## Constraints and decisions

- **Maintainer decision (binding):** use `tests/schemas/valid/multi_column_groups.sql` `Items_set_codes` (two nullable columns, `code TEXT` and `weight REAL`). Put the NULL in the **second** column (`weight`). Write the rows with `quiver_database_update_set_group` **with a mask**.
- Root `AGENTS.md`, Design Decisions, "A set group's rows come back in `rowid` order, and that order is not a promise": the order is *"consistent across every reader of the group, otherwise unspecified … that coincidence must not become a contract."* So the test must **not** assert which row index holds which row. It finds the NULL-bearing row by its `code` value. The C++ core test `Database.ReadSetByIdOrderMatchesGroupReader` (`tests/test_database_read_set.cpp` ~L92) follows the same rule.
- Root `AGENTS.md`, "Time-series group NULLs round-trip via a per-cell presence mask" and `src/c/AGENTS.md` "Multi-Column Time Series": for a NULL cell (`mask[r] == 0`) the data slot is a placeholder the caller must ignore. The test asserts the mask for the NULL cell and **not** the placeholder value. Asserting `0.0` would pin an implementation detail the contract tells callers to ignore.
- `include/quiver/c/database.h` (comment above `quiver_database_read_vector_group_by_id`, ~L227-230): the result is *"Freed by quiver_database_free_time_series_data"*. The test frees it that way.
- `src/c/AGENTS.md` "Multi-Column Time Series": in `update_*_group` a NULL `column_has_value` entry means that column is dense. The maintainer asked for "a mask". The test passes an explicit all-ones mask for `code` and `{1, 0}` for `weight`, so every mask entry is readable at a glance.
- `tests/AGENTS.md`, "Schemas": *"every set value column must be part of the UNIQUE constraint"*. `Items_set_codes` already declares `UNIQUE (id, code, weight)`. SQLite treats each NULL as distinct, so `('beta', NULL)` inserts fine. No schema change is needed.
- Test-only change with no user-visible behaviour, so no CHANGELOG entry. None of the 0.12.0 entries are test-only.

Alternatives considered and rejected:
- **`relations.sql`** (the vector test's fixture): its set groups have one column each, so the test could not check two names, two type tags and a NULL in a non-first column. The maintainer chose `multi_column_groups.sql`.
- **Writing the rows with `quiver_element_set_array_*` + `quiver_database_update_element`**: the maintainer chose `quiver_database_update_set_group` with a mask. It also names exactly one table, so it avoids the array fan-out the root `AGENTS.md` warns about.
- **Two tests (NULL-cell and empty-group)**: the two verifiers disagreed. I kept one test. The empty-group check is five assertions on the same fixture, and a second test would repeat about twenty lines of setup for them.
- **A new schema whose declaration order differs from alphabetical order**: `code` sorts before `weight` in both orders, so the names assertion cannot tell declaration order from alphabetical order. I rejected the extra schema. Declaration order is a property of `get_set_metadata`, which the C++ core suites already cover, and not of this entry point. The test still pins `code` at index 0 and `weight` at index 1, which is what a caller depends on.
- **An error-path case (unknown group / unknown collection)**: out of scope. The vector sibling has none, and the messages belong to the core (plans 07 and 57 are changing them).

## Changes

No production code changes. No C API, FFI declaration (Julia `c_api.jl`, Dart `bindings.dart`, Python `_c_api.py`, JS `loader.ts`), binding wrapper or Lua changes. The generator is not run.

### 1. `tests/test_c_api_database_read_set.cpp`: add `DatabaseCApi.ReadSetGroupByIdPreservesNullCells`

Put it at the end of the "Read set by Id tests" section. That is right after `TEST(DatabaseCApi, ReadSetByIdEmpty)` (currently ~L219-249) and right before this banner (currently ~L251):
```cpp
// ============================================================================
// Read set null pointer tests
// ============================================================================
```

The test uses only headers the file already includes (`"test_utils.h"`, `<gtest/gtest.h>`, `<quiver/c/database.h>`, `<quiver/c/element.h>`, `<string>`). No new include is needed. `QUIVER_DATA_TYPE_*` comes from `<quiver/c/database.h>`.

New code (paste as is; `scripts/format.bat` may re-wrap the long call):
```cpp
// The set counterpart of ReadVectorGroupByIdPreservesNullCells, over a group with two value
// columns of different types and a NULL in the second one. A set group's row order is consistent
// but unspecified, so each row is located by its code, never by position. An element with no rows
// must come back as all-NULL outputs with zero counts.
TEST(DatabaseCApi, ReadSetGroupByIdPreservesNullCells) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("multi_column_groups.sql").c_str(), &options, &db),
              QUIVER_OK);

    const auto make = [&](const char* collection, const char* label) {
        quiver_element_t* e = nullptr;
        EXPECT_EQ(quiver_element_create(&e), QUIVER_OK);
        quiver_element_set_string(e, "label", label);
        int64_t id = 0;
        EXPECT_EQ(quiver_database_create_element(db, collection, e, &id), QUIVER_OK);
        EXPECT_EQ(quiver_element_destroy(e), QUIVER_OK);
        return id;
    };
    make("Configuration", "Config");
    const int64_t item = make("Items", "Item 1");
    const int64_t empty_item = make("Items", "Item 2");

    const char* names[] = {"code", "weight"};
    const int types[] = {QUIVER_DATA_TYPE_STRING, QUIVER_DATA_TYPE_FLOAT};
    const char* codes[] = {"alpha", "beta"};
    const double weights[] = {1.5, 0.0};  // slot 1 is masked out and never read
    const uint8_t code_mask[] = {1, 1};
    const uint8_t weight_mask[] = {1, 0};
    const void* data[] = {codes, weights};
    const uint8_t* masks[] = {code_mask, weight_mask};
    ASSERT_EQ(quiver_database_update_set_group(db, "Items", "codes", item, names, types, data, masks, 2, 2), QUIVER_OK);

    char** column_names = nullptr;
    int* column_types = nullptr;
    void** column_data = nullptr;
    uint8_t** column_has_value = nullptr;
    size_t column_count = 0;
    size_t row_count = 0;
    ASSERT_EQ(quiver_database_read_set_group_by_id(db,
                                                   "Items",
                                                   "codes",
                                                   item,
                                                   &column_names,
                                                   &column_types,
                                                   &column_data,
                                                   &column_has_value,
                                                   &column_count,
                                                   &row_count),
              QUIVER_OK);

    ASSERT_EQ(column_count, 2u);
    ASSERT_EQ(row_count, 2u);
    EXPECT_STREQ(column_names[0], "code");
    EXPECT_STREQ(column_names[1], "weight");
    EXPECT_EQ(column_types[0], QUIVER_DATA_TYPE_STRING);
    EXPECT_EQ(column_types[1], QUIVER_DATA_TYPE_FLOAT);

    auto** read_codes = static_cast<char**>(column_data[0]);
    auto* read_weights = static_cast<double*>(column_data[1]);
    ASSERT_NE(read_codes[0], nullptr);
    const size_t alpha = std::string(read_codes[0]) == "alpha" ? 0 : 1;
    const size_t beta = 1 - alpha;
    EXPECT_STREQ(read_codes[alpha], "alpha");
    EXPECT_STREQ(read_codes[beta], "beta");
    EXPECT_EQ(column_has_value[0][alpha], 1);
    EXPECT_EQ(column_has_value[0][beta], 1);
    EXPECT_EQ(column_has_value[1][alpha], 1);
    EXPECT_DOUBLE_EQ(read_weights[alpha], 1.5);
    EXPECT_EQ(column_has_value[1][beta], 0);  // the NULL weight; its data slot is a placeholder

    quiver_database_free_time_series_data(
        column_names, column_types, column_data, column_has_value, column_count, row_count);

    // No rows: every out-array is NULL and both counts are 0. The counts are seeded non-zero so
    // the assertion proves the call wrote them.
    column_names = nullptr;
    column_types = nullptr;
    column_data = nullptr;
    column_has_value = nullptr;
    column_count = 99;
    row_count = 99;
    ASSERT_EQ(quiver_database_read_set_group_by_id(db,
                                                   "Items",
                                                   "codes",
                                                   empty_item,
                                                   &column_names,
                                                   &column_types,
                                                   &column_data,
                                                   &column_has_value,
                                                   &column_count,
                                                   &row_count),
              QUIVER_OK);
    EXPECT_EQ(column_count, 0u);
    EXPECT_EQ(row_count, 0u);
    EXPECT_EQ(column_names, nullptr);
    EXPECT_EQ(column_types, nullptr);
    EXPECT_EQ(column_data, nullptr);
    EXPECT_EQ(column_has_value, nullptr);

    EXPECT_EQ(quiver_database_close(db), QUIVER_OK);
}
```

Why each piece is there:
- `make` is the same lambda as `DatabaseCApi.UpdateGroupNullStringEntryIsNull` (`tests/test_c_api_database_update.cpp` ~L1815), which uses the same schema. That keeps the two tests alike.
- `Items` has only `label` as a scalar, so a label-only element is valid.
- The `ASSERT_NE(read_codes[0], nullptr)` guard keeps a regression (a NULL string emitted for a non-NULL cell) from crashing `std::string(nullptr)`. Without it the process would crash instead of reporting a clean failure.
- `alpha`/`beta` are computed from the data, per the set-order decision. Both rows are then checked by content. So a swapped-row regression, where `weight` is paired with the wrong `code`, still fails.
- The pointer resets before the second call are needed because the first result was freed. Comparing freed pointers to `nullptr` would read invalid pointer values. The count sentinels (`99`) make the empty-branch assertion meaningful. The empty branch of `marshal_group_rows_to_c` (`src/c/database_helpers.h`, currently ~L290-297, `if (rows.empty()) { *out_column_names = nullptr; ... *out_row_count = 0; return; }`) sets all six outputs together.
- The empty result is not passed to `quiver_database_free_time_series_data`, because nothing was allocated. `ReadVectorGroupByIdPreservesNullCells` doesn't free its empty case either. The free function tolerates NULLs anyway.

## Tests

| Layer | Change |
|---|---|
| C++ core | none. `Database::read_set_group_by_id` is already covered: `Database.ReadSetByIdOrderMatchesGroupReader`, `Database.ReadSetIntegersByIdOrderMatchesGroupReader` (`tests/test_database_read_set.cpp`), `Database.UpdateGroupTypeErrorInsideDryRunKeepsExistingRows` (`tests/test_database_update.cpp`), and the create test in `tests/test_database_create.cpp` ~L442 |
| C API | **new** `DatabaseCApi.ReadSetGroupByIdPreservesNullCells` in `tests/test_c_api_database_read_set.cpp` (full code in Changes step 1), fixture `tests/schemas/valid/multi_column_groups.sql` |
| Lua | none. Lua has no whole-group readers, by design (root Design Decisions, "Lua has no row-aligned whole-group readers") |
| Julia / Python / JS | none here. Plan 18 moves them onto this entry point and owns their tests |
| Dart | none. `metadata_test.dart` ~L462 already calls the native reader |

No existing test changes and no assertion is modified. No new schema file, so nothing to register in `tests/AGENTS.md`. The C API test file already exists and is already in `tests/CMakeLists.txt`.

**Does the new test fail before the fix?** No. There is no code fix. The test pins behaviour that is correct today (verified by reading `quiver_database_read_set_group_by_id`, `Database::read_set_group_by_id` → `group_select_sql` / `group_rows_from_result` in `src/database_read.cpp` ~L170-216, and the FLOAT/STRING/empty branches of `marshal_group_rows_to_c`). The C++ core tests covering the same path pass on the current build (`Database.ReadSetByIdOrderMatchesGroupReader`, `Database.ReadSetIntegersByIdOrderMatchesGroupReader`: 2/2 passed). So do the neighbouring C API tests (`DatabaseCApi.ReadSet*`, `DatabaseCApi.ReadVectorGroupByIdPreservesNullCells`, `DatabaseCApi.UpdateGroupNullStringEntryIsNull`: 23/23 passed). To prove the new test is not vacuous, run the mutation check in Verification step 3.

## Docs and changelog

- **AGENTS.md**: no edit. I checked root, `src/c/AGENTS.md` and `tests/AGENTS.md`. None claims this function is untested, and none lists per-test contents that the new test would make stale. `tests/AGENTS.md` describes `test_c_api_*` files by area only ("Mirror the same areas with the `test_c_api_*` prefix …"), and that stays true.
- **Other docs** (`docs/*.md`, READMEs, `bindings/js/src/lua-api.ts`): no edit.
- **CHANGELOG.md**: no entry. The change is test-only and not user-visible.

## Verification

Run from the repo root (`C:\Development\Quiver\quiver1`), in order:

1. Build:
   ```bash
   cmake --build build --config Debug
   ```
   Expected: builds with no new warnings in `tests/test_c_api_database_read_set.cpp`.
2. Targeted test:
   ```bash
   ./build/bin/quiver_c_tests.exe --gtest_filter='DatabaseCApi.ReadSetGroupByIdPreservesNullCells'
   ```
   Expected: `[  PASSED  ] 1 test.`
3. Mutation check (proves the test is not vacuous; **revert before committing**). In `src/c/database_read.cpp`, `quiver_database_read_set_group_by_id`, change `db->db.get_set_metadata(collection, group)` to `db->db.get_vector_metadata(collection, group)`. Rebuild and re-run step 2. Expected: FAILED on the first `ASSERT_EQ(quiver_database_read_set_group_by_id(...), QUIVER_OK)`, because `Items` has no vector group `codes`. Then run `git checkout -- src/c/database_read.cpp` and rebuild.
4. Neighbouring C API suites:
   ```bash
   ./build/bin/quiver_c_tests.exe --gtest_filter='DatabaseCApi.ReadSet*:DatabaseCApi.ReadVectorGroupById*:DatabaseCApi.UpdateGroup*'
   ```
   Expected: all pass. That is 23 tests from the first two patterns (21 existing `ReadSet*`, the new one, and `ReadVectorGroupByIdPreservesNullCells`), plus the `UpdateGroup*` tests.
5. Full C++ and C suites:
   ```bash
   ./build/bin/quiver_tests.exe
   ./build/bin/quiver_c_tests.exe
   ```
   Expected: all pass.
6. Format:
   ```bash
   scripts/format.bat
   ```
   Then `git diff --stat`. Expected: only `tests/test_c_api_database_read_set.cpp` changed. If the binding formatters touch other files, that is pre-existing drift. Revert it with `git checkout -- <path>` and do not commit it.
7. Final:
   ```bash
   scripts/test-all.bat
   ```
   Expected: steps 1-6 pass. Step 7, the CLI smoke test, fails on the current tree independently of this plan. It references `example\example1.lua`, but the `example/` directory does not exist at HEAD 58dfe7a, and plan 65 fixes that. The binding suites (steps 3-6) are unaffected by this change. They are run only as a regression check.

## Acceptance criteria

- [ ] `tests/test_c_api_database_read_set.cpp` contains `TEST(DatabaseCApi, ReadSetGroupByIdPreservesNullCells)`, placed right after `ReadSetByIdEmpty` and before the "Read set null pointer tests" banner.
- [ ] The test uses `VALID_SCHEMA("multi_column_groups.sql")`, writes with `quiver_database_update_set_group` and a non-NULL `column_has_value` array, and puts the NULL in `weight` (the second column).
- [ ] It asserts `column_count == 2`, `row_count == 2`, names `code`/`weight`, types `STRING`/`FLOAT`, `column_has_value[1][beta] == 0`, and the non-NULL cells' values and masks. Rows are located by `code`, never by fixed index.
- [ ] It asserts the empty-group case: `QUIVER_OK`, both counts 0 (seeded to 99 first), and all four out-arrays NULL.
- [ ] It frees the non-empty result with `quiver_database_free_time_series_data`, destroys every element, and closes the database.
- [ ] The Verification step 3 mutation made the test fail, and the mutation was reverted (`git diff src/` is empty).
- [ ] `quiver_c_tests.exe` passes in full. No production file, FFI declaration, AGENTS.md or CHANGELOG changed.

## Pitfalls

- **Do not assert a fixed row order.** The rows currently come back in write order (`ORDER BY rowid`, and the writer deletes and re-inserts). A test asserting `column_data[0][0] == "alpha"` would pass today but would pin the coincidence the root Design Decision forbids making a contract.
- **Do not assert the NULL cell's placeholder (`read_weights[beta] == 0.0`).** The contract says to ignore it.
- **`std::string(nullptr)` is UB.** Keep the `ASSERT_NE(read_codes[0], nullptr)` before building the `alpha` index.
- **Reset the out-pointers before the second call.** After `quiver_database_free_time_series_data` they hold freed addresses. Comparing those to `nullptr` reads invalid pointer values, and if the call ever stopped writing them the test would compare garbage.
- **Mask/array constness:** `const uint8_t* masks[]` and `const void* data[]` convert implicitly to the `const uint8_t* const*` / `const void* const*` parameters. Do not add casts.
- **`size_t` comparisons:** use the `2u` / `0u` literals as written, so the comparisons stay unsigned and avoid sign-compare warnings. The older vector test uses bare `1`, which works but is not the model here.
- **`format.bat` also runs the binding formatters** (JuliaFormatter, dart format, ruff, biome). Commit only the C++ test file. If the run modifies any `.bat` file, restore CRLF with `git checkout -- <file>`, since working-tree `.bat` files are CRLF.
- **Revert the mutation from Verification step 3.** Leaving `get_vector_metadata` in place breaks Dart's `readSetGroupById`.
- If plan 23 has already landed, `marshal_group_rows_to_c` differs in its INTEGER branch only. This test does not touch that branch and needs no adjustment.

## Out of scope

- Julia/Python/JS switching to the native reader, and their tests: plan **18**.
- `quiver_clear_last_error` (also untested per the finding) is deleted by plan **21**, not tested.
- `quiver_version` has no test either (same finding). No plan owns it, and it is not part of this item.
- Error-path cases for this entry point (unknown collection, unknown group). The messages belong to the core and are being reworked by plans **07** and **57**.
- The REAL-into-INTEGER narrowing in the marshaller's INTEGER branch: plan **23**.
- Leak/free cleanups in other C API tests: plan **69**.
