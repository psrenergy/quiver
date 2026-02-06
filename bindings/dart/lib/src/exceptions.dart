import 'package:ffi/ffi.dart';

import 'ffi/library_loader.dart';

/// Check for C API errors and throw the appropriate DatabaseException.
void check(int err) {
  if (err != 0) {
    final detail = bindings.quiver_get_last_error().cast<Utf8>().toDartString();
    if (detail.isEmpty) {
      print('WARNING check: C API returned error but quiver_get_last_error() is empty');
      throw const DatabaseException('Unknown error');
    }
    throw DatabaseException(detail);
  }
}

/// Base exception for Quiver errors.
class DatabaseException implements Exception {
  final String message;
  const DatabaseException(this.message);

  @override
  String toString() => 'DatabaseException: $message';
}

class LuaException extends DatabaseException {
  const LuaException(super.message);
}
