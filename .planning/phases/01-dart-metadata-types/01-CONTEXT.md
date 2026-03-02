# Phase 1: Dart Metadata Types - Context

**Gathered:** 2026-03-01
**Status:** Ready for planning

<domain>
## Phase Boundary

Replace all Dart 3 anonymous record types with proper `ScalarMetadata` and `GroupMetadata` classes in the Dart binding. The C++/C API layer is unchanged — this is a Dart-only refactoring to match the Julia and Python bindings' type structure.

</domain>

<decisions>
## Implementation Decisions

### Field naming convention
- Use Dart-idiomatic **camelCase** for all fields: `dataType`, `notNull`, `primaryKey`, `defaultValue`, `isForeignKey`, `referencesCollection`, `referencesColumn`
- Same field set as Julia/Python (8 fields for ScalarMetadata, 3 for GroupMetadata) — only casing differs
- `GroupMetadata.dimensionColumn` uses **empty string** for vectors/sets (not nullable) — matches Julia/Python behavior
- No special documentation needed — the camelCase rule in CLAUDE.md already covers this

### Class design style
- **Simple `final` fields** with `const` constructor — no equality/hashCode/toString overrides
- Constructor uses **named required parameters**: `ScalarMetadata({required this.name, required this.dataType, ...})`
- New **standalone `metadata.dart`** file in `bindings/dart/lib/src/` — imported by `database.dart`, not a `part` file
- Mirrors Python's `metadata.py` pattern and follows `element.dart`/`exceptions.dart` standalone file pattern

### Data type representation
- `dataType` field stays as **`int`** matching C enum values (0=INTEGER, 1=FLOAT, 2=STRING, 3=DATE_TIME)
- No Dart enum — consistent with Julia which uses `C.quiver_data_type_t` integer values
- Switch statements in `readScalarsById`/`readVectorsById`/`readSetsById` continue using `quiver_data_type_t.QUIVER_DATA_TYPE_INTEGER` constants

### Export surface
- Export `ScalarMetadata` and `GroupMetadata` from `quiverdb.dart` with explicit `show` clause
- Keep `quiver_data_type_t` exported — users need it to interpret `dataType` field values
- Move `_parseScalarMetadata`/`_parseGroupMetadata` from `database.dart` to `metadata.dart` as **factory constructors** (e.g., `ScalarMetadata.fromNative(quiver_scalar_metadata_t)`)

### Claude's Discretion
- Exact factory constructor implementation details
- Whether `fromNative` constructors are `factory` or named constructors
- Test structure and assertion patterns for new metadata class tests

</decisions>

<specifics>
## Specific Ideas

No specific requirements — standard refactoring following the established patterns in Julia/Python bindings.

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `_parseScalarMetadata` (database.dart:99-113): Existing parsing logic to extract into factory constructor
- `_parseGroupMetadata` (database.dart:133-157): Existing parsing logic for group metadata
- `element.dart`, `exceptions.dart`: Standalone file pattern to follow for `metadata.dart`
- `bindings/python/src/quiverdb/metadata.py`: Reference implementation with `ScalarMetadata` and `GroupMetadata`
- `bindings/julia/src/database_metadata.jl`: Reference `struct ScalarMetadata` and `struct GroupMetadata`

### Established Patterns
- Dart `part`/`part of` for database extensions (database_metadata.dart, database_read.dart, etc.)
- Standalone files for independent types (element.dart, exceptions.dart) — metadata classes follow this
- `Arena`-based FFI memory management in all database methods
- `check()` helper for error code validation

### Integration Points
- `database.dart`: Remove `_parseScalarMetadata`/`_parseGroupMetadata`, import `metadata.dart`
- `database_metadata.dart`: All return types change from inline records to `ScalarMetadata`/`GroupMetadata`
- `database_read.dart`: `readScalarsById`/`readVectorsById`/`readSetsById` access `.name`/`.dataType` on metadata class instances (should work unchanged since field names match current record fields)
- `quiverdb.dart`: Add export line for `ScalarMetadata` and `GroupMetadata`

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 01-dart-metadata-types*
*Context gathered: 2026-03-01*
