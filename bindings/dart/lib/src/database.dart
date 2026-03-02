import 'dart:ffi';

import 'package:ffi/ffi.dart';

import 'ffi/bindings.dart';
import 'ffi/library_loader.dart';
import 'date_time.dart';
import 'element.dart';
import 'exceptions.dart';
import 'metadata.dart';

part 'database_create.dart';
part 'database_options.dart';
part 'database_csv_export.dart';
part 'database_csv_import.dart';
part 'database_delete.dart';
part 'database_transaction.dart';
part 'database_metadata.dart';
part 'database_query.dart';
part 'database_read.dart';
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

      check(
        bindings.quiver_database_from_schema(
          dbPath.toNativeUtf8(allocator: arena).cast(),
          schemaPath.toNativeUtf8(allocator: arena).cast(),
          optionsPtr,
          outDbPtr,
        ),
      );

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

      check(
        bindings.quiver_database_from_migrations(
          dbPath.toNativeUtf8(allocator: arena).cast(),
          migrationsPath.toNativeUtf8(allocator: arena).cast(),
          optionsPtr,
          outDbPtr,
        ),
      );

      return Database._(outDbPtr.value);
    } finally {
      arena.releaseAll();
    }
  }

  void _ensureNotClosed() {
    if (_isClosed) {
      throw StateError('Database has been closed');
    }
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

  /// Returns true if the database passes integrity checks.
  bool isHealthy() {
    _ensureNotClosed();
    final arena = Arena();
    try {
      final outHealthy = arena<Int>();
      check(bindings.quiver_database_is_healthy(_ptr, outHealthy));
      return outHealthy.value != 0;
    } finally {
      arena.releaseAll();
    }
  }

  /// Returns the database file path.
  String path() {
    _ensureNotClosed();
    final arena = Arena();
    try {
      final outPath = arena<Pointer<Char>>();
      check(bindings.quiver_database_path(_ptr, outPath));
      return outPath.value.cast<Utf8>().toDartString();
    } finally {
      arena.releaseAll();
    }
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
