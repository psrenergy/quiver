import 'dart:ffi';

import 'package:ffi/ffi.dart';

import 'ffi/bindings.dart';
import 'ffi/library_loader.dart';
import 'element.dart';
import 'exceptions.dart';

/// A wrapper for the PSR Database.
///
/// Use [Database.fromSchema] to create a new database from a SQL schema file.
/// After use, call [close] to free native resources.
class Database {
  Pointer<psr_database_t> _ptr;
  bool _isClosed = false;

  Database._(this._ptr);

  /// Internal: Returns the native pointer for use by other library components.
  Pointer<psr_database_t> get ptr => _ptr;

  /// Creates a new database from a SQL schema file.
  factory Database.fromSchema(String dbPath, String schemaPath) {
    final arena = Arena();
    try {
      final optionsPtr = arena<psr_database_options_t>();
      optionsPtr.ref = bindings.psr_database_options_default();

      final ptr = bindings.psr_database_from_schema(
        dbPath.toNativeUtf8(allocator: arena).cast(),
        schemaPath.toNativeUtf8(allocator: arena).cast(),
        optionsPtr,
      );

      if (ptr == nullptr) {
        throw const SchemaException('Failed to create database from schema');
      }

      return Database._(ptr);
    } finally {
      arena.releaseAll();
    }
  }

  void _ensureNotClosed() {
    if (_isClosed) {
      throw const DatabaseOperationException('Database has been closed');
    }
  }

  /// Creates a new element in the specified collection.
  int createElement(String collection, Map<String, Object?> values) {
    _ensureNotClosed();

    final element = Element();
    try {
      for (final entry in values.entries) {
        element.set(entry.key, entry.value);
      }
      return createElementFromBuilder(collection, element);
    } finally {
      element.dispose();
    }
  }

  /// Creates a new element in the specified collection using an Element builder.
  int createElementFromBuilder(String collection, Element element) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final id = bindings.psr_database_create_element(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        element.ptr.cast(),
      );

      if (id < 0) {
        throw DatabaseException.fromError(id, "Failed to create element in '$collection'");
      }

