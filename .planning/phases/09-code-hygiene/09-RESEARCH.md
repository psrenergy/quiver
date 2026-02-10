# Phase 9: Code Hygiene - Research

**Researched:** 2026-02-10
**Domain:** SQL injection prevention via identifier validation, clang-tidy static analysis integration
**Confidence:** HIGH

## Summary

Phase 9 has two distinct work areas: (1) eliminating SQL string concatenation risks in schema queries by validating identifiers against the loaded schema, and (2) integrating clang-tidy static analysis into the project. Both are well-understood problems with clear solutions.

The codebase currently constructs SQL queries by concatenating table names, column names, and attribute names directly into SQL strings. This is pervasive -- approximately 80+ concatenation sites across 8 source files. However, the codebase already has a Schema object loaded at runtime that knows all valid table names, column names, and foreign keys. The fix is to add a validation layer that checks identifiers against the schema before they reach SQL construction, not to switch to parameterized queries (since SQL does not support parameterized identifiers -- only parameterized values, which the codebase already uses correctly).

For clang-tidy, the project builds with MinGW GCC (Strawberry Perl's c++.exe) on Windows, uses C++20, and already generates `compile_commands.json`. clang-tidy v20.1.7 is installed at `C:\Program Files\LLVM\bin\clang-tidy.exe`. The main challenge will be suppressing noise from third-party headers (sol2, spdlog, sqlite3) and handling the naming convention split between C++ (`snake_case` members/methods) and C API (`quiver_snake_case` global functions with `_t` type suffixes).

**Primary recommendation:** Add an `assert_valid_identifier()` helper that validates table/column names against the loaded Schema, call it at every SQL concatenation point. Create a `.clang-tidy` config targeting the project's own code with appropriate header filters and suppressions.

## Standard Stack

### Core
| Tool | Version | Purpose | Why Standard |
|------|---------|---------|--------------|
| clang-tidy | 20.1.7 | Static analysis | Already installed, industry standard for C++ |
| compile_commands.json | N/A | Build database for clang-tidy | Already generated via CMAKE_EXPORT_COMPILE_COMMANDS |

### Supporting
| Tool | Version | Purpose | When to Use |
|------|---------|---------|-------------|
| CMake | 3.29.2 | Build system integration | For adding clang-tidy targets |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| clang-tidy | cppcheck | clang-tidy has better modernize/readability checks and is already installed |
| Manual identifier validation | SQLite quoting (`"identifier"`) | Quoting hides bugs -- validation catches them early |

## Architecture Patterns

### Pattern 1: SQL Identifier Validation via Schema Lookup

**What:** A centralized function that validates SQL identifiers (table names, column names) against the loaded Schema before they are concatenated into SQL strings.

**When to use:** Every place where a user-supplied or computed string becomes part of an SQL statement outside of a `?` parameter.

**Current state -- unvalidated concatenation (80+ sites):**
```cpp
// database_read.cpp -- table name and column name concatenated directly
auto sql = "SELECT " + attribute + " FROM " + collection;

// database_update.cpp -- table name and column name concatenated
auto sql = "UPDATE " + collection + " SET " + attribute + " = ? WHERE id = ?";

// schema.cpp -- PRAGMA with table name (these are special -- see below)
auto sql = "PRAGMA table_info(" + table + ")";
```

**Recommended approach -- validate before concatenate:**
```cpp
// New helper in database_impl.h or a new sql_helpers.h
void require_valid_identifier(const Schema& schema, const std::string& table,
                               const std::string& column, const char* operation) {
    if (!schema.has_table(table)) {
        throw std::runtime_error(
            std::string("Cannot ") + operation + ": table not found in schema: " + table);
    }
    if (!column.empty()) {
        const auto* table_def = schema.get_table(table);
        if (!table_def->has_column(column)) {
            throw std::runtime_error(
                std::string("Cannot ") + operation + ": column '" + column +
                "' not found in table '" + table + "'");
        }
    }
}
```

**Key insight:** The existing `require_collection()` already validates collection names. Many methods already call `require_collection()`. The gap is that:
1. Attribute/column names are NOT validated against the schema before SQL concatenation
2. Derived table names (vector_table, set_table, ts_table) from `Schema::find_*_table()` ARE already validated (they throw if not found), but this is not explicit at the concatenation site
3. PRAGMA queries in `schema.cpp` run during schema loading (before Schema exists), so they cannot use schema validation -- they need a different approach (character-class validation)

### Pattern 2: Identifier Character Validation for Pre-Schema Queries

**What:** For the 4 PRAGMA queries in `schema.cpp` that run before the Schema is loaded, validate that identifier strings contain only safe characters (alphanumeric + underscore).

**When to use:** Only in `schema.cpp` where identifiers come from SQLite's own `sqlite_master` table (already trusted), but validation adds defense-in-depth.

```cpp
inline bool is_safe_identifier(const std::string& name) {
    return std::all_of(name.begin(), name.end(), [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
    });
}
```

### Pattern 3: clang-tidy Configuration with Header Filtering

**What:** A `.clang-tidy` config at project root that applies checks only to project code, filtering out third-party headers.

**Why this matters:** The project includes heavy template libraries (sol2, spdlog, tomlplusplus) that will generate thousands of warnings if not filtered. The `HeaderFilterRegex` must match only project headers.

### Anti-Patterns to Avoid

- **SQLite identifier quoting as a fix:** Using `"double-quoted"` identifiers in SQL strings hides bugs instead of catching them. A misspelled column name like `"valeu"` would pass through quoting but fail at runtime. Validation catches it early with a clear error message.
- **Parameterized identifiers:** SQL `?` parameters only work for values, not for table/column names. This is a fundamental SQL limitation, not something to work around.
- **Running clang-tidy on third-party code:** Never include deps in analysis. Use `HeaderFilterRegex` to exclude `_deps/`, `build/`, etc.
- **Enabling all clang-tidy checks at once:** Start with the required check families. Adding everything (e.g., `cppcoreguidelines-*`) creates massive noise.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Static analysis | Custom linting scripts | clang-tidy | Thousands of battle-tested checks, AST-aware |
| SQL identifier safety | Regex-based SQL parsing | Schema validation + character checks | Schema is the source of truth; regex can't parse SQL |

**Key insight:** The Schema object is already loaded and contains complete knowledge of all valid tables and columns. Use it as the validation authority.

## Common Pitfalls

### Pitfall 1: clang-tidy False Positives from sol2/spdlog Headers
**What goes wrong:** Template-heavy libraries like sol2 generate warnings in project code when templates are instantiated.
**Why it happens:** clang-tidy analyzes the expanded template, which may violate naming or style rules.
**How to avoid:** Use `HeaderFilterRegex` to only analyze project headers: `HeaderFilterRegex: 'include/quiver/|src/'`. Add `// NOLINT` comments for specific unavoidable cases in `lua_runner.cpp`.
**Warning signs:** Hundreds of warnings from a single file (lua_runner.cpp is 1127 lines of sol2 template usage).

### Pitfall 2: PRAGMA Queries Cannot Use Schema Validation
**What goes wrong:** Attempting to validate PRAGMA identifiers against the Schema, but the Schema doesn't exist yet (it's being built from these PRAGMAs).
**Why it happens:** `Schema::load_from_database()` uses PRAGMAs to discover tables, so the Schema doesn't exist during this phase.
**How to avoid:** Use character-class validation (alphanumeric + underscore only) for PRAGMA identifiers in `schema.cpp`. The table names come from `sqlite_master` which is trusted, but validation adds defense-in-depth.
**Warning signs:** Circular dependency between Schema loading and Schema validation.

