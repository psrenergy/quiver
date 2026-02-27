part of 'database.dart';

/// Read operations for Database.
extension DatabaseRead on Database {
  // ==========================================================================
  // Read all scalar/vector/set values
  // ==========================================================================

  /// Reads all integer values for a scalar attribute from a collection.
  List<int> readScalarIntegers(String collection, String attribute) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outValues = arena<Pointer<Int64>>();
      final outCount = arena<Size>();

      check(
        bindings.quiver_database_read_scalar_integers(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          attribute.toNativeUtf8(allocator: arena).cast(),
          outValues,
          outCount,
        ),
      );

      final count = outCount.value;
      if (count == 0 || outValues.value == nullptr) {
        return [];
      }

      final result = List<int>.generate(count, (i) => outValues.value[i]);
      bindings.quiver_database_free_integer_array(outValues.value);
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  /// Reads all float values for a scalar attribute from a collection.
  List<double> readScalarFloats(String collection, String attribute) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outValues = arena<Pointer<Double>>();
      final outCount = arena<Size>();

      check(
        bindings.quiver_database_read_scalar_floats(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          attribute.toNativeUtf8(allocator: arena).cast(),
          outValues,
          outCount,
        ),
      );

      final count = outCount.value;
      if (count == 0 || outValues.value == nullptr) {
        return [];
      }

      final result = List<double>.generate(count, (i) => outValues.value[i]);
      bindings.quiver_database_free_float_array(outValues.value);
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  /// Reads all string values for a scalar attribute from a collection.
  List<String> readScalarStrings(String collection, String attribute) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outValues = arena<Pointer<Pointer<Char>>>();
      final outCount = arena<Size>();

      check(
        bindings.quiver_database_read_scalar_strings(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          attribute.toNativeUtf8(allocator: arena).cast(),
          outValues,
          outCount,
        ),
      );

      final count = outCount.value;
      if (count == 0 || outValues.value == nullptr) {
        return [];
      }

      final result = List<String>.generate(
        count,
        (i) => outValues.value[i].cast<Utf8>().toDartString(),
      );
      bindings.quiver_database_free_string_array(outValues.value, count);
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  /// Reads all int vectors for a vector attribute from a collection.
  List<List<int>> readVectorIntegers(String collection, String attribute) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outVectors = arena<Pointer<Pointer<Int64>>>();
      final outSizes = arena<Pointer<Size>>();
      final outCount = arena<Size>();

      check(
        bindings.quiver_database_read_vector_integers(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          attribute.toNativeUtf8(allocator: arena).cast(),
          outVectors,
          outSizes,
          outCount,
        ),
      );

      final count = outCount.value;
      if (count == 0 || outVectors.value == nullptr) {
        return [];
      }

      final result = <List<int>>[];
      for (var i = 0; i < count; i++) {
        final size = outSizes.value[i];
        if (size == 0 || outVectors.value[i] == nullptr) {
          result.add([]);
        } else {
          result.add(List<int>.generate(size, (j) => outVectors.value[i][j]));
        }
      }
      bindings.quiver_database_free_integer_vectors(
        outVectors.value,
        outSizes.value,
        count,
      );
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  /// Reads all float vectors for a vector attribute from a collection.
  List<List<double>> readVectorFloats(String collection, String attribute) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outVectors = arena<Pointer<Pointer<Double>>>();
      final outSizes = arena<Pointer<Size>>();
      final outCount = arena<Size>();

      check(
        bindings.quiver_database_read_vector_floats(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          attribute.toNativeUtf8(allocator: arena).cast(),
          outVectors,
          outSizes,
          outCount,
        ),
      );

      final count = outCount.value;
      if (count == 0 || outVectors.value == nullptr) {
        return [];
      }

      final result = <List<double>>[];
      for (var i = 0; i < count; i++) {
        final size = outSizes.value[i];
        if (size == 0 || outVectors.value[i] == nullptr) {
          result.add([]);
        } else {
          result.add(
            List<double>.generate(size, (j) => outVectors.value[i][j]),
          );
        }
      }
      bindings.quiver_database_free_float_vectors(
        outVectors.value,
        outSizes.value,
        count,
      );
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  /// Reads all string vectors for a vector attribute from a collection.
  List<List<String>> readVectorStrings(String collection, String attribute) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outVectors = arena<Pointer<Pointer<Pointer<Char>>>>();
      final outSizes = arena<Pointer<Size>>();
      final outCount = arena<Size>();

