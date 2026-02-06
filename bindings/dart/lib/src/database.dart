import 'dart:ffi';

import 'package:ffi/ffi.dart';

import 'ffi/bindings.dart';
import 'ffi/library_loader.dart';
import 'date_time.dart';
import 'element.dart';
import 'exceptions.dart';

part 'database_create.dart';
part 'database_csv.dart';
part 'database_delete.dart';
part 'database_metadata.dart';
part 'database_query.dart';
part 'database_read.dart';
part 'database_relations.dart';
part 'database_update.dart';

/// A wrapper for the Quiver database.
///
/// Use [Database.fromSchema] to create a new database from a SQL schema file.
/// After use, call [close] to free native resources.
class Database {
  Pointer<quiver_database_t> _ptr;
  bool _isClosed = false;

  Database._(this._ptr);

  /// Internal: Returns the native pointer for use by other library components.
  Pointer<quiver_database_t> get ptr => _ptr;

  /// Creates a new database from a SQL schema file.
  factory Database.fromSchema(String dbPath, String schemaPath) {
    final arena = Arena();
    try {
      final optionsPtr = arena<quiver_database_options_t>();
      optionsPtr.ref = bindings.quiver_database_options_default();
      final outDbPtr = arena<Pointer<quiver_database_t>>();

      final err = bindings.quiver_database_from_schema(
        dbPath.toNativeUtf8(allocator: arena).cast(),
        schemaPath.toNativeUtf8(allocator: arena).cast(),
        optionsPtr,
        outDbPtr,
      );

      if (err != quiver_error_t.QUIVER_OK) {
        final errorPtr = bindings.quiver_get_last_error();
        final errorMsg = errorPtr.cast<Utf8>().toDartString();
        throw DatabaseException(
          errorMsg.isNotEmpty ? errorMsg : 'Failed to create database from schema',
        );
      }

      return Database._(outDbPtr.value);
    } finally {
      arena.releaseAll();
    }
  }

  /// Creates a new database from a migrations directory.
  /// The migrations directory should contain numbered subdirectories (1/, 2/, etc.)
  /// each with an up.sql file.
  factory Database.fromMigrations(String dbPath, String migrationsPath) {
    final arena = Arena();
    try {
      final optionsPtr = arena<quiver_database_options_t>();
      optionsPtr.ref = bindings.quiver_database_options_default();
      final outDbPtr = arena<Pointer<quiver_database_t>>();

      final err = bindings.quiver_database_from_migrations(
        dbPath.toNativeUtf8(allocator: arena).cast(),
        migrationsPath.toNativeUtf8(allocator: arena).cast(),
        optionsPtr,
        outDbPtr,
      );

      if (err != quiver_error_t.QUIVER_OK) {
        final errorPtr = bindings.quiver_get_last_error();
        final errorMsg = errorPtr.cast<Utf8>().toDartString();
        throw DatabaseException(
          errorMsg.isNotEmpty ? errorMsg : 'Failed to create database from migrations',
        );
      }

      return Database._(outDbPtr.value);
    } finally {
      arena.releaseAll();
    }
  }

  void _ensureNotClosed() {
    if (_isClosed) {
      throw const DatabaseException('Database has been closed');
    }
  }

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
  _parseScalarMetadata(
    quiver_scalar_metadata_t attribute,
  ) {
    return (
      name: attribute.name.cast<Utf8>().toDartString(),
      dataType: attribute.data_type,
      notNull: attribute.not_null != 0,
      primaryKey: attribute.primary_key != 0,
      defaultValue: attribute.default_value == nullptr ? null : attribute.default_value.cast<Utf8>().toDartString(),
      isForeignKey: attribute.is_foreign_key != 0,
      referencesCollection: attribute.references_collection == nullptr
          ? null
          : attribute.references_collection.cast<Utf8>().toDartString(),
      referencesColumn: attribute.references_column == nullptr
          ? null
          : attribute.references_column.cast<Utf8>().toDartString(),
    );
  }

  int _getValueDataType(
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
  ) {
    if (valueColumns.isNotEmpty) {
      return valueColumns.first.dataType;
    }
    return quiver_data_type_t.QUIVER_DATA_TYPE_STRING;
  }

  ({Pointer<Int> types, Pointer<Pointer<Void>> values}) _marshalParams(
    Arena arena,
    List<Object?> params,
  ) {
    final types = arena<Int>(params.length);
    final values = arena<Pointer<Void>>(params.length);

    for (var i = 0; i < params.length; i++) {
      final p = params[i];
      if (p == null) {
        types[i] = quiver_data_type_t.QUIVER_DATA_TYPE_NULL;
        values[i] = nullptr;
      } else if (p is int) {
        types[i] = quiver_data_type_t.QUIVER_DATA_TYPE_INTEGER;
        final ptr = arena<Int64>();
        ptr.value = p;
        values[i] = ptr.cast();
      } else if (p is double) {
        types[i] = quiver_data_type_t.QUIVER_DATA_TYPE_FLOAT;
        final ptr = arena<Double>();
        ptr.value = p;
        values[i] = ptr.cast();
      } else if (p is String) {
        types[i] = quiver_data_type_t.QUIVER_DATA_TYPE_STRING;
        values[i] = p.toNativeUtf8(allocator: arena).cast();
      } else {
        throw ArgumentError('Unsupported parameter type: ${p.runtimeType}');
      }
    }

    return (types: types, values: values);
  }

  /// Closes the database and frees native resources.
  void close() {
    if (_isClosed) return;
    bindings.quiver_database_close(_ptr);
    _isClosed = true;
  }

  /// Prints schema information to stdout.
  void describe() {
    _ensureNotClosed();
    check(bindings.quiver_database_describe(_ptr));
  }
}