### Pitfall 3: MinGW GCC + clang-tidy Compatibility
**What goes wrong:** clang-tidy may not find MinGW headers or may report false positives due to GCC-specific extensions.
**Why it happens:** The project builds with `Strawberry\c\bin\c++.exe` (MinGW GCC), but clang-tidy uses Clang's frontend.
**How to avoid:** Pass `--extra-arg=--target=x86_64-w64-mingw32` or use `-p build` to point at the compile_commands.json. May need `--extra-arg=-I<path>` for MinGW system headers.
**Warning signs:** System header not found errors, `unknown type name '__int128'`, or similar platform-specific issues.

### Pitfall 4: readability-identifier-naming Configuration Complexity
**What goes wrong:** The project has two naming conventions: C++ uses `snake_case` for everything, C API uses `quiver_` prefixed `snake_case` with `_t` suffixed types. A single naming rule breaks one or the other.
**Why it happens:** C API files (`src/c/*.cpp`) define C-linkage functions with different naming patterns than the C++ library.
**How to avoid:** Either: (a) disable `readability-identifier-naming` for C API files via per-directory `.clang-tidy`, or (b) configure naming rules that accommodate both patterns, or (c) use NOLINT for C API declarations.
**Warning signs:** Hundreds of naming violations in C API code.

