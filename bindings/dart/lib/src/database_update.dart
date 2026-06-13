part of 'database.dart';

/// Update operations for Database.
extension DatabaseUpdate on Database {
  /// Updates an element's attributes by ID using a map of values.
  /// Handles scalars, vectors, and sets uniformly - any attributes in the map will be updated.
  void updateElement(String collection, int id, Map<String, Object?> values) {
    _ensureNotClosed();
    final element = Element();
    try {
      for (final entry in values.entries) {
        element.set(entry.key, entry.value);
      }
      updateElementFromBuilder(collection, id, element);
    } finally {
      element.dispose();
    }
  }

  /// Updates an element's attributes by ID using an Element builder.
  /// Handles scalars, vectors, and sets uniformly - any attributes in the Element will be updated.
  void updateElementFromBuilder(String collection, int id, Element element) {
    _ensureNotClosed();
    final arena = Arena();
    try {
      check(
        bindings.quiver_database_update_element(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          id,
          element.ptr.cast(),
        ),
      );
    } finally {
      arena.releaseAll();
    }
  }

  // ==========================================================================
  // Update time series attributes
  // ==========================================================================

  /// Updates a time series group by element ID (replaces all rows).
  /// Takes a Map of column names to typed Lists.
  /// Supported value types: int, double, String, DateTime.
  /// An empty Map clears all rows for that element.
  void updateTimeSeriesGroup(
    String collection,
    String group,
    int id,
    Map<String, List<Object?>> data,
  ) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      if (data.isEmpty) {
        check(
          bindings.quiver_database_update_time_series_group(
            _ptr,
            collection.toNativeUtf8(allocator: arena).cast(),
            group.toNativeUtf8(allocator: arena).cast(),
            id,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            0,
            0,
          ),
        );
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
      final columnHasValue = arena<Pointer<Uint8>>(columnCount);

      var i = 0;
      for (final entry in data.entries) {
        columnNames[i] = entry.key.toNativeUtf8(allocator: arena).cast();
        final column = _marshalTimeSeriesColumn(arena, entry.value);
        columnTypes[i] = column.type;
        columnData[i] = column.data;
        columnHasValue[i] = column.hasValue;
        i++;
      }

      check(
        bindings.quiver_database_update_time_series_group(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          group.toNativeUtf8(allocator: arena).cast(),
          id,
          columnNames,
          columnTypes,
          columnData,
          columnHasValue,
          columnCount,
          rowCount,
        ),
      );
    } finally {
      arena.releaseAll();
    }
  }

  // ==========================================================================
  // Add time series row
  // ==========================================================================

  /// Adds or updates a single time series row by element ID.
  /// Takes a Map of column names to scalar values.
  /// Supported value types: int, double, String, DateTime.
  /// Calling with the same dimension PK upserts (value columns overwritten).
  void addTimeSeriesRow(
    String collection,
    String group,
    int id,
    Map<String, Object> row,
  ) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final columnCount = row.length;
      final columnNames = arena<Pointer<Char>>(columnCount);
      final columnTypes = arena<Int>(columnCount);
      final columnData = arena<Pointer<Void>>(columnCount);

      var i = 0;
      for (final entry in row.entries) {
        columnNames[i] = entry.key.toNativeUtf8(allocator: arena).cast();
        final column = _marshalTimeSeriesColumn(arena, [entry.value]);
        columnTypes[i] = column.type;
        columnData[i] = column.data;
        i++;
      }

      check(
        bindings.quiver_database_add_time_series_row(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          group.toNativeUtf8(allocator: arena).cast(),
          id,
          columnNames,
          columnTypes,
          columnData,
          columnCount,
        ),
      );
    } finally {
      arena.releaseAll();
    }
  }

  // ==========================================================================
  // Update time series files
  // ==========================================================================

  /// Updates time series files paths for a collection.
  /// Takes a map of column name to file path (null to clear the path).
  void updateTimeSeriesFiles(String collection, Map<String, String?> paths) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final count = paths.length;

      if (count == 0) {
        return;
      }

      final columns = arena<Pointer<Char>>(count);
      final pathPtrs = arena<Pointer<Char>>(count);

      var i = 0;
      for (final entry in paths.entries) {
        columns[i] = entry.key.toNativeUtf8(allocator: arena).cast();
        if (entry.value != null) {
          pathPtrs[i] = entry.value!.toNativeUtf8(allocator: arena).cast();
        } else {
          pathPtrs[i] = nullptr;
        }
        i++;
      }

      check(
        bindings.quiver_database_update_time_series_files(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          columns,
          pathPtrs,
          count,
        ),
      );
    } finally {
      arena.releaseAll();
    }
  }

  /// Marshals one time series column into arena-allocated typed + mask arrays,
  /// returning its quiver_data_type_t tag, data pointer, and per-cell NULL mask.
  /// Supported value types: int, double, String, DateTime; a `null` entry becomes
  /// a SQL NULL (mask 0) with a placeholder in the data array. An all-null (or
  /// empty) column is tagged FLOAT with a zeroed placeholder — the C API ignores
  /// the type tag and data for masked-out cells.
  ({int type, Pointer<Void> data, Pointer<Uint8> hasValue}) _marshalTimeSeriesColumn(
    Arena arena,
    List<Object?> values,
  ) {
    final mask = arena<Uint8>(values.length);
    for (var r = 0; r < values.length; r++) {
      mask[r] = values[r] == null ? 0 : 1;
    }

    Object? first;
    for (final v in values) {
      if (v != null) {
        first = v;
        break;
      }
    }

    if (first == null) {
      // All-null (or empty) column.
      final arr = arena<Double>(values.isEmpty ? 1 : values.length);
      for (var r = 0; r < values.length; r++) {
        arr[r] = 0.0;
      }
      return (
        type: quiver_data_type_t.QUIVER_DATA_TYPE_FLOAT,
        data: arr.cast(),
        hasValue: mask,
      );
    }
    if (first is int) {
      final arr = arena<Int64>(values.length);
      for (var r = 0; r < values.length; r++) {
        arr[r] = values[r] == null ? 0 : values[r] as int;
      }
      return (
        type: quiver_data_type_t.QUIVER_DATA_TYPE_INTEGER,
        data: arr.cast(),
        hasValue: mask,
      );
    }
    if (first is double) {
      final arr = arena<Double>(values.length);
      for (var r = 0; r < values.length; r++) {
        arr[r] = values[r] == null ? 0.0 : values[r] as double;
      }
      return (
        type: quiver_data_type_t.QUIVER_DATA_TYPE_FLOAT,
        data: arr.cast(),
        hasValue: mask,
      );
    }
    if (first is String) {
      final arr = arena<Pointer<Char>>(values.length);
      for (var r = 0; r < values.length; r++) {
        arr[r] = values[r] == null ? nullptr : (values[r] as String).toNativeUtf8(allocator: arena).cast();
      }
      return (
        type: quiver_data_type_t.QUIVER_DATA_TYPE_STRING,
        data: arr.cast(),
        hasValue: mask,
      );
    }
    if (first is DateTime) {
      final arr = arena<Pointer<Char>>(values.length);
      for (var r = 0; r < values.length; r++) {
        arr[r] = values[r] == null
            ? nullptr
            : dateTimeToString(
                values[r] as DateTime,
              ).toNativeUtf8(allocator: arena).cast();
      }
      return (
        type: quiver_data_type_t.QUIVER_DATA_TYPE_STRING,
        data: arr.cast(),
        hasValue: mask,
      );
    }
    throw ArgumentError('Unsupported value type: ${first.runtimeType}');
  }
}