      return id;
    } finally {
      arena.releaseAll();
    }
  }

  /// Sets a scalar relation (foreign key) between two elements by their labels.
  void setScalarRelation(String collection, String attribute, String fromLabel, String toLabel) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final err = bindings.psr_database_set_scalar_relation(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        attribute.toNativeUtf8(allocator: arena).cast(),
        fromLabel.toNativeUtf8(allocator: arena).cast(),
        toLabel.toNativeUtf8(allocator: arena).cast(),
      );

      if (err != psr_error_t.PSR_OK) {
        throw DatabaseException.fromError(err, "Failed to set scalar relation '$attribute' in '$collection'");
      }
    } finally {
      arena.releaseAll();
    }
  }

  /// Reads scalar relation values (target labels) for a FK attribute.
  /// Returns null for elements with no relation set.
  List<String?> readScalarRelation(String collection, String attribute) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outValues = arena<Pointer<Pointer<Char>>>();
      final outCount = arena<Size>();

      final err = bindings.psr_database_read_scalar_relation(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        attribute.toNativeUtf8(allocator: arena).cast(),
        outValues,
        outCount,
      );

      if (err != psr_error_t.PSR_OK) {
        throw DatabaseException.fromError(err, "Failed to read scalar relation '$attribute' from '$collection'");
      }

      final count = outCount.value;
      if (count == 0 || outValues.value == nullptr) {
        return [];
      }

      final result = <String?>[];
      for (var i = 0; i < count; i++) {
        final ptr = outValues.value[i];
        if (ptr == nullptr) {
          result.add(null);
        } else {
          final s = ptr.cast<Utf8>().toDartString();
          result.add(s.isEmpty ? null : s);
        }
      }
      bindings.psr_free_string_array(outValues.value, count);
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  /// Reads all integer values for a scalar attribute from a collection.
  List<int> readScalarIntegers(String collection, String attribute) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outValues = arena<Pointer<Int64>>();
      final outCount = arena<Size>();

      final err = bindings.psr_database_read_scalar_integers(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        attribute.toNativeUtf8(allocator: arena).cast(),
        outValues,
        outCount,
      );

      if (err != psr_error_t.PSR_OK) {
        throw DatabaseException.fromError(err, "Failed to read scalar integers from '$collection.$attribute'");
      }

      final count = outCount.value;
      if (count == 0 || outValues.value == nullptr) {
        return [];
      }

      final result = List<int>.generate(count, (i) => outValues.value[i]);
      bindings.psr_free_int_array(outValues.value);
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  /// Reads all double values for a scalar attribute from a collection.
  List<double> readScalarDoubles(String collection, String attribute) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outValues = arena<Pointer<Double>>();
      final outCount = arena<Size>();

      final err = bindings.psr_database_read_scalar_doubles(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        attribute.toNativeUtf8(allocator: arena).cast(),
        outValues,
        outCount,
      );

      if (err != psr_error_t.PSR_OK) {
        throw DatabaseException.fromError(err, "Failed to read scalar doubles from '$collection.$attribute'");
      }

      final count = outCount.value;
      if (count == 0 || outValues.value == nullptr) {
        return [];
      }

      final result = List<double>.generate(count, (i) => outValues.value[i]);
      bindings.psr_free_double_array(outValues.value);
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

      final err = bindings.psr_database_read_scalar_strings(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        attribute.toNativeUtf8(allocator: arena).cast(),
        outValues,
        outCount,
      );

      if (err != psr_error_t.PSR_OK) {
        throw DatabaseException.fromError(err, "Failed to read scalar strings from '$collection.$attribute'");
      }

      final count = outCount.value;
      if (count == 0 || outValues.value == nullptr) {
        return [];
      }

      final result = List<String>.generate(count, (i) => outValues.value[i].cast<Utf8>().toDartString());
      bindings.psr_free_string_array(outValues.value, count);
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

      final err = bindings.psr_database_read_vector_integers(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        attribute.toNativeUtf8(allocator: arena).cast(),
        outVectors,
        outSizes,
        outCount,
      );

      if (err != psr_error_t.PSR_OK) {
        throw DatabaseException.fromError(err, "Failed to read vector integers from '$collection.$attribute'");
      }

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
      bindings.psr_free_int_vectors(outVectors.value, outSizes.value, count);
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  /// Reads all double vectors for a vector attribute from a collection.
  List<List<double>> readVectorDoubles(String collection, String attribute) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outVectors = arena<Pointer<Pointer<Double>>>();
      final outSizes = arena<Pointer<Size>>();
      final outCount = arena<Size>();

      final err = bindings.psr_database_read_vector_doubles(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        attribute.toNativeUtf8(allocator: arena).cast(),
        outVectors,
        outSizes,
        outCount,
      );

      if (err != psr_error_t.PSR_OK) {
        throw DatabaseException.fromError(err, "Failed to read vector doubles from '$collection.$attribute'");
      }

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
          result.add(List<double>.generate(size, (j) => outVectors.value[i][j]));
        }
      }
      bindings.psr_free_double_vectors(outVectors.value, outSizes.value, count);
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

      final err = bindings.psr_database_read_vector_strings(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        attribute.toNativeUtf8(allocator: arena).cast(),
        outVectors,
        outSizes,
        outCount,
      );

      if (err != psr_error_t.PSR_OK) {
        throw DatabaseException.fromError(err, "Failed to read vector strings from '$collection.$attribute'");
      }

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
          result.add(List<String>.generate(size, (j) => outVectors.value[i][j].cast<Utf8>().toDartString()));
        }
      }
      bindings.psr_free_string_vectors(outVectors.value, outSizes.value, count);
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

      final err = bindings.psr_database_read_set_integers(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        attribute.toNativeUtf8(allocator: arena).cast(),
        outSets,
        outSizes,
        outCount,
      );

      if (err != psr_error_t.PSR_OK) {
        throw DatabaseException.fromError(err, "Failed to read set integers from '$collection.$attribute'");
      }

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
      bindings.psr_free_int_vectors(outSets.value, outSizes.value, count);
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  /// Reads all double sets for a set attribute from a collection.
  List<List<double>> readSetDoubles(String collection, String attribute) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outSets = arena<Pointer<Pointer<Double>>>();
      final outSizes = arena<Pointer<Size>>();
      final outCount = arena<Size>();

      final err = bindings.psr_database_read_set_doubles(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        attribute.toNativeUtf8(allocator: arena).cast(),
        outSets,
        outSizes,
        outCount,
      );

      if (err != psr_error_t.PSR_OK) {
        throw DatabaseException.fromError(err, "Failed to read set doubles from '$collection.$attribute'");
      }

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
      bindings.psr_free_double_vectors(outSets.value, outSizes.value, count);
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

      final err = bindings.psr_database_read_set_strings(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        attribute.toNativeUtf8(allocator: arena).cast(),
        outSets,
        outSizes,
        outCount,
      );

      if (err != psr_error_t.PSR_OK) {
        throw DatabaseException.fromError(err, "Failed to read set strings from '$collection.$attribute'");
      }

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
          result.add(List<String>.generate(size, (j) => outSets.value[i][j].cast<Utf8>().toDartString()));
        }
      }
      bindings.psr_free_string_vectors(outSets.value, outSizes.value, count);
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  // Read scalar by ID methods

  /// Reads an integer value for a scalar attribute by element ID.
  /// Returns null if the element is not found.
  int? readScalarIntegerById(String collection, String attribute, int id) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outValue = arena<Int64>();
      final outHasValue = arena<Int>();

      final err = bindings.psr_database_read_scalar_integers_by_id(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        attribute.toNativeUtf8(allocator: arena).cast(),
        id,
        outValue,
        outHasValue,
      );

      if (err != psr_error_t.PSR_OK) {
        throw DatabaseException.fromError(err, "Failed to read scalar integer by id from '$collection.$attribute'");
      }

      if (outHasValue.value == 0) {
        return null;
      }
      return outValue.value;
    } finally {
      arena.releaseAll();
    }
  }

  /// Reads a double value for a scalar attribute by element ID.
  /// Returns null if the element is not found.
  double? readScalarDoubleById(String collection, String attribute, int id) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outValue = arena<Double>();
      final outHasValue = arena<Int>();

      final err = bindings.psr_database_read_scalar_doubles_by_id(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        attribute.toNativeUtf8(allocator: arena).cast(),
        id,
        outValue,
        outHasValue,
      );

      if (err != psr_error_t.PSR_OK) {
        throw DatabaseException.fromError(err, "Failed to read scalar double by id from '$collection.$attribute'");
      }

      if (outHasValue.value == 0) {
        return null;
      }
      return outValue.value;
    } finally {
      arena.releaseAll();
    }
  }

  /// Reads a string value for a scalar attribute by element ID.
  /// Returns null if the element is not found.
  String? readScalarStringById(String collection, String attribute, int id) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outValue = arena<Pointer<Char>>();
      final outHasValue = arena<Int>();

      final err = bindings.psr_database_read_scalar_strings_by_id(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        attribute.toNativeUtf8(allocator: arena).cast(),
        id,
        outValue,
        outHasValue,
      );

      if (err != psr_error_t.PSR_OK) {
        throw DatabaseException.fromError(err, "Failed to read scalar string by id from '$collection.$attribute'");
      }

      if (outHasValue.value == 0 || outValue.value == nullptr) {
        return null;
      }
      final result = outValue.value.cast<Utf8>().toDartString();
      bindings.psr_string_free(outValue.value);
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  // Read vector by ID methods

  /// Reads integer vector for a vector attribute by element ID.
  List<int> readVectorIntegersById(String collection, String attribute, int id) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outValues = arena<Pointer<Int64>>();
      final outCount = arena<Size>();

      final err = bindings.psr_database_read_vector_integers_by_id(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        attribute.toNativeUtf8(allocator: arena).cast(),
        id,
        outValues,
        outCount,
      );

      if (err != psr_error_t.PSR_OK) {
        throw DatabaseException.fromError(err, "Failed to read vector integers by id from '$collection.$attribute'");
      }

      final count = outCount.value;
      if (count == 0 || outValues.value == nullptr) {
        return [];
      }

      final result = List<int>.generate(count, (i) => outValues.value[i]);
      bindings.psr_free_int_array(outValues.value);
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  /// Reads double vector for a vector attribute by element ID.
  List<double> readVectorDoublesById(String collection, String attribute, int id) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outValues = arena<Pointer<Double>>();
      final outCount = arena<Size>();

      final err = bindings.psr_database_read_vector_doubles_by_id(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        attribute.toNativeUtf8(allocator: arena).cast(),
        id,
        outValues,
        outCount,
      );

      if (err != psr_error_t.PSR_OK) {
        throw DatabaseException.fromError(err, "Failed to read vector doubles by id from '$collection.$attribute'");
      }

      final count = outCount.value;
      if (count == 0 || outValues.value == nullptr) {
        return [];
      }

      final result = List<double>.generate(count, (i) => outValues.value[i]);
      bindings.psr_free_double_array(outValues.value);
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  /// Reads string vector for a vector attribute by element ID.
  List<String> readVectorStringsById(String collection, String attribute, int id) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outValues = arena<Pointer<Pointer<Char>>>();
      final outCount = arena<Size>();

      final err = bindings.psr_database_read_vector_strings_by_id(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        attribute.toNativeUtf8(allocator: arena).cast(),
        id,
        outValues,
        outCount,
      );

      if (err != psr_error_t.PSR_OK) {
        throw DatabaseException.fromError(err, "Failed to read vector strings by id from '$collection.$attribute'");
      }

      final count = outCount.value;
      if (count == 0 || outValues.value == nullptr) {
        return [];
      }

      final result = List<String>.generate(count, (i) => outValues.value[i].cast<Utf8>().toDartString());
      bindings.psr_free_string_array(outValues.value, count);
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  // Read set by ID methods

  /// Reads integer set for a set attribute by element ID.
  List<int> readSetIntegersById(String collection, String attribute, int id) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outValues = arena<Pointer<Int64>>();
      final outCount = arena<Size>();

      final err = bindings.psr_database_read_set_integers_by_id(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        attribute.toNativeUtf8(allocator: arena).cast(),
        id,
        outValues,
        outCount,
      );

      if (err != psr_error_t.PSR_OK) {
        throw DatabaseException.fromError(err, "Failed to read set integers by id from '$collection.$attribute'");
      }

      final count = outCount.value;
      if (count == 0 || outValues.value == nullptr) {
        return [];
      }

      final result = List<int>.generate(count, (i) => outValues.value[i]);
      bindings.psr_free_int_array(outValues.value);
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  /// Reads double set for a set attribute by element ID.
  List<double> readSetDoublesById(String collection, String attribute, int id) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outValues = arena<Pointer<Double>>();
      final outCount = arena<Size>();

      final err = bindings.psr_database_read_set_doubles_by_id(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        attribute.toNativeUtf8(allocator: arena).cast(),
        id,
        outValues,
        outCount,
      );

      if (err != psr_error_t.PSR_OK) {
        throw DatabaseException.fromError(err, "Failed to read set doubles by id from '$collection.$attribute'");
      }

      final count = outCount.value;
      if (count == 0 || outValues.value == nullptr) {
        return [];
      }

      final result = List<double>.generate(count, (i) => outValues.value[i]);
      bindings.psr_free_double_array(outValues.value);
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  /// Reads string set for a set attribute by element ID.
  List<String> readSetStringsById(String collection, String attribute, int id) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outValues = arena<Pointer<Pointer<Char>>>();
      final outCount = arena<Size>();

      final err = bindings.psr_database_read_set_strings_by_id(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        attribute.toNativeUtf8(allocator: arena).cast(),
        id,
        outValues,
        outCount,
      );

      if (err != psr_error_t.PSR_OK) {
        throw DatabaseException.fromError(err, "Failed to read set strings by id from '$collection.$attribute'");
      }

      final count = outCount.value;
      if (count == 0 || outValues.value == nullptr) {
        return [];
      }

      final result = List<String>.generate(count, (i) => outValues.value[i].cast<Utf8>().toDartString());
      bindings.psr_free_string_array(outValues.value, count);
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  // Read element IDs

  /// Reads all element IDs from a collection.
  List<int> readElementIds(String collection) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outIds = arena<Pointer<Int64>>();
      final outCount = arena<Size>();

      final err = bindings.psr_database_read_element_ids(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        outIds,
        outCount,
      );

      if (err != psr_error_t.PSR_OK) {
        throw DatabaseException.fromError(err, "Failed to read element ids from '$collection'");
      }

      final count = outCount.value;
      if (count == 0 || outIds.value == nullptr) {
        return [];
      }

      final result = List<int>.generate(count, (i) => outIds.value[i]);
      bindings.psr_free_int_array(outIds.value);
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  /// Returns the structure and data type of an attribute.
  ({String structure, String dataType}) getAttributeType(String collection, String attribute) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outStructure = arena<Int32>();
      final outDataType = arena<Int32>();

      final err = bindings.psr_database_get_attribute_type(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        attribute.toNativeUtf8(allocator: arena).cast(),
        outStructure,
        outDataType,
      );

      if (err != psr_error_t.PSR_OK) {
        throw DatabaseException.fromError(err, "Failed to get attribute type for '$collection.$attribute'");
      }

      final structure = switch (outStructure.value) {
        0 => 'scalar',
        1 => 'vector',
        2 => 'set',
        _ => 'unknown',
      };

      final dataType = switch (outDataType.value) {
        0 => 'integer',
        1 => 'real',
        2 => 'text',
        _ => 'unknown',
      };

      return (structure: structure, dataType: dataType);
    } finally {
      arena.releaseAll();
    }
  }

  /// Deletes an element by ID from a collection.
  /// CASCADE DELETE handles cleanup of related vector/set tables.
  void deleteElementById(String collection, int id) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final err = bindings.psr_database_delete_element_by_id(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        id,
      );

      if (err != psr_error_t.PSR_OK) {
        throw DatabaseException.fromError(err, "Failed to delete element $id from '$collection'");
      }
    } finally {
      arena.releaseAll();
    }
  }

  /// Updates an element's scalar attributes by ID using a map of values.
  /// Note: Vector and set attributes are ignored in update operations.
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

  /// Updates an element's scalar attributes by ID using an Element builder.
  /// Note: Vector and set attributes are ignored in update operations.
  void updateElementFromBuilder(String collection, int id, Element element) {
    _ensureNotClosed();
    final arena = Arena();
    try {
      final err = bindings.psr_database_update_element(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        id,
        element.ptr.cast(),
      );
      if (err != psr_error_t.PSR_OK) {
        throw DatabaseException.fromError(err, "Failed to update element $id in '$collection'");
      }
    } finally {
      arena.releaseAll();
    }
  }

  /// Closes the database and frees native resources.
  void close() {
    if (_isClosed) return;
    bindings.psr_database_close(_ptr);
    _isClosed = true;
  }
}
