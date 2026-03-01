# Phase 1: Bug Fixes and Element Dedup - Context

**Gathered:** 2026-02-28
**Status:** Ready for planning

<domain>
## Phase Boundary

Fix all known bugs in element operations (update_element type validation, describe duplicate headers, import_csv parameter naming) and extract a shared group insertion helper used by both create_element and update_element. All changes are internal refactoring and bug fixes -- no new public API surface. Every change requires passing tests across C++, C API, Julia, Dart, and Python.

</domain>

<decisions>
## Implementation Decisions

### Error consistency
- Both `create_element` and `update_element` accept empty arrays -- `create_element` skips them silently (element created with no group data), `update_element` clears existing rows for that group
- Error messages are unified across both methods -- the shared helper produces identical messages for the same failure type
- Error prefix retains the calling method name: `"Cannot create_element: ..."` or `"Cannot update_element: ..."` -- the helper receives the caller name as a parameter

### Describe output
- Add time series groups to `describe()` output alongside existing Vectors and Sets display
- Dimension column highlighted with brackets: `data: [date_time] value(Real), count(Integer)`
- Fix column ordering to match schema-definition order (not alphabetical from std::map iteration) -- use PRAGMA table_info or equivalent to get definition order
- Each category header (Scalars, Vectors, Sets, Time Series) prints exactly once per collection, not per matching table

### Helper scope
- Full pipeline helper on `Database::Impl` covering routing (arrays to vector/set/time series tables) + type validation + row insertion
- Replace semantic for group updates: DELETE existing rows for the specific group being updated, then INSERT new rows. Groups not mentioned in the update call remain untouched
- Helper accepts caller name parameter for error message prefixes
- Dedup at C++ level only -- C API remains a thin wrapper calling C++ methods

### import_csv parameter rename
- Rename `table` to `collection` in the C++ header declaration (`include/quiver/database.h:128`) -- implementation and all bindings already use `collection`

### Claude's Discretion
- Time series files display in describe() (whether to show them if present)
- Internal helper method signature and parameter design
- Test structure for regression tests

</decisions>

<specifics>
## Specific Ideas

- Error messages should follow established patterns: "Cannot {method}: {reason}", "{Entity} not found: {identifier}", "Failed to {operation}: {reason}"
- The shared helper naturally ensures type validation (BUG-01) because create_element's validate_array calls become part of the shared path

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `TypeValidator::validate_array()` (src/type_validator.h): Already exists in create_element path, missing from update_element -- the shared helper will incorporate it
- `TypeValidator::validate_scalar()`: Used by both methods for scalar validation
- `Impl::resolve_element_fk_labels()`: FK resolution already shared between both methods
- `Impl::TransactionGuard`: RAII transaction guard used by both methods
- `Schema::find_all_tables_for_column()`: Routing logic that maps array names to group tables
- `PRAGMA table_info`: SQLite pragma that returns columns in schema-definition order

### Established Patterns
- `database_impl.h`: Central location for `Impl` private methods -- new helper goes here
- Error messages follow 3 strict patterns defined in CONVENTIONS.md
- `Impl::require_collection()`: Validates collection exists before operations
- Group table type routing uses `GroupTableType` enum (Vector, Set, TimeSeries)

### Integration Points
- `database_create.cpp` and `database_update.cpp` both call the new shared helper
- `database_describe.cpp` restructured with time series support
- `include/quiver/database.h` parameter rename (table -> collection)
- All existing tests must continue to pass unchanged

</code_context>

<deferred>
## Deferred Ideas

None -- discussion stayed within phase scope

</deferred>

---

*Phase: 01-bug-fixes-and-element-dedup*
*Context gathered: 2026-02-28*
