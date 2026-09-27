# 23 — C API group marshaller: stop narrowing REAL cells into INTEGER columns

**Batch** 3 · **Severity** low · **Breaking** no. The behaviour changes only for a non-STRICT table that holds a non-integral REAL in an INTEGER column, and such a value can only get there through raw SQL · **Size** S · **Layers** C API (`src/c/database_helpers.h`), C API tests, one new test schema, `src/c/AGENTS.md`, `tests/AGENTS.md`, `CHANGELOG.md`. No C++ core, FFI declaration, binding wrapper or Lua change.

**Depends on** 22 (soft). Plan 22 renames `quiver_database_query_integer_params` to `quiver_database_query_integer`, and the new test calls that function. The fix itself needs nothing from 22. Both spellings are given in the Tests section, so this plan also applies if 22 has not landed.

**Overlaps with**
- **03** rewords the comment above `unmarshal_group_columns_to_rows` in `src/c/database_helpers.h` and two "rows[0]" sentences in the "Multi-Column Time Series" section of `src/c/AGENTS.md`. This plan edits `marshal_group_rows_to_c` further down the same header and a different bullet (`quiver_database_read_time_series_group()`) in the same section. The two edits do not share any text.
- **17** rewrites `quiver_database_read_time_series_row` (`src/c/database_time_series.cpp`) and its `src/c/AGENTS.md` bullet. **This plan must not touch either** (maintainer decision).
- **19** adds a C API test for `quiver_database_read_set_group_by_id`. That test goes through the same marshaller, but only its STRING and FLOAT branches, so it is unaffected by this change.
- **18** makes Julia, Python and JS call `quiver_database_read_{vector,set}_group_by_id` natively. Once it lands those bindings inherit this fix with no code change of their own. No shared text.
- **01, 02** each add a schema to the `valid/` list in `tests/AGENTS.md`, and **75** edits other claims in that file. This plan adds one more name to the same list, so insert it into whatever the list says when you get there.
- **69** is about leak hygiene in the C API tests. The new test frees every handle it opens, so it adds no work for 69.

## Why

`marshal_group_rows_to_c` (`src/c/database_helpers.h`, `case QUIVER_DATA_TYPE_INTEGER`, currently ~L321-336) is the one encoder behind all three columnar group readers: `quiver_database_read_vector_group_by_id` and `quiver_database_read_set_group_by_id` (`src/c/database_read.cpp`, ~L421 and ~L459) and `quiver_database_read_time_series_group` (`src/c/database_time_series.cpp`, ~L157). Its INTEGER column branch contains its own narrowing rule:

```cpp
                    if (std::holds_alternative<int64_t>(val)) {
                        arr[r] = std::get<int64_t>(val);
                        mask[r] = 1;
                    } else if (std::holds_alternative<double>(val)) {
                        arr[r] = static_cast<int64_t>(std::get<double>(val));
                        mask[r] = 1;
                    } else {
```

A REAL cell in an INTEGER column is truncated toward zero and reported as **present**. The C++ core sets no such policy. `Database::read_vector_group_by_id` → `group_rows_from_result` (`src/database_read.cpp`, ~L181-192) copies each `Value` out of `execute` unchanged, and `execute` (`src/database.cpp`) tags each cell by `sqlite3_column_type`. So the narrowing exists only in the C API. It contradicts the core's one read rule, `Row::get_integer` (`src/row.cpp`, ~L29-34):

```cpp
std::optional<int64_t> Row::get_integer(size_t index) const {
    if (const auto* val = std::get_if<int64_t>(&values_[index])) {
        return *val;
    }
    return std::nullopt;
}
```

**How the case is reached.** STRICT is a schema convention, not a rule the code enforces: nothing in `src/` checks for it (`grep -rn STRICT src/` finds only comments). In a non-STRICT table, INTEGER affinity converts only what it can convert losslessly. `2.0` is stored as `2`, but `1.5` stays REAL. No API write path can store that value, because `TypeValidator`, `value_matches_type` and `import_csv`'s `parse_integer` all reject a double for an INTEGER column. Raw SQL through `query_*` can. Reproduction (read from the code; the new test below shows it by failing before the fix):

