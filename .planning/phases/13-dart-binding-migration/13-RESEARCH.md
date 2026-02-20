# Phase 13: Dart Binding Migration - Research

**Researched:** 2026-02-19
**Domain:** Dart FFI binding migration for multi-column time series C API
**Confidence:** HIGH

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Columnar Map parameter: `updateTimeSeriesGroup(col, grp, id, {'date_time': [...], 'temp': [...], 'humidity': [...]})`
- Each key is a column name matching the schema, value is a typed List
- Strict types only -- no auto-coercion. List<int> for INTEGER, List<double> for REAL, List<String> for TEXT. Throw on type mismatch
- No Dart-side validation of column names -- let C API validate and surface its error messages (CAPI-05)
- Empty Map = clear all rows: `updateTimeSeriesGroup(col, grp, id, {})` clears rows for that element
- Return `Map<String, List<Object>>` -- columnar format with column names as keys, typed lists as values
- Concrete runtime types: `List<int>` for INTEGER, `List<double>` for REAL, `List<String>` for TEXT
- Empty results return empty `Map<String, List<Object>>()` (no keys, no data)
- Auto-parse dimension column to Dart DateTime on read (dimension column returns `List<DateTime>`)
- Only dimension column (identified via metadata) gets DateTime treatment -- other TEXT columns remain String
- Update accepts both DateTime and String for the dimension column (DateTime auto-formatted to string)
- Throw error on parse failure -- if stored string can't parse to DateTime, throw
- Reuse existing `date_time.dart` parsing logic (stringToDateTime/dateTimeToString)
- Replace in-place -- no deprecation wrappers, no backwards compatibility
- Old row-oriented interface removed entirely
- Regenerate FFI bindings from current C headers using `bindings/dart/generator/generator.bat`
- Rewrite existing Dart time series tests to use new Map-based interface
- Add multi-column mixed-type tests using Phase 11 schema (mixed_time_series.sql)
- Mirror Julia's decisions as closely as possible with Dart-idiomatic syntax

### Claude's Discretion
- Exact Map parameter type (Map<String, List<Object>> vs Map<String, dynamic> for the update signature)
- Internal marshaling strategy for Map -> C API columnar arrays
- Arena allocation patterns for safe FFI calls
- Test file organization (new file vs extend existing)

### Deferred Ideas (OUT OF SCOPE)
None
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| BIND-03 | Dart updateTimeSeriesGroup accepts Map: `updateTimeSeriesGroup(col, grp, id, {'date_time': [...], 'temp': [...]})` | Existing `updateTimeSeriesGroup` in `database_update.dart` (line 273) must be rewritten from row-oriented `List<Map<String, Object?>>` to columnar `Map<String, List<Object>>`. Marshaling pattern established by `_marshalParams` in `database.dart`. New C API signature requires `column_names[]`, `column_types[]`, `column_data[]` parallel arrays. |
| BIND-04 | Dart readTimeSeriesGroup returns multi-column data with correct types per column | Existing `readTimeSeriesGroup` in `database_read.dart` (line 892) must be rewritten from 2-output (date_times + values) to multi-column output (column_names, column_types, column_data, column_count, row_count). Metadata infrastructure already exists via `getTimeSeriesMetadata` for identifying dimension column for DateTime parsing. |
</phase_requirements>

## Summary

Phase 13 migrates two Dart methods (`updateTimeSeriesGroup` and `readTimeSeriesGroup`) from the old 2-column C API to the new multi-column C API introduced in Phase 11. The Dart FFI bindings file (`bindings.dart`) is auto-generated and currently stale -- it still references the old function signatures. The first step is regenerating FFI bindings via `dart run ffigen`, then rewriting the two Dart wrapper methods to use the new signatures.

The core challenge is marshaling between Dart's `Map<String, List<Object>>` and the C API's parallel arrays of `column_names[]`, `column_types[]`, and `column_data[]` (where `column_data` is `void**` with each element being a typed array pointer). Dart's `dart:ffi` represents `void**` as `Pointer<Pointer<Void>>`, and the binding must cast individual column arrays to the correct native pointer type (`Pointer<Int64>`, `Pointer<Double>`, `Pointer<Pointer<Char>>`) based on the column type.

