part of 'database.dart';

/// Query operations for Database.
extension DatabaseQuery on Database {
  /// Executes a SQL query and returns the first column of the first row as a String.
  /// Returns null if the query returns no rows.
  String? queryString(String sql) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outValue = arena<Pointer<Char>>();
      final outHasValue = arena<Int>();

      check(
        bindings.quiver_database_query_string(
          _ptr,
          sql.toNativeUtf8(allocator: arena).cast(),
          outValue,
          outHasValue,
        ),
      );

      if (outHasValue.value == 0 || outValue.value == nullptr) {
        return null;
      }

      final result = outValue.value.cast<Utf8>().toDartString();
      bindings.quiver_string_free(outValue.value);
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  /// Executes a SQL query and returns the first column of the first row as an int.
  /// Returns null if the query returns no rows.
  int? queryInteger(String sql) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outValue = arena<Int64>();
      final outHasValue = arena<Int>();

      check(
        bindings.quiver_database_query_integer(
          _ptr,
          sql.toNativeUtf8(allocator: arena).cast(),
          outValue,
          outHasValue,
        ),
      );

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
  double? queryFloat(String sql) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outValue = arena<Double>();
      final outHasValue = arena<Int>();

      check(
        bindings.quiver_database_query_float(
          _ptr,
          sql.toNativeUtf8(allocator: arena).cast(),
          outValue,
          outHasValue,
        ),
      );

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
  DateTime? queryDateTime(String sql) {
    final result = queryString(sql);
    if (result == null) return null;
    return stringToDateTime(result);
  }

  /// Executes a parameterized SQL query and returns the first column of the first row as a String.
  /// Returns null if the query returns no rows.
  /// Parameters can be int, double, String, or null.
  String? queryStringParams(String sql, List<Object?> params) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final nativeParams = _marshalParams(arena, params);
      final outValue = arena<Pointer<Char>>();
      final outHasValue = arena<Int>();

      check(
        bindings.quiver_database_query_string_params(
          _ptr,
          sql.toNativeUtf8(allocator: arena).cast(),
          nativeParams.types,
          nativeParams.values,
          params.length,
          outValue,
          outHasValue,
        ),
      );

      if (outHasValue.value == 0 || outValue.value == nullptr) {
        return null;
      }

      final result = outValue.value.cast<Utf8>().toDartString();
      bindings.quiver_string_free(outValue.value);
      return result;
    } finally {
      arena.releaseAll();
    }
  }

  /// Executes a parameterized SQL query and returns the first column of the first row as an int.
  /// Returns null if the query returns no rows.
  int? queryIntegerParams(String sql, List<Object?> params) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final nativeParams = _marshalParams(arena, params);
      final outValue = arena<Int64>();
      final outHasValue = arena<Int>();

      check(
        bindings.quiver_database_query_integer_params(
          _ptr,
          sql.toNativeUtf8(allocator: arena).cast(),
          nativeParams.types,
          nativeParams.values,
          params.length,
          outValue,
          outHasValue,
        ),
      );

      if (outHasValue.value == 0) {
        return null;
      }

      return outValue.value;
    } finally {
      arena.releaseAll();
    }
  }

  /// Executes a parameterized SQL query and returns the first column of the first row as a double.
  /// Returns null if the query returns no rows.
  double? queryFloatParams(String sql, List<Object?> params) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final nativeParams = _marshalParams(arena, params);
      final outValue = arena<Double>();
      final outHasValue = arena<Int>();

      check(
        bindings.quiver_database_query_float_params(
          _ptr,
          sql.toNativeUtf8(allocator: arena).cast(),
          nativeParams.types,
          nativeParams.values,
          params.length,
          outValue,
          outHasValue,
        ),
      );

      if (outHasValue.value == 0) {
        return null;
      }

      return outValue.value;
    } finally {
      arena.releaseAll();
    }
  }

  /// Executes a parameterized SQL query and returns the first column of the first row as a DateTime.
  /// Returns null if the query returns no rows.
  DateTime? queryDateTimeParams(String sql, List<Object?> params) {
    final result = queryStringParams(sql, params);
    if (result == null) return null;
    return stringToDateTime(result);
  }
}
