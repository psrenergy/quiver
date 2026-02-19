# Phase 10: Cross-Layer Documentation and Final Verification - Research

**Researched:** 2026-02-10
**Domain:** Cross-layer API naming documentation, CLAUDE.md authoring, full-stack test verification
**Confidence:** HIGH

## Summary

Phase 10 is a documentation and verification phase, not a code change phase. The primary deliverable is a new section in CLAUDE.md that documents the naming conventions across all five layers (C++, C API, Julia, Dart, Lua) with concrete cross-layer mapping examples. The secondary deliverable is running the full test suite (`scripts/build-all.bat`) end-to-end and confirming all tests pass.

The research involved a comprehensive audit of all public method/function names across every layer. The naming conventions are now fully standardized after Phases 1-8: C++ uses `snake_case`, C API uses `quiver_database_snake_case`, Julia uses `snake_case` with `!` for mutating functions, Dart uses `camelCase`, and Lua matches C++ `snake_case` exactly. The mapping is mechanical and predictable. A few binding-only convenience methods exist (e.g., `read_all_scalars_by_id` in Julia/Dart/Lua, `query_date_time` in Julia/Dart) that do not have direct C++ or C API counterparts -- these are thin wrappers that compose existing core operations.

The build infrastructure (`scripts/build-all.bat`) is already well-structured: it builds C++, runs C++ tests, C API tests, Julia tests, and Dart tests sequentially with fail-fast behavior. No changes to the build scripts are needed. The verification task is purely executing the existing pipeline and confirming green status.

**Primary recommendation:** Write a "Cross-Layer Naming Conventions" section in CLAUDE.md with a mapping table showing representative operations across all 5 layers, then run `scripts/build-all.bat` to confirm end-to-end pass.

## Standard Stack

This phase does not introduce new libraries or tools. It uses only:

### Core
| Tool | Version | Purpose | Why Standard |
|------|---------|---------|--------------|
| CLAUDE.md | N/A | Project documentation file | Already exists, self-updating per project principles |
| scripts/build-all.bat | N/A | Full-stack build and test | Already exists, runs all 5 test suites |

### Supporting
| Tool | Version | Purpose | When to Use |
|------|---------|---------|-------------|
| scripts/test-all.bat | N/A | Test-only (no rebuild) | When code is already built and just need re-verification |

## Architecture Patterns

### Pattern 1: Cross-Layer Naming Convention Table

**What:** A reference table in CLAUDE.md showing how a single logical operation maps to each layer's naming convention.

**When to use:** Any time a developer needs to find the equivalent function/method in another layer.

**Structure:** Group by operation category (lifecycle, CRUD, read, update, metadata, query, etc.) and show the transformation rules, then provide representative examples.

**Naming convention rules per layer:**

| Layer | Convention | Word Separator | Mutating Suffix | Prefix |
|-------|-----------|----------------|-----------------|--------|
| C++ | `snake_case` | `_` | None | None (method on `Database`) |
| C API | `snake_case` | `_` | None | `quiver_database_` |
| Julia | `snake_case` | `_` | `!` for mutating | None (free function taking `db::Database`) |
| Dart | `camelCase` | capital letter | None | None (method on `Database`) |
| Lua | `snake_case` | `_` | None | None (method on `db` userdata) |

**Transformation rules:**
1. **C++ to C API:** Prefix with `quiver_database_`. Example: `create_element` -> `quiver_database_create_element`
2. **C++ to Julia:** Same name, add `!` suffix if mutating (create/update/delete). Example: `create_element` -> `create_element!`
3. **C++ to Dart:** Convert `snake_case` to `camelCase`. Example: `read_scalar_integers` -> `readScalarIntegers`
4. **C++ to Lua:** Same name exactly. Example: `read_scalar_integers` -> `read_scalar_integers`

### Pattern 2: Binding-Only Convenience Methods Documentation

**What:** Some bindings provide additional convenience methods that compose core operations. These should be documented separately from the core cross-layer mapping.

**Julia-only convenience methods:**
- `read_scalar_date_time_by_id` -- wraps `read_scalar_string_by_id` + date parsing
- `read_vector_date_time_by_id` -- wraps `read_vector_strings_by_id` + date parsing
- `read_set_date_time_by_id` -- wraps `read_set_strings_by_id` + date parsing
- `query_date_time` -- wraps `query_string` + date parsing
- `read_all_scalars_by_id` -- iterates `list_scalar_attributes` + typed reads
- `read_all_vectors_by_id` -- iterates `list_vector_groups` + typed reads
- `read_all_sets_by_id` -- iterates `list_set_groups` + typed reads
- `read_vector_group_by_id` -- multi-column vector read composed from metadata + per-column reads
- `read_set_group_by_id` -- multi-column set read composed from metadata + per-column reads

