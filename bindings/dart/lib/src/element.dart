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
      throw const DatabaseException('Element has been disposed');
    }
  }

  /// Sets a value on this element.
  ///
  /// Supported types:
  /// - `null` - sets a null value
  /// - `int` - 64-bit integer
  /// - `double` - 64-bit floating point
  /// - `String` - UTF-8 string
  /// - `List<int>` - array of integers
  /// - `List<double>` - array of floats
  /// - `List<String>` - array of strings
  void set(String name, Object? value) {
    _ensureNotDisposed();

    switch (value) {
      case null:
        setNull(name);
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
      case List v when v.isEmpty:
        throw DatabaseException("Empty list not allowed for '$name'");
      case List v:
        _setMixedList(name, v);
      default:
        throw DatabaseException(
          "Unsupported type ${value.runtimeType} for '$name'",
        );
    }
  }

  void _setMixedList(String name, List<dynamic> values) {
    final first = values.first;
    if (first is int) {
      setArrayInteger(name, values.cast<int>());
    } else if (first is double) {
      setArrayFloat(name, values.cast<double>());
    } else if (first is String) {
      setArrayString(name, values.cast<String>());
    } else {
      throw DatabaseException(
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

  /// Sets an array of integers.
  void setArrayInteger(String name, List<int> values) {
    _ensureNotDisposed();
    final namePtr = name.toNativeUtf8();
    final arrayPtr = malloc<Int64>(values.length);

    try {
      for (var i = 0; i < values.length; i++) {
        arrayPtr[i] = values[i];
      }
      check(
        bindings.quiver_element_set_array_integer(
          _ptr,
          namePtr.cast(),
          arrayPtr,
          values.length,
        ),
      );
    } finally {
      malloc.free(namePtr);
      malloc.free(arrayPtr);
    }
  }

  /// Sets an array of floats.
  void setArrayFloat(String name, List<double> values) {
    _ensureNotDisposed();
    final namePtr = name.toNativeUtf8();
    final arrayPtr = malloc<Double>(values.length);

    try {
      for (var i = 0; i < values.length; i++) {
        arrayPtr[i] = values[i];
      }
      check(
        bindings.quiver_element_set_array_float(
          _ptr,
          namePtr.cast(),
          arrayPtr,
          values.length,
        ),
      );
    } finally {
      malloc.free(namePtr);
      malloc.free(arrayPtr);
    }
  }

  /// Sets an array of strings.
  void setArrayString(String name, List<String> values) {
    _ensureNotDisposed();
    final namePtr = name.toNativeUtf8();
    final stringPtrs = <Pointer<Utf8>>[];
    final arrayPtr = malloc<Pointer<Char>>(values.length);

    try {
      for (var i = 0; i < values.length; i++) {
        final strPtr = values[i].toNativeUtf8();
        stringPtrs.add(strPtr);
        arrayPtr[i] = strPtr.cast();
      }
      check(
        bindings.quiver_element_set_array_string(
          _ptr,
          namePtr.cast(),
          arrayPtr,
          values.length,
        ),
      );
    } finally {
      malloc.free(namePtr);
      for (final ptr in stringPtrs) {
        malloc.free(ptr);
      }
      malloc.free(arrayPtr);
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
}