      check(
        bindings.quiver_database_read_vector_strings(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          attribute.toNativeUtf8(allocator: arena).cast(),
          outVectors,
          outSizes,
          outCount,
        ),
      );

      final count = outCount.value;
      if (count == 0 || outVectors.value == nullptr) {
        return [];
      }

      final result = <List<String>>[];
      for (var i = 0; i < count; i++) {
        final size = outSizes.value[i];
        if (size == 0 || outVectors.value[i] == nullptr) {
          result.add([]);
        } else {
          result.add(
            List<String>.generate(
              size,
              (j) => outVectors.value[i][j].cast<Utf8>().toDartString(),
            ),
          );
        }
      }
      bindings.quiver_database_free_string_vectors(
        outVectors.value,
        outSizes.value,
        count,
      );
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  /// Reads all int sets for a set attribute from a collection.
  List<List<int>> readSetIntegers(String collection, String attribute) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outSets = arena<Pointer<Pointer<Int64>>>();
      final outSizes = arena<Pointer<Size>>();
      final outCount = arena<Size>();

      check(
        bindings.quiver_database_read_set_integers(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          attribute.toNativeUtf8(allocator: arena).cast(),
          outSets,
          outSizes,
          outCount,
        ),
      );

      final count = outCount.value;
      if (count == 0 || outSets.value == nullptr) {
        return [];
      }

      final result = <List<int>>[];
      for (var i = 0; i < count; i++) {
        final size = outSizes.value[i];
        if (size == 0 || outSets.value[i] == nullptr) {
          result.add([]);
        } else {
          result.add(List<int>.generate(size, (j) => outSets.value[i][j]));
        }
      }
      bindings.quiver_database_free_integer_vectors(
        outSets.value,
        outSizes.value,
        count,
      );
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  /// Reads all float sets for a set attribute from a collection.
  List<List<double>> readSetFloats(String collection, String attribute) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outSets = arena<Pointer<Pointer<Double>>>();
      final outSizes = arena<Pointer<Size>>();
      final outCount = arena<Size>();

      check(
        bindings.quiver_database_read_set_floats(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          attribute.toNativeUtf8(allocator: arena).cast(),
          outSets,
          outSizes,
          outCount,
        ),
      );

      final count = outCount.value;
      if (count == 0 || outSets.value == nullptr) {
        return [];
      }

      final result = <List<double>>[];
      for (var i = 0; i < count; i++) {
        final size = outSizes.value[i];
        if (size == 0 || outSets.value[i] == nullptr) {
          result.add([]);
        } else {
          result.add(List<double>.generate(size, (j) => outSets.value[i][j]));
        }
      }
      bindings.quiver_database_free_float_vectors(
        outSets.value,
        outSizes.value,
        count,
      );
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  /// Reads all string sets for a set attribute from a collection.
  List<List<String>> readSetStrings(String collection, String attribute) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outSets = arena<Pointer<Pointer<Pointer<Char>>>>();
      final outSizes = arena<Pointer<Size>>();
      final outCount = arena<Size>();

      check(
        bindings.quiver_database_read_set_strings(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          attribute.toNativeUtf8(allocator: arena).cast(),
          outSets,
          outSizes,
          outCount,
        ),
      );

      final count = outCount.value;
      if (count == 0 || outSets.value == nullptr) {
        return [];
      }

      final result = <List<String>>[];
      for (var i = 0; i < count; i++) {
        final size = outSizes.value[i];
        if (size == 0 || outSets.value[i] == nullptr) {
          result.add([]);
        } else {
          result.add(
            List<String>.generate(
              size,
              (j) => outSets.value[i][j].cast<Utf8>().toDartString(),
            ),
          );
        }
      }
      bindings.quiver_database_free_string_vectors(
        outSets.value,
        outSizes.value,
        count,
      );
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  // ==========================================================================
  // Read scalar by Id
  // ==========================================================================

