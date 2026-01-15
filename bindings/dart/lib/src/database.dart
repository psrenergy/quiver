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

  /// Reads all integer values for a scalar attribute from a collection.
  List<int> readScalarInts(String collection, String attribute) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outValues = arena<Pointer<Int64>>();
      final outCount = arena<Size>();

      final err = bindings.psr_database_read_scalar_ints(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        attribute.toNativeUtf8(allocator: arena).cast(),
        outValues,
        outCount,
      );

      if (err != psr_error_t.PSR_OK) {
        throw DatabaseException.fromError(err, "Failed to read scalar ints from '$collection.$attribute'");
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
  List<List<int>> readVectorInts(String collection, String attribute) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outVectors = arena<Pointer<Pointer<Int64>>>();
      final outSizes = arena<Pointer<Size>>();
      final outCount = arena<Size>();

      final err = bindings.psr_database_read_vector_ints(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        attribute.toNativeUtf8(allocator: arena).cast(),
        outVectors,
        outSizes,
        outCount,
      );

      if (err != psr_error_t.PSR_OK) {
        throw DatabaseException.fromError(err, "Failed to read vector ints from '$collection.$attribute'");
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

  /// Closes the database and frees native resources.
  void close() {
    if (_isClosed) return;
    bindings.psr_database_close(_ptr);
    _isClosed = true;
  }
}
