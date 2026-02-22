# Pitfalls Research

**Domain:** Adding CSV export with enum value resolution and date formatting to a C++/C API library with Julia, Dart, and Lua bindings
**Researched:** 2026-02-22
**Confidence:** HIGH

## Critical Pitfalls

### Pitfall 1: CSV Field Escaping -- Silent Data Corruption

**What goes wrong:**
CSV output contains unescaped commas, double quotes, or newlines inside field values. When the CSV is opened in Excel or parsed by another tool, columns shift or rows split. The data appears valid but is structurally corrupted.

Example: A label `"Item, Large"` written without quoting produces `Item 1,Item, Large,42` -- a parser sees 4 columns instead of 3. A label containing `"He said ""hello"""` (with embedded quotes) must be double-escaped to `"""He said """"hello""""""`. Getting this wrong produces parse failures.

**Why it happens:**
Developers often test with clean data (no commas, no quotes, no newlines). RFC 4180 has precise escaping rules, but they are counterintuitive:
- Fields containing commas, double quotes, or line breaks (CRLF or LF) MUST be enclosed in double quotes
- A double quote inside a quoted field MUST be escaped by preceding it with another double quote (`""`)
- Backslash escaping (`\"`) is NOT valid CSV -- it is a JSON/C convention that leaks into naive implementations

**How to avoid:**
Write a single `csv_escape_field(const std::string& value)` function in the C++ core that handles ALL escaping. Logic:

```cpp
std::string csv_escape_field(const std::string& value) {
    bool needs_quoting = false;
    for (char c : value) {
        if (c == ',' || c == '"' || c == '\n' || c == '\r') {
            needs_quoting = true;
            break;
        }
    }
    if (!needs_quoting) return value;

    std::string result = "\"";
    for (char c : value) {
        if (c == '"') result += "\"\"";
        else result += c;
    }
    result += "\"";
    return result;
}
```

Test with at minimum these edge cases:
1. Field containing comma: `"Item, Large"` -> `"""Item, Large"""`
2. Field containing double quote: `He said "hello"` -> `"He said ""hello"""`
3. Field containing newline: `Line1\nLine2` -> `"Line1\nLine2"`
4. Field containing all three simultaneously
5. Empty string: `""` (valid, but verify)
6. Field that is exactly `""` (two double-quote characters)

**Warning signs:**
- Tests only use alphanumeric labels
- No test case with commas inside label or attribute values
- String values in the database created via `Element().set("label", "Item, Large")` but not verified in CSV output
- Using `std::replace` or regex instead of a proper field-level escaper

**Phase to address:**
Phase 1 (C++ core CSV implementation) -- the escape function must be written and tested before any CSV output reaches users.

---

### Pitfall 2: Enum Map Lookup Fails Silently -- Value Not In Map

**What goes wrong:**
The caller provides an enum map like `{"status": {1: "Active", 2: "Inactive"}}`, but the database contains a status value of `3` (added after the map was created, or a data entry error). The export either crashes, writes `3` as a raw integer (silently wrong), or writes an empty string (data loss).

**Why it happens:**
The enum map is caller-provided, not schema-derived. There is no guarantee it covers all values in the database. This is the most common bug in CSV export with enum resolution: the map is assumed to be exhaustive but is not.

**How to avoid:**
Define a clear contract: when an integer value has no entry in the enum map, write the raw integer value as-is. This is the safest default -- no data loss, no crash, and the caller can see unmapped values in the output. Do NOT throw an error (the export should not abort mid-file because of one unmapped value) and do NOT write an empty string (data loss).

In C++:
```cpp
std::string resolve_enum(int64_t value,
                         const std::map<int64_t, std::string>& enum_map) {
    auto it = enum_map.find(value);
    if (it != enum_map.end()) return it->second;
    return std::to_string(value);  // fallback: raw integer
}
```

Test with:
1. Value present in map -> label written
2. Value NOT present in map -> raw integer written
3. Empty enum map provided -> all values written as raw integers
4. Negative integer values (e.g., -1 as sentinel)
5. Value of 0 (edge case: is 0 a valid key?)