### Pitfall 5: Validation Overhead on Hot Paths
**What goes wrong:** Adding validation to every SQL construction could add measurable overhead if called in tight loops.
**Why it happens:** Methods like `update_vector_integers` construct SQL inside a loop per-element.
**How to avoid:** Validate once at the method entry point, not inside loops. The existing pattern already does this -- `require_collection` is called once, then the loop runs. Attribute validation should follow the same pattern.
**Warning signs:** Validation calls inside `for` loops that iterate over vector/set rows.

## Code Examples

### Inventory of SQL Concatenation Sites

**Files with SQL string concatenation (ordered by severity):**

| File | Concat Sites | Types | Current Validation |
|------|-------------|-------|-------------------|
| `database_read.cpp` | 19 | table + column names | `require_collection()` only (no column check) |
| `database_update.cpp` | 26 | table + column names | `require_collection()` + `find_vector/set_table()` |
| `database_create.cpp` | 8 | table + column names | `require_collection()` + `find_table_for_column()` |
| `database_relations.cpp` | 4 | table + column names | `require_collection()` + FK lookup |
| `database_time_series.cpp` | 10 | table + column names | `require_collection()` + `find_time_series_table()` |
| `database_delete.cpp` | 1 | table name only | `require_collection()` |
| `database.cpp` | 1 | PRAGMA user_version (integer only) | N/A (integer, not identifier) |
| `schema.cpp` | 4 | PRAGMA table/fk/index names | None (pre-schema) |

**Total: ~73 SQL concatenation sites across 8 files.**

### Validation Gap Analysis

**Already validated (safe):**
- Collection names via `require_collection()` -- present in ALL public methods
- Vector/set/time-series table names via `Schema::find_*_table()` -- these throw if table not found
- PRAGMA user_version -- concatenates an integer (`std::to_string(version)`), not a user string

**NOT validated (the gap to fix):**
- Column/attribute names in `database_read.cpp` (19 sites) -- no column existence check
- Column/attribute names in `database_update.cpp` scalar updates (3 sites) -- validated by TypeValidator but NOT by schema has_column
- Column names built dynamically in `create_element` / `update_element` (from Element) -- validated by TypeValidator
- FK `to_table` name in `database_relations.cpp` (2 sites) -- comes from schema FK metadata (already trusted)
- PRAGMA identifiers in `schema.cpp` (4 sites) -- table/index names from sqlite_master (trusted source, but no character validation)

**Key finding:** The `TypeValidator::validate_scalar()` already calls `schema_.get_data_type(table, column)` which throws if the column doesn't exist. So many paths ARE indirectly validated via TypeValidator. The read paths are the main gap -- they skip TypeValidator entirely.

### Recommended Validation Helper

```cpp
// In database_impl.h, added to Impl struct
void require_column(const std::string& table, const std::string& column, const char* operation) const {
    require_schema(operation);
    const auto* table_def = schema->get_table(table);
    if (!table_def) {
        throw std::runtime_error(std::string("Cannot ") + operation + ": table not found: " + table);
    }
    if (!table_def->has_column(column)) {
        throw std::runtime_error(std::string("Cannot ") + operation +
                                  ": column '" + column + "' not found in table '" + table + "'");
    }
}
```

### Recommended .clang-tidy Configuration