**Dart-only convenience methods:**
- `readScalarDateTimeById` -- wraps `readScalarStringById` + date parsing
- `readVectorDateTimesById` -- wraps `readVectorStringsById` + date parsing
- `readSetDateTimesById` -- wraps `readSetStringsById` + date parsing
- `queryDateTime` / `queryDateTimeParams` -- wraps `queryString` + date parsing
- `readAllScalarsById` -- iterates `listScalarAttributes` + typed reads
- `readAllVectorsById` -- iterates `listVectorGroups` + typed reads
- `readAllSetsById` -- iterates `listSetGroups` + typed reads
- `readVectorGroupById` -- multi-column vector read
- `readSetGroupById` -- multi-column set read

**Lua-only convenience methods:**
- `read_all_scalars_by_id` -- iterates `list_scalar_attributes` + typed reads
- `read_all_vectors_by_id` -- iterates `list_vector_groups` + typed reads
- `read_all_sets_by_id` -- iterates `list_set_groups` + typed reads

### Pattern 3: CLAUDE.md Section Placement

**What:** The new naming conventions section should be placed between the existing "Naming Convention" subsection under "C++ Patterns" and the "C API Patterns" section. However, since it covers ALL layers, it is better as a top-level section.

**Recommendation:** Add a new top-level section `## Cross-Layer Naming Conventions` after the existing `## Core API` section and before `## Bindings`. This keeps it close to the API surface documentation and makes it a natural reference point.

### Anti-Patterns to Avoid
- **Exhaustive listing of every function:** The table should show representative examples per category, not every single function. The transformation rules are mechanical -- once you know the pattern, you can derive any name.
- **Duplicating existing documentation:** The C++ Naming Convention and C API Patterns sections already exist in CLAUDE.md. The new section should cross-reference them, not repeat them.
- **Documenting internal/private APIs:** Only public API methods that are bound across layers belong in the mapping table.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Full test execution | Custom test runner | `scripts/build-all.bat` | Already handles build + test in correct order with fail-fast |
| Test-only verification | Manual per-suite commands | `scripts/test-all.bat` | Runs all 4 test suites with summary |

**Key insight:** The build/test infrastructure is already complete. Phase 10 verification is executing existing scripts, not building new ones.

## Common Pitfalls

### Pitfall 1: Documentation Drift from Code
**What goes wrong:** The cross-layer naming table becomes stale as new methods are added in future development.
**Why it happens:** Documentation is not automatically validated against code.
**How to avoid:** Keep the documentation prescriptive (rules + representative examples) rather than exhaustive (every function listed). The transformation rules are mechanical -- they remain valid as long as the naming conventions hold.
**Warning signs:** The mapping table lists individual functions rather than showing patterns.