**Warning signs:**
- No test case for unmapped values
- Code uses `.at()` instead of `.find()` (throws `std::out_of_range`)
- Export aborts on first unmapped value instead of falling back

**Phase to address:**
Phase 1 (C++ core CSV implementation) -- the fallback behavior must be decided and tested before the C API layer is built on top.

---

### Pitfall 3: Enum Map Attribute Name Mismatch -- Map Key Does Not Match Schema

**What goes wrong:**
The caller provides an enum map keyed by attribute name: `{"status": {1: "Active"}}`. But the actual scalar attribute in the schema is named `status_code`, or the map uses the collection-qualified name `Items.status` instead of just `status`. The lookup silently fails and all values for that attribute are written as raw integers.

**Why it happens:**
The C++ `list_scalar_attributes()` returns metadata with `name` fields matching the SQL column names. The caller may use different conventions (camelCase, prefixed, collection-qualified). Since the enum map lookup is a string comparison, a mismatch means the map is ignored for that attribute -- no error, just wrong output.

**How to avoid:**
1. Use the exact column name from the schema (returned by `list_scalar_attributes()`) as the map key. Document this clearly.
2. When an attribute name in the enum map does NOT match any scalar attribute in the collection, log a warning (spdlog::warn). This catches typos without aborting the export.
3. Do NOT try to "fuzzy match" or normalize names -- that introduces its own class of bugs.

Test with:
1. Enum map key matches attribute name exactly -> resolution works
2. Enum map key does NOT match any attribute -> warning logged, no crash, values written as raw integers
3. Enum map contains keys for attributes in a DIFFERENT collection -> ignored silently (not an error)

**Warning signs:**
- No test verifying behavior when enum map keys do not match schema attributes
- No logging for unused enum map entries
- Assumption that caller "will always get the name right"

**Phase to address:**
Phase 1 (C++ core) -- warning logging for unmatched keys. Phase 3+ (bindings) -- document that enum map keys must be exact column names.

---

### Pitfall 4: C API Memory Ownership Ambiguity for Options Struct Strings

**What goes wrong:**
The CSV export options struct contains a `date_time_format` string (e.g., `"%Y-%m-%d"`) and enum map data (arrays of strings). The C API function receives pointers to these strings. If the C API assumes the strings are valid only for the duration of the call and the caller frees them immediately, this works. But if the C API stores the pointers for later use (e.g., to process lazily), the pointers dangle. Conversely, if the C API copies the strings but the caller also frees them, there is no leak but wasted effort. The real danger is the undocumented case.

In this project's existing pattern, `quiver_database_options_t` contains only `int` and `enum` -- no string pointers. The CSV options struct will be the FIRST struct in the C API to contain string pointers passed as input. This is a precedent-setting design.

**Why it happens:**
The existing `quiver_database_options_t` has no strings, so the existing pattern (`const quiver_database_options_t* options`) gives no guidance on string lifetime. When the CSV options struct adds `const char* date_time_format` and enum map arrays, the ownership contract is ambiguous unless explicitly documented.

**How to avoid:**
Follow the established pattern: the C API function copies everything it needs during the call. The caller owns all input pointers and can free them immediately after the call returns. This matches how existing C API functions handle `const char*` parameters (e.g., `collection`, `attribute`) -- they are borrowed for the duration of the call.

Concretely:
- The C API `quiver_database_export_csv()` function copies `date_time_format` and all enum map data into C++ `std::string` and `std::map` objects at the top of the function, before any processing.
- Document in the header: "All pointer arguments are borrowed for the duration of the call. The caller retains ownership."
- Do NOT store raw pointers from the options struct in any persistent state.

**Warning signs:**
- No comment in the C API header about string ownership
- Options struct stored by pointer for "later" processing
- Use-after-free crashes when binding arena allocators release temporary strings

**Phase to address:**
Phase 2 (C API layer) -- ownership contract must be explicit in the header comment. Bindings can then use arena/temporary allocators confidently.