The existing Dart codebase establishes strong patterns for this work: `Arena` for scoped native allocation, `_marshalParams` for type-discriminated marshaling, and `_parseGroupMetadata` / `getTimeSeriesMetadata` for metadata access. The Julia Phase 12 implementation provides a complete behavioral reference.

**Primary recommendation:** Regenerate FFI bindings first, then rewrite `updateTimeSeriesGroup` and `readTimeSeriesGroup` following the existing Arena/check pattern, using metadata to determine column types for read and DateTime handling.

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| dart:ffi | Dart 3.10+ | Native function calls, pointer types | Built-in, required for C interop |
| package:ffi | ^2.1.0 | Arena allocator, Utf8 extension, calloc | Standard companion to dart:ffi |
| package:ffigen | ^11.0.0 | Auto-generates Dart FFI bindings from C headers | Already configured in pubspec.yaml |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| package:test | ^1.24.0 | Test framework | All test files |
| package:path | ^1.9.1 | Path manipulation in tests | Schema file paths |

### Alternatives Considered
None -- all libraries are already in use and locked by the project.

## Architecture Patterns

### Existing File Structure (no changes needed)
```
bindings/dart/
  lib/src/
    database.dart          # Main class + _marshalParams, _parseGroupMetadata
    database_update.dart   # Extension: updateTimeSeriesGroup (MODIFY)
    database_read.dart     # Extension: readTimeSeriesGroup (MODIFY)
    database_metadata.dart # Extension: getTimeSeriesMetadata (EXISTS, no change)
    date_time.dart         # stringToDateTime, dateTimeToString (EXISTS, no change)
    ffi/bindings.dart      # Auto-generated FFI (REGENERATE)
  test/
    database_time_series_test.dart  # Time series tests (REWRITE)
```

### Pattern 1: Arena-Scoped FFI Calls
**What:** Every FFI call is wrapped in Arena try/finally for automatic native memory cleanup.
**When to use:** Every method that allocates native memory for FFI parameters.
**Example:** (from existing `database_update.dart`)
```dart
void updateVectorIntegers(String collection, String attribute, int id, List<int> values) {
  _ensureNotClosed();
  final arena = Arena();
  try {
    final nativeValues = arena<Int64>(values.length);
    for (var i = 0; i < values.length; i++) {
      nativeValues[i] = values[i];
    }
    check(bindings.quiver_database_update_vector_integers(
      _ptr,
      collection.toNativeUtf8(allocator: arena).cast(),
      attribute.toNativeUtf8(allocator: arena).cast(),
      id, nativeValues, values.length,
    ));
  } finally {
    arena.releaseAll();
  }
}
```

### Pattern 2: Check + Free for Read Results
**What:** C API allocates output arrays; Dart copies values to Dart objects, then calls the matching free function.
**When to use:** All read operations that receive C-allocated output arrays.
**Example:** (from existing `database_read.dart`)
```dart
// 1. Allocate output pointers in arena
// 2. Call C API via check()
// 3. Copy C data to Dart collections
// 4. Call bindings.quiver_database_free_xxx() to release C memory
// 5. Return Dart collections
```

### Pattern 3: Type-Discriminated Marshaling
**What:** Switch on data type enum to determine pointer cast and allocation strategy.
**When to use:** When handling heterogeneous column types (INTEGER/FLOAT/STRING).
**Example:** (from existing `_marshalParams` in `database.dart`)
```dart
if (p is int) {
  types[i] = quiver_data_type_t.QUIVER_DATA_TYPE_INTEGER;
  final ptr = arena<Int64>();
  ptr.value = p;
  values[i] = ptr.cast();
} else if (p is double) {
  types[i] = quiver_data_type_t.QUIVER_DATA_TYPE_FLOAT;
  // ...
}
```

### Anti-Patterns to Avoid
- **Building Dart-side validation of column names/types:** The C API already validates (CAPI-05). Dart binding must remain thin per project philosophy.
- **Auto-coercion (int -> double):** User decision explicitly says "strict types only" for Dart (unlike Julia which auto-coerces). Throw on type mismatch.
- **Keeping old API alongside new:** Decision is replace in-place. No deprecation.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Column name validation | Type checking column names vs schema | C API validation (CAPI-05) | C API already does this with clear error messages |
| DateTime string formatting | Custom formatting logic | `dateTimeToString()` / `stringToDateTime()` from `date_time.dart` | Already exists and tested |
| Group metadata parsing | Manual FFI struct parsing | `_parseGroupMetadata()` / `getTimeSeriesMetadata()` | Already exists in `database_metadata.dart` |
| Native memory management | Manual malloc/free | `Arena` from `package:ffi` | Automatic cleanup in finally block |
| FFI binding generation | Hand-written FFI declarations | `dart run ffigen` | Config already in `pubspec.yaml` |

