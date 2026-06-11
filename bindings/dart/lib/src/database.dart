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

  static Pointer<quiver_database_options_t> _makeOptions(
    Arena arena,
    bool readOnly,
    int? consoleLevel,
  ) {
    final optionsPtr = arena<quiver_database_options_t>();
    optionsPtr.ref = bindings.quiver_database_options_default();
    optionsPtr.ref.read_only = readOnly ? 1 : 0;
    if (consoleLevel != null) {
      optionsPtr.ref.console_level = consoleLevel;
    }
    return optionsPtr;
  }

  /// Creates a new database from a SQL schema file.
  ///
  /// [consoleLevel] takes a `quiver_log_level_t` constant (e.g.
  /// `quiver_log_level_t.QUIVER_LOG_OFF`).
  factory Database.fromSchema(
    String dbPath,
    String schemaPath, {
    bool readOnly = false,
    int? consoleLevel,
  }) {
    final arena = Arena();
    try {
      final optionsPtr = _makeOptions(arena, readOnly, consoleLevel);
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
  factory Database.fromMigrations(
    String dbPath,
    String migrationsPath, {
    bool readOnly = false,
    int? consoleLevel,
  }) {
    final arena = Arena();
    try {
      final optionsPtr = _makeOptions(arena, readOnly, consoleLevel);
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

  /// Opens an existing database file.
  factory Database.open(
    String dbPath, {
    bool readOnly = false,
    int? consoleLevel,
  }) {
    final arena = Arena();
    try {
      final optionsPtr = _makeOptions(arena, readOnly, consoleLevel);
      final outDbPtr = arena<Pointer<quiver_database_t>>();

      check(
        bindings.quiver_database_open(
          dbPath.toNativeUtf8(allocator: arena).cast(),
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
    List<Object?> parameters,
  ) {
    final types = arena<Int>(parameters.length);
    final values = arena<Pointer<Void>>(parameters.length);

    for (var i = 0; i < parameters.length; i++) {
      final p = parameters[i];
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
