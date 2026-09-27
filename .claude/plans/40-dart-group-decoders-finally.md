# 40 — Dart group decoders: free the C result in `finally`

**Batch** 4 · **Severity** low · **Breaking** no · **Size** S · **Layers** Dart binding only
**Depends on** none · **Overlaps with** 17 (Dart `readTimeSeriesRow` in the same file), 18 (the whole-group readers call `_decodeGroupRows`; 18 changes Julia/Python/JS, not Dart), 41 (comment edits in the same file)

## Why

`bindings/dart/lib/src/database_read.dart` has two decoders of the columnar C group result that
free it only on the success path:

1. `_decodeGroupRows` (currently ~L1116-1175), used by `readVectorGroupById` and `readSetGroupById`.
   It calls `stringToDateTime(ptr[r].cast<Utf8>().toDartString(), collection, colName)` for
   `DATE_TIME` columns inside the decode loop, then frees afterwards:
   ```dart
       bindings.quiver_database_free_time_series_data(
         outColNames.value,
         outColTypes.value,
         outColData.value,
         outColHasValue.value,
         colCount,
         rowCount,
       );

       return rows;
   ```
2. `readTimeSeriesGroup` (currently ~L1187-1275). It calls `getTimeSeriesMetadata(collection,
   group)` and `stringToDateTime(...)` for the dimension column while the C buffers are held, and
   frees with `// Free C-allocated memory` just before `return result;`.

`stringToDateTime` (`lib/src/date_time.dart`) throws `ArgumentError` on a value outside the
`YYYY-MM-DD[THH:MM:SS]` grammar, and `toDartString` throws `FormatException` on invalid UTF-8.
Either leaks every name/type/data/mask array that `marshal_group_rows_to_c` allocated. A caller that
catches the error and keeps iterating leaks on every call. An unreadable date can be in the table if
the database predates the DATE_TIME write gate, or was written by another tool or by raw SQL.

The binding already treats this as a bug elsewhere: `LuaRunner.run` (`lib/src/lua_runner.dart`,
currently ~L75-82) frees its C string "in its own finally so a decode failure cannot leak it".

Principle: Ownership — explicit, unambiguous resource release.

## Constraints and decisions

- **Maintainer note (binding):** add one Dart test. It writes a malformed `date_time` by raw SQL and
  asserts that `readTimeSeriesGroup` throws `ArgumentError`.
- Only the `finally` is needed. Do not reorder `getTimeSeriesMetadata` before the C read. Once the
  free sits in a `finally`, the order does not matter, and moving the lookup would add a metadata
  round-trip on the empty-result path.
- Leave the other string readers alone. They can only throw on non-UTF-8 TEXT.
- Leave the empty-result early returns (`if (colCount == 0 || rowCount == 0) return ...`) **before**
  the `try`. The C API allocates nothing in that case.

## Changes

### 1. `_decodeGroupRows`

Current structure:
```dart
    final colCount = outColCount.value;
    final rowCount = outRowCount.value;

    if (colCount == 0 || rowCount == 0) return [];

    final rows = List.generate(rowCount, (_) => <String, Object?>{});
    for (var c = 0; c < colCount; c++) {
      ...decode...
    }

    bindings.quiver_database_free_time_series_data(
      outColNames.value, outColTypes.value, outColData.value, outColHasValue.value, colCount, rowCount,
    );

    return rows;
  }
```
New:
```dart
    final colCount = outColCount.value;
    final rowCount = outRowCount.value;

    if (colCount == 0 || rowCount == 0) return [];

    // The result is C-heap allocated, so the caller's Arena cannot own it: free it in a finally so
    // a DateTime parse or UTF-8 decode failure mid-loop cannot leak it.
    try {
      final rows = List.generate(rowCount, (_) => <String, Object?>{});
      for (var c = 0; c < colCount; c++) {
        ...decode, unchanged...
      }
      return rows;
    } finally {
      bindings.quiver_database_free_time_series_data(
        outColNames.value,
        outColTypes.value,
        outColData.value,
        outColHasValue.value,
        colCount,
        rowCount,
      );
    }
  }
```
The decode loop body moves inside the `try` byte for byte. Update the method's leading comment,
`// Decodes the columnar typed-arrays + per-cell mask result ... then frees the C allocations.`,
to end with "... and always frees the C allocations, even when decoding throws."

