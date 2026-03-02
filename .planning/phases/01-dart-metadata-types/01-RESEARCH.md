# Phase 1: Dart Metadata Types - Research

**Researched:** 2026-03-01
**Domain:** Dart class design, FFI interop, record-to-class refactoring
**Confidence:** HIGH

## Summary

This phase replaces Dart 3 anonymous record types with proper `ScalarMetadata` and `GroupMetadata` classes. The refactoring is mechanically straightforward: the existing record fields and the target class fields use identical names and types, so all downstream code (tests, convenience methods in `database_read.dart`) continues to work unchanged. The primary risk is missing a call site, but the Dart analyzer will catch any mismatches at compile time.

The codebase already has reference implementations in Julia (`database_metadata.jl` lines 1-16) and Python (`metadata.py` lines 26-46) that define the exact same struct/class with the same field set. The Dart implementation mirrors these with camelCase naming per the locked decision.

**Primary recommendation:** Create `metadata.dart` with two `final` classes using `const` constructors with named required parameters, add `fromNative` factory constructors that extract the parsing logic from `database.dart`, then mechanically replace all inline record type annotations across three files.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Use Dart-idiomatic **camelCase** for all fields: `dataType`, `notNull`, `primaryKey`, `defaultValue`, `isForeignKey`, `referencesCollection`, `referencesColumn`
- Same field set as Julia/Python (8 fields for ScalarMetadata, 3 for GroupMetadata) -- only casing differs
- `GroupMetadata.dimensionColumn` uses **empty string** for vectors/sets (not nullable) -- matches Julia/Python behavior
- No special documentation needed -- the camelCase rule in CLAUDE.md already covers this
- **Simple `final` fields** with `const` constructor -- no equality/hashCode/toString overrides
- Constructor uses **named required parameters**: `ScalarMetadata({required this.name, required this.dataType, ...})`
- New **standalone `metadata.dart`** file in `bindings/dart/lib/src/` -- imported by `database.dart`, not a `part` file
- Mirrors Python's `metadata.py` pattern and follows `element.dart`/`exceptions.dart` standalone file pattern
- `dataType` field stays as **`int`** matching C enum values (0=INTEGER, 1=FLOAT, 2=STRING, 3=DATE_TIME)
- No Dart enum -- consistent with Julia which uses `C.quiver_data_type_t` integer values
- Switch statements in `readScalarsById`/`readVectorsById`/`readSetsById` continue using `quiver_data_type_t.QUIVER_DATA_TYPE_INTEGER` constants
- Export `ScalarMetadata` and `GroupMetadata` from `quiverdb.dart` with explicit `show` clause
- Keep `quiver_data_type_t` exported -- users need it to interpret `dataType` field values
- Move `_parseScalarMetadata`/`_parseGroupMetadata` from `database.dart` to `metadata.dart` as **factory constructors** (e.g., `ScalarMetadata.fromNative(quiver_scalar_metadata_t)`)

### Claude's Discretion
- Exact factory constructor implementation details
- Whether `fromNative` constructors are `factory` or named constructors
- Test structure and assertion patterns for new metadata class tests

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| DART-01 | Define ScalarMetadata class with 8 named fields | Class design pattern from element.dart; field names from CONTEXT.md decisions; native type `quiver_scalar_metadata_t` at bindings.dart:2675 |
| DART-02 | Define GroupMetadata class with 3 named fields (groupName, dimensionColumn, valueColumns) | Field names from CONTEXT.md; `valueColumns` is `List<ScalarMetadata>`; native type `quiver_group_metadata_t` at bindings.dart:2697 |
| DART-03 | Replace inline record types in database.dart | Two parse functions at lines 89-157 become factory constructors in metadata.dart; `_parseScalarMetadata` and `_parseGroupMetadata` removed |
| DART-04 | Replace inline record types in database_metadata.dart | 8 methods with inline record return types (lines 6-15, 43-58, 85-101, 128-139, 189-207, 264-282, 355-371, 398-416) become `ScalarMetadata`, `GroupMetadata`, `List<ScalarMetadata>`, `List<GroupMetadata>` |
| DART-05 | Replace inline record types in database_read.dart dispatch code | `readScalarsById` (line 799), `readVectorsById` (line 824), `readSetsById` (line 851) access `.name` and `.dataType` on metadata -- field names match, no code changes needed beyond the type annotation removal that happens in DART-04 |
| DART-06 | Export from quiverdb.dart | Add `export 'src/metadata.dart' show ScalarMetadata, GroupMetadata;` to quiverdb.dart |
| DART-07 | All existing Dart tests pass unchanged | API compatibility guaranteed: record field access syntax (`.name`) is identical to class field access; all 15 test files use these field names |
| DART-08 | Add tests for ScalarMetadata and GroupMetadata construction and field access | New test group in existing metadata_test.dart or standalone; verify constructor, field access, nullable fields |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| dart:ffi | SDK 3.10+ | Foreign function interface | Required for native struct access in factory constructors |
| package:ffi | ^2.1.0 | FFI helpers (Arena, Utf8, etc.) | Already used throughout codebase |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| package:test | ^1.24.0 | Test framework | Already configured in pubspec.yaml |
| package:path | ^1.9.1 | Path construction for test schema files | Already used in all test files |

