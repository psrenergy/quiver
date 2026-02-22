# Project Research Summary

**Project:** Quiver v0.4 — CSV Export with Enum Resolution and Date Formatting
**Domain:** Cross-layer feature implementation in a C++/C API/bindings library (SQLite wrapper)
**Researched:** 2026-02-22
**Confidence:** HIGH

## Executive Summary

Quiver v0.4 adds real CSV export to replace non-functional stubs across all five layers (C++, C API, Julia, Dart, Lua). The implementation pattern is well-understood: two distinct export methods (`export_scalars_csv` and `export_group_csv`) replace the ambiguous generic `export_csv` stub, share a `CsvExportOptions` struct for enum resolution and date formatting, and propagate through the established layer-by-layer architecture. Zero new dependencies are required — all CSV writing uses ~15 lines of C++ stdlib, date formatting uses `strftime`, and the options struct follows the flat-parallel-arrays pattern already established by parameterized queries and time series updates.

The recommended approach is strictly layer-ordered: C++ core first (new file `src/database_csv.cpp`, new header `include/quiver/csv_export_options.h`), then C API (new file `src/c/database_csv.cpp`, new header `include/quiver/c/csv_export_options.h`, FFI generators re-run), then bindings in parallel (Julia, Dart, Lua). The critical design decision is the shape of the C API options struct: the `enum_labels` map must be flattened to parallel arrays for FFI. The exact flattening scheme — whether to use a flat `(attribute, value, label)` struct array or offset-based parallel arrays — must be locked in before any binding work begins, as changing it invalidates all downstream binding code.

The dominant risk is coordination across layers: six files must change in a specific order, FFI generators must be re-run after every C API header change, and the Dart build cache must be cleared after struct layout changes. All critical pitfalls (CSV escaping correctness, enum fallback behavior, date column scoping, file I/O error handling) are Phase 1 concerns resolvable entirely within the C++ layer before any FFI work begins. If the C++ core is solid and the C API options struct shape is correct, the bindings are mechanical.

## Key Findings

### Recommended Stack

No new dependencies are required for v0.4. The entire implementation uses the C++20 standard library (`std::ofstream`, `std::get_time`, `strftime`, `std::map`) plus the existing SQLite 3.50.2, spdlog, sol2, and GoogleTest already in the project. This is consistent with Quiver's "simple solutions over complex abstractions" principle — CSV writing is too simple to justify a library, and every C++ CSV library found focuses on parsing rather than writing.

**Core technologies:**
- `std::ofstream` + hand-rolled `csv_escape_field()`: CSV file output — ~15 lines handles all RFC 4180 quoting; no library justified
- `std::get_time` + `strftime`: Date/time reformatting from SQLite TEXT — string-to-string reformat via `std::tm`; universally compatible format specifiers (Julia `Dates.format`, Dart `DateFormat`, Lua `os.date` all accept `strftime` patterns)
- `std::map<std::string, std::map<int64_t, std::string>>`: C++ enum_labels representation — natural nested lookup; flattened to parallel arrays at the C API boundary
- `quiver_csv_export_options_t` flat struct: C API representation — caller-owned, no alloc/free needed, consistent with the parallel-arrays pattern from `quiver_database_update_time_series_group` and `quiver_database_query_*_params`

### Expected Features

Two export operations cover the full v0.4 scope. Scalars export produces one row per element (label + all scalar attributes, `id` excluded). Group export produces one row per group entry (label replaces `id` via JOIN, actual schema column names, structural columns like `vector_index` excluded from output). Both share the `CsvExportOptions` struct and the same internal CSV writing logic.

**Must have (table stakes):**
- `export_scalars_csv(collection, path, options)` — primary use case; one row per element with label and all scalar attributes
- `export_group_csv(collection, group, path, options)` — exports any vector, set, or time series group; label replaces id
- RFC 4180 compliant CSV writing — header row always present; quoting on comma/quote/newline; NULL as empty field
- `CsvExportOptions` struct with `enum_labels` and `date_time_format` — defaults mean no transformation (passthrough)
- Enum value resolution via caller-provided map — graceful fallback to raw integer for unmapped values
- Date/time column formatting via `strftime` format string — applied only to `DataType::DateTime` columns (identified by `date_` name prefix)
- Full propagation through C API, Julia, Dart, and Lua bindings

