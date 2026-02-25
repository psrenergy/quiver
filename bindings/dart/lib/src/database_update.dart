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
  // Update vector attributes
  // ==========================================================================

  /// Updates an integer vector attribute by element ID (replaces entire vector).
  void updateVectorIntegers(String collection, String attribute, int id, List<int> values) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final nativeValues = arena<Int64>(values.length);
      for (var i = 0; i < values.length; i++) {
        nativeValues[i] = values[i];
      }

      check(
        bindings.quiver_database_update_vector_integers(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          attribute.toNativeUtf8(allocator: arena).cast(),
          id,
          nativeValues,
          values.length,
        ),
      );
    } finally {
      arena.releaseAll();
    }
  }

  /// Updates a float vector attribute by element ID (replaces entire vector).
  void updateVectorFloats(String collection, String attribute, int id, List<double> values) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final nativeValues = arena<Double>(values.length);
      for (var i = 0; i < values.length; i++) {
        nativeValues[i] = values[i];
      }

      check(
        bindings.quiver_database_update_vector_floats(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          attribute.toNativeUtf8(allocator: arena).cast(),
          id,
          nativeValues,
          values.length,
        ),
      );
    } finally {
      arena.releaseAll();
    }
  }

  /// Updates a string vector attribute by element ID (replaces entire vector).
  void updateVectorStrings(String collection, String attribute, int id, List<String> values) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final nativePtrs = arena<Pointer<Char>>(values.length);
      for (var i = 0; i < values.length; i++) {
        nativePtrs[i] = values[i].toNativeUtf8(allocator: arena).cast();
      }

      check(
        bindings.quiver_database_update_vector_strings(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          attribute.toNativeUtf8(allocator: arena).cast(),
          id,
          nativePtrs,
          values.length,
        ),
      );
    } finally {
      arena.releaseAll();
    }
  }

  // ==========================================================================
  // Update set attributes
  // ==========================================================================

  /// Updates an integer set attribute by element ID (replaces entire set).
  void updateSetIntegers(String collection, String attribute, int id, List<int> values) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final nativeValues = arena<Int64>(values.length);
      for (var i = 0; i < values.length; i++) {
        nativeValues[i] = values[i];
      }

      check(
        bindings.quiver_database_update_set_integers(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          attribute.toNativeUtf8(allocator: arena).cast(),
          id,
          nativeValues,
          values.length,
        ),
      );
    } finally {
      arena.releaseAll();
    }
  }

  /// Updates a float set attribute by element ID (replaces entire set).
  void updateSetFloats(String collection, String attribute, int id, List<double> values) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final nativeValues = arena<Double>(values.length);
      for (var i = 0; i < values.length; i++) {
        nativeValues[i] = values[i];
      }

      check(
        bindings.quiver_database_update_set_floats(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          attribute.toNativeUtf8(allocator: arena).cast(),
          id,
          nativeValues,
          values.length,
        ),
      );
    } finally {
      arena.releaseAll();
    }
  }

  /// Updates a string set attribute by element ID (replaces entire set).
  void updateSetStrings(String collection, String attribute, int id, List<String> values) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final nativePtrs = arena<Pointer<Char>>(values.length);
      for (var i = 0; i < values.length; i++) {
        nativePtrs[i] = values[i].toNativeUtf8(allocator: arena).cast();
      }

      check(
        bindings.quiver_database_update_set_strings(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          attribute.toNativeUtf8(allocator: arena).cast(),
          id,
          nativePtrs,
          values.length,
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
  void updateTimeSeriesGroup(String collection, String group, int id, Map<String, List<Object>> data) {
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

      var i = 0;
      for (final entry in data.entries) {
        columnNames[i] = entry.key.toNativeUtf8(allocator: arena).cast();
        final values = entry.value;

        if (values.isEmpty) {
          columnTypes[i] = quiver_data_type_t.QUIVER_DATA_TYPE_STRING;
          columnData[i] = nullptr;
        } else if (values.first is int) {
          columnTypes[i] = quiver_data_type_t.QUIVER_DATA_TYPE_INTEGER;
          final arr = arena<Int64>(rowCount);
          for (var r = 0; r < rowCount; r++) {
            arr[r] = values[r] as int;
          }
          columnData[i] = arr.cast();
        } else if (values.first is double) {
          columnTypes[i] = quiver_data_type_t.QUIVER_DATA_TYPE_FLOAT;
          final arr = arena<Double>(rowCount);
          for (var r = 0; r < rowCount; r++) {
            arr[r] = values[r] as double;
          }
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

      check(
        bindings.quiver_database_update_time_series_group(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          group.toNativeUtf8(allocator: arena).cast(),
          id,
          columnNames,
          columnTypes,
          columnData,
          columnCount,
          rowCount,
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
}
