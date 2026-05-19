---
phase: 01-c-core-add-time-series-row
reviewed: 2026-05-19T00:00:00Z
depth: standard
files_reviewed: 4
files_reviewed_list:
  - include/quiver/database.h
  - src/database_time_series.cpp
  - tests/schemas/valid/multi_dim_time_series.sql
  - tests/test_database_time_series.cpp
findings:
  critical: 0
  warning: 3
  info: 4
  total: 7
status: issues_found
---

# Phase 01: Code Review Report — `Database::add_time_series_row`

**Reviewed:** 2026-05-19
**Depth:** standard
**Files Reviewed:** 4
**Status:** issues_found

## Summary

`Database::add_time_series_row` is correctly implemented and exercises the contract surfaced in
the public API: PK-driven dimension extraction (via `column_order` + `primary_key`), validation
against the schema with the canonical Pattern-1 message, `INSERT OR REPLACE` upsert, and nest-aware
`TransactionGuard` semantics. All 9 GTest cases trace through the implementation cleanly, including
the multi-dim PK upsert and the partial-value-column NULL fan-out. SQL injection is not reachable —
table names come from the validated schema and column names come from caller keys that are
explicitly checked against `schema_types` before being appended into the SQL string.

No correctness bugs found. The findings below are quality and consistency issues:

- 3 **WARNINGS**: a non-deterministic / weakly-specified validation ordering, a subtle ordering
  difference between `add_time_series_row` and its sibling `update_time_series_group` that creates
  a maintenance trap, and an FK-constraint failure path that surfaces a raw SQLite error message
  instead of a Pattern-2 "not found" error.
- 4 **INFOs**: missing CLAUDE.md update (self-updating principle violated), redundant local
  `schema_types` lookup map, missing FK-violation test coverage, and a misleading code comment.

## Warnings

### WR-01: Validation ordering depends on `std::map` key order, not declaration order

**File:** `src/database_time_series.cpp:250-261`
**Issue:** The unknown-column / type-mismatch validation loop iterates the caller's `row` map.
`std::map<std::string, Value>` iterates in lexicographic key order, so the **first** error reported
depends on alphabetical column names, not the declaration order or the order in which the caller
listed the entries. This is observable in tests `AddTimeSeriesRowErrors` cases (d) and (e):

  - Case (d): row = `{date_time, block, load}` with `load = int64{42}` on a REAL column. Iteration
    visits `block` (OK), `date_time` (OK), `load` (FAIL). The test passes only because no other
    columns alphabetically precede `load` with a type mismatch.
  - Case (e): row = `{date_time, block, load}` with `block = 1.5` on an INTEGER column. Iteration
    visits `block` (FAIL) first.

If a future schema adds a column like `aux_amount REAL` with a type mismatch alongside an existing
mismatch on `load`, the error message would silently switch to `aux_amount`. Two of the existing
`AddTimeSeriesRowErrors` cases are alphabetically lucky.

Also: the missing-dimension check at lines 245-249 iterates `dim_cols` in **declaration order**
(`column_order`), while the column-name/type check iterates `row` in **alphabetical order**. Two
different ordering policies sit five lines apart. Test (a) (`'block' missing`) and test (b)
(`'date_time' missing`) both pass because the missing-dim check fires first; if a caller omits a
dim column AND has a type mismatch, the dim-missing error wins. That's fine, but the dual policy
is not documented anywhere.

**Fix:** Either (a) document the contract — "the first error reported, when multiple are present,
is implementation-defined" — in the public method comment in `include/quiver/database.h`, or
(b) iterate in declaration order for both checks (use `table_def->column_order` to drive the
type-validation loop) so tests don't silently depend on alphabetic ordering. Option (b) is
preferred — it makes error messages stable under schema evolution.

```cpp
// Option (b): iterate in declaration order, matching the missing-dim check.
for (const auto& col_name : table_def->column_order) {
    if (col_name == "id") continue;
    auto it = row.find(col_name);
    if (it == row.end()) continue;  // optional / omitted value column
    const auto& value = it->second;
    auto type_it = schema_types.find(col_name);
    // ... existing checks
}
// Also: detect unknown caller keys with a second pass keyed off `row`.
for (const auto& [col_name, _] : row) {
    if (schema_types.find(col_name) == schema_types.end()) {
        throw std::runtime_error("Cannot add_time_series_row: column '" + col_name +
                                 "' not found in group '" + group + "' for collection '" +
                                 collection + "'");
    }
}
```