**Should have (competitive):**
- Schema-aware column discovery (not hardcoded SQL) — uses `list_scalar_attributes()` and group metadata for schema-agnostic export
- `label` replaces `id` in all CSV output — human-readable, self-documenting exports by default
- Warning logging when enum map keys do not match schema attributes — catches typos without aborting

**Defer (v0.5+):**
- CSV import — significantly harder (type inference, schema validation, error handling); explicitly out of scope for v0.4
- Column selection/filtering — callers can use `query_string()` for custom SQL
- Streaming/chunked export — not needed at Quiver's target data sizes (thousands to tens of thousands of rows)
- Custom delimiters — RFC 4180 comma-only is correct for this use case

### Architecture Approach

The implementation follows a strict four-phase dependency chain: C++ core establishes the public API and all business logic, C API wraps it with flat structs and runs FFI generators, then Julia/Dart/Lua bindings run in parallel. A new file `src/database_csv.cpp` (and its C API counterpart `src/c/database_csv.cpp`) keeps CSV concerns separate from the existing `database_describe.cpp`. The existing stubs in `database_describe.cpp` and `src/c/database.cpp` are removed during Phase 1 and Phase 2 respectively.

**Major components:**
1. `CsvExportOptions` struct (`include/quiver/csv_export_options.h`) — C++ plain value type with Rule of Zero; `enum_labels` nested map and `date_time_format` string; default construction means no transformation
2. `src/database_csv.cpp` — C++ implementation; uses single SELECT queries (not N individual `read_scalar_*_by_id` calls) for performance; group export uses JOIN query to resolve label from parent collection; CSV escaping, enum resolution, and date formatting all at this layer
3. `quiver_csv_export_options_t` flat struct (`include/quiver/c/csv_export_options.h`) — C API representation with parallel arrays for enum_labels; caller-owned, no free function needed
4. `src/c/database_csv.cpp` — C API wrappers; `convert_csv_options()` helper rebuilds nested C++ map from flat arrays; validates `sum(attribute_entry_counts) == total_enum_entries`
5. Binding wrappers (Julia `database_csv.jl`, Dart `database_csv.dart`, Lua in `lua_runner.cpp`) — each flattens native map type to the C struct (Julia/Dart) or converts Lua table directly to C++ struct (Lua via sol2, bypassing C API)

### Critical Pitfalls

1. **CSV field escaping produces corrupt output** — must use a dedicated `csv_escape_field()` function covering comma, double-quote, carriage return, and newline; backslash escaping (`\"`) is NOT valid RFC 4180; must test with labels containing commas and embedded quotes, not just alphanumeric data

2. **FFI complexity explosion from poorly-shaped C API options struct** — the `enum_labels` nested map must be flattened for C API; using a flat `(attribute, value, label)` struct array per entry is simpler for bindings than offset-based parallel arrays; this shape is locked in Phase 2 and cannot change without rewriting all bindings

3. **Stale FFI bindings after C API header changes** — Julia and Dart bindings are auto-generated; if FFI generators are not re-run after every C API header change, bindings silently call the wrong function signature causing runtime crashes; Dart's `.dart_tool/` cache must also be cleared after struct layout changes

4. **Date formatting applied to wrong columns** — `date_time_format` must only be applied to columns where `ScalarMetadata.data_type == DataType::DateTime` (identified by `date_` name prefix); must never attempt to detect dates by parsing string values

5. **Unmapped enum values cause silent data loss** — when an integer value has no entry in the caller's enum map, write the raw integer (not empty string, not crash); `.at()` throws `std::out_of_range` and must not be used; use `.find()` with explicit fallback

## Implications for Roadmap

Based on research, the dependency chain is strict: C++ core must precede C API, which must precede bindings. The architecture research even provides a specific build order. Three phases are appropriate for this milestone.

### Phase 1: C++ Core — Options Struct, Export Logic, Tests

**Rationale:** All business logic lives in C++. The C API and bindings are thin wrappers — they cannot be written until the C++ signatures and behavior are settled. This phase also resolves the four Phase 1 pitfalls (CSV escaping, enum fallback, date column scoping, file I/O errors) entirely within the C++ layer before FFI complexity is introduced.

