---
phase: 02-c-api-quiver-database-add-time-series-row
reviewed: 2026-05-19T00:00:00Z
depth: standard
files_reviewed: 3
files_reviewed_list:
  - include/quiver/c/database.h
  - src/c/database_time_series.cpp
  - tests/test_c_api_database_time_series.cpp
findings:
  critical: 0
  warning: 2
  info: 4
  total: 6
status: issues_found
---

# Phase 02: Code Review Report -- `quiver_database_add_time_series_row` (C API)

**Reviewed:** 2026-05-19
**Depth:** standard
**Files Reviewed:** 3
**Status:** issues_found

## Summary

The new C API wrapper `quiver_database_add_time_series_row` is a thin, faithful
forwarder to `Database::add_time_series_row`. The marshaling logic is parallel
to the existing `quiver_database_update_time_series_group` (single row vs. row
loop), uses the canonical try/catch + `quiver_set_last_error` pattern, and the
header docstring is colocated with the declaration. Twelve new GTest cases pin
the contract at the FFI boundary, including a transaction-matrix test that
exercises the nest-aware `TransactionGuard` indirectly through three phases.

**No correctness bugs and no security issues found in the new code.**
Marshaling is type-tagged (caller-declared types in `column_types[]`), and the
column names that flow into the underlying SQL are validated server-side
against `schema_types` before reaching the SQL builder -- no injection path
through this C API entry point.

The findings below are quality/consistency issues:

- 2 **WARNINGS**: CLAUDE.md not updated (project's "Self-Updating" principle is
  explicit), and the new header docstring inherits the "INSERT OR REPLACE
  destructive replace" documentation gap also flagged in the Phase 01 review
  (IN-04) -- callers reading only `database.h` will not learn that omitted
  value columns are *not* preserved across upsert.
- 4 **INFOs**: a misleading comment in `AddTimeSeriesRowMultiDimInsert`, a
  small test-coverage gap (per-argument null-arg granularity + missing-group /
  missing-collection / missing-element symmetry with `update_time_series_group`),
  a minor docstring typo on the string pointer type, and a small DRY
  observation that the new C API function duplicates the `update`-variant
  marshaling switch with only the loop dimension changed.

## Warnings

### WR-01: CLAUDE.md not updated to document `quiver_database_add_time_series_row`

**File:** `CLAUDE.md` (Database Class section, Cross-Layer Naming Conventions table)
**Issue:** The project's `CLAUDE.md` is explicit: *"Self-Updating: Always keep
CLAUDE.md up to date with codebase changes."* The same omission was flagged
in the Phase 01 REVIEW (IN-01) for the C++ core method, and Phase 01 closed
without fixing it. Phase 02 ships the C API binding without addressing it
either. As a result:

- The "Database Class" bullet listing time-series methods (`read_time_series_group`,
  `update_time_series_group`, `read_time_series_row`) still does not include
  `add_time_series_row`.
- The "Cross-Layer Naming Conventions" representative-examples table has no
  row for the new method, so the next contributor wiring Julia/Dart/Lua/Python
  bindings (Phase 03) does not have the canonical name mapping to copy from.
- The "Time series read/update" patterns block in the C API section does not
  mention that there is now a single-row entry point alongside the bulk
  `update_time_series_group`.

This compounds: every layer beyond C++ (C API, future bindings) needs to
re-derive the contract from source rather than from project documentation.

**Fix:** Add a bullet under the "Time series:" entry in the Database Class
section, and one row to the cross-layer table:

```markdown
# Database Class
- Time series: `read_time_series_group()`, `update_time_series_group()`,
  `add_time_series_row()` (single-row upsert; PK = id + every dim column) --
  ...
```

```markdown
# Cross-Layer table
| Time series row insert | `add_time_series_row()` |
  `quiver_database_add_time_series_row()` | (TBD Phase 03) |
  (TBD Phase 03) | (TBD Phase 03) |
```

### WR-02: Header docstring omits the "INSERT OR REPLACE destructive replace" caveat

**File:** `include/quiver/c/database.h:302-307`
**Issue:** The new doc comment says:

```c
// Upserts on the time-series PK (id + every dimension column);
// calling twice with the same PK overwrites value columns
```

This is true for the case where the second call supplies the same set of value
columns. But the underlying C++ implementation uses `INSERT OR REPLACE`, which
on PK conflict **deletes the existing row** and inserts a fresh one. Value
columns omitted from the *second* call do not retain their previous values --
they get the schema default (NULL for nullable value columns). The same gap was
documented in Phase 01 REVIEW IN-04 against the C++ method comment, but the C
API docstring now repeats it almost verbatim ("overwrites value columns") in a
form that a binding author could easily read as "supplied columns overwrite,
omitted columns preserve". The Phase 02 test `AddTimeSeriesRowPartialValueColumns`
only exercises *inserts* (two different PKs), so this distinction is not visible
in the test suite either.

Binding authors writing Julia/Dart/Lua wrappers in Phase 03 will copy this
docstring -- and propagate the ambiguity into every binding.

**Fix:** Replace the one-line upsert sentence with an explicit clarification:

```c
// Upserts on the time-series PK (id + every dimension column): a second call
// with the same PK destructively replaces the existing row (INSERT OR REPLACE).
// Value columns omitted from the second call are NOT preserved -- they get the
// schema default (NULL for nullable value columns). For partial-update
// semantics, read with quiver_database_read_time_series_group, modify, and
// write back with quiver_database_update_time_series_group.
```

Optionally add one GTest case asserting the destructive-replace semantic
(insert with `flag=1`, upsert same PK omitting `flag`, expect `flag` to be
NULL/0 on read-back).

## Info

### IN-01: Misleading comment about value-column read ordering in `AddTimeSeriesRowMultiDimInsert`