**Key insight:** The existing Dart binding infrastructure (Arena, check, metadata helpers, date_time helpers) handles all cross-cutting concerns. The migration is purely about changing the FFI call site signatures and the Dart-to-C marshaling logic.

## Common Pitfalls

### Pitfall 1: Stale FFI Bindings
**What goes wrong:** The auto-generated `bindings.dart` still has the old 2-column signatures (date_times + values). Attempting to call the new 9-arg C API through old bindings will fail at runtime or compile time.
**Why it happens:** FFI bindings are generated once and not auto-updated when C headers change.
**How to avoid:** Run `dart run ffigen` (or `bindings/dart/generator/generator.bat`) FIRST before any code changes.
**Warning signs:** Compile errors about wrong number of arguments to `quiver_database_read_time_series_group` or `quiver_database_update_time_series_group`.

### Pitfall 2: void** Marshaling in Dart FFI
**What goes wrong:** The C API uses `void**` for `column_data` (array of typed array pointers). Dart FFI represents this as `Pointer<Pointer<Void>>`. Incorrectly casting or allocating typed arrays into `void*` slots can cause memory corruption or wrong values.
**Why it happens:** `void*` is type-erased; each slot must be independently cast to the correct pointer type based on `column_types[i]`.
**How to avoid:** For update: allocate typed arrays in Arena (`arena<Int64>(rowCount)`, `arena<Double>(rowCount)`, etc.), then cast to `Pointer<Void>` and store in the `Pointer<Pointer<Void>>` array. For read: read `column_types[i]` first, then cast `column_data[i]` to the correct typed pointer before reading values.
**Warning signs:** Values read back as garbage, wrong integers, or crashes.

### Pitfall 3: Free Function Signature Mismatch
**What goes wrong:** The old `quiver_database_free_time_series_data` took `(char**, double*, size_t)`. The new one takes `(char**, int*, void**, size_t, size_t)`. Using the old free with new data structures will leak or corrupt memory.
**Why it happens:** The free function signature changed alongside the read function in Phase 11.
**How to avoid:** After regenerating bindings, verify the new free function signature matches. Update the read method's cleanup code to call the new 5-arg free.
**Warning signs:** Memory leaks, double-free crashes, test failures in read operations.

### Pitfall 4: DateTime Dimension Column Identification
**What goes wrong:** Incorrectly identifying which column is the dimension column causes DateTime parsing on value columns or raw strings on the dimension column.
**Why it happens:** The dimension column is identified via metadata, not by name convention alone.
**How to avoid:** Call `getTimeSeriesMetadata(collection, group)` and use `metadata.dimensionColumn` to identify which column gets DateTime treatment. This pattern is already used in the existing `readTimeSeriesGroup` (line 918).
**Warning signs:** TypeError at runtime when dimension column returns String instead of DateTime.

### Pitfall 5: List Length Validation on Update
**What goes wrong:** Passing columns with different list lengths creates an inconsistent update. The C API validates column names/types but does NOT validate that all arrays have the same length -- it trusts `row_count`.
**Why it happens:** The C API takes a single `row_count` parameter and assumes all arrays are that length.
**How to avoid:** Validate in Dart that all List values in the Map have equal length before marshaling. Throw `ArgumentError` for mismatches (same as Julia, line 149-155 of `database_update.jl`).
**Warning signs:** Buffer overflows, garbage data in database, crashes.

### Pitfall 6: Dart .dart_tool Cache After DLL Rebuild
**What goes wrong:** After rebuilding the C library with new API functions, old cached DLL is used by Dart tests.
**Why it happens:** Dart caches compiled native assets in `.dart_tool/`.
**How to avoid:** Clear `.dart_tool/hooks_runner/` and `.dart_tool/lib/` directories before running tests after a C API rebuild (as noted in CLAUDE.md).
**Warning signs:** "Symbol not found" or "function not found" errors at runtime even though the C library was rebuilt.