No new dependencies required. This phase uses only existing libraries.

## Architecture Patterns

### Recommended File Structure
```
bindings/dart/lib/src/
  metadata.dart          # NEW: ScalarMetadata, GroupMetadata classes
  database.dart          # MODIFIED: remove _parseScalarMetadata/_parseGroupMetadata, add import
  database_metadata.dart # MODIFIED: return types change from records to classes
  database_read.dart     # NO CHANGES NEEDED (field names already match)
bindings/dart/lib/
  quiverdb.dart          # MODIFIED: add export for metadata.dart
bindings/dart/test/
  metadata_test.dart     # MODIFIED: add construction/field-access tests
```

### Pattern 1: Standalone Type File (Existing Pattern)
**What:** Independent type classes live in their own file, imported (not `part of`) by database.dart.
**When to use:** Types that don't need access to Database internals.
**Example (element.dart, exceptions.dart):**
```dart
// element.dart - standalone file
import 'dart:ffi';
import 'package:ffi/ffi.dart';
import 'ffi/bindings.dart';
import 'ffi/library_loader.dart';

class Element {
  // ... self-contained class
}
```

`metadata.dart` follows this exact pattern. It imports `dart:ffi`, `package:ffi`, and `ffi/bindings.dart` for the native struct types.

### Pattern 2: Const Constructor with Named Required Parameters
**What:** Immutable value types with `final` fields and `const` constructor.
**When to use:** Data-carrying types that are constructed once and read many times.
**Example:**
```dart
class ScalarMetadata {
  final String name;
  final int dataType;
  final bool notNull;
  final bool primaryKey;
  final String? defaultValue;
  final bool isForeignKey;
  final String? referencesCollection;
  final String? referencesColumn;

  const ScalarMetadata({
    required this.name,
    required this.dataType,
    required this.notNull,
    required this.primaryKey,
    required this.defaultValue,
    required this.isForeignKey,
    required this.referencesCollection,
    required this.referencesColumn,
  });
}
```

### Pattern 3: Factory Constructor for Native Parsing
**What:** Named constructor or factory that converts a native FFI struct into a Dart class.
**When to use:** When bridging between C structs and Dart types.
**Example:**
```dart
/// Creates a ScalarMetadata from a native quiver_scalar_metadata_t struct.
ScalarMetadata.fromNative(quiver_scalar_metadata_t native)
    : name = native.name.cast<Utf8>().toDartString(),
      dataType = native.data_type,
      notNull = native.not_null != 0,
      primaryKey = native.primary_key != 0,
      defaultValue = native.default_value == nullptr
          ? null
          : native.default_value.cast<Utf8>().toDartString(),
      isForeignKey = native.is_foreign_key != 0,
      referencesCollection = native.references_collection == nullptr
          ? null
          : native.references_collection.cast<Utf8>().toDartString(),
      referencesColumn = native.references_column == nullptr
          ? null
          : native.references_column.cast<Utf8>().toDartString();
```

**Design choice:** Use a named constructor (not `factory`) because the initializer list form is cleaner and avoids the unnecessary `return` statement. The `const` primary constructor is still available for tests.