  /// Reads an integer value for a scalar attribute by element Id.
  /// Returns null if the element is not found.
  int? readScalarIntegerById(String collection, String attribute, int id) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outValue = arena<Int64>();
      final outHasValue = arena<Int>();

      check(
        bindings.quiver_database_read_scalar_integer_by_id(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          attribute.toNativeUtf8(allocator: arena).cast(),
          id,
          outValue,
          outHasValue,
        ),
      );

      if (outHasValue.value == 0) {
        return null;
      }
      return outValue.value;
    } finally {
      arena.releaseAll();
    }
  }

  /// Reads a float value for a scalar attribute by element Id.
  /// Returns null if the element is not found.
  double? readScalarFloatById(String collection, String attribute, int id) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outValue = arena<Double>();
      final outHasValue = arena<Int>();

      check(
        bindings.quiver_database_read_scalar_float_by_id(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          attribute.toNativeUtf8(allocator: arena).cast(),
          id,
          outValue,
          outHasValue,
        ),
      );

      if (outHasValue.value == 0) {
        return null;
      }
      return outValue.value;
    } finally {
      arena.releaseAll();
    }
  }

  /// Reads a string value for a scalar attribute by element Id.
  /// Returns null if the element is not found.
  String? readScalarStringById(String collection, String attribute, int id) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outValue = arena<Pointer<Char>>();
      final outHasValue = arena<Int>();

      check(
        bindings.quiver_database_read_scalar_string_by_id(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          attribute.toNativeUtf8(allocator: arena).cast(),
          id,
          outValue,
          outHasValue,
        ),
      );

      if (outHasValue.value == 0 || outValue.value == nullptr) {
        return null;
      }
      final result = outValue.value.cast<Utf8>().toDartString();
      bindings.quiver_element_free_string(outValue.value);
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  /// Reads a DateTime value for a scalar attribute by element Id.
  /// Returns null if the element is not found.
  DateTime? readScalarDateTimeById(
    String collection,
    String attribute,
    int id,
  ) {
    final strValue = readScalarStringById(collection, attribute, id);
    return strValue == null ? null : stringToDateTime(strValue);
  }

  // ==========================================================================
  // Read vector by Id
  // ==========================================================================

  /// Reads integer vector for a vector attribute by element Id.
  List<int> readVectorIntegersById(
    String collection,
    String attribute,
    int id,
  ) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outValues = arena<Pointer<Int64>>();
      final outCount = arena<Size>();

      check(
        bindings.quiver_database_read_vector_integers_by_id(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          attribute.toNativeUtf8(allocator: arena).cast(),
          id,
          outValues,
          outCount,
        ),
      );

      final count = outCount.value;
      if (count == 0 || outValues.value == nullptr) {
        return [];
      }

      final result = List<int>.generate(count, (i) => outValues.value[i]);
      bindings.quiver_database_free_integer_array(outValues.value);
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  /// Reads float vector for a vector attribute by element Id.
  List<double> readVectorFloatsById(
    String collection,
    String attribute,
    int id,
  ) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outValues = arena<Pointer<Double>>();
      final outCount = arena<Size>();

      check(
        bindings.quiver_database_read_vector_floats_by_id(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          attribute.toNativeUtf8(allocator: arena).cast(),
          id,
          outValues,
          outCount,
        ),
      );

      final count = outCount.value;
      if (count == 0 || outValues.value == nullptr) {
        return [];
      }

      final result = List<double>.generate(count, (i) => outValues.value[i]);
      bindings.quiver_database_free_float_array(outValues.value);
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  /// Reads string vector for a vector attribute by element Id.
  List<String> readVectorStringsById(
    String collection,
    String attribute,
    int id,
  ) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outValues = arena<Pointer<Pointer<Char>>>();
      final outCount = arena<Size>();

      check(
        bindings.quiver_database_read_vector_strings_by_id(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          attribute.toNativeUtf8(allocator: arena).cast(),
          id,
          outValues,
          outCount,
        ),
      );

      final count = outCount.value;
      if (count == 0 || outValues.value == nullptr) {
        return [];
      }

      final result = List<String>.generate(
        count,
        (i) => outValues.value[i].cast<Utf8>().toDartString(),
      );
      bindings.quiver_database_free_string_array(outValues.value, count);
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  /// Reads DateTime vector for a vector attribute by element Id.
  List<DateTime> readVectorDateTimesById(
    String collection,
    String attribute,
    int id,
  ) {
    return readVectorStringsById(
      collection,
      attribute,
      id,
    ).map((s) => stringToDateTime(s)).toList();
  }

  // ==========================================================================
  // Read set by Id
  // ==========================================================================

  /// Reads integer set for a set attribute by element Id.
  List<int> readSetIntegersById(String collection, String attribute, int id) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outValues = arena<Pointer<Int64>>();
      final outCount = arena<Size>();

      check(
        bindings.quiver_database_read_set_integers_by_id(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          attribute.toNativeUtf8(allocator: arena).cast(),
          id,
          outValues,
          outCount,
        ),
      );

      final count = outCount.value;
      if (count == 0 || outValues.value == nullptr) {
        return [];
      }

      final result = List<int>.generate(count, (i) => outValues.value[i]);
      bindings.quiver_database_free_integer_array(outValues.value);
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  /// Reads float set for a set attribute by element Id.
  List<double> readSetFloatsById(String collection, String attribute, int id) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outValues = arena<Pointer<Double>>();
      final outCount = arena<Size>();

      check(
        bindings.quiver_database_read_set_floats_by_id(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          attribute.toNativeUtf8(allocator: arena).cast(),
          id,
          outValues,
          outCount,
        ),
      );

      final count = outCount.value;
      if (count == 0 || outValues.value == nullptr) {
        return [];
      }

      final result = List<double>.generate(count, (i) => outValues.value[i]);
      bindings.quiver_database_free_float_array(outValues.value);
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  /// Reads string set for a set attribute by element Id.
  List<String> readSetStringsById(String collection, String attribute, int id) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outValues = arena<Pointer<Pointer<Char>>>();
      final outCount = arena<Size>();

      check(
        bindings.quiver_database_read_set_strings_by_id(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          attribute.toNativeUtf8(allocator: arena).cast(),
          id,
          outValues,
          outCount,
        ),
      );

      final count = outCount.value;
      if (count == 0 || outValues.value == nullptr) {
        return [];
      }

      final result = List<String>.generate(
        count,
        (i) => outValues.value[i].cast<Utf8>().toDartString(),
      );
      bindings.quiver_database_free_string_array(outValues.value, count);
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  /// Reads DateTime set for a set attribute by element Id.
  List<DateTime> readSetDateTimesById(
    String collection,
    String attribute,
    int id,
  ) {
    return readSetStringsById(
      collection,
      attribute,
      id,
    ).map((s) => stringToDateTime(s)).toList();
  }

  // ==========================================================================
  // Read element Ids
  // ==========================================================================

  /// Reads all element Ids from a collection.
  List<int> readElementIds(String collection) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outIds = arena<Pointer<Int64>>();
      final outCount = arena<Size>();

      check(
        bindings.quiver_database_read_element_ids(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          outIds,
          outCount,
        ),
      );

      final count = outCount.value;
      if (count == 0 || outIds.value == nullptr) {
        return [];
      }

      final result = List<int>.generate(count, (i) => outIds.value[i]);
      bindings.quiver_database_free_integer_array(outIds.value);
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  // ==========================================================================
  // Read all attributes by Id (convenience methods)
  // ==========================================================================

  /// Reads all scalar attributes for an element by Id.
  /// Returns a map of attribute name to value.
  /// DateTime columns are converted to DateTime objects.
  Map<String, Object?> readScalarsById(String collection, int id) {
    _ensureNotClosed();

    final result = <String, Object?>{};
    for (final attribute in listScalarAttributes(collection)) {
      final name = attribute.name;
      switch (attribute.dataType) {
        case quiver_data_type_t.QUIVER_DATA_TYPE_INTEGER:
          result[name] = readScalarIntegerById(collection, name, id);
        case quiver_data_type_t.QUIVER_DATA_TYPE_FLOAT:
          result[name] = readScalarFloatById(collection, name, id);
        case quiver_data_type_t.QUIVER_DATA_TYPE_STRING:
          result[name] = readScalarStringById(collection, name, id);
        case quiver_data_type_t.QUIVER_DATA_TYPE_DATE_TIME:
          result[name] = readScalarDateTimeById(collection, name, id);
      }
    }
    return result;
  }

  /// Reads all vector attributes for an element by Id.
  /// Returns a map of group name to list of values.
  /// DateTime columns are converted to DateTime objects.
  Map<String, List<Object>> readVectorsById(String collection, int id) {
    _ensureNotClosed();

    final result = <String, List<Object>>{};
    for (final group in listVectorGroups(collection)) {
      final name = group.groupName;
      final dataType = _getValueDataType(group.valueColumns);
      switch (dataType) {
        case quiver_data_type_t.QUIVER_DATA_TYPE_INTEGER:
          result[name] = readVectorIntegersById(collection, name, id);
        case quiver_data_type_t.QUIVER_DATA_TYPE_FLOAT:
          result[name] = readVectorFloatsById(collection, name, id);
        case quiver_data_type_t.QUIVER_DATA_TYPE_STRING:
          result[name] = readVectorStringsById(collection, name, id);
        case quiver_data_type_t.QUIVER_DATA_TYPE_DATE_TIME:
          result[name] = readVectorDateTimesById(collection, name, id);
      }
    }
    return result;
  }

  /// Reads all set attributes for an element by Id.
  /// Returns a map of group name to list of values.
  /// DateTime columns are converted to DateTime objects.
  Map<String, List<Object>> readSetsById(String collection, int id) {
    _ensureNotClosed();

    final result = <String, List<Object>>{};
    for (final group in listSetGroups(collection)) {
      final name = group.groupName;
      final dataType = _getValueDataType(group.valueColumns);
      switch (dataType) {
        case quiver_data_type_t.QUIVER_DATA_TYPE_INTEGER:
          result[name] = readSetIntegersById(collection, name, id);
        case quiver_data_type_t.QUIVER_DATA_TYPE_FLOAT:
          result[name] = readSetFloatsById(collection, name, id);
        case quiver_data_type_t.QUIVER_DATA_TYPE_STRING:
          result[name] = readSetStringsById(collection, name, id);
        case quiver_data_type_t.QUIVER_DATA_TYPE_DATE_TIME:
          result[name] = readSetDateTimesById(collection, name, id);
      }
    }
    return result;
  }

  /// Reads a vector group for an element by Id, returning rows as maps.
  /// Each row contains column names mapped to their values.
  /// Useful for multi-column vector tables.
  List<Map<String, Object?>> readVectorGroupById(
    String collection,
    String group,
    int id,
  ) {
    _ensureNotClosed();

    // Get metadata for this group
    final metadata = getVectorMetadata(collection, group);
    final columns = metadata.valueColumns;

    if (columns.isEmpty) return [];

    // Read each column's data
    final columnData = <String, List<Object>>{};
    int rowCount = 0;

    for (final col in columns) {
      final name = col.name;
      List<Object> values;

      switch (col.dataType) {
        case quiver_data_type_t.QUIVER_DATA_TYPE_INTEGER:
          values = readVectorIntegersById(collection, name, id);
        case quiver_data_type_t.QUIVER_DATA_TYPE_FLOAT:
          values = readVectorFloatsById(collection, name, id);
        case quiver_data_type_t.QUIVER_DATA_TYPE_STRING:
          values = readVectorStringsById(collection, name, id);
        case quiver_data_type_t.QUIVER_DATA_TYPE_DATE_TIME:
          values = readVectorDateTimesById(collection, name, id);
        default:
          throw ArgumentError('Unknown data type: ${col.dataType}');
      }

      columnData[name] = values;
      rowCount = values.length;
    }

    // Transpose columns to rows
    final rows = <Map<String, Object?>>[];
    for (int i = 0; i < rowCount; i++) {
      final row = <String, Object?>{};
      for (final entry in columnData.entries) {
        row[entry.key] = entry.value[i];
      }
      rows.add(row);
    }

    return rows;
  }

  /// Reads a set group for an element by Id, returning rows as maps.
  /// Each row contains column names mapped to their values.
  /// Useful for multi-column set tables.
  List<Map<String, Object?>> readSetGroupById(
    String collection,
    String group,
    int id,
  ) {
    _ensureNotClosed();

    // Get metadata for this group
    final metadata = getSetMetadata(collection, group);
    final columns = metadata.valueColumns;

    if (columns.isEmpty) return [];

    // Read each column's data
    final columnData = <String, List<Object>>{};
    int rowCount = 0;

    for (final col in columns) {
      final name = col.name;
      List<Object> values;

      switch (col.dataType) {
        case quiver_data_type_t.QUIVER_DATA_TYPE_INTEGER:
          values = readSetIntegersById(collection, name, id);
        case quiver_data_type_t.QUIVER_DATA_TYPE_FLOAT:
          values = readSetFloatsById(collection, name, id);
        case quiver_data_type_t.QUIVER_DATA_TYPE_STRING:
          values = readSetStringsById(collection, name, id);
        case quiver_data_type_t.QUIVER_DATA_TYPE_DATE_TIME:
          values = readSetDateTimesById(collection, name, id);
        default:
          throw ArgumentError('Unknown data type: ${col.dataType}');
      }

      columnData[name] = values;
      rowCount = values.length;
    }

    // Transpose columns to rows
    final rows = <Map<String, Object?>>[];
    for (int i = 0; i < rowCount; i++) {
      final row = <String, Object?>{};
      for (final entry in columnData.entries) {
        row[entry.key] = entry.value[i];
      }
      rows.add(row);
    }

    return rows;
  }

  // ==========================================================================
  // Read time series by Id
  // ==========================================================================

  /// Reads a time series group for an element by Id.
  /// Returns a Map of column names to typed Lists.
  /// The dimension column is parsed to List<DateTime>.
  /// INTEGER columns return List<int>, FLOAT columns return List<double>,
  /// other TEXT columns return List<String>.
  Map<String, List<Object>> readTimeSeriesGroup(
    String collection,
    String group,
    int id,
  ) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outColNames = arena<Pointer<Pointer<Char>>>();
      final outColTypes = arena<Pointer<Int>>();
      final outColData = arena<Pointer<Pointer<Void>>>();
      final outColCount = arena<Size>();
      final outRowCount = arena<Size>();

      check(
        bindings.quiver_database_read_time_series_group(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          group.toNativeUtf8(allocator: arena).cast(),
          id,
          outColNames,
          outColTypes,
          outColData,
          outColCount,
          outRowCount,
        ),
      );

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
              rowCount,
              (r) => stringToDateTime(ptr[r].cast<Utf8>().toDartString()),
            );
          } else {
            result[colName] = List<String>.generate(
              rowCount,
              (r) => ptr[r].cast<Utf8>().toDartString(),
            );
          }
        }
      }

      // Free C-allocated memory
      bindings.quiver_database_free_time_series_data(
        outColNames.value,
        outColTypes.value,
        outColData.value,
        colCount,
        rowCount,
      );

      return result;
    } finally {
      arena.releaseAll();
    }
  }

  // ==========================================================================
  // Read time series files
  // ==========================================================================

  /// Reads time series files paths for a collection.
  /// Returns a map of column name to file path (null if not set).
  Map<String, String?> readTimeSeriesFiles(String collection) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outColumns = arena<Pointer<Pointer<Char>>>();
      final outPaths = arena<Pointer<Pointer<Char>>>();
      final outCount = arena<Size>();

      check(
        bindings.quiver_database_read_time_series_files(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          outColumns,
          outPaths,
          outCount,
        ),
      );

      final count = outCount.value;
      if (count == 0 || outColumns.value == nullptr) {
        return {};
      }

      final result = <String, String?>{};
      for (var i = 0; i < count; i++) {
        final column = outColumns.value[i].cast<Utf8>().toDartString();
        final path = outPaths.value[i] == nullptr ? null : outPaths.value[i].cast<Utf8>().toDartString();
        result[column] = path;
      }

      bindings.quiver_database_free_time_series_files(
        outColumns.value,
        outPaths.value,
        count,
      );
      return result;
    } finally {
      arena.releaseAll();
    }
  }
}
