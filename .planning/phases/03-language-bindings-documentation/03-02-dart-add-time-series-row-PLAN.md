---
id: 03-02-dart-add-time-series-row
phase: 03
plan: 02
type: execute
wave: 1
depends_on: []
files_modified:
  - bindings/dart/lib/src/ffi/bindings.dart
  - bindings/dart/lib/src/database_update.dart
  - bindings/dart/test/database_time_series_test.dart
requirements: [DART-11]
autonomous: true

must_haves:
  truths:
    - "Dart users can call db.addTimeSeriesRow(collection, group, id, row) where row is a Map<String, Object>"
    - "Calling addTimeSeriesRow twice with the same dimension PK upserts (value columns overwritten)"
    - "Multi-dimension PK schemas (date_time + block) round-trip through the Dart wrapper"
    - "bindings/dart/test/test.bat exits 0 with the new group('Add Time Series Row') present"
  artifacts:
    - path: "bindings/dart/lib/src/ffi/bindings.dart"
      provides: "Regenerated FFI binding containing the quiver_database_add_time_series_row function"
      contains: "quiver_database_add_time_series_row"
    - path: "bindings/dart/lib/src/database_update.dart"
      provides: "Dart idiomatic wrapper addTimeSeriesRow"
      contains: "void addTimeSeriesRow"
    - path: "bindings/dart/test/database_time_series_test.dart"
      provides: "Insert/upsert/multi-dim coverage"
      contains: "group('Add Time Series Row'"
  key_links:
    - from: "bindings/dart/lib/src/database_update.dart"
      to: "bindings.quiver_database_add_time_series_row"
      via: "FFI call through the regenerated bindings.dart"
      pattern: "bindings\\.quiver_database_add_time_series_row"
    - from: "bindings/dart/lib/src/database_update.dart"
      to: "Arena"
      via: "arena.releaseAll() releases all native allocations after the ccall"
      pattern: "arena\\.releaseAll"
---

<objective>
Ship the Dart idiomatic wrapper `Database.addTimeSeriesRow(String collection, String group, int id, Map<String, Object> row)` that mirrors the marshaling structure of the sibling `updateTimeSeriesGroup` (Map + runtimeType dispatch + Arena allocation), structurally simplified to single-element typed arrays per column with no `rowCount` parameter. Add `group('Add Time Series Row')` coverage (insert + upsert + multi-dim) and run `bindings/dart/test/test.bat` to verify the round-trip.

Purpose: Closes DART-11. Per D-02 / D-05 / D-06 / D-07 / D-08 / D-10 in `.planning/phases/03-language-bindings-documentation/03-CONTEXT.md`.
Output: Regenerated `bindings.dart` with the new FFI binding, new `addTimeSeriesRow` method alongside `updateTimeSeriesGroup` in the `DatabaseUpdate` extension, new tests under the existing `database_time_series_test.dart`.
</objective>

<execution_context>
@$HOME/.claude/get-shit-done/workflows/execute-plan.md
@$HOME/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@.planning/PROJECT.md
@.planning/ROADMAP.md
@.planning/STATE.md
@.planning/phases/03-language-bindings-documentation/03-CONTEXT.md
@CLAUDE.md
@include/quiver/c/database.h
@bindings/dart/lib/src/database_update.dart
@bindings/dart/test/database_time_series_test.dart

<interfaces>
<!-- C API symbol the Dart wrapper marshals against, per CONTEXT.md canonical_refs -->

From include/quiver/c/database.h (lines 308-315):
```c
QUIVER_C_API quiver_error_t quiver_database_add_time_series_row(quiver_database_t* db,
                                                                const char* collection,
                                                                const char* group,
                                                                int64_t id,
                                                                const char* const* column_names,
                                                                const int* column_types,
                                                                const void* const* column_data,
                                                                size_t column_count);
```