---

### Pitfall 5: FFI Complexity Explosion -- Nested Map Structure Through Flat C API

**What goes wrong:**
The enum map is conceptually `map<string, map<int64_t, string>>` (attribute name -> value -> label). Passing this through a flat C API requires either:
(a) A deeply nested array-of-arrays-of-arrays structure that is nightmarish to marshal in Julia/Dart/Lua
(b) A flattened parallel-arrays encoding that is error-prone and hard to validate

Either way, every binding must independently implement the marshaling logic. Bugs in one binding (e.g., off-by-one in array indexing) are invisible until a specific test triggers them.

**Why it happens:**
The existing C API handles maps through parallel arrays (e.g., time series files: `columns[]` + `paths[]`). But enum maps are TWO levels deep: for each attribute, there is a set of (int, string) pairs. The existing parallel-array pattern extends, but the complexity compounds:

```c
// Flattened representation:
const char* const* attr_names;        // ["status", "category"]
const size_t* attr_entry_counts;      // [2, 3]  (status has 2 entries, category has 3)
const int64_t* attr_entry_keys;       // [1, 2, 10, 20, 30]  (flattened)
const char* const* attr_entry_values; // ["Active", "Inactive", "A", "B", "C"]
size_t attr_count;                    // 2
```

This is 5 parallel arrays with an offset-based indexing scheme. Every binding must correctly compute cumulative offsets. One wrong offset corrupts all subsequent mappings.

**How to avoid:**
Simplify the C API by flattening the enum map into a single-level structure with explicit per-entry attribute names (denormalized). Instead of nested arrays, use a flat array of (attribute, key, value) triples:

```c
typedef struct {
    const char* attribute;      // scalar attribute name
    int64_t value;              // integer value in database
    const char* label;          // human-readable label for CSV
} quiver_csv_enum_entry_t;
```

Then the C API function takes:
```c
quiver_error_t quiver_database_export_csv(
    quiver_database_t* db,
    const char* collection,
    const char* path,
    const char* date_time_format,           // NULL for no formatting
    const quiver_csv_enum_entry_t* enum_entries,  // NULL for no enum resolution
    size_t enum_entry_count
);
```

This is trivial to marshal in every binding:
- Julia: `Vector{QuiverCsvEnumEntry}` with `@ccall`
- Dart: `Pointer<quiver_csv_enum_entry_t>` with `Struct` subclass
- Lua: array of `{attribute, value, label}` tables

The C++ layer groups entries by attribute internally. The FFI boundary stays flat.

**Warning signs:**
- C API function signature has 8+ parameters for enum map arrays
- Binding code has manual offset calculation (`sum(attr_entry_counts[0:i])`)
- Different bindings produce different CSV output for the same database (marshaling bugs)
- Lua binding is significantly more complex than Julia/Dart (Lua tables do not map to nested C arrays)

**Phase to address:**
Phase 2 (C API design) -- the struct shape must be decided before any binding work begins. Getting this wrong means rewriting all four bindings.

---

### Pitfall 6: File I/O Error Handling -- Partial Writes and Path Validation

**What goes wrong:**
The export writes 500 rows to a CSV file, then fails on row 501 (disk full, permissions change, network drive disconnects). The file now contains a partial export -- 500 rows of valid data with no indication it is incomplete. The caller reads the file assuming it is complete.

Alternatively: the output path does not exist (`/nonexistent/dir/output.csv`), or the file exists but is read-only, or the path is a directory. The error surfaces as a low-level C++ `std::ofstream` failure with no useful message.

**Why it happens:**
`std::ofstream` does not throw by default -- it sets `failbit`/`badbit` silently. If the code does not check stream state after each write (or at minimum after `close()`), partial writes go undetected. And the default `what()` message from `std::ios_base::failure` is platform-dependent and cryptic.

