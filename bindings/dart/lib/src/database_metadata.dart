part of 'database.dart';

/// Metadata operations for Database.
extension DatabaseMetadata on Database {
  /// Returns metadata for a scalar attribute.
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
  getScalarMetadata(
    String collection,
    String attribute,
  ) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outMetadata = arena<quiver_scalar_metadata_t>();

      final err = bindings.quiver_database_get_scalar_metadata(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        attribute.toNativeUtf8(allocator: arena).cast(),
        outMetadata,
      );

      if (err != quiver_error_t.QUIVER_OK) {
        throw DatabaseException.fromError(err, "Failed to get scalar metadata for '$collection.$attribute'");
      }

      final result = (
        name: outMetadata.ref.name.cast<Utf8>().toDartString(),
        dataType: outMetadata.ref.data_type,
        notNull: outMetadata.ref.not_null != 0,
        primaryKey: outMetadata.ref.primary_key != 0,
        defaultValue: outMetadata.ref.default_value == nullptr
            ? null
            : outMetadata.ref.default_value.cast<Utf8>().toDartString(),
        isForeignKey: outMetadata.ref.is_foreign_key != 0,
        referencesCollection: outMetadata.ref.references_collection == nullptr
            ? null
            : outMetadata.ref.references_collection.cast<Utf8>().toDartString(),
        referencesColumn: outMetadata.ref.references_column == nullptr
            ? null
            : outMetadata.ref.references_column.cast<Utf8>().toDartString(),
      );

      bindings.quiver_free_scalar_metadata(outMetadata);
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  /// Returns metadata for a vector group, including all value columns in the group.
  ({
    String groupName,
    List<
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
    >
    valueColumns,
  })
  getVectorMetadata(String collection, String groupName) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outMetadata = arena<quiver_vector_metadata_t>();

      final err = bindings.quiver_database_get_vector_metadata(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        groupName.toNativeUtf8(allocator: arena).cast(),
        outMetadata,
      );

      if (err != quiver_error_t.QUIVER_OK) {
        throw DatabaseException.fromError(err, "Failed to get vector metadata for '$collection.$groupName'");
      }

      final valueColumns =
          <
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
          >[];
      final count = outMetadata.ref.value_column_count;
      for (var i = 0; i < count; i++) {
        valueColumns.add(_parseScalarMetadata(outMetadata.ref.value_columns[i]));
      }

      final result = (
        groupName: outMetadata.ref.group_name.cast<Utf8>().toDartString(),
        valueColumns: valueColumns,
      );

      bindings.quiver_free_vector_metadata(outMetadata);
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  /// Returns metadata for a set group, including all value columns in the group.
  ({
    String groupName,
    List<
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
    >
    valueColumns,
  })
  getSetMetadata(String collection, String groupName) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outMetadata = arena<quiver_set_metadata_t>();

      final err = bindings.quiver_database_get_set_metadata(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        groupName.toNativeUtf8(allocator: arena).cast(),
        outMetadata,
      );

      if (err != quiver_error_t.QUIVER_OK) {
        throw DatabaseException.fromError(err, "Failed to get set metadata for '$collection.$groupName'");
      }

      final valueColumns =
          <
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
          >[];
      final count = outMetadata.ref.value_column_count;
      for (var i = 0; i < count; i++) {
        valueColumns.add(_parseScalarMetadata(outMetadata.ref.value_columns[i]));
      }

      final result = (
        groupName: outMetadata.ref.group_name.cast<Utf8>().toDartString(),
        valueColumns: valueColumns,
      );

      bindings.quiver_free_set_metadata(outMetadata);
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  /// Lists all scalar attributes for a collection with full metadata.
  List<
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
  >
  listScalarAttributes(
    String collection,
  ) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outMetadata = arena<Pointer<quiver_scalar_metadata_t>>();
      final outCount = arena<Size>();

      final err = bindings.quiver_database_list_scalar_attributes(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        outMetadata,
        outCount,
      );

      if (err != quiver_error_t.QUIVER_OK) {
        throw DatabaseException.fromError(err, "Failed to list scalar attributes for '$collection'");
      }

      final count = outCount.value;
      if (count == 0 || outMetadata.value == nullptr) {
        return [];
      }

      final result =
          <
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
          >[];
      for (var i = 0; i < count; i++) {
        result.add(_parseScalarMetadata(outMetadata.value[i]));
      }
      bindings.quiver_free_scalar_metadata_array(outMetadata.value, count);
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  /// Lists all vector groups for a collection with full metadata.
  List<
    ({
      String groupName,
      List<
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
      >
      valueColumns,
    })
  >
  listVectorGroups(String collection) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outMetadata = arena<Pointer<quiver_vector_metadata_t>>();
      final outCount = arena<Size>();

      final err = bindings.quiver_database_list_vector_groups(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        outMetadata,
        outCount,
      );

      if (err != quiver_error_t.QUIVER_OK) {
        throw DatabaseException.fromError(err, "Failed to list vector groups for '$collection'");
      }

      final count = outCount.value;
      if (count == 0 || outMetadata.value == nullptr) {
        return [];
      }