### Anti-Patterns to Avoid
- **Making `fromNative` the primary constructor:** Users should be able to construct metadata directly for testing. Keep the named-parameter `const` constructor as the default.
- **Adding `part of` directive to metadata.dart:** The `_parseScalarMetadata`/`_parseGroupMetadata` methods in `database.dart` are private to the Database class. Moving parsing into factory constructors on the metadata classes eliminates the need for `part of` -- `metadata.dart` is an independent import.
- **Adding equality/hashCode/toString:** The user explicitly decided against these overrides. Simple `final` fields only.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| FFI string conversion | Manual pointer arithmetic | `native.name.cast<Utf8>().toDartString()` | Standard pattern already used throughout codebase |
| Null pointer checks | Custom null-check helpers | Direct `== nullptr` comparison | Dart FFI idiom, matches existing code |

**Key insight:** The parsing logic already exists in `_parseScalarMetadata` and `_parseGroupMetadata`. This is a move-and-restructure operation, not a write-from-scratch operation.

## Common Pitfalls

### Pitfall 1: Missing a Record Type Occurrence
**What goes wrong:** A record type annotation is left in one of the three files, causing a type mismatch.
**Why it happens:** The record type is repeated verbatim in ~12 locations across `database_metadata.dart` (both in return types and in local variable type annotations).
**How to avoid:** After refactoring, run `dart analyze` -- any remaining record type that should be a class will produce a type error. Also grep for the record field pattern `({` in the three target files.
**Warning signs:** Dart analyzer warnings about record/class type mismatches.

### Pitfall 2: Breaking the `part of` Relationship
**What goes wrong:** `database_metadata.dart` and `database_read.dart` are `part of database.dart`. If `metadata.dart` is made a `part` file too, import conflicts arise.
**Why it happens:** Confusion between standalone imports and the `part`/`part of` system.
**How to avoid:** `metadata.dart` is a standalone file with its own imports. `database.dart` imports it with a regular `import` statement. The `part` files (`database_metadata.dart`, `database_read.dart`) see the import transitively.
**Warning signs:** "Part of two libraries" or "URI already imported" analyzer errors.

### Pitfall 3: Forgetting to Remove the Old Parse Methods
**What goes wrong:** `_parseScalarMetadata` and `_parseGroupMetadata` remain in `database.dart` as dead code.
**Why it happens:** Focusing only on adding the new code, not removing the old code.
**How to avoid:** Delete the two methods (database.dart lines 89-157) after adding the factory constructors. All call sites change from `_parseScalarMetadata(native)` to `ScalarMetadata.fromNative(native)`.
**Warning signs:** Dead code warnings from the analyzer.

### Pitfall 4: Import Visibility in Part Files
**What goes wrong:** `database_metadata.dart` (a `part of database.dart` file) cannot import `metadata.dart` directly.
**Why it happens:** Part files share the import namespace of their parent library file.
**How to avoid:** Add `import 'metadata.dart';` to `database.dart` (the parent library file). Part files automatically see all imports from the parent.
**Warning signs:** "Undefined class 'ScalarMetadata'" errors in part files.

## Code Examples