**How to avoid:**
1. Enable exceptions on the output stream: `ofs.exceptions(std::ofstream::failbit | std::ofstream::badbit)`. This ensures any write failure throws immediately.
2. Write to a temporary file (e.g., `path + ".tmp"`), then rename to the final path atomically after successful completion. This prevents partial files from existing at the target path.
3. Validate the output path before starting:
   - Parent directory exists
   - Path is not a directory
   - File is writable (or creatable)
4. Use Quiver's error message patterns:
   - `"Failed to export_csv: could not open file: {path}"` (open failure)
   - `"Failed to export_csv: write error at row {n}: {system_error}"` (mid-write failure)
   - `"Cannot export_csv: output path is a directory: {path}"` (precondition failure)

**Warning signs:**
- No test for nonexistent output directory
- No test for read-only output file
- `std::ofstream` opened without exception mask
- No temporary file / atomic rename pattern
- Tests always write to `./test_output.csv` in the build directory (always writable, never fails)

**Phase to address:**
Phase 1 (C++ core) -- file I/O error handling and the temp-file-then-rename pattern must be implemented with the core export logic.

---

### Pitfall 7: Date Formatting Applied to Non-Date Columns

**What goes wrong:**
The `date_time_format` option is intended for columns with `date_` prefix (time series dimension columns). But the implementation applies it broadly to any TEXT column whose value "looks like" a date, or worse, to ALL TEXT columns. Labels like `"2025-01-15 Meeting Notes"` get mangled by date parsing.

Alternatively, the implementation uses the schema's `DataType` to identify date columns, but the schema only has `TEXT` type -- there is no `DATE_TIME` SQL column type in SQLite's STRICT mode. The `DataType::DateTime` classification in Quiver comes from column name heuristics (prefix `date_`), not from the SQL type system.

**Why it happens:**
The `DataType::DateTime` enum in Quiver is a convention, not a database constraint. SQLite STRICT mode only has `TEXT`, `INTEGER`, `REAL`, `BLOB`, and `ANY`. Quiver's metadata system infers `DateTime` from column names starting with `date_`. If the implementation uses the metadata `data_type` field, it works correctly. But if it tries to parse values or use SQL column types, it fails.

**How to avoid:**
1. Only apply date formatting to columns whose `ScalarMetadata.data_type` is `DataType::DateTime` (for scalar exports) or whose column name matches the time series dimension column (for group exports).
2. Never attempt to parse TEXT values to detect dates -- use metadata only.
3. If `date_time_format` is provided but a collection has no DateTime columns, the format string is simply unused -- not an error.
4. If `date_time_format` is NULL/empty, write DateTime column values as-is (raw TEXT from SQLite).

**Warning signs:**
- Implementation contains date-parsing logic that inspects string values
- Non-date TEXT columns get reformatted or produce errors
- Test schema has no `date_` prefixed scalar columns, so the feature is untested for scalars

**Phase to address:**
Phase 1 (C++ core) -- date formatting must be scoped to metadata-identified DateTime columns only.

---

### Pitfall 8: `export_csv` Signature Change Breaks Existing Stubs and Bindings

**What goes wrong:**
The existing `export_csv(const std::string& table, const std::string& path)` signature exists in C++, C API, Julia, Dart, and FFI bindings. Changing this signature to add the options parameter requires updating ALL of these locations simultaneously. If any binding is missed, it either fails to compile (good) or calls the wrong C API function (bad -- if function overloading creates ambiguity).

More subtly: the Julia and Dart FFI bindings are auto-generated from the C API header. If the C API signature changes but the generators are not re-run, the bindings call the old function signature, causing stack corruption or segfaults at runtime (wrong number of arguments to `ccall`/`dart:ffi`).

**Why it happens:**
The existing stubs have been propagated across all five layers (C++, C API, Julia FFI, Julia wrapper, Dart FFI, Dart wrapper, Lua). Changing a function signature is not a single-file change -- it is a coordinated change across 6+ files. The auto-generated FFI files (`c_api.jl`, `bindings.dart`) must be regenerated, not hand-edited.

