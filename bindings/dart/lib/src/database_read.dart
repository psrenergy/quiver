part of 'database.dart';

/// Read operations for Database.
extension DatabaseRead on Database {
  // ==========================================================================
  // Read all scalar/vector/set values
  // ==========================================================================

  /// Reads all integer values for a scalar attribute. One entry per element;
  /// a SQL NULL is `null`.
  List<int?> readScalarIntegers(String collection, String attribute) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outValues = arena<Pointer<Int64>>();
      final outMask = arena<Pointer<Uint8>>();
      final outCount = arena<Size>();

      check(
        bindings.quiver_database_read_scalar_integers(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          attribute.toNativeUtf8(allocator: arena).cast(),
          outValues,
          outMask,
          outCount,
        ),
      );

      final count = outCount.value;
      if (count == 0 || outValues.value == nullptr) {
        return [];
      }

      final values = outValues.value;
      final mask = outMask.value;
      final result = List<int?>.generate(
        count,
        (i) => mask[i] != 0 ? values[i] : null,
      );
      bindings.quiver_database_free_integer_array(values);
      bindings.quiver_database_free_mask(mask);
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  /// Reads all boolean values stored as integer scalar attributes.
  /// One entry is returned per element; a SQL NULL is `null`.
  List<bool?> readScalarBooleans(String collection, String attribute) {
    return readScalarIntegers(
      collection,
      attribute,
    ).map((value) => _integerToBoolean(value, collection, attribute)).toList();
  }

  /// Reads all float values for a scalar attribute. One entry per element;
  /// a SQL NULL is `null`.
  List<double?> readScalarFloats(String collection, String attribute) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outValues = arena<Pointer<Double>>();
      final outMask = arena<Pointer<Uint8>>();
      final outCount = arena<Size>();

      check(
        bindings.quiver_database_read_scalar_floats(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          attribute.toNativeUtf8(allocator: arena).cast(),
          outValues,
          outMask,
          outCount,
        ),
      );

      final count = outCount.value;
      if (count == 0 || outValues.value == nullptr) {
        return [];
      }