Sibling wrapper template (the exact structure to mirror), from bindings/dart/lib/src/database_update.dart:47-149:
- Method signature: `void updateTimeSeriesGroup(String collection, String group, int id, Map<String, List<Object>> data)`
- Allocates an `Arena` and frees it in `finally { arena.releaseAll(); }`
- Empty-data branch passes nullptrs and `0, 0` for column_count, row_count
- Validates equal vector lengths
- Per-entry dispatch on `entry.value.first.runtimeType` (`is int`, `is double`, `is String`, `is DateTime`)
- Each branch allocates `arena<Int64>(rowCount)` / `arena<Double>(rowCount)` / `arena<Pointer<Char>>(rowCount)` and fills it; for DateTime uses `dateTimeToString(...).toNativeUtf8(allocator: arena).cast()`
- Final `check(bindings.quiver_database_update_time_series_group(_ptr, collection_nat, group_nat, id, columnNames, columnTypes, columnData, columnCount, rowCount))`

For `addTimeSeriesRow`, the row is `Map<String, Object>` (scalar per column, not a list). `columnCount == row.length`. No `rowCount` argument. Per-entry dispatch on `entry.value.runtimeType` (without `.first`). Each branch allocates a length-1 native array.
</interfaces>
</context>

<tasks>

<task type="auto">
  <name>Task 1: Regenerate Dart bindings.dart</name>
  <read_first>
    include/quiver/c/database.h (lines 295-326)
    bindings/dart/generator/generator.bat
    bindings/dart/lib/src/ffi/bindings.dart (verify current state — should NOT yet contain the symbol)
    CLAUDE.md (Dart Notes — DLL refresh procedure)
  </read_first>
  <action>
    Run `bindings/dart/generator/generator.bat` from the repository root. The generator parses `include/quiver/c/database.h` and emits `bindings/dart/lib/src/ffi/bindings.dart`. Do not hand-edit `bindings.dart`. Commit only the regenerated file.
  </action>
  <verify>
    <automated>findstr /C:"quiver_database_add_time_series_row" bindings\dart\lib\src\ffi\bindings.dart</automated>
  </verify>
  <done>`bindings/dart/lib/src/ffi/bindings.dart` contains the FFI typedef + lookup binding for `quiver_database_add_time_series_row` with the 8-parameter signature (db, collection, group, id, columnNames, columnTypes, columnData, columnCount) and no rowCount.</done>
</task>