      final result =
          <
            ({
              String groupName,
              List<
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
              >
              valueColumns,
            })
          >[];
      for (var i = 0; i < count; i++) {
        final meta = outMetadata.value[i];
        final valueColumns =
            <
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
            >[];
        for (var j = 0; j < meta.value_column_count; j++) {
          valueColumns.add(_parseScalarMetadata(meta.value_columns[j]));
        }
        result.add((groupName: meta.group_name.cast<Utf8>().toDartString(), valueColumns: valueColumns));
      }
      bindings.quiver_free_vector_metadata_array(outMetadata.value, count);
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  /// Lists all set groups for a collection with full metadata.
  List<
    ({
      String groupName,
      List<
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
      >
      valueColumns,
    })
  >
  listSetGroups(String collection) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outMetadata = arena<Pointer<quiver_set_metadata_t>>();
      final outCount = arena<Size>();

      final err = bindings.quiver_database_list_set_groups(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        outMetadata,
        outCount,
      );

      if (err != quiver_error_t.QUIVER_OK) {
        throw DatabaseException.fromError(err, "Failed to list set groups for '$collection'");
      }

      final count = outCount.value;
      if (count == 0 || outMetadata.value == nullptr) {
        return [];
      }

      final result =
          <
            ({
              String groupName,
              List<
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
              >
              valueColumns,
            })
          >[];
      for (var i = 0; i < count; i++) {
        final meta = outMetadata.value[i];
        final valueColumns =
            <
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
            >[];
        for (var j = 0; j < meta.value_column_count; j++) {
          valueColumns.add(_parseScalarMetadata(meta.value_columns[j]));
        }
        result.add((groupName: meta.group_name.cast<Utf8>().toDartString(), valueColumns: valueColumns));
      }
      bindings.quiver_free_set_metadata_array(outMetadata.value, count);
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  /// Returns the current schema version (migration number) of the database.
  int currentVersion() {
    _ensureNotClosed();
    final version = bindings.quiver_database_current_version(_ptr);
    if (version < 0) {
      throw const DatabaseOperationException('Failed to get current version');
    }
    return version;
  }

  /// Returns metadata for a time series group, including all value columns in the group.
  ({
    String groupName,
    List<
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
    >
    valueColumns,
  })
  getTimeSeriesMetadata(String collection, String groupName) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outMetadata = arena<quiver_time_series_metadata_t>();

      final err = bindings.quiver_database_get_time_series_metadata(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        groupName.toNativeUtf8(allocator: arena).cast(),
        outMetadata,
      );

      if (err != quiver_error_t.QUIVER_OK) {
        throw DatabaseException.fromError(err, "Failed to get time series metadata for '$collection.$groupName'");
      }

      final valueColumns =
          <
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
          >[];
      final count = outMetadata.ref.value_column_count;
      for (var i = 0; i < count; i++) {
        valueColumns.add(_parseScalarMetadata(outMetadata.ref.value_columns[i]));
      }

      final result = (
        groupName: outMetadata.ref.group_name.cast<Utf8>().toDartString(),
        valueColumns: valueColumns,
      );

      bindings.quiver_free_time_series_metadata(outMetadata);
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  /// Lists all time series groups for a collection with full metadata.
  List<
    ({
      String groupName,
      List<
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
      >
      valueColumns,
    })
  >
  listTimeSeriesGroups(String collection) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outMetadata = arena<Pointer<quiver_time_series_metadata_t>>();
      final outCount = arena<Size>();

      final err = bindings.quiver_database_list_time_series_groups(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        outMetadata,
        outCount,
      );

      if (err != quiver_error_t.QUIVER_OK) {
        throw DatabaseException.fromError(err, "Failed to list time series groups for '$collection'");
      }

      final count = outCount.value;
      if (count == 0 || outMetadata.value == nullptr) {
        return [];
      }

      final result =
          <
            ({
              String groupName,
              List<
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
              >
              valueColumns,
            })
          >[];
      for (var i = 0; i < count; i++) {
        final meta = outMetadata.value[i];
        final valueColumns =
            <
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
            >[];
        for (var j = 0; j < meta.value_column_count; j++) {
          valueColumns.add(_parseScalarMetadata(meta.value_columns[j]));
        }
        result.add((groupName: meta.group_name.cast<Utf8>().toDartString(), valueColumns: valueColumns));
      }
      bindings.quiver_free_time_series_metadata_array(outMetadata.value, count);
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  // ==========================================================================
  // Time series files metadata
  // ==========================================================================

  /// Returns whether the collection has a time_series_files table.
  bool hasTimeSeriesFiles(String collection) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outResult = arena<Int>();

      final err = bindings.quiver_database_has_time_series_files(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        outResult,
      );

      if (err != quiver_error_t.QUIVER_OK) {
        throw DatabaseException.fromError(err, "Failed to check time series files for '$collection'");
      }

      return outResult.value != 0;
    } finally {
      arena.releaseAll();
    }
  }

  /// Lists all column names in the time_series_files table for a collection.
  List<String> listTimeSeriesFilesColumns(String collection) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outColumns = arena<Pointer<Pointer<Char>>>();
      final outCount = arena<Size>();

      final err = bindings.quiver_database_list_time_series_files_columns(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        outColumns,
        outCount,
      );

      if (err != quiver_error_t.QUIVER_OK) {
        throw DatabaseException.fromError(err, "Failed to list time series files columns for '$collection'");
      }

      final count = outCount.value;
      if (count == 0 || outColumns.value == nullptr) {
        return [];
      }

      final result = List<String>.generate(count, (i) => outColumns.value[i].cast<Utf8>().toDartString());
      bindings.quiver_free_string_array(outColumns.value, count);
      return result;
    } finally {
      arena.releaseAll();
    }
  }
}