      final values = outValues.value;
      final mask = outMask.value;
      final result = List<double?>.generate(
        count,
        (i) => mask[i] != 0 ? values[i] : null,
      );
      bindings.quiver_database_free_float_array(values);
      bindings.quiver_database_free_mask(mask);
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  /// Reads all string values for a scalar attribute. One entry per element;
  /// a SQL NULL is `null`.
  List<String?> readScalarStrings(String collection, String attribute) {
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

      final result = List<String?>.generate(count, (i) {
        final ptr = outValues.value[i];
        return ptr == nullptr ? null : ptr.cast<Utf8>().toDartString();
      });
      bindings.quiver_database_free_string_array(outValues.value, count);
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  /// Reads all DateTime values for a scalar attribute. One entry per element;
  /// a SQL NULL is `null`.
  List<DateTime?> readScalarDateTimes(String collection, String attribute) {
    return readScalarStrings(collection, attribute)
        .map(
          (value) => value == null ? null : stringToDateTime(value, collection, attribute),
        )
        .toList();
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

  /// Reads all boolean vectors stored as integer vector attributes.
  ///
  /// NULL cells are dropped and only elements that own rows are returned, so the
  /// result is not positionally aligned with [readElementIds] (unlike
  /// [readScalarBooleans]).
  List<List<bool>> readVectorBooleans(String collection, String attribute) {
    return readVectorIntegers(
      collection,
      attribute,
    ).map((values) => values.map((value) => _integerToBooleanNonNull(value, collection, attribute)).toList()).toList();
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

  /// Reads all DateTime vectors for a vector attribute from a collection.
  ///
  /// NULL cells are dropped and only elements that own rows are returned, so the
  /// result is not positionally aligned with [readElementIds] (unlike
  /// [readScalarDateTimes]).
  List<List<DateTime>> readVectorDateTimes(
    String collection,
    String attribute,
  ) {
    return readVectorStrings(collection, attribute)
        .map(
          (values) => values.map((value) => stringToDateTime(value, collection, attribute)).toList(),
        )
        .toList();
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

  /// Reads all boolean sets stored as integer set attributes.
  ///
  /// Same alignment caveat as [readVectorBooleans]: NULL cells are dropped and
  /// only elements that own rows are returned.
  List<List<bool>> readSetBooleans(String collection, String attribute) {
    return readSetIntegers(
      collection,
      attribute,
    ).map((values) => values.map((value) => _integerToBooleanNonNull(value, collection, attribute)).toList()).toList();
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

  /// Reads all DateTime sets for a set attribute from a collection.
  ///
  /// Same alignment caveat as [readVectorDateTimes]: NULL cells are dropped and
  /// only elements that own rows are returned.
  List<List<DateTime>> readSetDateTimes(String collection, String attribute) {
    return readSetStrings(collection, attribute)
        .map(
          (values) => values.map((value) => stringToDateTime(value, collection, attribute)).toList(),
        )
        .toList();
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

  /// Reads a boolean stored as an integer scalar attribute by element Id.
  /// Returns null if the element or value is absent.
  bool? readScalarBooleanById(String collection, String attribute, int id) {
    return _integerToBoolean(readScalarIntegerById(collection, attribute, id), collection, attribute);
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
      bindings.quiver_database_free_string(outValue.value);
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
    return strValue == null ? null : stringToDateTime(strValue, collection, attribute);
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

  /// Reads a boolean vector stored as an integer vector attribute by element Id.
  List<bool> readVectorBooleansById(String collection, String attribute, int id) {
    return readVectorIntegersById(
      collection,
      attribute,
      id,
    ).map((value) => _integerToBooleanNonNull(value, collection, attribute)).toList();
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
    ).map((s) => stringToDateTime(s, collection, attribute)).toList();
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

  /// Reads a boolean set stored as an integer set attribute by element Id.
  List<bool> readSetBooleansById(String collection, String attribute, int id) {
    return readSetIntegersById(
      collection,
      attribute,
      id,
    ).map((value) => _integerToBooleanNonNull(value, collection, attribute)).toList();
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
    ).map((s) => stringToDateTime(s, collection, attribute)).toList();
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

  /// Returns the current number of elements in a collection.
  int numberOfElements(String collection) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outCount = arena<Int64>();

      check(
        bindings.quiver_database_number_of_elements(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          outCount,
        ),
      );

      return outCount.value;
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
        default:
          throw ArgumentError('Unknown data type: ${attribute.dataType}');
      }
    }
    return result;
  }

  /// Reads all vector attributes for an element by Id.
  /// Returns a map of column name to list of values.
  /// DateTime columns are converted to DateTime objects.
  Map<String, List<Object>> readVectorsById(String collection, int id) {
    _ensureNotClosed();

    final result = <String, List<Object>>{};
    for (final group in listVectorGroups(collection)) {
      for (final col in group.valueColumns) {
        final name = col.name;
        switch (col.dataType) {
          case quiver_data_type_t.QUIVER_DATA_TYPE_INTEGER:
            result[name] = readVectorIntegersById(collection, name, id);
          case quiver_data_type_t.QUIVER_DATA_TYPE_FLOAT:
            result[name] = readVectorFloatsById(collection, name, id);
          case quiver_data_type_t.QUIVER_DATA_TYPE_STRING:
            result[name] = readVectorStringsById(collection, name, id);
          case quiver_data_type_t.QUIVER_DATA_TYPE_DATE_TIME:
            result[name] = readVectorDateTimesById(collection, name, id);
          default:
            throw ArgumentError('Unknown data type: ${col.dataType}');
        }
      }
    }
    return result;
  }

  /// Reads all set attributes for an element by Id.
  /// Returns a map of column name to list of values.
  /// DateTime columns are converted to DateTime objects.
  Map<String, List<Object>> readSetsById(String collection, int id) {
    _ensureNotClosed();

    final result = <String, List<Object>>{};
    for (final group in listSetGroups(collection)) {
      for (final col in group.valueColumns) {
        final name = col.name;
        switch (col.dataType) {
          case quiver_data_type_t.QUIVER_DATA_TYPE_INTEGER:
            result[name] = readSetIntegersById(collection, name, id);
          case quiver_data_type_t.QUIVER_DATA_TYPE_FLOAT:
            result[name] = readSetFloatsById(collection, name, id);
          case quiver_data_type_t.QUIVER_DATA_TYPE_STRING:
            result[name] = readSetStringsById(collection, name, id);
          case quiver_data_type_t.QUIVER_DATA_TYPE_DATE_TIME:
            result[name] = readSetDateTimesById(collection, name, id);
          default:
            throw ArgumentError('Unknown data type: ${col.dataType}');
        }
      }
    }
    return result;
  }

  /// Reads all scalar, vector, and set attributes for an element by Id.
  /// Returns a single map merging all attribute types.
  Map<String, Object?> readElementById(String collection, int id) {
    final result = <String, Object?>{};
    result.addAll(readScalarsById(collection, id));
    result.addAll(readVectorsById(collection, id));
    result.addAll(readSetsById(collection, id));
    return result;
  }

  /// Reads a vector group for an element by Id, returning rows as maps.
  /// Each row contains column names mapped to their values; a SQL NULL cell
  /// is `null` (rows stay positionally aligned). DATE_TIME columns are
  /// parsed to DateTime.
  List<Map<String, Object?>> readVectorGroupById(
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
      final outColHasValue = arena<Pointer<Pointer<Uint8>>>();
      final outColCount = arena<Size>();
      final outRowCount = arena<Size>();

      check(
        bindings.quiver_database_read_vector_group_by_id(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          group.toNativeUtf8(allocator: arena).cast(),
          id,
          outColNames,
          outColTypes,
          outColData,
          outColHasValue,
          outColCount,
          outRowCount,
        ),
      );

      return _decodeGroupRows(
        collection,
        outColNames,
        outColTypes,
        outColData,
        outColHasValue,
        outColCount,
        outRowCount,
      );
    } finally {
      arena.releaseAll();
    }
  }

  /// Reads a set group for an element by Id, returning rows as maps.
  /// Each row contains column names mapped to their values; a SQL NULL cell
  /// is `null` (rows stay positionally aligned). DATE_TIME columns are
  /// parsed to DateTime.
  List<Map<String, Object?>> readSetGroupById(
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
      final outColHasValue = arena<Pointer<Pointer<Uint8>>>();
      final outColCount = arena<Size>();
      final outRowCount = arena<Size>();

      check(
        bindings.quiver_database_read_set_group_by_id(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          group.toNativeUtf8(allocator: arena).cast(),
          id,
          outColNames,
          outColTypes,
          outColData,
          outColHasValue,
          outColCount,
          outRowCount,
        ),
      );

      return _decodeGroupRows(
        collection,
        outColNames,
        outColTypes,
        outColData,
        outColHasValue,
        outColCount,
        outRowCount,
      );
    } finally {
      arena.releaseAll();
    }
  }

  // Decodes the columnar typed-arrays + per-cell mask result of the group
  // read C functions into row maps, then frees the C allocations.
  List<Map<String, Object?>> _decodeGroupRows(
    String collection,
    Pointer<Pointer<Pointer<Char>>> outColNames,
    Pointer<Pointer<Int>> outColTypes,
    Pointer<Pointer<Pointer<Void>>> outColData,
    Pointer<Pointer<Pointer<Uint8>>> outColHasValue,
    Pointer<Size> outColCount,
    Pointer<Size> outRowCount,
  ) {
    final colCount = outColCount.value;
    final rowCount = outRowCount.value;

    if (colCount == 0 || rowCount == 0) return [];

    final rows = List.generate(rowCount, (_) => <String, Object?>{});
    for (var c = 0; c < colCount; c++) {
      final colName = outColNames.value[c].cast<Utf8>().toDartString();
      final colType = outColTypes.value[c];
      final mask = outColHasValue.value[c];

      if (colType == quiver_data_type_t.QUIVER_DATA_TYPE_INTEGER) {
        final ptr = outColData.value[c].cast<Int64>();
        for (var r = 0; r < rowCount; r++) {
          rows[r][colName] = mask[r] != 0 ? ptr[r] : null;
        }
      } else if (colType == quiver_data_type_t.QUIVER_DATA_TYPE_FLOAT) {
        final ptr = outColData.value[c].cast<Double>();
        for (var r = 0; r < rowCount; r++) {
          rows[r][colName] = mask[r] != 0 ? ptr[r] : null;
        }
      } else if (colType == quiver_data_type_t.QUIVER_DATA_TYPE_DATE_TIME) {
        final ptr = outColData.value[c].cast<Pointer<Char>>();
        for (var r = 0; r < rowCount; r++) {
          // Never toDartString a masked-out (NULL) pointer.
          rows[r][colName] = mask[r] != 0
              ? stringToDateTime(
                  ptr[r].cast<Utf8>().toDartString(),
                  collection,
                  colName,
                )
              : null;
        }
      } else {
        final ptr = outColData.value[c].cast<Pointer<Char>>();
        for (var r = 0; r < rowCount; r++) {
          rows[r][colName] = mask[r] != 0 ? ptr[r].cast<Utf8>().toDartString() : null;
        }
      }
    }

    bindings.quiver_database_free_time_series_data(
      outColNames.value,
      outColTypes.value,
      outColData.value,
      outColHasValue.value,
      colCount,
      rowCount,
    );

    return rows;
  }

  // ==========================================================================
  // Read time series by Id
  // ==========================================================================

  /// Reads a time series group for an element by Id.
  /// Returns a Map of column names to typed Lists.
  /// The dimension column is parsed to List<DateTime>.
  /// INTEGER columns return List<int?>, FLOAT columns return List<double?>,
  /// other TEXT columns return List<String?>; a SQL NULL cell is `null`.
  Map<String, List<Object?>> readTimeSeriesGroup(
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
      final outColHasValue = arena<Pointer<Pointer<Uint8>>>();
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
          outColHasValue,
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

      // Per-cell NULL mask: mask[r] == 0 means SQL NULL, surfaced as null. The
      // dimension column's mask is always all 1, so it stays a dense List<DateTime>.
      final result = <String, List<Object?>>{};
      for (var c = 0; c < colCount; c++) {
        final colName = outColNames.value[c].cast<Utf8>().toDartString();
        final colType = outColTypes.value[c];
        final mask = outColHasValue.value[c];

        if (colType == quiver_data_type_t.QUIVER_DATA_TYPE_INTEGER) {
          final ptr = outColData.value[c].cast<Int64>();
          result[colName] = List<int?>.generate(rowCount, (r) => mask[r] != 0 ? ptr[r] : null);
        } else if (colType == quiver_data_type_t.QUIVER_DATA_TYPE_FLOAT) {
          final ptr = outColData.value[c].cast<Double>();
          result[colName] = List<double?>.generate(rowCount, (r) => mask[r] != 0 ? ptr[r] : null);
        } else {
          // STRING or DATE_TIME
          final ptr = outColData.value[c].cast<Pointer<Char>>();
          if (colName == dimCol) {
            result[colName] = List<DateTime>.generate(
              rowCount,
              (r) => stringToDateTime(
                ptr[r].cast<Utf8>().toDartString(),
                collection,
                colName,
              ),
            );
          } else {
            // Never toDartString a masked-out (NULL) pointer.
            result[colName] = List<String?>.generate(
              rowCount,
              (r) => mask[r] != 0 ? ptr[r].cast<Utf8>().toDartString() : null,
            );
          }
        }
      }

      // Free C-allocated memory
      bindings.quiver_database_free_time_series_data(
        outColNames.value,
        outColTypes.value,
        outColData.value,
        outColHasValue.value,
        colCount,
        rowCount,
      );

      return result;
    } finally {
      arena.releaseAll();
    }
  }

  /// Reads one value per element for a time series attribute at [dateTime].
  ///
  /// Uses "last non-null value at or before [dateTime]" lookup semantics.
  /// Entries are typed by the column (`int`, `double`, or `String`); elements
  /// with no matching data yield `null`.
  List<Object?> readTimeSeriesRow(
    String collection,
    String group,
    String attribute,
    DateTime dateTime,
  ) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outDataType = arena<Int>();
      final outValues = arena<Pointer<Void>>();
      final outCount = arena<Size>();

      check(
        bindings.quiver_database_read_time_series_row(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          group.toNativeUtf8(allocator: arena).cast(),
          attribute.toNativeUtf8(allocator: arena).cast(),
          dateTimeToString(dateTime).toNativeUtf8(allocator: arena).cast(),
          outDataType,
          outValues,
          outCount,
        ),
      );

      final count = outCount.value;
      if (count == 0 || outValues.value == nullptr) {
        return [];
      }

      switch (outDataType.value) {
        case quiver_data_type_t.QUIVER_DATA_TYPE_INTEGER:
          final ptr = outValues.value.cast<Int64>();
          final result = List<Object?>.generate(count, (i) => ptr[i]);
          bindings.quiver_database_free_integer_array(ptr);
          return result;
        case quiver_data_type_t.QUIVER_DATA_TYPE_FLOAT:
          final ptr = outValues.value.cast<Double>();
          final result = List<Object?>.generate(count, (i) => ptr[i]);
          bindings.quiver_database_free_float_array(ptr);
          return result;
        default:
          // STRING or DATE_TIME; NULL entries mark elements with no data
          final ptr = outValues.value.cast<Pointer<Char>>();
          final result = List<Object?>.generate(
            count,
            (i) => ptr[i] == nullptr ? null : ptr[i].cast<Utf8>().toDartString(),
          );
          bindings.quiver_database_free_string_array(ptr, count);
          return result;
      }
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
