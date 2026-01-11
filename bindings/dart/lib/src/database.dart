import 'dart:ffi';

import 'package:ffi/ffi.dart';

import 'ffi/bindings.dart';
import 'ffi/library_loader.dart';
import 'element.dart';
import 'exceptions.dart';

/// Extracts a Dart value from a native psr_value_t pointer.
Object? _extractValue(Pointer<psr_value_t> ptr, {Object? defaultValue}) {
  final value = ptr.ref;
  switch (value.type) {
    case psr_value_type_t.PSR_VALUE_NULL:
      return defaultValue;
    case psr_value_type_t.PSR_VALUE_INT64:
      return value.data.int_value;
    case psr_value_type_t.PSR_VALUE_DOUBLE:
      final d = value.data.double_value;
      return d.isNaN && defaultValue != null ? defaultValue : d;
    case psr_value_type_t.PSR_VALUE_STRING:
      return value.data.string_value.cast<Utf8>().toDartString();
    case psr_value_type_t.PSR_VALUE_ARRAY:
      final array = value.data.array_value;
      final count = array.count;
      final elements = array.elements;
      return List.generate(count, (i) {
        final elemPtr = Pointer<psr_value_t>.fromAddress(
          elements.address + i * sizeOf<psr_value_t>(),
        );
        return _extractValue(elemPtr, defaultValue: defaultValue);
      });
    default:
      return defaultValue;
  }
}

/// Extracts a value at a specific index from a psr_value_t array.
Object? _extractValueAtIndex(
  Pointer<psr_value_t> valuesPtr,
  int index, {
  Object? defaultValue,
}) {
  final ptr = Pointer<psr_value_t>.fromAddress(
    valuesPtr.address + index * sizeOf<psr_value_t>(),
  );
  return _extractValue(ptr, defaultValue: defaultValue);
}

/// Frees a read result.
void _freeResult(psr_read_result_t result) {
  final ptr = calloc<psr_read_result_t>()..ref = result;
  bindings.psr_read_result_free(ptr);
  calloc.free(ptr);
}

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

  /// Throws if result has an error.
  void _checkResult(psr_read_result_t result, String context) {
    if (result.error != 0) {
      throw DatabaseException.fromError(result.error, context);
    }
  }

  /// Extracts all values from a read result and frees it.
  List<Object?> _extractResults(psr_read_result_t result, Object? defaultValue) {
    try {
      return List.generate(
        result.count,
        (i) => _extractValueAtIndex(result.values, i, defaultValue: defaultValue),
      );
    } finally {
      _freeResult(result);
    }
  }

  /// Extracts a single value from a read result and frees it.
  Object? _extractSingleResult(psr_read_result_t result, Object? defaultValue) {
    try {
      return _extractValue(result.values, defaultValue: defaultValue);
    } finally {
      _freeResult(result);
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

  /// Closes the database and frees native resources.
  void close() {
    if (_isClosed) return;
    bindings.psr_database_close(_ptr);
    _isClosed = true;
  }

  // ============================================================
  // Read Operations
  // ============================================================

  /// Reads all values of a scalar attribute from a collection.
  List<Object?> readScalarParameters(String collection, String attribute, {Object? defaultValue}) {
    _ensureNotClosed();
    final arena = Arena();
    try {
      final result = bindings.psr_database_read_scalar_parameters(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        attribute.toNativeUtf8(allocator: arena).cast(),
      );
      _checkResult(result, "Failed to read scalar parameters '$attribute' from '$collection'");
      return _extractResults(result, defaultValue);
    } finally {
      arena.releaseAll();
    }
  }

  /// Reads a scalar attribute value for a specific element by label.
  Object? readScalarParameter(String collection, String attribute, String label, {Object? defaultValue}) {
    _ensureNotClosed();
    final arena = Arena();
    try {
      final result = bindings.psr_database_read_scalar_parameter(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        attribute.toNativeUtf8(allocator: arena).cast(),
        label.toNativeUtf8(allocator: arena).cast(),
      );
      _checkResult(result, "Failed to read scalar parameter '$attribute' for '$label' from '$collection'");
      return _extractSingleResult(result, defaultValue);
    } finally {
      arena.releaseAll();
    }
  }

  /// Reads all values of a vector attribute from a collection.
  List<List<Object?>> readVectorParameters(String collection, String attribute, {Object? defaultValue}) {
    _ensureNotClosed();
    final arena = Arena();
    try {
      final result = bindings.psr_database_read_vector_parameters(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        attribute.toNativeUtf8(allocator: arena).cast(),
      );
      _checkResult(result, "Failed to read vector parameters '$attribute' from '$collection'");
      return _extractResults(result, defaultValue).map((v) => (v as List<Object?>?) ?? []).toList();
    } finally {
      arena.releaseAll();
    }
  }

  /// Reads a vector attribute value for a specific element by label.
  List<Object?> readVectorParameter(String collection, String attribute, String label, {Object? defaultValue}) {
    _ensureNotClosed();
    final arena = Arena();
    try {
      final result = bindings.psr_database_read_vector_parameter(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        attribute.toNativeUtf8(allocator: arena).cast(),
        label.toNativeUtf8(allocator: arena).cast(),
      );
      _checkResult(result, "Failed to read vector parameter '$attribute' for '$label' from '$collection'");
      final value = _extractSingleResult(result, defaultValue);
      return (value as List<Object?>?) ?? [];
    } finally {
      arena.releaseAll();
    }
  }

  /// Reads all values of a set attribute from a collection.
  List<List<Object?>> readSetParameters(String collection, String attribute, {Object? defaultValue}) {
    _ensureNotClosed();
    final arena = Arena();
    try {
      final result = bindings.psr_database_read_set_parameters(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        attribute.toNativeUtf8(allocator: arena).cast(),
      );
      _checkResult(result, "Failed to read set parameters '$attribute' from '$collection'");
      return _extractResults(result, defaultValue).map((v) => (v as List<Object?>?) ?? []).toList();
    } finally {
      arena.releaseAll();
    }
  }

  /// Reads a set attribute value for a specific element by label.
  List<Object?> readSetParameter(String collection, String attribute, String label, {Object? defaultValue}) {
    _ensureNotClosed();
    final arena = Arena();
    try {
      final result = bindings.psr_database_read_set_parameter(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        attribute.toNativeUtf8(allocator: arena).cast(),
        label.toNativeUtf8(allocator: arena).cast(),
      );
      _checkResult(result, "Failed to read set parameter '$attribute' for '$label' from '$collection'");
      final value = _extractSingleResult(result, defaultValue);
      return (value as List<Object?>?) ?? [];
    } finally {
      arena.releaseAll();
    }
  }
}
