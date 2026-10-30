import 'dart:ffi';

import 'package:ffi/ffi.dart';

import 'ffi/bindings.dart';
import 'ffi/library_loader.dart';
import 'date_time.dart';
import 'exceptions.dart';

/// A builder for creating database elements.
///
/// Elements are used to insert data into collections.
/// After use, call [dispose] to free native memory.
class Element {
  final Pointer<quiver_element_t> _ptr;
  bool _isDisposed = false;

  Element._(this._ptr);

  /// Creates a new empty element.
  factory Element() {
    final arena = Arena();
    try {
      final outElementPtr = arena<Pointer<quiver_element_t>>();
      check(bindings.quiver_element_create(outElementPtr));
      return Element._(outElementPtr.value);
    } finally {
      arena.releaseAll();
    }
  }

  /// Internal pointer for FFI calls.
  Pointer<quiver_element_t> get ptr {
    _ensureNotDisposed();
    return _ptr;
  }

  void _ensureNotDisposed() {
    if (_isDisposed) {
      throw StateError('Element has been disposed');
    }
  }

  /// Sets a value on this element.
  ///
  /// Supported types:
  /// - `null` - sets a null value
  /// - `int` - 64-bit integer
  /// - `bool` - stored as the integer 1 or 0
  /// - `double` - 64-bit floating point
  /// - `String` - UTF-8 string
  /// - `DateTime` - converted to ISO 8601 string
  /// - `List<int?>` - array of integers (a null cell is a SQL NULL)
  /// - `List<bool?>` - array of booleans (stored as integers)
  /// - `List<double?>` - array of floats
  /// - `List<String?>` - array of strings
  /// - `List<DateTime?>` - array of datetimes (converted to ISO 8601 strings)
  void set(String name, Object? value) {
    _ensureNotDisposed();

    switch (value) {
      case null:
        setNull(name);
      case bool v:
        setInteger(name, v ? 1 : 0);
      case int v:
        setInteger(name, v);
      case double v:
        setFloat(name, v);
      case String v:
        setString(name, v);
      case DateTime v:
        setDateTime(name, v);
      case List<int> v:
        setArrayInteger(name, v);
      case List<double> v:
        setArrayFloat(name, v);
      case List<String> v:
        setArrayString(name, v);
      case List<DateTime> v:
        setArrayString(name, v.map(dateTimeToString).toList());
      case List v:
        _setMixedList(name, v);
      default:
        throw ArgumentError(
          "Unsupported type ${value.runtimeType} for '$name'",
        );
    }
  }

  void _setMixedList(String name, List<Object?> values) {
    final arena = Arena();
    try {
      final cells = marshalCells(arena, values, "'$name'", 'array element');
      final namePtr = name.toNativeUtf8(allocator: arena).cast<Char>();
      check(switch (cells.type) {
        quiver_data_type_t.QUIVER_DATA_TYPE_INTEGER => bindings.quiver_element_set_array_integer(
          _ptr,
          namePtr,
          cells.data.cast(),
          values.length,
          cells.mask,
        ),
        quiver_data_type_t.QUIVER_DATA_TYPE_FLOAT => bindings.quiver_element_set_array_float(
          _ptr,
          namePtr,
          cells.data.cast(),
          values.length,
          cells.mask,
        ),
        _ => bindings.quiver_element_set_array_string(_ptr, namePtr, cells.data.cast(), values.length, cells.mask),
      });
    } finally {
      arena.releaseAll();
    }
  }

  /// Sets an integer value.
  void setInteger(String name, int value) {
    _ensureNotDisposed();
    final namePtr = name.toNativeUtf8();
    try {
      check(bindings.quiver_element_set_integer(_ptr, namePtr.cast(), value));
    } finally {
      malloc.free(namePtr);
    }
  }

  /// Sets a float value.
  void setFloat(String name, double value) {
    _ensureNotDisposed();
    final namePtr = name.toNativeUtf8();
    try {
      check(bindings.quiver_element_set_float(_ptr, namePtr.cast(), value));
    } finally {
      malloc.free(namePtr);
    }
  }