### WR-02: SQL build iteration order diverges from `update_time_series_group`

**File:** `src/database_time_series.cpp:269-279` vs `src/database_time_series.cpp:166-183`
**Issue:** `add_time_series_row` builds the INSERT column list by iterating `row` directly (line
274: `for (const auto& [col_name, value] : row)`), producing alphabetical column ordering in the
generated SQL. By contrast, `update_time_series_group` (line 167) builds a `value_columns` list
from the first row, then prepends the dim_col explicitly (line 175: `"(id, " + dim_col`), then
appends value columns. Both routes produce valid SQL — SQLite does not care about column order
in `INSERT ... (cols) VALUES (...)` — but the two functions look like they should be structurally
parallel and aren't. A future reader who refactors one is liable to "fix" the other inconsistently.

There is also a subtle correctness concern: in `add_time_series_row` the dimension columns appear
in the SQL list because they happen to be present in `row` and the map iteration emits them. If
a maintainer ever changes the dim-existence guard at lines 245-249 from `row.find(dim_col) ==
row.end()` to something that allows omitted-dim-with-default behavior, the SQL would silently drop
the dim cols. The current implementation is correct, but the coupling is implicit.

**Fix:** Either (a) refactor `update_time_series_group` to also iterate the row directly (removing
the structural divergence), or (b) make `add_time_series_row` explicitly emit `dim_cols` first and
then iterate `row` for value-only columns (mirroring lines 175-179). Option (b) makes the dim/value
distinction explicit in the SQL build:

```cpp
std::string insert_sql = "INSERT OR REPLACE INTO " + ts_table + " (id";
std::string placeholders = "?";
std::vector<Value> parameters;
parameters.emplace_back(id);

for (const auto& dim_col : dim_cols) {
    insert_sql += ", " + dim_col;
    placeholders += ", ?";
    parameters.emplace_back(row.at(dim_col));
}
for (const auto& [col_name, value] : row) {
    if (std::find(dim_cols.begin(), dim_cols.end(), col_name) != dim_cols.end()) continue;
    insert_sql += ", " + col_name;
    placeholders += ", ?";
    parameters.emplace_back(value);
}
insert_sql += ") VALUES (" + placeholders + ")";
```

### WR-03: FK violation on unknown `id` surfaces a raw SQLite error, not Pattern 2

**File:** `src/database_time_series.cpp:207-285`
**Issue:** When the caller passes `id = 999` and no `Resource` (or `Collection`) row exists, the
`INSERT OR REPLACE` will fail on the `id REFERENCES Resource(id)` FK constraint. The failure
propagates out of `execute()` as `"Failed to execute statement: FOREIGN KEY constraint failed"`
(Pattern 3), which is informationally weak — the caller does not know which FK failed or what to
do. `update_time_series_group` has the same gap, so this isn't a regression introduced by this
phase, but the new method inherits it. The cleaner contract (and one the docstring in
`database.h:120-125` implicitly suggests by calling out "validation matches update_time_series_group")
is a Pattern-2 throw: `"Element not found: id N in collection 'Resource'"`. There is no test
covering this path for either method.

**Fix:** Add an existence check immediately after `require_collection` (or as a helper called by
both methods):

```cpp
auto exists = execute("SELECT 1 FROM " + collection + " WHERE id = ?", {id});
if (exists.empty()) {
    throw std::runtime_error("Element not found: id " + std::to_string(id) +
                             " in collection '" + collection + "'");
}
```