```yaml
Checks: >
  -*,
  bugprone-*,
  modernize-*,
  performance-*,
  readability-identifier-naming,
  readability-redundant-*,
  readability-simplify-*,
  -bugprone-easily-swappable-parameters,
  -modernize-use-trailing-return-type,
  -modernize-avoid-c-arrays,
  -bugprone-narrowing-conversions

WarningsAsErrors: ''

HeaderFilterRegex: '(include/quiver/|src/)'

CheckOptions:
  # C++ naming conventions
  - key: readability-identifier-naming.ClassCase
    value: CamelCase
  - key: readability-identifier-naming.StructCase
    value: CamelCase
  - key: readability-identifier-naming.EnumCase
    value: CamelCase
  - key: readability-identifier-naming.FunctionCase
    value: lower_case
  - key: readability-identifier-naming.MethodCase
    value: lower_case
  - key: readability-identifier-naming.VariableCase
    value: lower_case
  - key: readability-identifier-naming.MemberCase
    value: lower_case
  - key: readability-identifier-naming.PrivateMemberSuffix
    value: '_'
  - key: readability-identifier-naming.NamespaceCase
    value: lower_case
  - key: readability-identifier-naming.EnumConstantCase
    value: CamelCase
  - key: readability-identifier-naming.ConstexprVariableCase
    value: lower_case
  - key: readability-identifier-naming.GlobalConstantCase
    value: lower_case
```

### CMake Integration for clang-tidy

```cmake
# Add to root CMakeLists.txt alongside existing format target
find_program(CLANG_TIDY clang-tidy)
if(CLANG_TIDY)
    add_custom_target(tidy
        COMMAND ${CLANG_TIDY}
            -p ${CMAKE_BINARY_DIR}
            --header-filter="(include/quiver/|src/)"
            ${ALL_SOURCE_FILES}
        COMMENT "Running clang-tidy"
    )
endif()
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| SQL string quoting | Schema-based identifier validation | Best practice for ORMs/query builders | Catches bugs at validation time, not SQL execution time |
| Manual static analysis | clang-tidy in CI | Widely adopted 2018+ | Automated, consistent, catches real bugs |

## Open Questions

1. **How strict should PRAGMA identifier validation be?**
   - What we know: PRAGMA names come from `sqlite_master` (trusted source). Character-class validation is defense-in-depth.
   - What's unclear: Whether the effort is worth it given these names are already from SQLite itself.
   - Recommendation: Add character-class validation -- it's cheap (5 lines) and provides defense-in-depth. Match pattern: `^[A-Za-z_][A-Za-z0-9_]*$`.

2. **Should clang-tidy run in CI or just as a developer target?**
   - What we know: No CI system was observed in the codebase.
   - What's unclear: Whether there's an external CI pipeline.
   - Recommendation: Start as a CMake custom target (`cmake --build build --target tidy`). Add to CI when CI exists.

3. **How to handle lua_runner.cpp with clang-tidy?**
   - What we know: This 1127-line file is almost entirely sol2 template instantiations. It already needs `/bigobj` on MSVC.
   - What's unclear: How many clang-tidy warnings it will generate even with header filtering.
   - Recommendation: Run clang-tidy on it early in the phase to measure. May need per-function `// NOLINT` or a `.clang-tidy` override in the source directory.

4. **C API naming convention handling?**
   - What we know: C API uses `quiver_database_*` function names and `quiver_*_t` type names. C++ uses `CamelCase` classes and `snake_case` methods.
   - What's unclear: Whether `readability-identifier-naming` can be configured to handle both conventions.
   - Recommendation: Use `HeaderFilterRegex` that includes C API headers, but configure `GlobalFunctionCase` to `lower_case` (which accommodates both `snake_case` C functions and C++ methods). May need NOLINT for a few C API struct names that use `_t` suffix.

## Detailed File-by-File SQL Concatenation Catalog

