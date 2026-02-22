# Phase 6: C API - Context

**Gathered:** 2026-02-22
**Status:** Ready for planning

<domain>
## Phase Boundary

Expose the C++ CSV export functionality through FFI-safe C API functions. This includes a flat options struct with parallel arrays for enum_labels, a default factory, and an updated export function signature. The C++ export logic (Phase 5) is complete and unchanged — this phase wraps it for FFI consumers and validates through C API tests.

</domain>

<decisions>
## Implementation Decisions

### Enum labels struct shape
- Claude's discretion on flat layout approach (grouped-by-attribute vs flat triples) — pick what works best with FFI generators and existing codebase patterns
- Claude's discretion on whether enum labels are a separate sub-struct or inlined directly into the options struct

### Options parameter style
- Replace existing `quiver_database_export_csv(db, collection, group, path)` stub with new signature that includes `const quiver_csv_export_options_t* opts` parameter — breaking change is acceptable (WIP project)
- Options passed by const pointer, consistent with `quiver_database_options_t` pattern
- Caller-owned memory: function borrows pointers during the call, does not copy
- `quiver_csv_export_options_default()` returns zero-initialized struct with `date_time_format = ""` (empty string = no formatting, matching C++ convention)
- Options struct is always required — no NULL shortcut for defaults
- Group parameter: empty string `""` means scalar export, `NULL` is an error (consistent with C++ std::string semantics)

### Header & type placement
- New `include/quiver/c/csv.h` header for `quiver_csv_export_options_t` struct and `quiver_csv_export_options_default()` factory
- `quiver_database_export_csv()` function declaration stays in `include/quiver/c/database.h` (database.h includes csv.h)
- Implementation in new `src/c/database_csv.cpp` file, mirroring C++ `src/database_csv.cpp` pattern
- Keep existing `quiver_database_import_csv()` stub as-is

### Test organization
- New `tests/test_c_api_database_csv.cpp` test file
- Mirror the full C++ CSV test suite through C API (same scenarios: scalar export, group export, enum labels, date formatting, empty collection, NULL values, RFC 4180 escaping)
- Reuse existing test schemas from `tests/schemas/valid/`

### Const correctness
- Full const qualifiers on options struct: `const char* const*` for string arrays, `const int64_t*` for values
- Signals borrowing semantics at the type level

### Claude's Discretion
- Enum labels flat array layout approach (grouped vs triples vs other)
- Whether enum labels are a sub-struct or inlined in options
- Whether to provide a `quiver_csv_export_options_free()` function
- Header documentation style for ownership contract
- Options struct validation (trust caller vs validate consistency)

</decisions>

<specifics>
## Specific Ideas

- Options struct should feel consistent with existing `quiver_database_options_t` pattern — const pointer parameter, factory default function
- The `database_csv.cpp` C wrapper should be self-contained like other `src/c/database_*.cpp` files
- C API tests should be comprehensive enough to catch any marshaling bugs between C struct and C++ types

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 06-c-api*
*Context gathered: 2026-02-22*