**How to avoid:**
1. Change the C++ signature first, adding the options parameter with a default value for backward compatibility during transition.
2. Update the C API header, adding new function(s) rather than modifying the existing stub (or modify in place if the stub was never functional -- which it was not).
3. Re-run FFI generators: `bindings/julia/generator/generator.bat` and `bindings/dart/generator/generator.bat`.
4. Update Julia and Dart wrapper code to pass the new parameters.
5. Update Lua binding registration in `lua_runner.cpp`.
6. Delete or repurpose the `import_csv` stub if it remains out of scope.

The order matters: C++ first, then C API header, then generator, then bindings.

**Warning signs:**
- Binding tests pass locally because old DLL is cached -- but fail on clean build
- Dart's `.dart_tool/` cache contains stale bindings
- Julia `Manifest.toml` references old binding artifact
- `import_csv` stub still exists but is dead code

**Phase to address:**
Phase 1 (C++ signature change), Phase 2 (C API), Phase 3+ (bindings) -- but the signature design must be finalized in Phase 2 before any binding work starts.

---

### Pitfall 9: Scalars-Only vs. Group Export -- Two Different CSV Shapes, One API

**What goes wrong:**
The API has two export modes: scalar CSV (one row per element, columns are scalar attributes) and group CSV (one row per vector/set/time-series entry, columns are group value columns). These produce fundamentally different CSV structures. If the API uses a single function that determines mode from the `table` parameter, edge cases arise:

- What if the collection has no scalar attributes? (Empty CSV? Error?)
- What if the group name does not exist? (Should it throw or write empty file?)
- What if the caller passes a raw SQL table name that is neither a collection nor a group?

**Why it happens:**
The existing stub signature `export_csv(table, path)` uses a generic `table` parameter that does not distinguish between "export scalars for collection X" and "export group Y for collection X". The v0.4 requirements specify separate export operations for scalars and groups, but the single-function API obscures this.

**How to avoid:**
Use two separate C++ methods:
```cpp
void export_scalars_csv(const std::string& collection,
                        const std::string& path,
                        const CsvExportOptions& options = {});

void export_group_csv(const std::string& collection,
                      const std::string& group,
                      const std::string& path,
                      const CsvExportOptions& options = {});
```

This makes the intent explicit, avoids parameter overloading confusion, and follows the existing naming pattern (`read_scalar_*` vs. `read_vector_*`). The C API follows suit with `quiver_database_export_scalars_csv` and `quiver_database_export_group_csv`.

**Warning signs:**
- Single `export_csv` function with a `mode` parameter or auto-detection logic
- Caller confusion about what `table` means (collection name? SQL table name? group name?)
- Group export requires both collection and group name but signature only takes one

**Phase to address:**
Phase 1 (C++ API design) -- the function split must be decided before implementation begins.

---

### Pitfall 10: Label Column Lookup Failure for Group CSVs

**What goes wrong:**
Group CSVs replace the `id` foreign key column with the element's `label` from the parent collection. This requires a JOIN or lookup: for each row in the vector/set/time series table, find the label from the parent collection where `id` matches. If the implementation uses a separate query per row (N+1 problem), large exports become extremely slow. If it uses a single JOIN, the SQL must correctly reference the parent collection table -- which requires knowing the parent table name from the foreign key metadata.

**Why it happens:**
The vector/set/time series tables have `id INTEGER NOT NULL REFERENCES Items(id)`. The parent table name (`Items`) must be derived from the schema metadata to construct the JOIN. The existing `GroupMetadata` does not directly expose the parent collection name -- it must be inferred from the table naming convention (`Items_vector_values` -> parent is `Items`).

**How to avoid:**
Use a single SQL query with JOIN for the export:
```sql
SELECT parent.label, vt.value
FROM Items_vector_values vt
JOIN Items parent ON parent.id = vt.id
ORDER BY parent.label, vt.vector_index;
```

The parent table name is derivable from the group table name by stripping the `_vector_*`, `_set_*`, or `_time_series_*` suffix. This convention is already established in the schema patterns. Alternatively, the collection name is already provided as a parameter to `export_group_csv`, so the parent table IS the collection name.