## Code Examples

### Example 1: Update Marshaling (Map -> C API Columnar Arrays)
```dart
// Recommended implementation pattern for updateTimeSeriesGroup
void updateTimeSeriesGroup(String collection, String group, int id, Map<String, List<Object>> data) {
  _ensureNotClosed();
  final arena = Arena();
  try {
    // Empty map = clear
    if (data.isEmpty) {
      check(bindings.quiver_database_update_time_series_group(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        group.toNativeUtf8(allocator: arena).cast(),
        id,
        nullptr, nullptr, nullptr, 0, 0,
      ));
      return;
    }

    // Validate equal lengths
    final rowCount = data.values.first.length;
    for (final entry in data.entries) {
      if (entry.value.length != rowCount) {
        throw ArgumentError('All column lists must have the same length');
      }
    }

    final columnCount = data.length;
    final columnNames = arena<Pointer<Char>>(columnCount);
    final columnTypes = arena<Int>(columnCount);
    final columnData = arena<Pointer<Void>>(columnCount);

    var i = 0;
    for (final entry in data.entries) {
      columnNames[i] = entry.key.toNativeUtf8(allocator: arena).cast();
      final values = entry.value;

      if (values.isEmpty) {
        // Type doesn't matter for 0 rows, but need valid type
        columnTypes[i] = quiver_data_type_t.QUIVER_DATA_TYPE_STRING;
        columnData[i] = nullptr;
      } else if (values.first is int) {
        columnTypes[i] = quiver_data_type_t.QUIVER_DATA_TYPE_INTEGER;
        final arr = arena<Int64>(rowCount);
        for (var r = 0; r < rowCount; r++) { arr[r] = values[r] as int; }
        columnData[i] = arr.cast();
      } else if (values.first is double) {
        columnTypes[i] = quiver_data_type_t.QUIVER_DATA_TYPE_FLOAT;
        final arr = arena<Double>(rowCount);
        for (var r = 0; r < rowCount; r++) { arr[r] = values[r] as double; }
        columnData[i] = arr.cast();
      } else if (values.first is String) {
        columnTypes[i] = quiver_data_type_t.QUIVER_DATA_TYPE_STRING;
        final arr = arena<Pointer<Char>>(rowCount);
        for (var r = 0; r < rowCount; r++) {
          arr[r] = (values[r] as String).toNativeUtf8(allocator: arena).cast();
        }
        columnData[i] = arr.cast();
      } else if (values.first is DateTime) {
        columnTypes[i] = quiver_data_type_t.QUIVER_DATA_TYPE_STRING;
        final arr = arena<Pointer<Char>>(rowCount);
        for (var r = 0; r < rowCount; r++) {
          arr[r] = dateTimeToString(values[r] as DateTime).toNativeUtf8(allocator: arena).cast();
        }
        columnData[i] = arr.cast();
      } else {
        throw ArgumentError('Unsupported value type: ${values.first.runtimeType}');
      }
      i++;
    }

    check(bindings.quiver_database_update_time_series_group(
      _ptr,
      collection.toNativeUtf8(allocator: arena).cast(),
      group.toNativeUtf8(allocator: arena).cast(),
      id, columnNames, columnTypes, columnData, columnCount, rowCount,
    ));
  } finally {
    arena.releaseAll();
  }
}
```

