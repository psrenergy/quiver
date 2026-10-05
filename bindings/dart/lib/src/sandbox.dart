import 'dart:ffi';

import 'package:ffi/ffi.dart';

import 'ffi/bindings.dart';
import 'ffi/library_loader.dart';
import 'database.dart';
import 'exceptions.dart';

/// A sandbox runner for executing scripts with access to a Quiver.
///
/// Use [Sandbox] to execute scripts that can interact with the database
/// via the `db` global object exposed to the script.
///
/// Example:
/// ```dart
/// final db = Database.fromSchema(':memory:', 'schema.sql');
/// final sandbox = Sandbox(db);
/// sandbox.run('''
///   db:create_element("Collection", { label = "Item 1" })
/// ''');
/// sandbox.dispose();
/// db.close();
/// ```
class Sandbox {
  Pointer<quiver_sandbox_t> _ptr;
  bool _isDisposed = false;

  /// Creates a new Sandbox for the given database.
  Sandbox(Database db) : _ptr = nullptr {
    final arena = Arena();
    try {
      final outRunnerPtr = arena<Pointer<quiver_sandbox_t>>();
      check(bindings.quiver_sandbox_new(db.ptr, outRunnerPtr));
      _ptr = outRunnerPtr.value;
    } finally {
      arena.releaseAll();
    }
  }

  void _ensureNotDisposed() {
    if (_isDisposed) {
      throw StateError('Sandbox has been disposed');
    }
  }

  /// Runs a script.
  ///
  /// The script has access to the database via the global `db` object.
  /// Available methods:
  /// - `db:create_element(collection, values)` - Create an element
  /// - `db:read_scalar_strings(collection, attribute)` - Read string scalars
  ///
  /// Returns the script's return value encoded as JSON, or `''` if it returned nothing.
  /// To execute a script without keeping its writes, wrap the call in [Database.dryRun].
  ///
  /// Throws [SandboxException] if the script fails to execute.
  String run(String script) {
    _ensureNotDisposed();

    final arena = Arena();
    try {
      final outResult = arena<Pointer<Char>>();
      final err = bindings.quiver_sandbox_run(
        _ptr,
        script.toNativeUtf8(allocator: arena).cast(),
        outResult,
      );

      if (err != quiver_error_t.QUIVER_OK) {
        final detail = bindings.quiver_get_last_error().cast<Utf8>().toDartString();
        throw SandboxException(detail);
      }

      // The result is C-heap allocated, so the Arena cannot own it: free it in its own finally so
      // a decode failure cannot leak it.
      final resultPtr = outResult.value;
      try {
        return resultPtr.cast<Utf8>().toDartString();
      } finally {
        bindings.quiver_sandbox_free_string(resultPtr);
      }
    } finally {
      arena.releaseAll();
    }
  }

  /// Disposes the Sandbox and frees native resources.
  void dispose() {
    if (_isDisposed) return;
    bindings.quiver_sandbox_free(_ptr);
    _isDisposed = true;
  }
}