### Pitfall 2: Missing Binding-Only Methods in Documentation
**What goes wrong:** The documentation suggests a 1:1 mapping across all layers, but some bindings have convenience methods that don't exist in other layers.
**Why it happens:** Julia and Dart add DateTime convenience methods and bulk-read helpers that compose core operations.
**How to avoid:** Document these explicitly as "binding-only convenience methods" in a separate subsection, with a note that they compose core operations.
**Warning signs:** A user expects `read_scalar_date_time_by_id` to exist in C++ but it does not (it's Julia/Dart-only).

### Pitfall 3: Julia Mutating Convention (!) Not Obvious
**What goes wrong:** Developer forgets to add `!` suffix when looking up the Julia equivalent of a mutating C++ method.
**Why it happens:** The `!` convention is Julia-specific and not obvious to non-Julia developers.
**How to avoid:** Explicitly document the rule: "Julia mutating functions (create/update/delete) append `!` suffix" and show examples.
**Warning signs:** Julia test failures due to calling `create_element` instead of `create_element!`.

### Pitfall 4: Dart Factory Method Naming Difference
**What goes wrong:** The Dart factory methods use Dart constructor syntax (`Database.fromSchema`) instead of the C++ static method syntax (`Database::from_schema`).
**Why it happens:** Dart convention uses named constructors for factory patterns.
**How to avoid:** Document factory method naming separately from regular method naming.
**Warning signs:** Looking for `fromSchema` as a regular method instead of a named constructor.

### Pitfall 5: Phase 9 Incomplete -- Blocking Dependency
**What goes wrong:** Phase 10 depends on Phase 9 (Code Hygiene), which is still pending. If Phase 9 changes break tests, Phase 10 verification fails.
**Why it happens:** Phase 9 plans to add validation to SQL concatenation sites and integrate clang-tidy. These changes could introduce regressions.
**How to avoid:** Phase 9 must complete and pass all tests before Phase 10 verification. Phase 10's documentation task (CLAUDE.md writing) can proceed independently, but the verification task must come after Phase 9.
**Warning signs:** Running `build-all.bat` and getting failures that trace back to Phase 9 changes.

## Code Examples

### Complete Cross-Layer Mapping (Representative Operations)

The following shows the actual current method names across all five layers, verified from source code:

**Lifecycle:**

| Operation | C++ | C API | Julia | Dart | Lua |
|-----------|-----|-------|-------|------|-----|
| Open from schema | `Database::from_schema()` | `quiver_database_from_schema()` | `from_schema()` | `Database.fromSchema()` | N/A |
| Open from migrations | `Database::from_migrations()` | `quiver_database_from_migrations()` | `from_migrations()` | `Database.fromMigrations()` | N/A |
| Close | `~Database()` | `quiver_database_close()` | `close!()` | `close()` | N/A |
| Describe | `describe()` | `quiver_database_describe()` | `describe()` | `describe()` | `describe()` |

**CRUD:**

| Operation | C++ | C API | Julia | Dart | Lua |
|-----------|-----|-------|-------|------|-----|
| Create element | `create_element()` | `quiver_database_create_element()` | `create_element!()` | `createElement()` | `create_element()` |
| Update element | `update_element()` | `quiver_database_update_element()` | `update_element!()` | `updateElement()` | `update_element()` |
| Delete element | `delete_element()` | `quiver_database_delete_element()` | `delete_element!()` | `deleteElement()` | `delete_element()` |

**Read Scalars:**

| Operation | C++ | C API | Julia | Dart | Lua |
|-----------|-----|-------|-------|------|-----|
| Read scalar integers | `read_scalar_integers()` | `quiver_database_read_scalar_integers()` | `read_scalar_integers()` | `readScalarIntegers()` | `read_scalar_integers()` |
| Read scalar integer by ID | `read_scalar_integer_by_id()` | `quiver_database_read_scalar_integer_by_id()` | `read_scalar_integer_by_id()` | `readScalarIntegerById()` | `read_scalar_integer_by_id()` |

**Update Scalars:**

| Operation | C++ | C API | Julia | Dart | Lua |
|-----------|-----|-------|-------|------|-----|
| Update scalar integer | `update_scalar_integer()` | `quiver_database_update_scalar_integer()` | `update_scalar_integer!()` | `updateScalarInteger()` | `update_scalar_integer()` |

**Vectors (same pattern as scalars, substitute "vector" for "scalar"):**

| Operation | C++ | C API | Julia | Dart | Lua |
|-----------|-----|-------|-------|------|-----|
| Read vector floats by ID | `read_vector_floats_by_id()` | `quiver_database_read_vector_floats_by_id()` | `read_vector_floats_by_id()` | `readVectorFloatsById()` | `read_vector_floats_by_id()` |
| Update vector strings | `update_vector_strings()` | `quiver_database_update_vector_strings()` | `update_vector_strings!()` | `updateVectorStrings()` | `update_vector_strings()` |

**Metadata:**

| Operation | C++ | C API | Julia | Dart | Lua |
|-----------|-----|-------|-------|------|-----|
| Get scalar metadata | `get_scalar_metadata()` | `quiver_database_get_scalar_metadata()` | `get_scalar_metadata()` | `getScalarMetadata()` | `get_scalar_metadata()` |
| List vector groups | `list_vector_groups()` | `quiver_database_list_vector_groups()` | `list_vector_groups()` | `listVectorGroups()` | `list_vector_groups()` |

**Time Series:**

| Operation | C++ | C API | Julia | Dart | Lua |
|-----------|-----|-------|-------|------|-----|
| Read time series group | `read_time_series_group()` | `quiver_database_read_time_series_group()` | `read_time_series_group()` | `readTimeSeriesGroup()` | `read_time_series_group()` |
| Update time series files | `update_time_series_files()` | `quiver_database_update_time_series_files()` | `update_time_series_files!()` | `updateTimeSeriesFiles()` | `update_time_series_files()` |

**Query:**

| Operation | C++ | C API | Julia | Dart | Lua |
|-----------|-----|-------|-------|------|-----|
| Query string | `query_string()` | `quiver_database_query_string()` | `query_string()` | `queryString()` | `query_string()` |
| Query string (parameterized) | `query_string(sql, params)` | `quiver_database_query_string_params()` | `query_string(db, sql, params)` | `queryStringParams()` | `query_string(sql, params)` |

**Relations:**

| Operation | C++ | C API | Julia | Dart | Lua |
|-----------|-----|-------|-------|------|-----|
| Update scalar relation | `update_scalar_relation()` | `quiver_database_update_scalar_relation()` | `update_scalar_relation!()` | `updateScalarRelation()` | `update_scalar_relation()` |
| Read scalar relation | `read_scalar_relation()` | `quiver_database_read_scalar_relation()` | `read_scalar_relation()` | `readScalarRelation()` | `read_scalar_relation()` |

**CSV:**

| Operation | C++ | C API | Julia | Dart | Lua |
|-----------|-----|-------|-------|------|-----|
| Export CSV | `export_csv()` | `quiver_database_export_csv()` | `export_csv()` | `exportCSV()` | N/A |
| Import CSV | `import_csv()` | `quiver_database_import_csv()` | `import_csv()` | `importCSV()` | N/A |

### Proposed CLAUDE.md Section Structure

```markdown
## Cross-Layer Naming Conventions

### Transformation Rules

| From C++ | To C API | To Julia | To Dart | To Lua |
|----------|----------|----------|---------|--------|
| `snake_case` | prefix `quiver_database_` | same + `!` if mutating | `camelCase` | same |
| `Database::from_schema()` | `quiver_database_from_schema()` | `from_schema()` | `Database.fromSchema()` | N/A |

### Naming Convention by Layer
- **C++**: `verb_[category_]type[_by_id]` pattern, snake_case
- **C API**: `quiver_database_` prefix + C++ name
- **Julia**: C++ name + `!` suffix for mutating operations (create/update/delete)
- **Dart**: camelCase conversion of C++ name
- **Lua**: Exact C++ method name (1:1 match)

### Representative Cross-Layer Examples
[table showing one example per operation category]

### Binding-Only Convenience Methods
[table showing methods unique to Julia/Dart/Lua]
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Ad-hoc naming per layer | Consistent mechanical transformation from C++ | Phases 3-8 (2026-02-10) | Any developer can predict method names in any layer from C++ name |
| No cross-layer docs | CLAUDE.md cross-reference section | Phase 10 (pending) | Single reference for all naming conventions |

## Open Questions

1. **Should the documentation include the full API surface or just representative examples?**
   - What we know: There are ~60 public methods on Database in C++. Listing all 60 in a 5-column table would be ~300 cells.
   - What's unclear: Whether the user wants exhaustive or representative documentation.
   - Recommendation: Use representative examples (1-2 per operation category) plus the mechanical transformation rules. This keeps the docs maintainable and avoids drift. If the user wants exhaustive, it can be added later.

2. **Should Phase 10 documentation task wait for Phase 9 to complete?**
   - What we know: Phase 10 depends on Phase 9. Phase 9 adds SQL validation and clang-tidy but should not change any public API names.
   - What's unclear: Whether Phase 9 is done in a separate branch that hasn't merged yet.
   - Recommendation: The documentation task (CLAUDE.md writing) can proceed independently since Phase 9 does not change naming. The verification task (build-all.bat) must wait for Phase 9 completion.

3. **Where in CLAUDE.md should the new section go?**
   - What we know: CLAUDE.md currently has Architecture, Principles, Build & Test, C++ Patterns, C API Patterns, Schema Conventions, Core API, Bindings sections.
   - What's unclear: Whether the user wants it as a subsection or top-level section.
   - Recommendation: Add as a new top-level section `## Cross-Layer Naming Conventions` after `## Core API` and before `## Bindings`. This places it right where developers transition from understanding the API to understanding how bindings map to it.

## Sources

### Primary (HIGH confidence)
- Direct source code analysis of all 5 layers:
  - `include/quiver/database.h` -- C++ public API (62 methods)
  - `include/quiver/c/database.h` -- C API functions (62 functions)
  - `bindings/julia/src/*.jl` -- Julia bindings (all 10 source files)
  - `bindings/dart/lib/src/*.dart` -- Dart bindings (all 9 source files)
  - `src/lua_runner.cpp` -- Lua bindings (1127 lines, all registered methods)
- `scripts/build-all.bat` -- build and test pipeline
- `scripts/test-all.bat` -- test-only pipeline
- `.planning/ROADMAP.md` -- phase dependencies and success criteria
- Existing CLAUDE.md -- current documentation structure

### Secondary (MEDIUM confidence)
- `.planning/phases/09-code-hygiene/09-RESEARCH.md` -- Phase 9 scope (does not change public API)

## Metadata

**Confidence breakdown:**
- Cross-layer naming mapping: HIGH -- verified directly from source code across all 5 layers
- CLAUDE.md section structure: HIGH -- existing document structure well-understood, placement is clear
- Verification approach: HIGH -- build-all.bat already exists and handles all test suites
- Phase 9 dependency handling: MEDIUM -- Phase 9 should not affect naming, but untested assumption

**Research date:** 2026-02-10
**Valid until:** 2026-03-10 (documentation phase, no rapidly changing dependencies)