### metadata.dart (Complete Implementation)
```dart
import 'dart:ffi';

import 'package:ffi/ffi.dart';

import 'ffi/bindings.dart';

/// Metadata for a scalar attribute in a collection.
class ScalarMetadata {
  final String name;
  final int dataType;
  final bool notNull;
  final bool primaryKey;
  final String? defaultValue;
  final bool isForeignKey;
  final String? referencesCollection;
  final String? referencesColumn;

  const ScalarMetadata({
    required this.name,
    required this.dataType,
    required this.notNull,
    required this.primaryKey,
    required this.defaultValue,
    required this.isForeignKey,
    required this.referencesCollection,
    required this.referencesColumn,
  });

  /// Creates a ScalarMetadata from a native quiver_scalar_metadata_t struct.
  ScalarMetadata.fromNative(quiver_scalar_metadata_t native)
      : name = native.name.cast<Utf8>().toDartString(),
        dataType = native.data_type,
        notNull = native.not_null != 0,
        primaryKey = native.primary_key != 0,
        defaultValue = native.default_value == nullptr
            ? null
            : native.default_value.cast<Utf8>().toDartString(),
        isForeignKey = native.is_foreign_key != 0,
        referencesCollection = native.references_collection == nullptr
            ? null
            : native.references_collection.cast<Utf8>().toDartString(),
        referencesColumn = native.references_column == nullptr
            ? null
            : native.references_column.cast<Utf8>().toDartString();
}

/// Metadata for a vector, set, or time series group in a collection.
class GroupMetadata {
  final String groupName;
  final String dimensionColumn;
  final List<ScalarMetadata> valueColumns;

  const GroupMetadata({
    required this.groupName,
    required this.dimensionColumn,
    required this.valueColumns,
  });

  /// Creates a GroupMetadata from a native quiver_group_metadata_t struct.
  GroupMetadata.fromNative(quiver_group_metadata_t native)
      : groupName = native.group_name.cast<Utf8>().toDartString(),
        dimensionColumn = native.dimension_column == nullptr
            ? ''
            : native.dimension_column.cast<Utf8>().toDartString(),
        valueColumns = List.generate(
          native.value_column_count,
          (i) => ScalarMetadata.fromNative(native.value_columns[i]),
        );
}
```

### database.dart Changes (Import + Remove Parse Methods)
```dart
// ADD this import:
import 'metadata.dart';

// REMOVE lines 89-157 (_parseScalarMetadata and _parseGroupMetadata methods)
```

### database_metadata.dart Return Type Changes
```dart
// BEFORE:
({
  String name,
  int dataType,
  bool notNull,
  bool primaryKey,
  String? defaultValue,
  bool isForeignKey,
  String? referencesCollection,
  String? referencesColumn,
})
getScalarMetadata(String collection, String attribute) {
  // ...
  final result = _parseScalarMetadata(outMetadata.ref);
  // ...
}

// AFTER:
ScalarMetadata getScalarMetadata(String collection, String attribute) {
  // ...
  final result = ScalarMetadata.fromNative(outMetadata.ref);
  // ...
}
```

### quiverdb.dart Export Addition
```dart
// ADD this line:
export 'src/metadata.dart' show ScalarMetadata, GroupMetadata;
```

