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
  ///
  /// - [dbPath] - Path to the SQLite database file.
  /// - [schemaPath] - Path to the SQL schema file.
  ///
  /// Throws [SchemaException] if the schema is invalid.
  /// Throws [DatabaseOperationException] if the database cannot be created.
  factory Database.fromSchema(String dbPath, String schemaPath) {
    final dbPathPtr = dbPath.toNativeUtf8();
    final schemaPathPtr = schemaPath.toNativeUtf8();
    final optionsPtr = calloc<psr_database_options_t>();

    try {
      // Get default options
      optionsPtr.ref = bindings.psr_database_options_default();

      final ptr = bindings.psr_database_from_schema(
        dbPathPtr.cast(),
        schemaPathPtr.cast(),
        optionsPtr,
      );

      if (ptr == nullptr) {
        throw const SchemaException('Failed to create database from schema');
      }

      return Database._(ptr);
    } finally {
      malloc.free(dbPathPtr);
      malloc.free(schemaPathPtr);
      calloc.free(optionsPtr);
    }
  }

  void _ensureNotClosed() {
    if (_isClosed) {
      throw const DatabaseOperationException('Database has been closed');
    }
  }

  /// Creates a new element in the specified collection.
  ///
  /// - [collection] - The name of the collection.
  /// - [values] - A map of attribute names to values.
  ///
  /// Returns the ID of the created element.
  ///
  /// Supported value types:
  /// - `null` - null value
  /// - `int` - 64-bit integer
  /// - `double` - 64-bit floating point
  /// - `String` - UTF-8 string
  /// - `List<int>` - array of integers
  /// - `List<double>` - array of doubles
  /// - `List<String>` - array of strings
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
  ///
  /// - [collection] - The name of the collection.
  /// - [element] - The Element builder containing the values.
  ///
  /// Returns the ID of the created element.
  ///
  /// Note: The caller is responsible for disposing the Element after use.
  int createElementFromBuilder(String collection, Element element) {
    _ensureNotClosed();

    final collectionPtr = collection.toNativeUtf8();
    try {
      final id = bindings.psr_database_create_element(
        _ptr,
        collectionPtr.cast(),
        element.ptr.cast(),
      );

      if (id < 0) {
        throw DatabaseException.fromError(
          id,
          "Failed to create element in '$collection'",
        );
      }

      return id;
    } finally {
      malloc.free(collectionPtr);
    }
  }

  /// Closes the database and frees native resources.
  ///
  /// After calling this method, the database cannot be used.
  void close() {
    if (_isClosed) return;
    bindings.psr_database_close(_ptr);
    _isClosed = true;
  }
}
