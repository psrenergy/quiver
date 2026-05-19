# Phase 2: C API quiver_database_add_time_series_row - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-05-19
**Phase:** 02-c-api-quiver-database-add-time-series-row
**Areas discussed:** Single-row FFI shape (resolved by user directive), Plan split, Test scenario breadth, DATE_TIME column_type handling

---

## Gray Area Selection (multi-select)

| Option | Description | Selected |
|--------|-------------|----------|
| Single-row FFI shape | Multi-row update shape with implicit row_count=1 vs. scalar-per-column shape with no row_count | Resolved by user directive |
| Plan split for Phase 2 | One combined plan vs. mirror Phase 1's split | Default applied (see below) |
| Test scenario breadth | Strictly 9 mirror scenarios vs. mirror + FFI-boundary tests | Default applied (see below) |
| DATE_TIME column_type handling | Include interchangeability test vs. skip | Default applied (see below) |

**User's response:** "You need to stick to the other code pattern"

**Interpretation:** Mirror `quiver_database_update_time_series_group`'s marshaling pattern. This directive directly resolves the FFI signature gray area; the remaining three default to the same "stick to the established pattern" interpretation under the session's no-clarifying-questions instruction.

---

## Single-row FFI shape

| Option | Description | Selected |
|--------|-------------|----------|
| Drop `row_count`, scalar-per-column data pointers | Each `column_data[c]` points to one typed value; mirrors C++ single-row signature | ✓ |
| Keep `row_count` for absolute symmetry with update | Always pass `row_count=1`; `column_data[c]` is 1-element typed array | |

**User's choice:** Drop `row_count` (resolved by user directive + REQUIREMENTS.md CAPI-11 which explicitly lists `column_names[] / column_types[] / column_data[] / column_count` and does not list `row_count`).

**Notes:** "Stick to the other code pattern" specifically refers to the marshaling pattern (typed-array switch, `QUIVER_REQUIRE`, try/catch+set_last_error). The phase being single-row by definition removes the row-loop and `row_count` parameter; the four-array shape is preserved.

---

## Plan split

| Option | Description | Selected |
|--------|-------------|----------|
| Mirror Phase 1: two-plan split | Plan 02-01 = decl + impl, Plan 02-02 = tests | ✓ |
| Combined plan | impl + tests in a single plan | |

**User's choice:** Default applied — mirror Phase 1.

**Notes:** Phase 1's split shipped in ~10min total (6min impl + 4min tests) with atomic per-task commits and zero regressions. Maintaining the same plan shape across phases gives a clean audit trail and is the lowest-risk choice under the "stick to the pattern" interpretation.

---

## Test scenario breadth

| Option | Description | Selected |
|--------|-------------|----------|
| Mirror + FFI-boundary additions | 9 C++ scenarios + 3 FFI-specific (NULL args, unknown column_type, NULL column arrays w/ count>0) | ✓ |
| Strict 9-scenario mirror | Only the C++ scenarios | |

**User's choice:** Default applied — mirror + FFI-boundary additions.

**Notes:** The existing `UpdateTimeSeriesGroup*` test block in `tests/test_c_api_database_time_series.cpp` follows the mirror-plus-FFI-boundary shape (it has both the C++ mirror tests and `TimeSeriesNullArguments`, `UpdateTimeSeriesGroupUnknownColumnType`, `UpdateTimeSeriesGroupNullColumnArraysWithCount`). "Stick to the pattern" extends to test conventions — the new `AddTimeSeriesRow*` block should match shape-for-shape.

---

## DATE_TIME column_type handling

| Option | Description | Selected |
|--------|-------------|----------|
| Skip — already proven for update path | Shared marshaling switch; duplicate test adds no coverage | ✓ |
| Add `AddTimeSeriesRowDateTimeStringInterchangeable` | Belt-and-suspenders mirror of `UpdateTimeSeriesGroupDateTimeStringInterchangeable` | |

**User's choice:** Default applied — skip.

**Notes:** The `QUIVER_DATA_TYPE_STRING` / `QUIVER_DATA_TYPE_DATE_TIME` branches both go through `std::string(column_data[c])` — identical code path between update and add. The existing update test (`tests/test_c_api_database_time_series.cpp:796`) already locks the behavior. If Phase 3 binding tests surface a regression, that's the right place to add coverage.

---

## Claude's Discretion

- SQL column iteration order inside the implementation — defer to C++ method's existing behavior (already proven in Phase 1).
- Internal helper extraction (e.g., shared `convert_typed_column_value` between update and add) — defer to executor judgment at review time; prefer literal duplication unless awkward at review per "simple over abstract" project principle.
- Exact test ordering inside `tests/test_c_api_database_time_series.cpp` — group `AddTimeSeriesRow*` immediately after `UpdateTimeSeriesGroup*` block.

## Deferred Ideas

- **`quiver_database_add_time_series_rows` (plural batched variant)** — CORE-15, deferred to v2 per REQUIREMENTS.md. Not in this phase.
- **Multi-element add (rows for multiple `id`s in one call)** — Out of scope per REQUIREMENTS.md, symmetric with update's per-id shape.
- **Shared marshaling helper between update and add** — Possible later cleanup if review surfaces awkward duplication.