  /// Sets a string value.
  void setString(String name, String value) {
    _ensureNotDisposed();
    final namePtr = name.toNativeUtf8();
    final valuePtr = value.toNativeUtf8();
    try {
      check(
        bindings.quiver_element_set_string(
          _ptr,
          namePtr.cast(),
          valuePtr.cast(),
        ),
      );
    } finally {
      malloc.free(namePtr);
      malloc.free(valuePtr);
    }
  }

  /// Sets a DateTime value (converted to ISO 8601 string).
  void setDateTime(String name, DateTime value) {
    setString(name, dateTimeToString(value));
  }

  /// Sets a null value.
  void setNull(String name) {
    _ensureNotDisposed();
    final namePtr = name.toNativeUtf8();
    try {
      check(bindings.quiver_element_set_null(_ptr, namePtr.cast()));
    } finally {
      malloc.free(namePtr);
    }
  }

  /// Sets an array of integers. A null cell is stored as a SQL NULL.
  void setArrayInteger(String name, List<int?> values) {
    _ensureNotDisposed();
    final namePtr = name.toNativeUtf8();
    final arrayPtr = malloc<Int64>(values.length);
    final maskPtr = malloc<Uint8>(values.length);

    try {
      for (var i = 0; i < values.length; i++) {
        arrayPtr[i] = values[i] ?? 0;
        maskPtr[i] = values[i] == null ? 0 : 1;
      }
      check(
        bindings.quiver_element_set_array_integer(
          _ptr,
          namePtr.cast(),
          arrayPtr,
          values.length,
          maskPtr,
        ),
      );
    } finally {
      malloc.free(namePtr);
      malloc.free(arrayPtr);
      malloc.free(maskPtr);
    }
  }

  /// Sets an array of floats. A null cell is stored as a SQL NULL.
  void setArrayFloat(String name, List<double?> values) {
    _ensureNotDisposed();
    final namePtr = name.toNativeUtf8();
    final arrayPtr = malloc<Double>(values.length);
    final maskPtr = malloc<Uint8>(values.length);

    try {
      for (var i = 0; i < values.length; i++) {
        arrayPtr[i] = values[i] ?? 0.0;
        maskPtr[i] = values[i] == null ? 0 : 1;
      }
      check(
        bindings.quiver_element_set_array_float(
          _ptr,
          namePtr.cast(),
          arrayPtr,
          values.length,
          maskPtr,
        ),
      );
    } finally {
      malloc.free(namePtr);
      malloc.free(arrayPtr);
      malloc.free(maskPtr);
    }
  }

  /// Sets an array of strings. A null cell is stored as a SQL NULL.
  void setArrayString(String name, List<String?> values) {
    _ensureNotDisposed();
    final namePtr = name.toNativeUtf8();
    final stringPtrs = <Pointer<Utf8>>[];
    final arrayPtr = malloc<Pointer<Char>>(values.length);
    final maskPtr = malloc<Uint8>(values.length);

    try {
      for (var i = 0; i < values.length; i++) {
        final value = values[i];
        if (value == null) {
          arrayPtr[i] = nullptr;
          maskPtr[i] = 0;
        } else {
          final strPtr = value.toNativeUtf8();
          stringPtrs.add(strPtr);
          arrayPtr[i] = strPtr.cast();
          maskPtr[i] = 1;
        }
      }
      check(
        bindings.quiver_element_set_array_string(
          _ptr,
          namePtr.cast(),
          arrayPtr,
          values.length,
          maskPtr,
        ),
      );
    } finally {
      malloc.free(namePtr);
      for (final ptr in stringPtrs) {
        malloc.free(ptr);
      }
      malloc.free(arrayPtr);
      malloc.free(maskPtr);
    }
  }

  /// Clears all values from this element.
  void clear() {
    _ensureNotDisposed();
    check(bindings.quiver_element_clear(_ptr));
  }

  /// Frees the native memory associated with this element.
  void dispose() {
    if (_isDisposed) return;
    check(bindings.quiver_element_destroy(_ptr));
    _isDisposed = true;
  }

