part of 'database.dart';

typedef TimeSeriesMetadata = ({
  String groupName,
  List<String> dimensionColumns,
  List<({String name, int dataType, bool notNull, bool primaryKey, String? defaultValue})> valueColumns,
});

extension DatabaseTimeSeries on Database {
  /// Adds a time series row for an element.
  void addTimeSeriesRow(
    String collection,
    String attribute,
    int elementId,
    double value,
    String dateTime, {
    Map<String, int> dimensions = const {},
  }) {
    _ensureNotClosed();
    final arena = Arena();
    try {
      final dimCount = dimensions.length;

      Pointer<Pointer<Char>> dimNamesPtr = nullptr;
      Pointer<Int64> dimValuesPtr = nullptr;

      if (dimCount > 0) {
        dimNamesPtr = arena<Pointer<Char>>(dimCount);
        dimValuesPtr = arena<Int64>(dimCount);

        var i = 0;
        for (final entry in dimensions.entries) {
          dimNamesPtr[i] = entry.key.toNativeUtf8(allocator: arena).cast();
          dimValuesPtr[i] = entry.value;
          i++;
        }
      }

      final err = bindings.quiver_database_add_time_series_row(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        attribute.toNativeUtf8(allocator: arena).cast(),
        elementId,
        value,
        dateTime.toNativeUtf8(allocator: arena).cast(),
        dimNamesPtr,
        dimValuesPtr,
        dimCount,
      );

      if (err != quiver_error_t.QUIVER_OK) {
        throw DatabaseOperationException('Failed to add time series row: $collection.$attribute');
      }
    } finally {
      arena.releaseAll();
    }
  }

  /// Updates a time series row for an element.
  void updateTimeSeriesRow(
    String collection,
    String attribute,
    int elementId,
    double value,
    String dateTime, {
    Map<String, int> dimensions = const {},
  }) {
    _ensureNotClosed();
    final arena = Arena();
    try {
      final dimCount = dimensions.length;

      Pointer<Pointer<Char>> dimNamesPtr = nullptr;
      Pointer<Int64> dimValuesPtr = nullptr;

      if (dimCount > 0) {
        dimNamesPtr = arena<Pointer<Char>>(dimCount);
        dimValuesPtr = arena<Int64>(dimCount);

        var i = 0;
        for (final entry in dimensions.entries) {
          dimNamesPtr[i] = entry.key.toNativeUtf8(allocator: arena).cast();
          dimValuesPtr[i] = entry.value;
          i++;
        }
      }

      final err = bindings.quiver_database_update_time_series_row(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        attribute.toNativeUtf8(allocator: arena).cast(),
        elementId,
        value,
        dateTime.toNativeUtf8(allocator: arena).cast(),
        dimNamesPtr,
        dimValuesPtr,
        dimCount,
      );

      if (err != quiver_error_t.QUIVER_OK) {
        throw DatabaseOperationException('Failed to update time series row: $collection.$attribute');
      }
    } finally {
      arena.releaseAll();
    }
  }

  /// Deletes all time series data for an element in a group.
  void deleteTimeSeries(String collection, String group, int elementId) {
    _ensureNotClosed();
    final arena = Arena();
    try {
      final err = bindings.quiver_database_delete_time_series(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        group.toNativeUtf8(allocator: arena).cast(),
        elementId,
      );

      if (err != quiver_error_t.QUIVER_OK) {
        throw DatabaseOperationException('Failed to delete time series: $collection.$group');
      }
    } finally {
      arena.releaseAll();
    }
  }

  /// Reads all time series data for an element in a group.
  /// Returns a list of rows, where each row is a map of column name to value.
  List<Map<String, dynamic>> readTimeSeriesTable(String collection, String group, int elementId) {
    _ensureNotClosed();
    final arena = Arena();
    try {
      final jsonPtr = arena<Pointer<Char>>();

      final err = bindings.quiver_database_read_time_series_table(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        group.toNativeUtf8(allocator: arena).cast(),
        elementId,
        jsonPtr,
      );

      if (err != quiver_error_t.QUIVER_OK) {
        throw DatabaseOperationException('Failed to read time series table: $collection.$group');
      }

      if (jsonPtr.value == nullptr) {
        return [];
      }

      final jsonStr = jsonPtr.value.cast<Utf8>().toDartString();
      bindings.quiver_free_string(jsonPtr.value);

      // Parse JSON
      final List<dynamic> rows = json.decode(jsonStr);
      return rows.map((row) => Map<String, dynamic>.from(row as Map)).toList();
    } finally {
      arena.releaseAll();
    }
  }

  /// Gets metadata for a time series group.
  TimeSeriesMetadata getTimeSeriesMetadata(String collection, String groupName) {
    _ensureNotClosed();
    final arena = Arena();
    try {
      final metaPtr = arena<quiver_time_series_metadata_t>();

      final err = bindings.quiver_database_get_time_series_metadata(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        groupName.toNativeUtf8(allocator: arena).cast(),
        metaPtr,
      );

      if (err != quiver_error_t.QUIVER_OK) {
        throw DatabaseOperationException('Failed to get time series metadata: $collection.$groupName');
      }

      final result = _convertTimeSeriesMetadata(metaPtr.ref);
      bindings.quiver_free_time_series_metadata(metaPtr);
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  /// Lists all time series groups for a collection.
  List<TimeSeriesMetadata> listTimeSeriesGroups(String collection) {
    _ensureNotClosed();
    final arena = Arena();
    try {
      final metaPtrPtr = arena<Pointer<quiver_time_series_metadata_t>>();
      final countPtr = arena<Size>();

      final err = bindings.quiver_database_list_time_series_groups(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        metaPtrPtr,
        countPtr,
      );

      if (err != quiver_error_t.QUIVER_OK) {
        throw DatabaseOperationException('Failed to list time series groups: $collection');
      }

      final count = countPtr.value;
      if (count == 0 || metaPtrPtr.value == nullptr) {
        return [];
      }

      final result = <TimeSeriesMetadata>[];
      for (var i = 0; i < count; i++) {
        result.add(_convertTimeSeriesMetadata(metaPtrPtr.value[i]));
      }

      bindings.quiver_free_time_series_metadata_array(metaPtrPtr.value, count);
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  TimeSeriesMetadata _convertTimeSeriesMetadata(quiver_time_series_metadata_t meta) {
    final groupName = meta.group_name.cast<Utf8>().toDartString();

    final dimensionColumns = <String>[];
    for (var i = 0; i < meta.dimension_count; i++) {
      dimensionColumns.add(meta.dimension_columns[i].cast<Utf8>().toDartString());
    }

    final valueColumns = <({String name, int dataType, bool notNull, bool primaryKey, String? defaultValue})>[];
    for (var i = 0; i < meta.value_column_count; i++) {
      valueColumns.add(_parseScalarMetadata(meta.value_columns[i]));
    }

    return (
      groupName: groupName,
      dimensionColumns: dimensionColumns,
      valueColumns: valueColumns,
    );
  }
}