### Example 2: Read Unmarshaling (C API Columnar Arrays -> Map)
```dart
// Recommended implementation pattern for readTimeSeriesGroup
Map<String, List<Object>> readTimeSeriesGroup(String collection, String group, int id) {
  _ensureNotClosed();
  final arena = Arena();
  try {
    final outColNames = arena<Pointer<Pointer<Char>>>();
    final outColTypes = arena<Pointer<Int>>();
    final outColData = arena<Pointer<Pointer<Void>>>();
    final outColCount = arena<Size>();
    final outRowCount = arena<Size>();

    check(bindings.quiver_database_read_time_series_group(
      _ptr,
      collection.toNativeUtf8(allocator: arena).cast(),
      group.toNativeUtf8(allocator: arena).cast(),
      id, outColNames, outColTypes, outColData, outColCount, outRowCount,
    ));

    final colCount = outColCount.value;
    final rowCount = outRowCount.value;

    if (colCount == 0 || rowCount == 0) return {};

    // Get dimension column for DateTime parsing
    final meta = getTimeSeriesMetadata(collection, group);
    final dimCol = meta.dimensionColumn;

    final result = <String, List<Object>>{};
    for (var c = 0; c < colCount; c++) {
      final colName = outColNames.value[c].cast<Utf8>().toDartString();
      final colType = outColTypes.value[c];

      if (colType == quiver_data_type_t.QUIVER_DATA_TYPE_INTEGER) {
        final ptr = outColData.value[c].cast<Int64>();
        result[colName] = List<int>.generate(rowCount, (r) => ptr[r]);
      } else if (colType == quiver_data_type_t.QUIVER_DATA_TYPE_FLOAT) {
        final ptr = outColData.value[c].cast<Double>();
        result[colName] = List<double>.generate(rowCount, (r) => ptr[r]);
      } else {
        // STRING or DATE_TIME
        final ptr = outColData.value[c].cast<Pointer<Char>>();
        if (colName == dimCol) {
          result[colName] = List<DateTime>.generate(
            rowCount, (r) => stringToDateTime(ptr[r].cast<Utf8>().toDartString()),
          );
        } else {
          result[colName] = List<String>.generate(
            rowCount, (r) => ptr[r].cast<Utf8>().toDartString(),
          );
        }
      }
    }

    // Free C-allocated memory
    bindings.quiver_database_free_time_series_data(
      outColNames.value, outColTypes.value, outColData.value, colCount, rowCount,
    );

    return result;
  } finally {
    arena.releaseAll();
  }
}
```

### Example 3: Test Pattern (Multi-Column Mixed Types)
```dart
// Test pattern matching Julia Phase 12 tests
test('multi-column update and read with mixed types', () {
  final db = Database.fromSchema(
    ':memory:',
    path.join(testsPath, 'schemas', 'valid', 'mixed_time_series.sql'),
  );
  try {
    db.createElement('Configuration', {'label': 'Test Config'});
    final id = db.createElement('Sensor', {'label': 'Sensor 1'});

    db.updateTimeSeriesGroup('Sensor', 'readings', id, {
      'date_time': ['2024-01-01T10:00:00', '2024-01-01T11:00:00'],
      'temperature': [20.5, 21.3],
      'humidity': [45, 50],
      'status': ['normal', 'high'],
    });

    final result = db.readTimeSeriesGroup('Sensor', 'readings', id);
    expect(result.length, equals(4)); // 4 columns
    expect(result['date_time'], isA<List<DateTime>>());
    expect(result['temperature'], equals([20.5, 21.3]));
    expect(result['humidity'], equals([45, 50]));
    expect(result['status'], equals(['normal', 'high']));
  } finally {
    db.close();
  }
});
```

## Critical Implementation Details

### FFI Type Mapping for New C API

| C Type | Dart FFI Native Type | Dart FFI Dart Type |
|--------|---------------------|-------------------|
| `char**` (column_names) | `Pointer<Pointer<Char>>` | same |
| `int*` (column_types) | `Pointer<Int>` or `Pointer<Int32>` | same |
| `void**` (column_data) | `Pointer<Pointer<Void>>` | same |
| `size_t` | `Size` | `int` |
| `int64_t*` (integer column) | `Pointer<Int64>` | same |
| `double*` (float column) | `Pointer<Double>` | same |
| `char**` (string column) | `Pointer<Pointer<Char>>` | same |

### Update Method Signature Decision