  @override
  String toString() {
    if (_isDisposed) return 'Element(disposed)';
    final arena = Arena();
    try {
      final outString = arena<Pointer<Char>>();
      check(bindings.quiver_element_to_string(_ptr, outString));
      final result = outString.value.cast<Utf8>().toDartString();
      bindings.quiver_database_free_string(outString.value);
      return result;
    } finally {
      arena.releaseAll();
    }
  }
}

/// Marshals one array of cells into [arena]-owned memory for the C API's array writers (the
/// element array setters and the vector/set/time-series group writers), returning its
/// quiver_data_type_t tag, data and per-cell presence mask (0 = SQL NULL, data slot a placeholder).
/// [owner] names the array in errors (`'attr'` or `column 'c'`), [noun] the unsupported-family one.
///
/// The first non-null cell picks the family and every cell the numeric type: a numeric array is
/// INTEGER unless some cell is a double, which widens it to FLOAT (so `[1, 2.5]` writes 1.0 and 2.5,
/// as in the other bindings). An empty or all-null array is INTEGER: the C API ignores the type and
/// data of a masked-out cell, and an empty one is passed as NULL arrays, never `arena<T>(0)`.
/// SQLite has no boolean type, so a bool is INTEGER 1/0 and reaches a REAL column, like an int, by
/// the int-for-REAL coercion. Every cell is converted rather than the list `cast`, which would defer
/// the check to iteration and throw a raw TypeError naming neither the array nor the cell.
({int type, Pointer<Void> data, Pointer<Uint8> mask}) marshalCells(
  Arena arena,
  List<Object?> values,
  String owner,
  String noun,
) {
  if (values.isEmpty) {
    return (type: quiver_data_type_t.QUIVER_DATA_TYPE_INTEGER, data: nullptr, mask: nullptr);
  }
  Never unsupported(Object? v, int i) =>
      throw ArgumentError("Unsupported value type ${v.runtimeType} in cell $i of $owner");

  final mask = arena<Uint8>(values.length);
  for (var i = 0; i < values.length; i++) {
    mask[i] = values[i] == null ? 0 : 1;
  }
  // A plain loop, not firstWhere(orElse: () => null), which throws on a non-nullable list.
  Object? first;
  for (final v in values) {
    if (v != null) {
      first = v;
      break;
    }
  }

  if (first == null || first is bool || first is int || first is double) {
    if (!values.any((v) => v is double)) {
      final data = arena<Int64>(values.length);
      for (var i = 0; i < values.length; i++) {
        data[i] = switch (values[i]) {
          null => 0,
          final bool b => b ? 1 : 0,
          final int v => v,
          final v => unsupported(v, i),
        };
      }
      return (type: quiver_data_type_t.QUIVER_DATA_TYPE_INTEGER, data: data.cast(), mask: mask);
    }
    final data = arena<Double>(values.length);
    for (var i = 0; i < values.length; i++) {
      data[i] = switch (values[i]) {
        null => 0.0,
        final double d => d,
        final int v => v.toDouble(),
        final bool b => b ? 1.0 : 0.0,
        final v => unsupported(v, i),
      };
    }
    return (type: quiver_data_type_t.QUIVER_DATA_TYPE_FLOAT, data: data.cast(), mask: mask);
  }
  if (first is String || first is DateTime) {
    final data = arena<Pointer<Char>>(values.length);
    for (var i = 0; i < values.length; i++) {
      data[i] = switch (values[i]) {
        null => nullptr,
        final String t when first is String => t.toNativeUtf8(allocator: arena).cast(),
        final DateTime d when first is DateTime => dateTimeToString(d).toNativeUtf8(allocator: arena).cast(),
        final v => unsupported(v, i),
      };
    }
    return (type: quiver_data_type_t.QUIVER_DATA_TYPE_STRING, data: data.cast(), mask: mask);
  }
  throw ArgumentError("Unsupported $noun type ${first.runtimeType} for $owner");
}