**Delivers:** `CsvExportOptions` header, `export_scalars_csv` and `export_group_csv` C++ implementations, removal of old stubs from `database_describe.cpp`, test schema `csv_export.sql`, and full C++ test coverage including edge cases (commas in labels, unmapped enum values, empty collections, date formatting).

**Addresses features from FEATURES.md:** All P1 C++ features — RFC 4180 CSV writer, `CsvExportOptions` struct, enum resolution, date formatting, NULL handling, label-instead-of-id.

**Avoids pitfalls:** P1 (CSV escaping), P2 (unmapped enum fallback), P3 (attribute name mismatch logging), P6 (file I/O error handling), P7 (date formatting scope), P9 (two-function API), P10 (JOIN-based label lookup).

### Phase 2: C API — Flat Options Struct, FFI Generation, Tests

**Rationale:** The shape of `quiver_csv_export_options_t` is the highest-risk design decision in the entire milestone — it locks in the marshaling contract for all three bindings. This phase must finalize the struct shape (flat per-entry `(attribute, value, label)` struct array recommended by PITFALLS.md to minimize binding complexity), update C API headers, remove old C API stubs, run FFI generators, and produce C API tests before any binding work begins.

**Delivers:** `quiver_csv_export_options_t` flat struct header, `quiver_database_export_scalars_csv` and `quiver_database_export_group_csv` C API functions, `convert_csv_options()` helper, regenerated Julia `c_api.jl` and Dart `bindings.dart`, C API test coverage mirroring C++ tests.

**Uses from STACK.md:** The flat parallel-arrays pattern already established by `quiver_database_update_time_series_group` and parameterized queries; Dart `ffigen` and Julia `Clang.jl` handle flat structs with pointer-to-primitive arrays correctly.

**Avoids pitfalls:** P4 (string ownership documented as borrowed-for-duration-of-call), P5 (flat struct avoids nested-array FFI nightmare), P8 (FFI generators re-run, not hand-edited).

### Phase 3: Bindings — Julia, Dart, Lua (Parallelizable)

**Rationale:** With a stable C API and regenerated FFI declarations, all three bindings can proceed in parallel. Each binding's job is mechanical: flatten the native map type to the C struct (Julia/Dart) or convert a Lua table directly to `CsvExportOptions` via sol2 (bypassing C API). The logic is identical — only the language syntax differs.

**Delivers:** Functional `export_scalars_csv!` / `export_group_csv!` in Julia, `exportScalarsCSV` / `exportGroupCSV` in Dart, `export_scalars_csv` / `export_group_csv` in Lua, and binding-level tests for all three.

**Implements:** The binding wrappers described in ARCHITECTURE.md, with each language's idiomatic map type (Julia `Dict`, Dart `Map`, Lua table) wrapping the C API.

**Avoids pitfalls:** P8 (clean-build test confirms no stale FFI), Lua integer key precision (Lua numbers to int64_t explicit conversion in sol2 binding).

### Phase Ordering Rationale

- C++ before C API: API signatures must be stable before FFI struct shapes are designed; reversing this order means C++ changes can cascade through all bindings
- C API before bindings: FFI generators must be run exactly once, after all C API header changes are complete; partial regeneration causes ABI mismatches
- Julia/Dart/Lua in parallel: No inter-binding dependencies; all three share the same C API surface; parallelizing saves time without risk
- The PITFALLS.md "phase to address" annotations directly confirm this ordering — 7 of 10 pitfalls are flagged as "Phase 1: C++ core," which validates the front-loading approach

### Research Flags

Phases with standard patterns (skip additional research):
- **Phase 1:** C++ CSV writing, `strftime` date formatting, and schema-driven column discovery are all well-documented patterns with clear implementation; no unknowns
- **Phase 3:** Binding wrappers follow the established mechanical transform rules in CLAUDE.md; Julia/Dart use existing FFI patterns for struct marshaling; Lua uses the established sol2 table-to-struct approach from time series