<task type="auto" tdd="true">
  <name>Task 2: Add Dart addTimeSeriesRow wrapper</name>
  <files>bindings/dart/lib/src/database_update.dart</files>
  <read_first>
    bindings/dart/lib/src/database_update.dart (lines 47-149 — the sibling `updateTimeSeriesGroup` template)
    bindings/dart/lib/src/ffi/bindings.dart (verify Task 1 regeneration committed the new binding)
    include/quiver/c/database.h (lines 308-315)
    .planning/phases/03-language-bindings-documentation/03-CONTEXT.md (D-02)
  </read_first>
  <behavior>
    - Method `void addTimeSeriesRow(String collection, String group, int id, Map<String, Object> row)` exists inside the `extension DatabaseUpdate on Database` in `bindings/dart/lib/src/database_update.dart`
    - First calls `_ensureNotClosed()` (matches sibling)
    - Allocates `final arena = Arena()` and wraps everything in `try { ... } finally { arena.releaseAll(); }`
    - For each `MapEntry`: dispatches on `entry.value.runtimeType` (is int / is double / is String / is DateTime); allocates a length-1 native array of the appropriate type
    - DateTime branch uses `dateTimeToString(entry.value as DateTime).toNativeUtf8(allocator: arena).cast()`
    - Final `check(bindings.quiver_database_add_time_series_row(_ptr, collection.toNativeUtf8(...).cast(), group.toNativeUtf8(...).cast(), id, columnNames, columnTypes, columnData, columnCount))` — 8 arguments, no rowCount
    - Unsupported runtimeType throws `ArgumentError('Unsupported value type: ${entry.value.runtimeType}')` (mirrors sibling's error string — Dart ArgumentError is a pre-FFI guard, not a binding error string carrying "Cannot add_time_series_row:")
    - No binding-layer "Cannot add_time_series_row:" string introduced
  </behavior>
  <action>
    In `bindings/dart/lib/src/database_update.dart`, alongside `updateTimeSeriesGroup` (insert immediately after the `updateTimeSeriesGroup` method body ends, before the "Update time series files" section banner — the natural sibling location per D-02), add a new `void addTimeSeriesRow(String collection, String group, int id, Map<String, Object> row)` method on the `DatabaseUpdate` extension. Mirror the marshaling structure of `updateTimeSeriesGroup` exactly, with these adaptations:
    - Drop the empty-data clear-all branch — add_time_series_row has no clear semantics (D-10 says the C API will surface a canonical error if dimensions are missing).
    - Drop the equal-row-count validation loop — values are scalars.
    - Per-entry dispatch is on `entry.value.runtimeType` (no `.first` because there is no list).
    - In each branch, allocate `arena<Int64>(1)`, `arena<Double>(1)`, or `arena<Pointer<Char>>(1)` and write the single value at index `[0]`.
    - Final ccall is the 8-arg `bindings.quiver_database_add_time_series_row(...)` — no `rowCount`.
    No new helpers; no factoring with `updateTimeSeriesGroup`; "simple over abstract" per `<deferred>` in CONTEXT.md.
  </action>
  <verify>
    <automated>findstr /C:"void addTimeSeriesRow" bindings\dart\lib\src\database_update.dart &amp;&amp; findstr /C:"bindings.quiver_database_add_time_series_row" bindings\dart\lib\src\database_update.dart</automated>
  </verify>
  <done>`bindings/dart/lib/src/database_update.dart` contains the new `void addTimeSeriesRow` method on the `DatabaseUpdate` extension with Map dispatch, Arena lifetime, and the 8-argument FFI call. No `rowCount` argument. No new "Cannot add_time_series_row:" string.</done>
</task>

<task type="auto" tdd="true">
  <name>Task 3: Add Dart group('Add Time Series Row') coverage</name>
  <files>bindings/dart/test/database_time_series_test.dart</files>
  <read_first>
    bindings/dart/test/database_time_series_test.dart (lines 114-204 — the existing `group('Time Series Update')` as test-structure reference)
    bindings/dart/test/database_time_series_test.dart (lines 346-end — the `group('Multi-Column Time Series')` block for multi-column patterns)
    tests/schemas/valid/collections.sql (primary single-dim schema)
    tests/schemas/valid/multi_dim_time_series.sql (composite-PK schema for multi-dim test)
    .planning/phases/03-language-bindings-documentation/03-CONTEXT.md (D-07 Dart scenario list)
  </read_first>
  <behavior>
    - A new `group('Add Time Series Row', () { ... })` block exists in `database_time_series_test.dart`, placed after the existing `group('Multi-Column Time Series')` block
    - Test 1 — insert: create one element, call `db.addTimeSeriesRow('Collection', 'data', id, {'date_time': '2024-01-01', 'value': 10.0})`, then assert via `readTimeSeriesGroup` that the row is present
    - Test 2 — upsert: insert, then call again with same `date_time` and a new `value` (e.g. 99.0); assert `readTimeSeriesGroup` returns one row with `value == 99.0`
    - Test 3 — multi-dim happy path: use `tests/schemas/valid/multi_dim_time_series.sql`; call with both `date_time` and `block` populated plus a value column; assert round-trip
  </behavior>
  <action>
    In `bindings/dart/test/database_time_series_test.dart`, append a new `group('Add Time Series Row', () { ... })` block at the end of the file (after the existing `group('Multi-Column Time Series')` block). Use the same fixture-setup style as the existing time-series test groups (load `tests/schemas/valid/collections.sql` via `Database.fromSchema`, create `Configuration`, create one `Collection` element). For Test 3, load `tests/schemas/valid/multi_dim_time_series.sql` via the same `fromSchema` pattern. Use `expect(...)` for round-trip assertions. Per D-07 Dart, no error-path tests are required — they are covered by existing FFI error-relay tests elsewhere in the Dart suite.
  </action>
  <verify>
    <automated>findstr /C:"group('Add Time Series Row'" bindings\dart\test\database_time_series_test.dart</automated>
  </verify>
  <done>`bindings/dart/test/database_time_series_test.dart` contains `group('Add Time Series Row', ...)` with three test cases (insert, upsert, multi-dim). Existing groups unmodified.</done>
</task>

<task type="auto">
  <name>Task 4: Run Dart test suite</name>
  <files>bindings/dart/test/test.bat</files>
  <read_first>
    bindings/dart/test/test.bat
    CLAUDE.md (section "Build & Test" + "Dart Notes" — confirms `bindings/dart/test/test.bat` is the command and notes the DLL refresh procedure)
  </read_first>
  <action>
    Run `bindings/dart/test/test.bat` from the repository root. All tests including the new `group('Add Time Series Row')` must pass. If a stale DLL (incremental rebuild fails to pick up the new C symbol) causes the new tests to fail with a missing-symbol error, delete `bindings/dart/.dart_tool/hooks_runner/` and `bindings/dart/.dart_tool/lib/` per CLAUDE.md "Dart Notes" and re-run. Default first attempt is incremental rebuild per Claude's Discretion in CONTEXT.md.
  </action>
  <verify>
    <automated>bindings\dart\test\test.bat</automated>
  </verify>
  <done>`bindings/dart/test/test.bat` exits with code 0. Test output shows the three new "Add Time Series Row" cases as passing alongside the existing time-series tests.</done>
</task>

</tasks>

<threat_model>
## Trust Boundaries

| Boundary | Description |
|----------|-------------|
| Dart Map → C ABI | Caller-supplied scalar values cross the FFI boundary through Arena-allocated native arrays; the Dart layer owns the native memory lifetime via the Arena. |

## STRIDE Threat Register

| Threat ID | Category | Component | Disposition | Mitigation Plan |
|-----------|----------|-----------|-------------|-----------------|
| T-03-04 | Tampering | `addTimeSeriesRow` marshaling | mitigate | All native allocations go through the per-call `Arena`; `arena.releaseAll()` runs in `finally`. Mirrors the locked `updateTimeSeriesGroup` pattern. |
| T-03-05 | Information Disclosure | Error messages | accept | Error strings come from C++/C API only (per CLAUDE.md). No PII / no sensitive paths leaked by binding layer. |
| T-03-06 | Denial of Service | Misuse via wrong runtimeType | mitigate | The pre-FFI `runtimeType` dispatch throws `ArgumentError` for unsupported types — fails fast in Dart before crossing FFI. |
</threat_model>

<verification>
- `bindings/dart/lib/src/ffi/bindings.dart` contains the new FFI binding (Task 1 acceptance)
- `bindings/dart/lib/src/database_update.dart` exposes `addTimeSeriesRow` via the `DatabaseUpdate` extension on `Database` — reachable as `db.addTimeSeriesRow(...)` because the extension is part of the `Database` library (`part of 'database.dart';`)
- All Dart tests pass via `bindings/dart/test/test.bat`
- No new "Cannot add_time_series_row:" string introduced inside `bindings/dart/` (grep for that prefix should match zero lines under `bindings/dart/`)
</verification>

<success_criteria>
- All four tasks' acceptance criteria pass
- `db.addTimeSeriesRow` round-trips through Dart ↔ C ABI ↔ SQLite for insert, upsert, and multi-dim paths
- DART-11 is satisfied: signature matches roadmap success criterion #2 verbatim
</success_criteria>

<output>
Create `.planning/phases/03-language-bindings-documentation/03-02-SUMMARY.md` when done.
</output>
