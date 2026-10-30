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

  /// Label-addressed counterpart of [updateElement].
  void updateElementByLabel(String collection, String label, Map<String, Object?> values) {
    _ensureNotClosed();
    final element = Element();
    try {
      for (final entry in values.entries) {
        element.set(entry.key, entry.value);
      }
      updateElementFromBuilderByLabel(collection, label, element);
    } finally {
      element.dispose();
    }
  }

  /// Label-addressed counterpart of [updateElementFromBuilder].
  void updateElementFromBuilderByLabel(String collection, String label, Element element) {
    _ensureNotClosed();
    final arena = Arena();
    try {
      check(
        bindings.quiver_database_update_element_by_label(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          label.toNativeUtf8(allocator: arena).cast(),
          element.ptr.cast(),
        ),
      );
    } finally {
      arena.releaseAll();
    }
  }

  /// Points one scalar foreign-key relation at the element whose label is [targetLabel];
  /// null clears the relation.
  /// The column is derived as `collectionTo.toLowerCase() + '_' + relationType`.
  void updateRelation(
    String collectionFrom,
    String collectionTo,
    String relationType,
    int id,
    String? targetLabel,
  ) {
    _ensureNotClosed();
    final arena = Arena();
    try {
      check(
        bindings.quiver_database_update_relation(
          _ptr,
          collectionFrom.toNativeUtf8(allocator: arena).cast(),
          collectionTo.toNativeUtf8(allocator: arena).cast(),
          relationType.toNativeUtf8(allocator: arena).cast(),
          id,
          targetLabel == null ? nullptr : targetLabel.toNativeUtf8(allocator: arena).cast(),
        ),
      );
    } finally {
      arena.releaseAll();
    }
  }

  /// Label-addressed counterpart of [updateRelation].
  void updateRelationByLabel(
    String collectionFrom,
    String collectionTo,
    String relationType,
    String label,
    String? targetLabel,
  ) {
    _ensureNotClosed();
    final arena = Arena();
    try {
      check(
        bindings.quiver_database_update_relation_by_label(
          _ptr,
          collectionFrom.toNativeUtf8(allocator: arena).cast(),
          collectionTo.toNativeUtf8(allocator: arena).cast(),
          relationType.toNativeUtf8(allocator: arena).cast(),
          label.toNativeUtf8(allocator: arena).cast(),
          targetLabel == null ? nullptr : targetLabel.toNativeUtf8(allocator: arena).cast(),
        ),
      );
    } finally {
      arena.releaseAll();
    }
  }

  // ==========================================================================
  // Update vector / set groups
  // ==========================================================================

  /// Updates a vector group by element ID (replaces all rows).
  /// Takes a Map of column names to typed Lists; a `null` cell is a SQL NULL.
  /// Supported value types: int, bool (INTEGER 1/0), double, String, DateTime.
  /// An empty Map clears all rows for that element.
  ///
  /// Prefer this over passing the group's columns through [updateElement] when a
  /// column name is shared by two groups of the same collection (legal for foreign
  /// keys): `(collection, group)` names exactly one table, a column name alone does not.
  void updateVectorGroup(
    String collection,
    String group,
    int id,
    Map<String, List<Object?>> data,
  ) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final cols = _marshalGroupColumns(arena, data);
      check(
        bindings.quiver_database_update_vector_group(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          group.toNativeUtf8(allocator: arena).cast(),
          id,
          cols.names,
          cols.types,
          cols.data,
          cols.hasValue,
          cols.columnCount,
          cols.rowCount,
        ),
      );
    } finally {
      arena.releaseAll();
    }
  }

  /// Label-addressed counterpart of [updateVectorGroup].
  void updateVectorGroupByLabel(
    String collection,
    String group,
    String label,
    Map<String, List<Object?>> data,
  ) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final cols = _marshalGroupColumns(arena, data);
      check(
        bindings.quiver_database_update_vector_group_by_label(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          group.toNativeUtf8(allocator: arena).cast(),
          label.toNativeUtf8(allocator: arena).cast(),
          cols.names,
          cols.types,
          cols.data,
          cols.hasValue,
          cols.columnCount,
          cols.rowCount,
        ),
      );
    } finally {
      arena.releaseAll();
    }
  }

  /// Updates a set group by element ID (replaces all rows).
  /// Takes a Map of column names to typed Lists; a `null` cell is a SQL NULL.
  /// Supported value types: int, bool (INTEGER 1/0), double, String, DateTime.
  /// An empty Map clears all rows for that element.
  ///
  /// See [updateVectorGroup] for why this is preferred over [updateElement] for
  /// groups whose column names are shared across the collection.
  void updateSetGroup(
    String collection,
    String group,
    int id,
    Map<String, List<Object?>> data,
  ) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final cols = _marshalGroupColumns(arena, data);
      check(
        bindings.quiver_database_update_set_group(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          group.toNativeUtf8(allocator: arena).cast(),
          id,
          cols.names,
          cols.types,
          cols.data,
          cols.hasValue,
          cols.columnCount,
          cols.rowCount,
        ),
      );
    } finally {
      arena.releaseAll();
    }
  }

  /// Label-addressed counterpart of [updateSetGroup].
  void updateSetGroupByLabel(
    String collection,
    String group,
    String label,
    Map<String, List<Object?>> data,
  ) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final cols = _marshalGroupColumns(arena, data);
      check(
        bindings.quiver_database_update_set_group_by_label(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          group.toNativeUtf8(allocator: arena).cast(),
          label.toNativeUtf8(allocator: arena).cast(),
          cols.names,
          cols.types,
          cols.data,
          cols.hasValue,
          cols.columnCount,
          cols.rowCount,
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
  /// Supported value types: int, bool (INTEGER 1/0), double, String, DateTime.
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
      final cols = _marshalGroupColumns(arena, data);
      check(
        bindings.quiver_database_update_time_series_group(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          group.toNativeUtf8(allocator: arena).cast(),
          id,
          cols.names,
          cols.types,
          cols.data,
          cols.hasValue,
          cols.columnCount,
          cols.rowCount,
        ),
      );
    } finally {
      arena.releaseAll();
    }
  }

  /// Label-addressed counterpart of [updateTimeSeriesGroup].
  void updateTimeSeriesGroupByLabel(
    String collection,
    String group,
    String label,
    Map<String, List<Object?>> data,
  ) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final cols = _marshalGroupColumns(arena, data);
      check(
        bindings.quiver_database_update_time_series_group_by_label(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          group.toNativeUtf8(allocator: arena).cast(),
          label.toNativeUtf8(allocator: arena).cast(),
          cols.names,
          cols.types,
          cols.data,
          cols.hasValue,
          cols.columnCount,
          cols.rowCount,
        ),
      );
    } finally {
      arena.releaseAll();
    }
  }

  // ==========================================================================
  // Add time series row
  // ==========================================================================

  /// Adds or patches a single time series row by element ID.
  /// Takes a Map of column names to scalar values; a `null` value is a SQL NULL.
  /// Supported value types: int, bool (INTEGER 1/0), double, String, DateTime.
  /// Calling with the same dimension PK patches: only the columns named in [row] are written,
  /// an explicit `null` clears one, and a value column left out keeps its current value.
  void upsertTimeSeriesRow(
    String collection,
    String group,
    int id,
    Map<String, Object?> row,
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
        final column = marshalCells(arena, [entry.value], "column '${entry.key}'", 'value');
        columnTypes[i] = column.type;
        // A `null` is a NULL data pointer, which the C API writes as an explicit SQL NULL (the
        // zeroed placeholder behind it would otherwise be written as data).
        columnData[i] = entry.value == null ? nullptr : column.data;
        i++;
      }

      check(
        bindings.quiver_database_upsert_time_series_row(
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

  /// Adds or updates a single time series row, addressed by label.
  /// Label-addressed counterpart of [upsertTimeSeriesRow].
  void upsertTimeSeriesRowByLabel(
    String collection,
    String group,
    String label,
    Map<String, Object?> row,
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
        final column = marshalCells(arena, [entry.value], "column '${entry.key}'", 'value');
        columnTypes[i] = column.type;
        // A `null` is a NULL data pointer, which the C API writes as an explicit SQL NULL (the
        // zeroed placeholder behind it would otherwise be written as data).
        columnData[i] = entry.value == null ? nullptr : column.data;
        i++;
      }

      check(
        bindings.quiver_database_upsert_time_series_row_by_label(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          group.toNativeUtf8(allocator: arena).cast(),
          label.toNativeUtf8(allocator: arena).cast(),
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
  /// Takes a map of column name to file path; only the named columns are written - a `null`
  /// value clears that column, and any column left out of the map keeps its current value.
  void updateTimeSeriesFiles(String collection, Map<String, String?> paths) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final count = paths.length;

      // An empty map still reaches the core, which validates the collection and its files table
      // before treating it as a no-op. NULL arrays, not arena(0): package:ffi throws when the
      // allocator returns NULL for a zero-byte request, which POSIX malloc/calloc may do.
      final columns = count == 0 ? nullptr : arena<Pointer<Char>>(count);
      final pathPtrs = count == 0 ? nullptr : arena<Pointer<Char>>(count);

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

  /// Marshals a whole column-oriented group payload into the four parallel arena-allocated arrays
  /// the columnar C group writers take (names, type tags, data pointers, per-cell masks), plus the
  /// column and row counts. Shared by the six group writers (vector / set / time-series, by id and
  /// by label); each still makes its own C call. An empty map (clear the group) yields NULL arrays
  /// and zero counts and allocates nothing. Jagged columns throw [ArgumentError] naming the column.
  ({
    Pointer<Pointer<Char>> names,
    Pointer<Int> types,
    Pointer<Pointer<Void>> data,
    Pointer<Pointer<Uint8>> hasValue,
    int columnCount,
    int rowCount,
  })
  _marshalGroupColumns(Arena arena, Map<String, List<Object?>> data) {
    if (data.isEmpty) {
      return (
        names: nullptr,
        types: nullptr,
        data: nullptr,
        hasValue: nullptr,
        columnCount: 0,
        rowCount: 0,
      );
    }

    final rowCount = data.values.first.length;
    for (final entry in data.entries) {
      if (entry.value.length != rowCount) {
        throw ArgumentError(
          "All column lists must have the same length, got ${entry.value.length} for '${entry.key}'",
        );
      }
    }

    final columnCount = data.length;
    final names = arena<Pointer<Char>>(columnCount);
    final types = arena<Int>(columnCount);
    final columnData = arena<Pointer<Void>>(columnCount);
    final hasValue = arena<Pointer<Uint8>>(columnCount);

    var i = 0;
    for (final entry in data.entries) {
      names[i] = entry.key.toNativeUtf8(allocator: arena).cast();
      final column = marshalCells(arena, entry.value, "column '${entry.key}'", 'value');
      types[i] = column.type;
      columnData[i] = column.data;
      hasValue[i] = column.mask;
      i++;
    }

    return (
      names: names,
      types: types,
      data: columnData,
      hasValue: hasValue,
      columnCount: columnCount,
      rowCount: rowCount,
    );
  }
}