**Recommendation:** Use `Map<String, List<Object>>` for the parameter type. Rationale:
- Mirrors the read return type exactly (`Map<String, List<Object>>`)
- `List<Object>` allows `List<int>`, `List<double>`, `List<String>`, and `List<DateTime>` at runtime
- Strict type checking happens at marshaling time (check first element's runtimeType)
- `Map<String, dynamic>` would be less self-documenting and allow non-list values
- Symmetry between read and update makes the API predictable

### Row Count Validation Strategy

Dart must validate that all lists in the Map have equal length before marshaling. This is the one validation that happens in the binding (not the C API), because the C API takes a single `row_count` and trusts all arrays are that length. Julia does this same validation (line 149-155 of `database_update.jl`).

### Strict Type Enforcement

Unlike Julia's auto-coercion (Int -> Float when schema expects REAL), Dart enforces strict types per user decision. Type is determined from the runtime type of list elements:
- `List<int>` elements -> `QUIVER_DATA_TYPE_INTEGER`
- `List<double>` elements -> `QUIVER_DATA_TYPE_FLOAT`
- `List<String>` elements -> `QUIVER_DATA_TYPE_STRING`
- `List<DateTime>` elements -> `QUIVER_DATA_TYPE_STRING` (auto-formatted)

If an `int` is passed for a REAL column, the C API will reject it with a type mismatch error. This is correct per user decision.

### Test File Organization

**Recommendation:** Rewrite `database_time_series_test.dart` in-place. It already has the right structure (groups for Metadata, Read, Update, Files). The existing tests need their `List<Map<String, Object?>>` calls changed to `Map<String, List<Object>>` calls. Add new multi-column tests at the end.

## Execution Order

1. **Regenerate FFI bindings** -- `bindings/dart/generator/generator.bat` (or `dart run ffigen` from `bindings/dart/`)
2. **Rewrite `updateTimeSeriesGroup`** in `database_update.dart` -- change signature and marshaling
3. **Rewrite `readTimeSeriesGroup`** in `database_read.dart` -- change signature and unmarshaling
4. **Rewrite existing tests** in `database_time_series_test.dart` -- update to new interface
5. **Add multi-column mixed-type tests** using `mixed_time_series.sql` schema
6. **Run Dart tests** -- `bindings/dart/test/test.bat`

Steps 2 and 3 can be done in either order. Steps 4 and 5 depend on steps 2 and 3.

## Open Questions

1. **ffigen output for void\*\***
   - What we know: The C header uses `void**` and `const void* const*` for column_data. Dart ffigen should map these to `Pointer<Pointer<Void>>`.
   - What's unclear: Exact ffigen output for `const void* const*` -- it may generate `Pointer<Pointer<Void>>` or require manual adjustment.
   - Recommendation: Regenerate first and inspect the output. If ffigen generates incorrect types, manual editing of the generated file may be needed (but this would be unusual for ffigen 11.x). **Confidence: MEDIUM** -- ffigen handles `void**` well but `const void* const*` is less common.

2. **Pointer<Int> vs Pointer<Int32> for column_types**
   - What we know: C API uses `int*` for column_types. On most platforms `int` is 32 bits. Dart FFI's `Int` maps to C `int` (platform-dependent), while `Int32` is always 32-bit.
   - What's unclear: How ffigen maps `int*` -- likely `Pointer<ffi.Int>` which is correct.
   - Recommendation: Use whatever ffigen generates. It handles platform-specific int sizing correctly. **Confidence: HIGH**

## Sources

### Primary (HIGH confidence)
- Codebase inspection: `bindings/dart/lib/src/database_update.dart` -- existing `updateTimeSeriesGroup` (old interface)
- Codebase inspection: `bindings/dart/lib/src/database_read.dart` -- existing `readTimeSeriesGroup` (old interface)
- Codebase inspection: `bindings/dart/lib/src/ffi/bindings.dart` -- current (stale) FFI bindings
- Codebase inspection: `src/c/database_time_series.cpp` -- new C API implementation (Phase 11)
- Codebase inspection: `include/quiver/c/database.h` -- new C API header signatures
- Codebase inspection: `bindings/julia/src/database_update.jl` -- Julia Phase 12 update implementation
- Codebase inspection: `bindings/julia/src/database_read.jl` -- Julia Phase 12 read implementation
- Codebase inspection: `bindings/julia/test/test_database_time_series.jl` -- Julia Phase 12 multi-column tests
- Codebase inspection: `tests/schemas/valid/mixed_time_series.sql` -- multi-column test schema

### Secondary (MEDIUM confidence)
- Dart FFI documentation for `Pointer<Pointer<Void>>` and void* handling -- based on training data, verified consistent with existing codebase patterns

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- all libraries already in use, no new dependencies
- Architecture: HIGH -- extending well-established patterns in the codebase
- Pitfalls: HIGH -- identified through direct codebase analysis (stale bindings, void** marshaling, free function changes)

**Research date:** 2026-02-19
**Valid until:** 2026-03-19 (stable -- no external dependency changes expected)