**Warning signs:**
- Export of 10,000-row vector table takes multiple seconds (N+1 queries)
- No JOIN in the export SQL
- Parent table name hardcoded or guessed instead of derived from the collection parameter

**Phase to address:**
Phase 1 (C++ core) -- the JOIN-based query must be used from the start.

---

## Technical Debt Patterns

Shortcuts that seem reasonable but create long-term problems.

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| Skipping CSV escaping for "known clean" data | Faster implementation | First user with a comma in a label gets corrupted output | Never |
| Passing enum map as raw parallel arrays in C API | Avoids defining a struct | Every binding implements offset arithmetic differently; bugs are binding-specific | Never -- use flat struct array |
| Silently dropping unmapped enum values (empty string) | "Cleaner" CSV output | Data loss; unmapped values invisible in output | Never -- fall back to raw integer |
| Writing CSV directly to final path (no temp file) | Simpler code | Partial files on disk after write errors; downstream tools process incomplete data | Only in Phase 1 prototype; must fix before shipping |
| Single `export_csv` function for both scalars and groups | Fewer functions to bind | Ambiguous parameter semantics; requires mode detection logic | Never -- two functions are clearer |
| Not re-running FFI generators after C API header changes | Saves a build step | Runtime segfaults from ABI mismatch; Dart/Julia call wrong function | Never |

## Integration Gotchas

Common mistakes when connecting CSV export across the C++/C/binding stack.

| Integration | Common Mistake | Correct Approach |
|-------------|----------------|------------------|
| C++ to C API | Defining `CsvExportOptions` as a C++ class with `std::map` members, then trying to expose it through C API | Define the C API struct first (flat arrays/structs), then convert to C++ types at the C API boundary |
| C API to Julia | Julia `ccall` passes `Vector{QuiverCsvEnumEntry}` but struct layout does not match C due to padding | Use `@assert sizeof(QuiverCsvEnumEntry) == ...` in Julia to verify struct size matches C |
| C API to Dart | Dart `Struct` subclass fields declared in wrong order or with wrong types | Re-run `bindings/dart/generator/generator.bat` after C API header changes; do not hand-edit `bindings.dart` |
| C API to Lua | Lua tables used as enum maps (`{status = {[1] = "Active"}}`) but integer keys are Lua numbers, not C int64_t | Convert Lua numbers to int64_t explicitly in the C++ Lua binding code (`sol::as` or static_cast) |
| C++ options struct to C struct | `std::map<std::string, std::map<int64_t, std::string>>` does not cross FFI boundary | Flatten to `std::vector<EnumEntry>` at C++ level, then marshal as C array |
| File path handling | Unix paths in tests, backslash paths on Windows | Use `std::filesystem::path` for path operations; test on both platforms |
| Enum map for group CSVs | Enum map designed for scalar INTEGER attributes but groups have different column types | Clearly scope enum resolution to INTEGER scalar columns only; document that group columns are not enum-resolved (they use actual column names) |

## Performance Traps

Patterns that work at small scale but fail as usage grows.

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|----------------|
| N+1 label lookups for group CSV | Export of 10K-row group takes 10+ seconds | Use JOIN query, not per-row label lookup | 1K+ rows in group table |
| String concatenation for CSV row building | O(n^2) string copies for large rows with many columns | Use `std::ostringstream` or pre-allocated buffer | 50+ columns per row |
| Loading entire group into memory before writing | Memory spike for large time series (1M+ rows) | Stream rows directly from SQLite cursor to file output | 100K+ rows in a single group |
| Enum map lookup per cell instead of pre-building lookup table | Redundant map traversal for every row | Build enum lookup once, reuse for all rows | Negligible until 100K+ rows, but still wasteful |

## Security Mistakes

Domain-specific security issues relevant to CSV export.