### 2. `readTimeSeriesGroup`

Inside the existing outer `try { ... } finally { arena.releaseAll(); }`, wrap everything after the
empty-result early return in its own `try/finally`:
```dart
      if (colCount == 0 || rowCount == 0) return {};

      // The result is C-heap allocated, so the Arena cannot own it: free it in its own finally so
      // a metadata lookup, DateTime parse or UTF-8 decode failure cannot leak it.
      try {
        // Get dimension column for DateTime parsing
        final meta = getTimeSeriesMetadata(collection, group);
        final dimCol = meta.dimensionColumn;

        ...existing decode loop building `result`, unchanged...

        return result;
      } finally {
        bindings.quiver_database_free_time_series_data(
          outColNames.value,
          outColTypes.value,
          outColData.value,
          outColHasValue.value,
          colCount,
          rowCount,
        );
      }
```
Delete the old `// Free C-allocated memory` block that sat before `return result;`.

## Tests

### `bindings/dart/test/database_time_series_group_test.dart`

Add inside `group('Time Series Read', () { ... })` (currently ~L9):

```dart
    test('malformed dimension value throws ArgumentError and the handle stays usable', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        final id = db.createElement('Collection', {'label': 'Item 1'});
        db.updateTimeSeriesGroup('Collection', 'data', id, {
          'date_time': ['2024-01-01T10:00:00'],
          'value': [1.5],
        });
        // Bypass the DATE_TIME write gate, as a pre-gate database or another tool would.
        db.queryString(
          'UPDATE Collection_time_series_data SET date_time = ? WHERE id = ?',
          ['2024-1-5', id],
        );

        expect(
          () => db.readTimeSeriesGroup('Collection', 'data', id),
          throwsA(isA<ArgumentError>()),
        );
        // A second read fails the same way: no crash, no double free.
        expect(
          () => db.readTimeSeriesGroup('Collection', 'data', id),
          throwsA(isA<ArgumentError>()),
        );
      } finally {
        db.close();
      }
    });
```

Before starting, check three things:
- The file's `testsPath` and `path` imports match the other tests in this file.
- `collections.sql` has `Collection_time_series_data(id, date_time TEXT, value REAL)`. It does at HEAD.
- `queryString(String sql, [List<Object?>? parameters])` exists (`lib/src/database_query.dart:8`). It
  prepares and steps an UPDATE, which returns no row, so the call returns `null`.

This test passes before the fix too, because the throw happens either way. It pins the error path;
the leak is shown by code review, not by the test. Say so in the commit message.

For `_decodeGroupRows`, add the same kind of test only if the schema offers a `date_`-prefixed
vector or set column. Check with `grep -ln "_vector_.*date_\|date_.*TEXT" tests/schemas/valid/*.sql`.
If none exists, skip it: the change is the same shape and code review covers it.

## Docs and changelog

- `bindings/dart/CLAUDE.md`: add one line near the `LuaRunner.run` note on freeing (currently
  ~L95-98): "The columnar group decoders (`_decodeGroupRows`, `readTimeSeriesGroup`) free the C
  result in their own `finally` for the same reason."
- `CHANGELOG.md`, under `## [0.11.0] — unreleased` → `### Fixed`:
  ```markdown
  - **Dart: the group readers no longer leak when decoding fails.** `readTimeSeriesGroup`,
    `readVectorGroupById` and `readSetGroupById` freed the C result only on success; a date value
    outside the accepted grammar (possible in a database written before the DATE_TIME write gate,
    or by raw SQL) leaked it on every call.
  ```

## Verification

From the repo root:
1. `bindings/dart/test/test.bat test/database_time_series_group_test.dart`
2. `bindings/dart/test/test.bat` (full suite)
3. `cd bindings/dart && dart analyze`
4. `scripts/format.bat`

## Acceptance criteria

- [ ] Both decoders free inside a `finally` that covers the whole decode, including the metadata
      lookup in `readTimeSeriesGroup`.
- [ ] The empty-result early returns stay before the `try`.
- [ ] The new test passes, the full suite is green and `dart analyze` is clean.

## Pitfalls

- Declare `result`/`rows` inside the `try`, and `return` from inside it.
- Do not free twice. Delete the old happy-path free calls.

## Out of scope

- The other readers' string decoding.
- Julia's equivalent (plan 36) and the Python and JS readers.
