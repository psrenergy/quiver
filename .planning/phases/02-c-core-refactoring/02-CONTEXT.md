# Phase 2: C++ Core Refactoring - Context

**Gathered:** 2026-03-01
**Status:** Ready for planning

<domain>
## Phase Boundary

Refactor C++ internals to use idiomatic RAII and template patterns. Eliminates manual `sqlite3_finalize` in `current_version()` and manual row-iteration loops in all scalar read methods. Pure internal refactoring — no public API changes, no behavior changes. All existing tests must pass unchanged.

</domain>

<decisions>
## Implementation Decisions

### RAII wrapper scope
- Define reusable `StmtPtr` typedef (`unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>`) in `database_impl.h`
- Use `StmtPtr` directly in `current_version()` — no helper method, no `Impl::prepare()`
- RAII handles cleanup on all paths: normal return and exception throw — no manual `sqlite3_finalize` calls
- `StmtPtr` lives in `database_impl.h` alongside `Database::Impl` where `sqlite3` is already visible

### Template fit for scalar reads
- Scalar reads call the template directly — no wrapper or alias
- Rename `read_grouped_values_by_id<T>` to `read_column_values<T>` for accuracy (it extracts column 0 from all rows, not "by id" specifically)
- Rename applies to all existing callers (vector/set by_id methods, `read_element_ids`)
- Validation (`require_collection`, `require_column`) stays in each method — not moved into template
- Each scalar read method becomes 4 lines: 2 require calls + SQL string + `return read_column_values<T>(execute(sql))`

### Scope of refactoring
- All 6 scalar read methods refactored (not just the 3 vector-returning ones):
  - `read_scalar_integers`, `read_scalar_floats`, `read_scalar_strings` → use `read_column_values<T>`
  - `read_scalar_integer_by_id`, `read_scalar_float_by_id`, `read_scalar_string_by_id` → use new `read_single_value<T>`
- New template `read_single_value<T>` added to `database_internal.h`: checks `result.empty()`, returns `nullopt` or extracts column 0 from row 0
- `read_single_value<T>` used only by scalar by_id methods (vector/set by_id return `vector<T>`, different type)
- `read_element_ids` already uses the template — gets the rename automatically, no other changes

### Claude's Discretion
- Exact `StmtPtr` typedef syntax and placement within `database_impl.h`
- Internal structure of `read_single_value<T>` template
- Whether `read_grouped_values_all<T>` should also be renamed for consistency (not required by scope)

</decisions>

<specifics>
## Specific Ideas

- Final method shape should match the existing vector/set by_id pattern — 4 lines per method, consistent across the file
- The rename from `read_grouped_values_by_id` to `read_column_values` is a codebase-wide rename touching all callers in `database_read.cpp`

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `database_internal.h`: Already contains `read_grouped_values_by_id<T>` and `read_grouped_values_all<T>` templates, plus `get_row_value` type-dispatchers
- `database_impl.h`: Contains `Database::Impl` struct with `sqlite3* db` member — natural home for `StmtPtr`

### Established Patterns
- Vector/set by_id methods already use `read_grouped_values_by_id<T>` (database_read.cpp:129-207) — scalar reads will match this pattern
- Type-dispatching via `get_row_value(row, index, static_cast<T*>(nullptr))` already handles int64_t/double/string extraction

### Integration Points
- `current_version()` in `database.cpp:214-233` — single function to refactor for RAII
- `database_read.cpp:6-94` — 6 scalar read methods to refactor for templates
- `database_internal.h` — receives new `read_single_value<T>` template and rename of `read_grouped_values_by_id` to `read_column_values`

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 02-c-core-refactoring*
*Context gathered: 2026-03-01*