**File:** `tests/test_c_api_database_time_series.cpp:1026-1027`
**Issue:** The test comment claims the read order from
`quiver_database_read_time_series_group` is *"dim_col first, then value columns
alphabetically -- block, date_time, flag, load"*. The bracketed list is wrong:
the actual order is `date_time` (dim col, first), then `block`, `flag`, `load`
(value columns alphabetically). The comment puts `block` first and inserts
`date_time` in the middle, which contradicts the "dim_col first" half of the
same sentence. The test code itself does not depend on positional ordering --
lines 1028-1043 build a name-to-index map and assert via that map -- so this
is a purely documentary defect and does not affect test correctness.

A future reader who skims the comment instead of the code will get the wrong
mental model of the read API.

**Fix:** Correct the comment to match reality:

```cpp
// Locate each column index (read order is dim_col first, then value columns
// alphabetically -- date_time, block, flag, load).
```

### IN-02: `AddTimeSeriesRowNullArguments` does not isolate each pointer-arg

**File:** `tests/test_c_api_database_time_series.cpp:1547-1586, 1625-1646`
**Issue:** `AddTimeSeriesRowNullArguments` exercises three cases (null `db`,
null `collection`, null `group`). `AddTimeSeriesRowNullColumnArraysWithCount`
exercises one case where all three of `column_names`, `column_types`, and
`column_data` are simultaneously NULL with `column_count > 0`. Because
`QUIVER_REQUIRE_3(column_names, column_types, column_data)` short-circuits on
the first null, only the `column_names` branch of the macro is actually
verified. There is no test that confirms passing a valid `column_names` with
a NULL `column_types` (or a NULL `column_data`) returns `QUIVER_ERROR` with
`"Null argument: column_types"` (or `"... column_data"`). This is symmetric
to a gap that already exists for the sibling `UpdateTimeSeriesGroup`-family
tests, so this is a *consistency* finding rather than a regression -- but the
phase added 12 tests and could have closed it cheaply.

**Fix:** Add two short cases (or extend `AddTimeSeriesRowNullColumnArraysWithCount`)
that vary which of `column_names` / `column_types` / `column_data` is the
single NULL pointer, asserting the per-arg error string from
`QUIVER_REQUIRE_1`. Example:

```cpp
// column_names valid, column_types NULL.
EXPECT_EQ(quiver_database_add_time_series_row(db, "Resource", "load", 1,
              col_names, nullptr, col_data, 3), QUIVER_ERROR);
EXPECT_NE(std::string(quiver_get_last_error()).find("Null argument: column_types"),
          std::string::npos);
```

### IN-03: No test for the missing-collection / missing-group / missing-element error paths

**File:** `tests/test_c_api_database_time_series.cpp` (no test)
**Issue:** Phase 01's REVIEW IN-03 noted the same gap for the C++ core method
(`add_time_series_row` was not tested against a nonexistent collection /
group / FK-violating `id`). Phase 02 closes the C API wrapping but did not
extend coverage to the wrapped error paths. Concretely, there is no test for:

1. `quiver_database_add_time_series_row(db, "NonexistentCollection", ...)`
   -- should surface `"Cannot add_time_series_row: collection not found:
   NonexistentCollection"` via `quiver_get_last_error`.
2. `quiver_database_add_time_series_row(db, "Collection", "nonexistent_group",
   ...)` -- should surface `"Time series group not found: 'nonexistent_group'
   in collection 'Collection'"` (Pattern 2 from `find_time_series_table`).
3. `quiver_database_add_time_series_row(db, "Collection", "data",
   /*id=*/999, valid_row)` with no element 999 -- should surface the FK
   violation (currently Pattern 3 from SQLite, per WR-03 in Phase 01 review).

The wrapper itself contains no logic for these paths beyond forwarding -- but
adding three short tests would lock the contract from the C ABI side and
make any future C++-layer change to the message format an immediate, visible
break.

**Fix:** Add three short tests mirroring the existing
`UpdateTimeSeriesGroupUnknownColumn` / `UpdateTimeSeriesGroupMissingDimension`
style (substring match on the expected error message).

### IN-04: Minor docstring typo + DRY observation on marshaling switch

**File:** `include/quiver/c/database.h:305`; `src/c/database_time_series.cpp:312-339`
**Issue:** Two small items combined into one finding:

1. The header docstring states *"const char** for STRING/DATE_TIME"*, but the
   implementation casts to `const char* const*` (line 325). Both are
   ABI-compatible with the `const char*[]` the tests pass in, so there is no
   runtime defect -- but for consistency with `update_time_series_group`'s
   exposure (line 290 uses the same shorthand), at least both should agree.
   The current value here is `"const char**"`; that wording is fine, but
   the implementation comment could note the explicit `const char* const*`
   reinterpretation.

2. The new function's marshaling switch (lines 316-330) is a structural
   duplicate of the inner `update_time_series_group` switch (lines 272-286),
   differing only in the row index (hard-coded `[0]` vs. variable `[r]`).
   This is exactly the kind of small duplication CLAUDE.md frames as
   *"Simple solutions over complex abstractions"* -- so refactoring is *not*
   automatically called for. But if a future change adds a fifth scalar type
   (e.g., `QUIVER_DATA_TYPE_BOOL`), both switches must be updated in lock-step
   or one will silently throw "unknown column type" while the other accepts.
   A helper function `marshal_value(int type, const void* col, size_t idx)`
   returning `quiver::Value` would unify them at the cost of one indirection.

**Fix:** Leave the docstring as-is or align the typing shorthand across both
function declarations. Defer (or skip) the marshal-helper extraction unless a
third C API entry point needs the same switch -- two is below the
duplication threshold that justifies the abstraction tax.

---

_Reviewed: 2026-05-19_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