### Test for Construction and Field Access (DART-08)
```dart
group('ScalarMetadata construction', () {
  test('const constructor sets all fields', () {
    const metadata = ScalarMetadata(
      name: 'test_field',
      dataType: 0, // INTEGER
      notNull: true,
      primaryKey: false,
      defaultValue: null,
      isForeignKey: false,
      referencesCollection: null,
      referencesColumn: null,
    );
    expect(metadata.name, equals('test_field'));
    expect(metadata.dataType, equals(0));
    expect(metadata.notNull, isTrue);
    expect(metadata.primaryKey, isFalse);
    expect(metadata.defaultValue, isNull);
    expect(metadata.isForeignKey, isFalse);
    expect(metadata.referencesCollection, isNull);
    expect(metadata.referencesColumn, isNull);
  });

  test('const constructor with FK fields', () {
    const metadata = ScalarMetadata(
      name: 'parent_id',
      dataType: 0,
      notNull: true,
      primaryKey: false,
      defaultValue: null,
      isForeignKey: true,
      referencesCollection: 'Parent',
      referencesColumn: 'id',
    );
    expect(metadata.isForeignKey, isTrue);
    expect(metadata.referencesCollection, equals('Parent'));
    expect(metadata.referencesColumn, equals('id'));
  });
});

group('GroupMetadata construction', () {
  test('const constructor sets all fields', () {
    const metadata = GroupMetadata(
      groupName: 'values',
      dimensionColumn: '',
      valueColumns: [],
    );
    expect(metadata.groupName, equals('values'));
    expect(metadata.dimensionColumn, equals(''));
    expect(metadata.valueColumns, isEmpty);
  });

  test('const constructor with dimension column and value columns', () {
    const metadata = GroupMetadata(
      groupName: 'data',
      dimensionColumn: 'date_time',
      valueColumns: [
        ScalarMetadata(
          name: 'value',
          dataType: 1,
          notNull: true,
          primaryKey: false,
          defaultValue: null,
          isForeignKey: false,
          referencesCollection: null,
          referencesColumn: null,
        ),
      ],
    );
    expect(metadata.groupName, equals('data'));
    expect(metadata.dimensionColumn, equals('date_time'));
    expect(metadata.valueColumns.length, equals(1));
    expect(metadata.valueColumns[0].name, equals('value'));
  });
});
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Dart 3 anonymous records for metadata | Proper named classes | This phase | Matches Julia/Python bindings, improves readability |

**Context:** Dart 3 (released May 2023) introduced records. The original Dart binding used them heavily for metadata return types. While functional, they create verbose inline type annotations repeated across multiple files (~300 lines of repetitive type definitions per the project STATE.md).

## Open Questions

1. **Where to place DART-08 tests?**
   - What we know: `metadata_test.dart` already exists with 377 lines of metadata operation tests. New construction tests could go at the top of this file.
   - What's unclear: Whether to add a separate group at the top of the existing file or create a new file.
   - Recommendation: Add new test groups to the existing `metadata_test.dart` at the top, before the database-dependent tests. Construction tests don't need a database, making them fast and self-contained.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | package:test ^1.24.0 |
| Config file | pubspec.yaml (dev_dependencies) |
| Quick run command | `dart test test/metadata_test.dart` (from bindings/dart/) |
| Full suite command | `bindings/dart/test/test.bat` |

### Phase Requirements to Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| DART-01 | ScalarMetadata class with 8 fields | unit | `dart test test/metadata_test.dart` | Needs new group |
| DART-02 | GroupMetadata class with 3 fields | unit | `dart test test/metadata_test.dart` | Needs new group |
| DART-03 | Records replaced in database.dart | compile | `dart analyze` | N/A (compile check) |
| DART-04 | Records replaced in database_metadata.dart | compile | `dart analyze` | N/A (compile check) |
| DART-05 | Records replaced in database_read.dart | integration | `dart test test/database_read_test.dart` | Existing tests |
| DART-06 | Export from quiverdb.dart | integration | `dart test test/metadata_test.dart` | Existing (already imports quiverdb) |
| DART-07 | All existing tests pass | full suite | `dart test` | All 15 test files exist |
| DART-08 | Construction and field access tests | unit | `dart test test/metadata_test.dart` | Needs new group |

### Sampling Rate
- **Per task commit:** `dart test test/metadata_test.dart`
- **Per wave merge:** `dart test` (full suite from bindings/dart/)
- **Phase gate:** Full suite green

### Wave 0 Gaps
None -- existing test infrastructure covers all phase requirements. The `metadata_test.dart` file already exists and exercises all metadata operations. DART-08 adds new test groups to this file.

## Sources

### Primary (HIGH confidence)
- Direct codebase inspection of all files involved:
  - `bindings/dart/lib/src/database.dart` (lines 89-157: parse methods to extract)
  - `bindings/dart/lib/src/database_metadata.dart` (all 471 lines: return types to change)
  - `bindings/dart/lib/src/database_read.dart` (lines 799-869: convenience methods using record fields)
  - `bindings/dart/lib/quiverdb.dart` (export surface)
  - `bindings/dart/lib/src/ffi/bindings.dart` (lines 2675-2706: native struct definitions)
  - `bindings/dart/lib/src/element.dart` (standalone file pattern reference)
  - `bindings/dart/lib/src/exceptions.dart` (standalone file pattern reference)
  - `bindings/dart/test/metadata_test.dart` (377 lines: existing tests that must pass unchanged)
  - `bindings/dart/test/database_time_series_test.dart` (metadata field access patterns)
  - `bindings/python/src/quiverdb/metadata.py` (reference implementation)
  - `bindings/julia/src/database_metadata.jl` (reference implementation)
  - `bindings/dart/pubspec.yaml` (SDK and dependency versions)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - no new dependencies, all libraries already in use
- Architecture: HIGH - follows existing patterns (element.dart, exceptions.dart), all code directly inspected
- Pitfalls: HIGH - all identified pitfalls are caught by the Dart analyzer at compile time

**Research date:** 2026-03-01
**Valid until:** 2026-04-01 (stable refactoring, no external dependency changes)
