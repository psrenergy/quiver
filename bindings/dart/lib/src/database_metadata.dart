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

      check(
        bindings.quiver_database_get_scalar_metadata(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          attribute.toNativeUtf8(allocator: arena).cast(),
          outMetadata,
        ),
      );

      final result = _parseScalarMetadata(outMetadata.ref);

      bindings.quiver_database_free_scalar_metadata(outMetadata);
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  /// Returns metadata for a vector group, including all value columns in the group.
  ({
    String groupName,
    String dimensionColumn,
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
      final outMetadata = arena<quiver_group_metadata_t>();

      check(
        bindings.quiver_database_get_vector_metadata(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          groupName.toNativeUtf8(allocator: arena).cast(),
          outMetadata,
        ),
      );

      final result = _parseGroupMetadata(outMetadata.ref);

      bindings.quiver_database_free_group_metadata(outMetadata);
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  /// Returns metadata for a set group, including all value columns in the group.
  ({
    String groupName,
    String dimensionColumn,
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
      final outMetadata = arena<quiver_group_metadata_t>();

      check(
        bindings.quiver_database_get_set_metadata(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          groupName.toNativeUtf8(allocator: arena).cast(),
          outMetadata,
        ),
      );

      final result = _parseGroupMetadata(outMetadata.ref);

      bindings.quiver_database_free_group_metadata(outMetadata);
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

      check(
        bindings.quiver_database_list_scalar_attributes(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          outMetadata,
          outCount,
        ),
      );

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
      bindings.quiver_database_free_scalar_metadata_array(outMetadata.value, count);
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  /// Lists all vector groups for a collection with full metadata.
  List<
    ({
      String groupName,
      String dimensionColumn,
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
      final outMetadata = arena<Pointer<quiver_group_metadata_t>>();
      final outCount = arena<Size>();

      check(
        bindings.quiver_database_list_vector_groups(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          outMetadata,
          outCount,
        ),
      );

      final count = outCount.value;
      if (count == 0 || outMetadata.value == nullptr) {
        return [];
      }

      final result =
          <
            ({
              String groupName,
              String dimensionColumn,
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
        result.add(_parseGroupMetadata(outMetadata.value[i]));
      }
      bindings.quiver_database_free_group_metadata_array(outMetadata.value, count);
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  /// Lists all set groups for a collection with full metadata.
  List<
    ({
      String groupName,
      String dimensionColumn,
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
      final outMetadata = arena<Pointer<quiver_group_metadata_t>>();
      final outCount = arena<Size>();

      check(
        bindings.quiver_database_list_set_groups(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          outMetadata,
          outCount,
        ),
      );

      final count = outCount.value;
      if (count == 0 || outMetadata.value == nullptr) {
        return [];
      }

      final result =
          <
            ({
              String groupName,
              String dimensionColumn,
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
        result.add(_parseGroupMetadata(outMetadata.value[i]));
      }
      bindings.quiver_database_free_group_metadata_array(outMetadata.value, count);
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  /// Returns the current schema version (migration number) of the database.
  int currentVersion() {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outVersion = arena<Int64>();

      check(
        bindings.quiver_database_current_version(_ptr, outVersion),
      );

      return outVersion.value;
    } finally {
      arena.releaseAll();
    }
  }

  /// Returns metadata for a time series group, including all value columns in the group.
  ({
    String groupName,
    String dimensionColumn,
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
      final outMetadata = arena<quiver_group_metadata_t>();

      check(
        bindings.quiver_database_get_time_series_metadata(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          groupName.toNativeUtf8(allocator: arena).cast(),
          outMetadata,
        ),
      );

      final result = _parseGroupMetadata(outMetadata.ref);

      bindings.quiver_database_free_group_metadata(outMetadata);
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  /// Lists all time series groups for a collection with full metadata.
  List<
    ({
      String groupName,
      String dimensionColumn,
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
      final outMetadata = arena<Pointer<quiver_group_metadata_t>>();
      final outCount = arena<Size>();

      check(
        bindings.quiver_database_list_time_series_groups(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          outMetadata,
          outCount,
        ),
      );

      final count = outCount.value;
      if (count == 0 || outMetadata.value == nullptr) {
        return [];
      }

      final result =
          <
            ({
              String groupName,
              String dimensionColumn,
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
        result.add(_parseGroupMetadata(outMetadata.value[i]));
      }
      bindings.quiver_database_free_group_metadata_array(outMetadata.value, count);
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

      check(
        bindings.quiver_database_has_time_series_files(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          outResult,
        ),
      );

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

      check(
        bindings.quiver_database_list_time_series_files_columns(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          outColumns,
          outCount,
        ),
      );

      final count = outCount.value;
      if (count == 0 || outColumns.value == nullptr) {
        return [];
      }

      final result = List<String>.generate(count, (i) => outColumns.value[i].cast<Utf8>().toDartString());
      bindings.quiver_database_free_string_array(outColumns.value, count);
      return result;
    } finally {
      arena.releaseAll();
    }
  }
}