| Mistake | Risk | Prevention |
|---------|------|------------|
| CSV injection via formula characters | Cell values starting with `=`, `+`, `-`, `@` are interpreted as formulas by Excel, enabling arbitrary code execution | For data export (not spreadsheet import), this is the downstream tool's responsibility. Document but do not try to escape formula characters -- that would corrupt legitimate data. |
| File path traversal in output path | Caller provides `../../etc/shadow` as output path | This is a caller-side concern; the C++ library writes to whatever path is given. Bindings should not second-guess the caller. |

## UX Pitfalls

Common user experience mistakes in this domain.

| Pitfall | User Impact | Better Approach |
|---------|-------------|-----------------|
| Enum map required even when no enums exist | Every export call must construct an empty map | Make enum map optional (NULL in C API, `nothing`/`null`/`nil` in bindings, default empty in C++) |
| Date format required even when no date columns exist | Every export call must provide format string | Make date format optional (NULL in C API means "use raw text") |
| No header row in CSV output | Downstream tools cannot auto-detect column names | Always write header row; no option to disable (RFC 4180 recommends it) |
| Column order differs between exports | Same collection exported twice gives different column order | Use deterministic column order: `label` first, then scalar attributes in `list_scalar_attributes()` order |
| Error on empty collection | Collection with zero elements throws instead of writing headers-only CSV | Write header row even for empty collections; the file is valid but has no data rows |
| Raw `id` column in scalar CSV | Users see meaningless autoincrement integers | Exclude `id` column from scalar CSV output; `label` serves as the row identifier |

## "Looks Done But Isn't" Checklist

Things that appear complete but are missing critical pieces.

- [ ] **CSV escaping:** Test with commas, quotes, newlines, and all three simultaneously in label and attribute values
- [ ] **Enum map fallback:** Test with unmapped integer values -- verify raw integer written, not crash or empty string
- [ ] **Enum map key mismatch:** Test with enum map containing attribute names that do not exist in the collection -- verify warning logged, no crash
- [ ] **Date formatting with NULL values:** Test export when a DateTime column contains NULL/empty string -- verify no crash, NULL written as empty field
- [ ] **Empty collection export:** Test export of collection with zero elements -- verify CSV file contains header row only
- [ ] **Group CSV label column:** Verify `id` column is replaced with `label` from parent collection, not raw integer
- [ ] **C API string ownership:** Verify all `const char*` arguments are borrowed (not stored) by the C API function -- test by freeing immediately after call returns
- [ ] **FFI generators re-run:** Julia and Dart FFI generators executed after C API header changes -- stale bindings produce runtime crashes
- [ ] **Dart cache cleared:** `.dart_tool/hooks_runner/` and `.dart_tool/lib/` cleaned after C API struct changes
- [ ] **Lua enum map marshaling:** Lua table `{[1] = "Active"}` correctly becomes `int64_t -> std::string` in C++ -- Lua number precision issues
- [ ] **File I/O errors:** Test with nonexistent output directory, read-only file, and verify descriptive error messages
- [ ] **Cross-platform paths:** Test with both forward-slash and backslash paths on Windows
- [ ] **Deterministic column order:** Multiple exports of the same collection produce identical CSV files
- [ ] **Large export:** Test with 10K+ rows to verify no N+1 query performance issue

## Recovery Strategies

When pitfalls occur despite prevention, how to recover.

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| CSV escaping bugs (P1) | LOW | Fix `csv_escape_field()`, re-run tests. Output files can be regenerated. |
| Unmapped enum crashes (P2) | LOW | Add fallback to `std::to_string()`. Single function change. |
| Attribute name mismatch (P3) | LOW | Add warning logging. No API change needed. |
| Memory ownership confusion (P4) | MEDIUM | If bindings already depend on wrong assumption, must update all binding code. Prevention is critical. |
| FFI complexity from nested arrays (P5) | HIGH | If bindings were built on nested-array C API, must redesign C API and rewrite all bindings. Flat struct approach avoids this entirely. |
| Partial file writes (P6) | LOW | Add temp-file-then-rename. Localized to C++ core. |
| Date formatting on wrong columns (P7) | LOW | Scope to metadata-identified DateTime columns. Single predicate change. |
| Signature change coordination (P8) | MEDIUM | Re-run generators, update wrappers. Known process but touches many files. |
| Single-function ambiguity (P9) | MEDIUM | Split into two functions. Requires C API and all binding updates. |
| N+1 label lookups (P10) | LOW | Replace per-row query with JOIN. Single SQL change. |

