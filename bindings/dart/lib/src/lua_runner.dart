import 'dart:ffi';

import 'package:ffi/ffi.dart';

import 'ffi/bindings.dart';
import 'ffi/library_loader.dart';
import 'database.dart';
import 'exceptions.dart';

/// A Lua script runner for executing Lua scripts with access to a PSR Database.
///
/// Use [LuaRunner] to execute Lua scripts that can interact with the database
/// via the `db` global object exposed to the script.
///
/// Example:
/// ```dart
/// final db = Database.fromSchema(':memory:', 'schema.sql');
/// final lua = LuaRunner(db);
/// lua.run('''
///   db:create_element("Collection", { label = "Item 1" })
/// ''');
/// lua.dispose();
/// db.close();
/// ```
class LuaRunner {
  Pointer<psr_lua_runner_t> _ptr;
  bool _isDisposed = false;

  /// Creates a new LuaRunner for the given database.
  LuaRunner(Database db) : _ptr = nullptr {
    _ptr = bindings.psr_lua_runner_new(db.ptr);
    if (_ptr == nullptr) {
      throw const DatabaseOperationException('Failed to create LuaRunner');
    }
  }

  void _ensureNotDisposed() {
    if (_isDisposed) {
      throw const DatabaseOperationException('LuaRunner has been disposed');
    }
  }

  /// Runs a Lua script.
  ///
  /// The script has access to the database via the global `db` object.
  /// Available methods:
  /// - `db:create_element(collection, values)` - Create an element
  /// - `db:read_scalar_strings(collection, attribute)` - Read string scalars
  ///
  /// Throws [LuaException] if the script fails to execute.
  void run(String script) {
    _ensureNotDisposed();

    final arena = Arena();
    try {
      final err = bindings.psr_lua_runner_run(
        _ptr,
        script.toNativeUtf8(allocator: arena).cast(),
      );

      if (err != margaux_error_t.MARGAUX_OK) {
        final errorPtr = bindings.psr_lua_runner_get_error(_ptr);
        if (errorPtr != nullptr) {
          final errorMsg = errorPtr.cast<Utf8>().toDartString();
          throw LuaException('Lua error: $errorMsg');
        } else {
          throw const LuaException('Lua script execution failed');
        }
      }
    } finally {
      arena.releaseAll();
    }
  }

  /// Disposes the LuaRunner and frees native resources.
  void dispose() {
    if (_isDisposed) return;
    bindings.psr_lua_runner_free(_ptr);
    _isDisposed = true;
  }
}
