part of 'database.dart';

/// Query operations for Database.
extension DatabaseQuery on Database {
  /// Executes a SQL query and returns the first column of the first row as a String.
  /// Returns null if the query returns no rows.
  /// Optional positional [parameters] bind to `?` placeholders (int, double, String, or null).
  String? queryString(String sql, [List<Object?>? parameters]) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outValue = arena<Pointer<Char>>();
      final outHasValue = arena<Int>();

      if (parameters == null) {
        check(
          bindings.quiver_database_query_string(
            _ptr,
            sql.toNativeUtf8(allocator: arena).cast(),
            outValue,
            outHasValue,
          ),
        );
      } else {
        final nativeParams = _marshalParams(arena, parameters);
        check(
          bindings.quiver_database_query_string_params(
            _ptr,
            sql.toNativeUtf8(allocator: arena).cast(),
            nativeParams.types,
            nativeParams.values,
            parameters.length,
            outValue,
            outHasValue,
          ),
        );
      }

      if (outHasValue.value == 0 || outValue.value == nullptr) {
        return null;
      }

      final result = outValue.value.cast<Utf8>().toDartString();
      bindings.quiver_database_free_string(outValue.value);
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  /// Executes a SQL query and returns the first column of the first row as an int.
  /// Returns null if the query returns no rows.
  /// Optional positional [parameters] bind to `?` placeholders (int, double, String, or null).
  int? queryInteger(String sql, [List<Object?>? parameters]) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outValue = arena<Int64>();
      final outHasValue = arena<Int>();

      if (parameters == null) {
        check(
          bindings.quiver_database_query_integer(
            _ptr,
            sql.toNativeUtf8(allocator: arena).cast(),
            outValue,
            outHasValue,
          ),
        );
      } else {
        final nativeParams = _marshalParams(arena, parameters);
        check(
          bindings.quiver_database_query_integer_params(
            _ptr,
            sql.toNativeUtf8(allocator: arena).cast(),
            nativeParams.types,
            nativeParams.values,
            parameters.length,
            outValue,
            outHasValue,
          ),
        );
      }

      if (outHasValue.value == 0) {
        return null;
      }

      return outValue.value;
    } finally {
      arena.releaseAll();
    }
  }

  /// Executes a SQL query and returns the first column of the first row as a double.
  /// Returns null if the query returns no rows.
  /// Optional positional [parameters] bind to `?` placeholders (int, double, String, or null).
  double? queryFloat(String sql, [List<Object?>? parameters]) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outValue = arena<Double>();
      final outHasValue = arena<Int>();

      if (parameters == null) {
        check(
          bindings.quiver_database_query_float(
            _ptr,
            sql.toNativeUtf8(allocator: arena).cast(),
            outValue,
            outHasValue,
          ),
        );
      } else {
        final nativeParams = _marshalParams(arena, parameters);
        check(
          bindings.quiver_database_query_float_params(
            _ptr,
            sql.toNativeUtf8(allocator: arena).cast(),
            nativeParams.types,
            nativeParams.values,
            parameters.length,
            outValue,
            outHasValue,
          ),
        );
      }

      if (outHasValue.value == 0) {
        return null;
      }

      return outValue.value;
    } finally {
      arena.releaseAll();
    }
  }

  /// Executes a SQL query and returns the first column of the first row as a DateTime.
  /// Returns null if the query returns no rows.
  /// Optional positional [parameters] bind to `?` placeholders (int, double, String, or null).
  DateTime? queryDateTime(String sql, [List<Object?>? parameters]) {
    final result = queryString(sql, parameters);
    if (result == null) return null;
    return stringToDateTime(result);
  }
}