If the project policy is to let SQLite enforce FK and surface its message verbatim ("clean code
over defensive code"), then this should be documented in the method comment in `database.h` so
binding authors know not to wrap or rewrite the FK error.

## Info

### IN-01: CLAUDE.md not updated to document `add_time_series_row`

**File:** `CLAUDE.md` (Database Class section)
**Issue:** The project principle in `CLAUDE.md` states *"Self-Updating: Always keep CLAUDE.md up to
date with codebase changes."* The Database Class API listing (around the "Time series:" bullet)
documents `read_time_series_group`, `update_time_series_group`, and `read_time_series_row`, but the
newly added `add_time_series_row` is absent. The Cross-Layer Naming Conventions table also lists
time-series read/update but not the new add method.
**Fix:** Add a bullet under "Time series:" naming the new method and a row to the cross-layer
examples table (note: bindings are out of scope for this phase, so the "Julia/Dart/Lua" cells
should be marked N/A for now, with a comment that they will be filled in by Phase 02).

### IN-02: Redundant `schema_types` map

**File:** `src/database_time_series.cpp:236-241`
**Issue:** `schema_types` is built by walking `table_def->columns` and copying `col.type` keyed by
`col_name`, then used at line 252 (`schema_types.find(col_name)`) and line 257
(`data_type_to_string(it->second)`). The same information is already available via
`table_def->get_column(col_name)`, which returns `const ColumnDefinition*` whose `.type` is the
identical `DataType`. The local `schema_types` map duplicates the schema's own lookup with no added
behavior, and the parallel block at lines 129-134 in `update_time_series_group` has the same
redundancy. Removing it shrinks the function by 5 lines and removes a map allocation per call.
**Fix:**
```cpp
for (const auto& [col_name, value] : row) {
    const auto* col = table_def->get_column(col_name);
    if (!col || col_name == "id") {
        throw std::runtime_error("Cannot add_time_series_row: column '" + col_name +
                                 "' not found in group '" + group + "' for collection '" +
                                 collection + "'");
    }
    if (!internal::value_matches_type(value, col->type)) {
        throw std::runtime_error("Cannot add_time_series_row: column '" + col_name + "' has type " +
                                 data_type_to_string(col->type) + " but received " +
                                 internal::value_type_name(value));
    }
}
```
(Note: the `col_name == "id"` guard preserves the existing behavior of treating `id` as not-a-value-column;
without it, a caller passing `{id: 5}` would slip past validation but then double-bind the `id`
parameter into the SQL — currently impossible because `schema_types` excludes `id`.)

### IN-03: No coverage for missing-collection / missing-element error paths

**File:** `tests/test_database_time_series.cpp` (no test)
**Issue:** The 9 new tests cover insert, upsert, multi-dim PK, partial value columns, validation
errors, and transaction semantics — but there is no test for:
  1. `add_time_series_row("NonexistentCollection", ...)` → should throw via `require_collection`
     with `"Cannot add_time_series_row: collection not found: NonexistentCollection"`.
  2. `add_time_series_row("Resource", "nonexistent_group", ...)` → should throw via
     `find_time_series_table` with `"Time series group 'nonexistent_group' not found for ..."`.
  3. `add_time_series_row("Resource", "load", 999, {valid_row})` → FK violation (see WR-03).

Adding these closes the symmetry with `update_time_series_group` and pins the error pattern for
the new method.
**Fix:** Add three short tests mirroring the existing `TimeSeriesGroupNotFound` /
`TimeSeriesCollectionNotFound` style.

### IN-04: Comment about "INSERT OR REPLACE ... value column omitted" is slightly misleading

**File:** `src/database_time_series.cpp:265-268`
**Issue:** The comment claims *"Any value column omitted from the caller's row is not listed in
the INSERT, so SQLite leaves it as the column DEFAULT (NULL for nullable value columns)."* This is
true for **INSERT**, but for **INSERT OR REPLACE** when there is a PK conflict, SQLite **deletes
the existing row** and inserts the new one. The omitted value columns will get the **column
DEFAULT** (NULL), **not** the previous row's value. The current comment glosses over this — a reader
might assume "omitted = preserve previous value", which is the opposite of what happens. The
`AddTimeSeriesRowPartialValueColumns` test only exercises insert (no upsert), so this distinction
is not visible in tests either.
**Fix:** Rewrite the comment to be explicit:

```cpp
// INSERT OR REPLACE: if a row with the same PK exists, SQLite DELETEs it and INSERTs
// a fresh row. Value columns omitted from the caller's `row` are NOT preserved from
// the previous row — they get the schema DEFAULT (NULL for nullable value columns).
// Callers who want partial-update semantics must read-modify-write through
// read_time_series_group / update_time_series_group instead.
```

Optionally add an upsert test where the second call omits a column and asserts that the omitted
column is NULL in the final row (proving the destructive-replace semantic).

---

_Reviewed: 2026-05-19_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