- Schema: a non-STRICT `Items_vector_counts(id, vector_index, quantity INTEGER)`.
- Writes: element `Items` id 1, then `INSERT INTO Items_vector_counts (id, vector_index, quantity) VALUES (1, 1, 7), (1, 2, 1.5)` via `quiver_database_query_integer`.

| Reader | Result for the `1.5` cell |
| --- | --- |
| `quiver_database_read_vector_integers_by_id(db, "Items", "quantity", 1)` → `read_column_values<int64_t>` → `Row::get_integer` | **dropped**: result is `[7]` |
| `read_scalar_integers` on an equivalent scalar | **NULL** (`read_column_values_nullable`) |
| `quiver_database_read_time_series_row` (INTEGER), for a time-series cell | `0`, the documented INTEGER no-data sentinel (`src/c/database_time_series.cpp`, `? std::get<int64_t>(values[i]) : 0`) |
| `quiver_database_read_vector_group_by_id(db, "Items", "counts", 1)` | **`1` with mask `1`**: truncated and reported present |

Every binding that decodes a group read from the C API honours the mask and nothing else (Dart `_decodeGroupRows` / `readTimeSeriesGroup`, Python and JS `read_time_series_group`, Julia's decoder). So Dart's `readVectorGroupById` returns `{quantity: 1}` for a cell that every per-column reader treats as absent.

Principles violated:
- Root `AGENTS.md`, Core API / Query: "`query_integer` does **not** narrow a REAL; that direction is lossy."
- Root "Intelligence: Logic resides in C++ layer". The C API adds a conversion policy the core does not have.
- Root "Clean code over defensive code … Delete unused code". The branch is dead for every schema that follows the conventions, and it is wrong for the one kind of schema that can reach it.

`git log -S` shows the branch came in with `28193aa` (multi-column time series, #81) and moved into the helper in `dc827f2`. No design decision or Do-Not-Fix entry mentions it.

## Constraints and decisions

- **Maintainer decision (binding):** "Deletion only; no shared helper; do not touch read_time_series_row. Add one C API test (non-STRICT table, 1.5 written via query, group read comes back masked)."
- Root `AGENTS.md`, **One scalar typing policy**: an int64 is accepted for INTEGER and REAL columns, and a double only for REAL. The read side of that rule lives in `Row::get_integer` / `Row::get_float`. `src/AGENTS.md` says of `Row::get_float`: "the one place the int64-for-REAL policy is implemented for reads … Don't re-add a widening branch at a call site." The FLOAT branch of `marshal_group_rows_to_c` widens int64 to double. That is the allowed direction, so **leave it alone**.
- Root "Self-Updating": `src/c/AGENTS.md` is the nearest AGENTS.md to `src/c/database_helpers.h`. `tests/AGENTS.md` lists every schema file.
- Root "All *.sql test schemas in `tests/schemas/`". The non-STRICT schema goes there as a file, not inline in the test.
- Root "Changelog": a user-visible change gets an entry under `## [0.11.0] — unreleased`. The change is not breaking, so the entry goes under `### Fixed`.

Alternatives considered and rejected:
- **One shared "cell rule" helper used by both `marshal_group_rows_to_c` and `read_time_series_row`** (the finding's original proposal). Rejected by the maintainer. The remaining difference it would close, an int64 in a FLOAT column on the row read, cannot happen: a REAL-declared column (STRICT or not) always reads back as `SQLITE_FLOAT`. The helper would also keep a conversion policy in the C API, which is the leak this plan removes. `read_time_series_row` belongs to plan 17.
- **Also delete the FLOAT branch's int64 widening.** Rejected. Widening is the documented read policy (`Row::get_float`), and the maintainer asked for this one deletion only.
- **Throw on a type-mismatched cell.** Rejected. Every other reader treats the cell as absent, and a read that throws on stored data would be a new, inconsistent policy.
- **Enforce STRICT in `SchemaValidator`.** Out of scope. That would be a breaking design change nobody asked for.
- **A test in every FFI binding.** Not done, per the maintainer decision ("one C API test"). The bindings have no conversion code of their own and only decode the mask, which their existing NULL-mask tests already cover.
- **Update the header comments in `include/quiver/c/database.h`** ("mask[c][r] == 0 means SQL NULL", ~L227-230 and ~L393-396). Not done. For a schema that follows the conventions (STRICT), mask 0 still means exactly SQL NULL. The non-STRICT TEXT-in-INTEGER cell was already reported as mask 0 before this change, and the header never mentioned it either. The non-STRICT rule is documented in `src/c/AGENTS.md` instead.

## Changes

### 1. `src/c/database_helpers.h`: delete the double-to-int64 branch in `marshal_group_rows_to_c`

Anchor: function `marshal_group_rows_to_c`, the `case QUIVER_DATA_TYPE_INTEGER: {` block (currently ~L321-336).

Current:

```cpp
            case QUIVER_DATA_TYPE_INTEGER: {
                auto* arr = new int64_t[row_count];
                (*out_column_data)[c] = arr;
                for (size_t r = 0; r < row_count; ++r) {
                    auto& val = rows[r].at(columns[c].first);
                    if (std::holds_alternative<int64_t>(val)) {
                        arr[r] = std::get<int64_t>(val);
                        mask[r] = 1;
                    } else if (std::holds_alternative<double>(val)) {
                        arr[r] = static_cast<int64_t>(std::get<double>(val));
                        mask[r] = 1;
                    } else {
                        arr[r] = 0;
                        mask[r] = 0;
                    }
                }
                break;
            }
```

New:

```cpp
            case QUIVER_DATA_TYPE_INTEGER: {
                auto* arr = new int64_t[row_count];
                (*out_column_data)[c] = arr;
                for (size_t r = 0; r < row_count; ++r) {
                    auto& val = rows[r].at(columns[c].first);
                    // Only an int64 is an INTEGER value. A REAL cell (only a non-STRICT table can
                    // hold one) is absent, never narrowed - Row::get_integer's rule.
                    if (std::holds_alternative<int64_t>(val)) {
                        arr[r] = std::get<int64_t>(val);
                        mask[r] = 1;
                    } else {
                        arr[r] = 0;
                        mask[r] = 0;
                    }
                }
                break;
            }
```

Why: this removes the only narrowing conversion in the C API read path. The two-line comment stays so the branch is not re-added "for symmetry" with the FLOAT branch below. Leave the `QUIVER_DATA_TYPE_FLOAT`, `STRING`/`DATE_TIME` and `default` cases, the `rows.empty()` early return and the `catch (...)` cleanup exactly as they are. `<variant>` is still used, so no include changes.

### 2. Nothing else in code

- **C++ core**: no change. The core already passes raw values through; the policy was the C API's.
- **Other C API functions**: `quiver_database_read_{vector,set}_group_by_id` and `quiver_database_read_time_series_group` call the helper unchanged. Do **not** touch `quiver_database_read_time_series_row` (plan 17), which already never narrows.
- **FFI declarations** (Julia `c_api.jl`, Dart `bindings.dart`, Python `_c_api.py`, JS `loader.ts`): no signature change, so there is nothing to regenerate or hand-edit.
- **Binding wrappers**: no change. Each one already maps mask 0 to its null (`nothing` / `null` / `None` / `null`).
- **Lua**: no change. Lua binds the C++ readers directly and never goes through `marshal_group_rows_to_c`.

## Tests

### New schema: `tests/schemas/valid/non_strict_vector.sql`

Create it with LF line endings. `.gitattributes` has `* text=auto`, so git normalizes it anyway, and SQLite does not care. The file needs no CMake registration: tests find it through `VALID_SCHEMA(...)` → `path_from(__FILE__, ...)`.

```sql
-- Schema: a vector group whose table is deliberately NOT STRICT
-- Tests: the C API group readers never narrow a REAL cell into an INTEGER column. Without STRICT,
-- INTEGER affinity keeps a non-integral value written through raw SQL (1.5) as REAL, which is the
-- only way a cell whose storage class differs from its declared type can reach a reader.
PRAGMA foreign_keys = ON;

CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Items (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Items_vector_counts (
    id INTEGER NOT NULL REFERENCES Items(id) ON DELETE CASCADE ON UPDATE CASCADE,
    vector_index INTEGER NOT NULL,
    quantity INTEGER,
    PRIMARY KEY (id, vector_index)
);
```

I checked this schema against `SchemaValidator` (`src/schema_validator.cpp`) by reading the code:
- `Configuration` exists.
- `Items` has `id` as its primary key and a TEXT, NOT NULL, UNIQUE `label`, and its name has no underscore.
- `Items_vector_counts` has a parent collection, a composite PK `(id, vector_index)`, a `vector_index` column, and an `id` FK to `Items` with CASCADE/CASCADE.
- `quantity` duplicates no attribute.

`Schema::query_columns` (`src/schema.cpp`) reads the declared type `INTEGER` through `PRAGMA table_info`, and that works the same with or without STRICT. Plan 06 adds parent-FK checks for set and time-series tables only. This schema has neither, so it stays valid after 06.

### New test: `tests/test_c_api_database_read_vector.cpp`

Add it at the end of the file, directly after `TEST(DatabaseCApi, ReadVectorGroupByIdPreservesNullCells)` (currently ~L591-643). The file already includes `test_utils.h`, `<gtest/gtest.h>`, `<quiver/c/database.h>` and `<quiver/c/element.h>`, so no new includes are needed.

**Query call spelling:** once plan 22 has landed, the parameterized query is `quiver_database_query_integer(db, sql, param_types, param_values, param_count, &out_value, &out_has_value)`. Check `include/quiver/c/database.h`. If `quiver_database_query_integer_params` still exists there (22 not landed), use that name with the **same arguments**.

```cpp
// A non-STRICT table's INTEGER column keeps a non-integral value as REAL (INTEGER affinity only
// converts what it can convert losslessly). The group reader must report that cell absent, as
// Row::get_integer and the per-column reader do - not truncate 1.5 to 1 and call it present.
TEST(DatabaseCApi, ReadVectorGroupByIdMasksRealCellInIntegerColumn) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("non_strict_vector.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_element_t* item = nullptr;
    ASSERT_EQ(quiver_element_create(&item), QUIVER_OK);
    quiver_element_set_string(item, "label", "Item 1");
    int64_t item_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Items", item, &item_id), QUIVER_OK);
    EXPECT_EQ(quiver_element_destroy(item), QUIVER_OK);

    // Every API write path rejects a double for an INTEGER column, so raw SQL is the only way in.
    int param_types[] = {QUIVER_DATA_TYPE_INTEGER, QUIVER_DATA_TYPE_INTEGER};
    const void* param_values[] = {&item_id, &item_id};
    int64_t unused = 0;
    int has_value = 1;
    ASSERT_EQ(quiver_database_query_integer(db,
                                            "INSERT INTO Items_vector_counts (id, vector_index, quantity) "
                                            "VALUES (?, 1, 7), (?, 2, 1.5)",
                                            param_types,
                                            param_values,
                                            2,
                                            &unused,
                                            &has_value),
              QUIVER_OK);
    EXPECT_EQ(has_value, 0);

    char** column_names = nullptr;
    int* column_types = nullptr;
    void** column_data = nullptr;
    uint8_t** column_has_value = nullptr;
    size_t column_count = 0;
    size_t row_count = 0;
    ASSERT_EQ(quiver_database_read_vector_group_by_id(db,
                                                      "Items",
                                                      "counts",
                                                      item_id,
                                                      &column_names,
                                                      &column_types,
                                                      &column_data,
                                                      &column_has_value,
                                                      &column_count,
                                                      &row_count),
              QUIVER_OK);

    ASSERT_EQ(column_count, 1);
    ASSERT_EQ(row_count, 2);
    EXPECT_STREQ(column_names[0], "quantity");
    EXPECT_EQ(column_types[0], QUIVER_DATA_TYPE_INTEGER);
    EXPECT_EQ(column_has_value[0][0], 1);
    EXPECT_EQ(static_cast<int64_t*>(column_data[0])[0], 7);
    EXPECT_EQ(column_has_value[0][1], 0);

    quiver_database_free_time_series_data(
        column_names, column_types, column_data, column_has_value, column_count, row_count);

    // The per-column reader drops the same cell, so the two readers agree.
    int64_t* values = nullptr;
    size_t count = 0;
    ASSERT_EQ(quiver_database_read_vector_integers_by_id(db, "Items", "quantity", item_id, &values, &count),
              QUIVER_OK);
    ASSERT_EQ(count, 1);
    EXPECT_EQ(values[0], 7);
    quiver_database_free_integer_array(values);

    quiver_database_close(db);
}
```

**Why each assertion is there:**
- `ASSERT_EQ(query..., QUIVER_OK)` guards the premise. If someone makes the schema STRICT, SQLite rejects `1.5` for an INTEGER column and the test fails loudly instead of passing vacuously.
- Row 0 (`7`, mask 1) shows the reader did not simply mask everything.
- `column_has_value[0][1] == 0` is the fix.
- The `read_vector_integers_by_id` block pins the agreement this plan exists for. It passes both before and after the fix, and is only a contrast.

**Fails before the fix?** Yes. Today the double branch sets `mask[1] = 1` and `arr[1] = 1`, so `EXPECT_EQ(column_has_value[0][1], 0)` fails with `Which is: '\x1' (1)`. Everything else in the test passes before and after.

**Existing tests that pin the old behaviour:** none. No test or binding writes a REAL into an INTEGER group column. `grep -rln "INSERT INTO" tests bindings` finds nothing, and every API write path rejects the value. On the current build, `quiver_c_tests.exe --gtest_filter='DatabaseCApi.ReadVector*:DatabaseCApi.ReadSet*:*TimeSeriesGroup*:*TimeSeriesNull*'` passes 75/75, and none of those tests goes through the deleted branch.

**Other layers:** no new tests, per the maintainer decision.
- C++ core: behaviour unchanged.
- Lua: does not use this path.
- Julia/Dart/Python/JS: no code changed. They decode the mask, which their existing time-series NULL tests cover.

## Docs and changelog

### `src/c/AGENTS.md`: "Multi-Column Time Series" section, the `quiver_database_read_time_series_group()` bullet (currently ~L189-193)

Old (last sentence of the bullet):

```
  NULL `char*` — NULL strings no longer fail the read. The dimension column's mask is always all 1.
```

New:

```
  NULL `char*` — NULL strings no longer fail the read. The dimension column's mask is always all 1.
  The vector/set group readers share this encoder (`marshal_group_rows_to_c`), and it follows the
  core's read rule rather than one of its own: a cell whose stored value the column's type cannot
  hold is reported absent (mask 0). Only a non-STRICT table written through raw SQL can hold such
  a cell (e.g. `1.5` in an INTEGER column). A REAL is never narrowed into an INTEGER column
  (`Row::get_integer`), while an int64 in a FLOAT column is widened (`Row::get_float`).
  `DatabaseCApi.ReadVectorGroupByIdMasksRealCellInIntegerColumn` pins it.
```

Do not edit the `quiver_database_read_time_series_row()` bullet below it (plan 17), or the two `rows[0]` sentences above it (plan 03).

### `tests/AGENTS.md`: "Schemas (`tests/schemas/`)" section

1. In the `- \`valid/\` — …` list (currently ~L162-165), insert `` `non_strict_vector.sql`, `` in alphabetical order, directly before `` `nullable_time_series.sql` ``. Plans 01 and 02 may have added names to this list by then, so keep whatever else is there. At HEAD the list's last line is:

   ```
     `multi_time_series.sql`, `nullable_time_series.sql`, `relations.sql`
   ```
   and becomes:
   ```
     `multi_time_series.sql`, `non_strict_vector.sql`, `nullable_time_series.sql`, `relations.sql`
   ```
   Re-wrap the list at about 100 columns if the line gets too long.

2. Add a sub-bullet after the `multi_column_groups.sql` sub-bullet (which ends "Note every set value column must be part of the UNIQUE constraint."):

   ```
     - `non_strict_vector.sql` is the one schema whose group table (`Items_vector_counts`) is
       deliberately **not** STRICT. Without STRICT, INTEGER affinity keeps a non-integral value
       written through raw SQL (`1.5`) as REAL. That is the only way to put a cell whose storage
       class differs from its declared type in front of a reader, and
       `DatabaseCApi.ReadVectorGroupByIdMasksRealCellInIntegerColumn` uses it to pin that the C
       API group marshaller reports such a cell absent instead of truncating it. Keep every other
       schema STRICT.
   ```

### Other docs

None. The root `AGENTS.md` already says "`query_integer` does **not** narrow a REAL". `docs/*.md`, the binding READMEs and `bindings/js/src/lua-api.ts` never mention this path.

### `CHANGELOG.md`

Add this at the **end** of the `### Fixed` list under `## [0.11.0] — unreleased`, after the last existing bullet. At HEAD that is the "Julia: updating `Artifacts.toml` …" entry, and earlier plans may have appended more.

```markdown
- **The C API group readers no longer truncate a REAL cell in an INTEGER column.**
  `quiver_database_read_vector_group_by_id`, `quiver_database_read_set_group_by_id` and
  `quiver_database_read_time_series_group` turned a stored `1.5` into `1` and reported it
  present. The cell is now absent (mask 0), the same as in the per-column integer readers, so the
  binding group readers built on these functions return null for it. Only a non-STRICT table
  written through raw SQL can hold such a value; STRICT schemas, as the conventions use, are
  unaffected.
```

## Verification

Run from the repo root (`C:\Development\Quiver\quiver1`), in PowerShell. In Git Bash, prefix each `.bat` with `cmd //c`.

1. Write the schema (Tests, first part) and the new test **before** touching `database_helpers.h`. Then:
   ```
   cmake --build build --config Debug
   ./build/bin/quiver_c_tests.exe --gtest_filter='DatabaseCApi.ReadVectorGroupByIdMasksRealCellInIntegerColumn'
   ```
   Expected: **FAILED**, on `EXPECT_EQ(column_has_value[0][1], 0)` (actual `'\x1' (1)`). Any other failure means the test or schema is wrong. For example, a `from_schema` or `query_integer` failure points at the schema or the query spelling (see plan 22 above).
2. Apply Change 1 (delete the branch), then:
   ```
   cmake --build build --config Debug
   ./build/bin/quiver_c_tests.exe --gtest_filter='DatabaseCApi.ReadVectorGroupByIdMasksRealCellInIntegerColumn'
   ```
   Expected: 1 test, PASSED.
3. The neighbouring group-reader tests:
   ```
   ./build/bin/quiver_c_tests.exe --gtest_filter='DatabaseCApi.ReadVector*:DatabaseCApi.ReadSet*:*TimeSeriesGroup*:*TimeSeriesNull*'
   ```
   Expected: all pass (75 at HEAD, plus this test and whatever plans 17 and 19 added).
4. Full native suites:
   ```
   ./build/bin/quiver_c_tests.exe
   ./build/bin/quiver_tests.exe
   ```
   Expected: all pass.
5. No FFI regeneration is needed (no signature change). Do **not** run `scripts/generator.bat`.
6. Binding suites. No binding code changed, so these are regression runs only:
   ```
   bindings\julia\test\test.bat
   bindings\dart\test\test.bat
   bindings\js\test\test.bat
   bindings\python\tests\test.bat
   ```
   Expected: all pass.
7. `scripts\format.bat`, then `git diff --stat`. Only these files should differ: `src/c/database_helpers.h`, `tests/test_c_api_database_read_vector.cpp`, `tests/schemas/valid/non_strict_vector.sql` (new, untracked until added), `src/c/AGENTS.md`, `tests/AGENTS.md`, `CHANGELOG.md`. If clang-format rewrapped the new test, keep its output.
8. `scripts\test-all.bat`. Expected: steps 1-6 PASS. Step 7 (CLI smoke test) fails at HEAD because `example\example1.lua` was deleted. That failure predates this change and is fixed by plan 65. If 65 has landed, step 7 passes too.

## Acceptance criteria

- [ ] The `else if (std::holds_alternative<double>(val))` branch is gone from the `QUIVER_DATA_TYPE_INTEGER` case of `marshal_group_rows_to_c`. `grep -n "static_cast<int64_t>(std::get<double>" src/c/database_helpers.h` prints nothing.
- [ ] The FLOAT case, the STRING/DATE_TIME case, `default`, the early return and the cleanup `catch` are byte-identical to before.
- [ ] `src/c/database_time_series.cpp` is untouched by this plan (`quiver_database_read_time_series_row` included).
- [ ] `tests/schemas/valid/non_strict_vector.sql` exists with exactly the SQL above, and `Items_vector_counts` has no `STRICT`.
- [ ] `DatabaseCApi.ReadVectorGroupByIdMasksRealCellInIntegerColumn` exists, failed before Change 1, and passes after it.
- [ ] `quiver_c_tests.exe` and `quiver_tests.exe` pass in full, and so do the four binding suites.
- [ ] `src/c/AGENTS.md` has the new sentences on the `read_time_series_group()` bullet. `tests/AGENTS.md` lists `non_strict_vector.sql` and has its sub-bullet.
- [ ] `CHANGELOG.md` has the Fixed entry under `## [0.11.0] — unreleased`, not prefixed **BREAKING**.
- [ ] No manifest version bump, no FFI file change, no binding change.

## Pitfalls

- **Query function name.** Before plan 22 the parameterized form is `quiver_database_query_integer_params`, and the plain `quiver_database_query_integer(db, sql, &out, &has)` takes **no** parameter arrays. After 22 there is only `quiver_database_query_integer` with the parameter arrays. Check the header and use the one that takes `param_types, param_values, param_count`.
- **Do not "fix" the FLOAT branch** to match. Its int64→double widening is the documented read policy (`Row::get_float`) and must stay.
- **Do not add a shared helper** or touch `read_time_series_row`. The maintainer decided against both, and plan 17 owns the row reader.
- **Do not add `STRICT` to the new schema's vector table.** The test depends on it being absent. With STRICT the `INSERT` of `1.5` fails (`cannot store REAL value in INTEGER column`), and the premise assertion catches that.
- **Group name vs column name.** `quiver_database_read_vector_group_by_id` takes the group name (`"counts"`, from `Items_vector_counts`), while `quiver_database_read_vector_integers_by_id` takes the column name (`"quantity"`). Swapping them gives "Vector group not found" or a column-not-found error.
- **Line endings.** Working-tree `.sql` files in this checkout are CRLF (`text=auto`), and the Write tool produces LF. Either works, and git normalizes on commit. Do not run `sed`/unix tools over `.bat` files while verifying: they are CRLF and would be silently converted.
- **`test-all.bat` step 7** fails until plan 65 lands (see Verification step 8). Do not try to fix it here.
- **`tests/AGENTS.md` list drift.** Plans 01 and 02 add schema names to the same `valid/` list. Insert `non_strict_vector.sql` alphabetically into the list as it is, and do not overwrite their additions.

## Out of scope

- The C++ core's own group readers (`Database::read_vector_group_by_id` and the others) still return the raw stored `Value`, so they give `1.5` for that cell. So does Lua, which binds them directly. A row-shaped `Value` can represent the stored value exactly, and only the C API's typed INTEGER array cannot. Not changed here.
- `quiver_database_read_time_series_row`'s sentinel encoding and its missing mask belong to plan **17**.
- The native vector/set group readers for Julia, Python and JS belong to plan **18**. The C API set-reader test belongs to plan **19**.
- Enforcing STRICT in `SchemaValidator` is not planned anywhere and would be a design decision.
- The header comments in `include/quiver/c/database.h` that say "mask 0 = SQL NULL" are deliberately unchanged (see Constraints).
