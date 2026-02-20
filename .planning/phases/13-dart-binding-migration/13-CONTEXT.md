# Phase 13: Dart Binding Migration - Context

**Gathered:** 2026-02-19
**Status:** Ready for planning

<domain>
## Phase Boundary

Migrate Dart's `updateTimeSeriesGroup` and `readTimeSeriesGroup` to use the new multi-column C API from Phase 11. Deliver idiomatic Dart Map interface for update and typed columnar results for read. Does not add new operations or change non-time-series bindings.

</domain>

<decisions>
## Implementation Decisions

### Update Map interface
- Columnar Map parameter: `updateTimeSeriesGroup(col, grp, id, {'date_time': [...], 'temp': [...], 'humidity': [...]})`
- Each key is a column name matching the schema, value is a typed List
- Strict types only — no auto-coercion. List<int> for INTEGER, List<double> for REAL, List<String> for TEXT. Throw on type mismatch
- No Dart-side validation of column names — let C API validate and surface its error messages (CAPI-05)
- Empty Map = clear all rows: `updateTimeSeriesGroup(col, grp, id, {})` clears rows for that element

### Read return format
- Return `Map<String, List<Object>>` — columnar format with column names as keys, typed lists as values
- Concrete runtime types: `List<int>` for INTEGER, `List<double>` for REAL, `List<String>` for TEXT
- Empty results return empty `Map<String, List<Object>>()` (no keys, no data)
- Type signature is `Map<String, List<Object>>` — values are always lists

### DateTime handling
- Auto-parse dimension column to Dart DateTime on read (dimension column returns `List<DateTime>`)
- Only dimension column (identified via metadata) gets DateTime treatment — other TEXT columns remain String
- Update accepts both DateTime and String for the dimension column (DateTime auto-formatted to string)
- Throw error on parse failure — if stored string can't parse to DateTime, throw (forces data consistency)
- Reuse existing `date_time.dart` parsing logic (stringToDateTime/dateTimeToString)

### Old API transition
- Replace in-place — no deprecation wrappers, no backwards compatibility
- Old row-oriented interface (`List<Map<String, Object?>>` parameter) removed entirely
- Regenerate FFI bindings from current C headers using `bindings/dart/generator/generator.bat`
- Rewrite existing Dart time series tests to use new Map-based interface
- Add multi-column mixed-type tests using Phase 11 schema (INTEGER + REAL + TEXT value columns)

### Overall approach
- Mirror Julia's decisions as closely as possible — same semantics (columnar Map, empty = clear, C API validates, DateTime on dimension only) with Dart-idiomatic syntax

### Claude's Discretion
- Exact Map parameter type (Map<String, List<Object>> vs Map<String, dynamic> for the update signature)
- Internal marshaling strategy for Map -> C API columnar arrays
- Arena allocation patterns for safe FFI calls
- Test file organization (new file vs extend existing)

</decisions>

<specifics>
## Specific Ideas

- Update and read should mirror Julia behavior: same semantics, Dart syntax
- Strict types — unlike Julia's auto-coercion, Dart should enforce exact type matching
- The C API already validates column names/types — binding stays thin per project philosophy
- Dimension column DateTime parsing reuses existing date_time.dart helpers

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 13-dart-binding-migration*
*Context gathered: 2026-02-19*