### database_read.cpp (19 sites)
All follow pattern: `"SELECT " + attribute + " FROM " + table [+ " WHERE id = ?"]`
- Lines 8, 24, 40: `read_scalar_integers/floats/strings` -- collection validated, attribute NOT validated
- Lines 57, 69, 81: `read_scalar_*_by_id` -- collection validated, attribute NOT validated
- Lines 94, 102, 110: `read_vector_*` -- table from `find_vector_table()` (validated), attribute NOT validated against that specific table
- Lines 118, 126, 134: `read_vector_*_by_id` -- same pattern
- Lines 142, 150, 158: `read_set_*` -- table from `find_set_table()` (validated), attribute NOT validated
- Lines 166, 174, 182: `read_set_*_by_id` -- same pattern
- Line 188: `read_element_ids` -- collection validated, "id" column is hardcoded (safe)

### database_update.cpp (26 sites)
- Lines 28, 36: `update_element` scalar UPDATE -- collection validated, column names come from Element (need validation)
- Lines 74, 88, 96: vector table INSERT in `update_element` -- table from `find_table_for_column()` (validated)
- Lines 104, 117, 125: set table INSERT in `update_element` -- same
- Lines 133, 146, 154: time series table INSERT in `update_element` -- same
- Lines 172, 186, 200: `update_scalar_*` -- collection validated, attribute validated by TypeValidator (which checks schema)
- Lines 217-269: `update_vector_*` -- table from `find_vector_table()` (validated), attribute in column position
- Lines 289-339: `update_set_*` -- table from `find_set_table()` (validated), attribute in column position

### database_create.cpp (8 sites)
- Line 24: INSERT INTO collection -- collection validated
- Line 102: INSERT INTO vector_table -- table from schema routing (validated)
- Line 139: INSERT INTO set_table -- table from schema routing (validated)
- Line 154: SELECT FROM fk.to_table -- to_table comes from schema metadata (trusted)
- Line 194: INSERT INTO ts_table -- table from schema routing (validated)

### database_relations.cpp (4 sites)
- Line 30: SELECT FROM to_table -- to_table from FK metadata (trusted)
- Line 38: UPDATE collection SET attribute -- collection validated, attribute checked against FK list
- Line 65: SELECT/JOIN collection + to_table + attribute -- all from schema FK metadata

### database_time_series.cpp (10 sites)
- Lines 76-82: SELECT with dynamic column list -- columns from table_def (trusted)
- Line 131: DELETE FROM ts_table -- table from `find_time_series_table()` (validated)
- Line 148: INSERT INTO ts_table -- table from find (validated), dim_col from table_def (trusted)
- Lines 229-235: SELECT for time_series_files -- columns from table_def (trusted)
- Lines 273-277: DELETE/INSERT for time_series_files -- table from find (validated)

### schema.cpp (4 sites)
- Line 296: `PRAGMA table_info(` + table `)` -- table comes from sqlite_master
- Line 331: `PRAGMA foreign_key_list(` + table `)` -- table comes from sqlite_master
- Line 360: `PRAGMA index_list(` + table `)` -- table comes from sqlite_master
- Line 374: `PRAGMA index_info(` + idx.name `)` -- index name comes from index_list PRAGMA

## Sources

### Primary (HIGH confidence)
- Direct codebase analysis of all 8 SQL-constructing source files
- `compile_commands.json` verified present at `build/compile_commands.json`
- clang-tidy v20.1.7 verified installed at `C:\Program Files\LLVM\bin\clang-tidy.exe`
- CMake 3.29.2 with `CMAKE_EXPORT_COMPILE_COMMANDS ON` already configured

### Secondary (MEDIUM confidence)
- clang-tidy check families (bugprone-*, modernize-*, performance-*, readability-identifier-naming) are stable and well-documented in LLVM docs
- MinGW + clang-tidy compatibility is generally good but may need extra args for system headers

## Metadata

**Confidence breakdown:**
- SQL validation approach: HIGH -- direct codebase analysis, clear gap identified, existing Schema provides validation authority
- clang-tidy configuration: HIGH -- tool is installed, compile_commands.json exists, check families are well-documented
- Naming convention handling: MEDIUM -- may need iteration to find right config for dual C++/C naming
- MinGW compatibility: MEDIUM -- known to work but may need extra args; needs testing

**Research date:** 2026-02-10
**Valid until:** 2026-03-10 (stable domain, no rapidly changing dependencies)