## Pitfall-to-Phase Mapping

How roadmap phases should address these pitfalls.

| Pitfall | Prevention Phase | Verification |
|---------|------------------|--------------|
| P1: CSV escaping | Phase 1: C++ core | Tests with commas, quotes, newlines in field values produce valid RFC 4180 output |
| P2: Unmapped enum values | Phase 1: C++ core | Test with value not in map: raw integer appears in CSV, no crash |
| P3: Attribute name mismatch | Phase 1: C++ core | Test with misspelled enum map key: warning logged, export completes |
| P4: C API string ownership | Phase 2: C API layer | Header comment documents borrowed semantics; binding tests free strings immediately after call |
| P5: FFI nested map complexity | Phase 2: C API design | C API uses flat struct array; all bindings marshal with < 10 lines of code each |
| P6: File I/O errors | Phase 1: C++ core | Tests for nonexistent directory and read-only file produce descriptive error messages |
| P7: Date formatting scope | Phase 1: C++ core | Non-DateTime TEXT columns are never reformatted; only `DataType::DateTime` columns use format string |
| P8: Signature coordination | Phase 2: C API + Phase 3: Bindings | FFI generators re-run; all binding tests pass on clean build |
| P9: Two export modes | Phase 1: C++ API design | Two separate methods exist: `export_scalars_csv` and `export_group_csv` |
| P10: Label lookup performance | Phase 1: C++ core | Group export SQL uses JOIN; 10K-row export completes in < 1 second |

## Sources

- [RFC 4180 - Common Format and MIME Type for Comma-Separated Values](https://datatracker.ietf.org/doc/html/rfc4180) -- HIGH confidence: canonical CSV escaping rules
- [Handling Special Characters in CSV Files (InventiveHQ)](https://inventivehq.com/blog/handling-special-characters-in-csv-files) -- MEDIUM confidence: practical guide to RFC 4180 escaping edge cases
- [Hacker News: CSV edge cases discussion](https://news.ycombinator.com/item?id=31254151) -- MEDIUM confidence: community experience with CSV escaping failures
- [CsvHelper Issue #82: Enum conversion failures](https://github.com/JoshClose/CsvHelper/issues/82) -- MEDIUM confidence: real-world enum mapping bug in CSV library
- [CsvHelper Issue #1619: Generic enum type converter](https://github.com/JoshClose/CsvHelper/issues/1619) -- MEDIUM confidence: enum type resolution complexity
- [Ownership Semantics for C Programmers](https://kwellig.garden/posts/ownership) -- MEDIUM confidence: C API pointer ownership patterns
- [SEI: Pointer Ownership Model in C/C++](https://www.sei.cmu.edu/blog/using-the-pointer-ownership-model-to-secure-memory-management-in-c-and-c/) -- HIGH confidence: authoritative memory ownership guidance
- [cppreference: std::strftime](https://en.cppreference.com/w/cpp/chrono/c/strftime) -- HIGH confidence: date formatting edge cases (NULL, empty, buffer overflow)
- [Dart FFI C Interop](https://dart.dev/interop/c-interop) -- HIGH confidence: Dart struct marshaling patterns
- Existing codebase: `src/c/database_helpers.h` (strdup_safe, parallel-array marshaling), `src/c/database_query.cpp` (convert_params pattern for typed arrays), `include/quiver/c/options.h` (existing options struct with no strings), `bindings/dart/lib/src/database_read.dart` (time series columnar FFI marshaling precedent)

---
*Pitfalls research for: Quiver v0.4 -- CSV Export with Enum Resolution and Date Formatting*
*Researched: 2026-02-22*