Phases that may need validation during planning:
- **Phase 2 (C API struct shape):** The ARCHITECTURE.md and PITFALLS.md suggest slightly different C API struct designs for enum_labels (ARCHITECTURE.md shows offset-based parallel arrays; PITFALLS.md recommends flat `(attribute, value, label)` struct per entry). The roadmap must resolve this discrepancy before Phase 2 implementation begins — the flat-struct-per-entry approach from PITFALLS.md is recommended as it minimizes binding complexity.

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | All technologies already in project; RFC 4180 is an official standard; strftime is C standard library; parallel-arrays pattern is proven in codebase |
| Features | HIGH | Feature set is narrow and precisely specified in PROJECT.md; competitor analysis confirms which features are table stakes vs. differentiators |
| Architecture | HIGH | Implementation plan is detailed and grounded in existing codebase patterns; new file locations, build order, and component boundaries all specified |
| Pitfalls | HIGH | 10 pitfalls documented with concrete test cases and recovery costs; grounded in actual project code (existing C API patterns, FFI generator behavior) |

**Overall confidence:** HIGH

### Gaps to Address

- **C API struct shape discrepancy:** ARCHITECTURE.md designs `quiver_csv_export_options_t` with offset-based per-attribute arrays (`attribute_names[]`, `attribute_entry_counts[]`, flattened `enum_values[]`/`enum_label_strings[]`). PITFALLS.md recommends a flat `quiver_csv_enum_entry_t` struct per entry (one `(attribute, value, label)` triple per row). The flat-struct approach is simpler for bindings and should be preferred, but this must be a conscious decision made at the start of Phase 2 — not during Phase 3 when bindings are already written.

- **`import_csv` stub disposition:** PROJECT.md and FEATURES.md say CSV import is out of scope for v0.4. The existing stubs should be removed or kept as empty declarations. This is a minor decision with no risk, but it should be made explicitly during Phase 1 rather than leaving dead stubs.

- **Empty collection behavior:** PITFALLS.md specifies that an empty collection should write a header-only CSV (not an error). This is the correct behavior but must be explicitly tested — collections with zero elements are an easily-missed edge case.

## Sources

### Primary (HIGH confidence)
- [RFC 4180: Common Format and MIME Type for CSV Files](https://www.rfc-editor.org/rfc/rfc4180) — canonical CSV quoting and escaping rules; confirmed 3 rules handle all edge cases
- [cppreference: std::strftime](https://en.cppreference.com/w/cpp/chrono/c/strftime) — date formatting edge cases; buffer overflow; thread safety (strftime itself is thread-safe, localtime is not)
- [Dart FFI C Interop documentation](https://dart.dev/interop/c-interop) — confirms flat structs with primitive pointers work correctly with ffigen; nested dynamic allocations cause binding bugs
- Quiver codebase — existing C API patterns in `database_time_series.cpp`, `database_query.cpp`, `database_helpers.h`; confirmed parallel-arrays as established idiom

### Secondary (MEDIUM confidence)
- [pandas DataFrame.to_csv()](https://pandas.pydata.org/docs/reference/api/pandas.DataFrame.to_csv.html) — reference API for CSV export options; confirms `date_format`, `na_rep`, header behavior
- [Datasette CSV Export](https://docs.datasette.io/en/stable/csv_export.html) + [Issue #2214](https://github.com/simonw/datasette/issues/2214) — FK label auto-resolution failure modes; justifies caller-provided enum map over auto-resolution
- [MSVC Blog: C++20 Chrono Extensions](https://devblogs.microsoft.com/cppblog/cpp20s-extensions-to-chrono-available-in-visual-studio-2019-version-16-10/) — confirms `std::chrono::parse` available but `strftime` is simpler for this use case
- [RFC 4180 edge cases (Hacker News)](https://news.ycombinator.com/item?id=31254151) — community experience confirming hand-rolling CSV writing is correct when RFC 4180 is followed

### Tertiary (MEDIUM confidence, limited to community sources)
- [CsvHelper Issues #82, #1619](https://github.com/JoshClose/CsvHelper/issues/82) — real-world enum mapping complexity in CSV libraries; validates caller-provided map approach
- [SEI: Pointer Ownership Model in C/C++](https://www.sei.cmu.edu/blog/using-the-pointer-ownership-model-to-secure-memory-management-in-c-and-c/) — C API string ownership patterns; confirms borrowed-for-duration-of-call as correct pattern

---
*Research completed: 2026-02-22*
*Ready for roadmap: yes*
