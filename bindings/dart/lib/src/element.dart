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
  final Pointer<quiver_element_t1> _ptr;
  bool _isDisposed = false;

  Element._(this._ptr);

  /// Creates a new empty element.
  factory Element() {
    final arena = Arena();
    try {
      final outElementPtr = arena<Pointer<quiver_element_t1>>();
      check(bindings.quiver_element_create(outElementPtr));
      return Element._(outElementPtr.value);
    } finally {
      arena.releaseAll();
    }
  }

  /// Internal pointer for FFI calls.
  Pointer<quiver_element_t1> get ptr {
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
  /// - `Map<String, Object?>` - recursively sets each entry as a separate attribute
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
      case Map<String, Object?> v:
        for (final entry in v.entries) {
          set(entry.key, entry.value);
        }
      case List v:
        _setMixedList(name, v);
      default:
        throw ArgumentError(
          "Unsupported type ${value.runtimeType} for '$name'",
        );
    }
  }

  void _setMixedList(String name, List<dynamic> values) {
    // Dispatch on the first non-null element; an empty or all-null list is
    // tagged integer (the type is irrelevant when every cell is NULL). Each branch then converts
    // per cell rather than `cast`ing the list, which defers the check to iteration and throws a
    // raw TypeError naming neither the attribute nor the cell. A bool is INTEGER 1/0 and an int
    // reaches a REAL column by the int-for-REAL coercion — the rules `_marshalGroupColumn` applies.
    Object? first;
    for (final v in values) {
      if (v != null) {
        first = v;
        break;
      }
    }
    if (first == null) {
      setArrayInteger(name, List<int?>.filled(values.length, null));
    } else if (first is bool || first is int) {
      setArrayInteger(name, [
        for (var i = 0; i < values.length; i++)
          switch (values[i]) {
            null => null,
            final bool b => b ? 1 : 0,
            final int v => v,
            final v => throw ArgumentError(
              "Unsupported value type ${v.runtimeType} in cell $i of '$name'",
            ),
          },
      ]);
    } else if (first is double) {
      setArrayFloat(name, [
        for (var i = 0; i < values.length; i++)
          switch (values[i]) {
            null => null,
            final double d => d,
            final int v => v.toDouble(),
            final bool b => b ? 1.0 : 0.0,
            final v => throw ArgumentError(
              "Unsupported value type ${v.runtimeType} in cell $i of '$name'",
            ),
          },
      ]);
    } else if (first is String) {
      setArrayString(name, [
        for (var i = 0; i < values.length; i++)
          switch (values[i]) {
            null => null,
            final String t => t,
            final v => throw ArgumentError(
              "Unsupported value type ${v.runtimeType} in cell $i of '$name'",
            ),
          },
      ]);
    } else if (first is DateTime) {
      setArrayString(name, [
        for (var i = 0; i < values.length; i++)
          switch (values[i]) {
            null => null,
            final DateTime d => dateTimeToString(d),
            final v => throw ArgumentError(
              "Unsupported value type ${v.runtimeType} in cell $i of '$name'",
            ),
          },
      ]);
    } else {
      throw ArgumentError(
        "Unsupported array element type ${first.runtimeType} for '$name'",
      );
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
