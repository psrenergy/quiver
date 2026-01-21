import 'dart:ffi';

import 'package:ffi/ffi.dart';

import 'ffi/bindings.dart';
import 'ffi/library_loader.dart';
import 'exceptions.dart';

/// A builder for creating database elements.
///
/// Elements are used to insert data into collections.
/// After use, call [dispose] to free native memory.
class Element {
  final Pointer<margaux_element_t1> _ptr;
  bool _isDisposed = false;

  /// Creates a new empty element.
  Element() : _ptr = bindings.margaux_element_create() {
    if (_ptr == nullptr) {
      throw const CreateElementException('Failed to create element');
    }
  }

  /// Internal pointer for FFI calls.
  Pointer<margaux_element_t1> get ptr {
    _ensureNotDisposed();
    return _ptr;
  }

  void _ensureNotDisposed() {
    if (_isDisposed) {
      throw const DatabaseOperationException('Element has been disposed');
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
      case List<int> v:
        setArrayInteger(name, v);
      case List<double> v:
        setArrayFloat(name, v);
      case List<String> v:
        setArrayString(name, v);
      case List v when v.isEmpty:
        throw InvalidArgumentException("Empty list not allowed for '$name'");
      case List v:
        _setMixedList(name, v);
      default:
        throw InvalidArgumentException(
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
      throw InvalidArgumentException(
        "Unsupported array element type ${first.runtimeType} for '$name'",
      );
    }
  }

  /// Sets an integer value.
  void setInteger(String name, int value) {
    _ensureNotDisposed();
    final namePtr = name.toNativeUtf8();
    try {
      final error = bindings.margaux_element_set_integer(_ptr, namePtr.cast(), value);
      if (error != 0) {
        throw DatabaseException.fromError(error, "Failed to set int '$name'");
      }
    } finally {
      malloc.free(namePtr);
    }
  }

  /// Sets a float value.
  void setFloat(String name, double value) {
    _ensureNotDisposed();
    final namePtr = name.toNativeUtf8();
    try {
      final error = bindings.margaux_element_set_float(_ptr, namePtr.cast(), value);
      if (error != 0) {
        throw DatabaseException.fromError(error, "Failed to set float '$name'");
      }
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
      final error = bindings.margaux_element_set_string(
        _ptr,
        namePtr.cast(),
        valuePtr.cast(),
      );
      if (error != 0) {
        throw DatabaseException.fromError(error, "Failed to set string '$name'");
      }
    } finally {
      malloc.free(namePtr);
      malloc.free(valuePtr);
    }
  }

  /// Sets a null value.
  void setNull(String name) {
    _ensureNotDisposed();
    final namePtr = name.toNativeUtf8();
    try {
      final error = bindings.margaux_element_set_null(_ptr, namePtr.cast());
      if (error != 0) {
        throw DatabaseException.fromError(error, "Failed to set null '$name'");
      }
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
      final error = bindings.margaux_element_set_array_integer(
        _ptr,
        namePtr.cast(),
        arrayPtr,
        values.length,
      );
      if (error != 0) {
        throw DatabaseException.fromError(error, "Failed to set int array '$name'");
      }
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
      final error = bindings.margaux_element_set_array_float(
        _ptr,
        namePtr.cast(),
        arrayPtr,
        values.length,
      );
      if (error != 0) {
        throw DatabaseException.fromError(error, "Failed to set float array '$name'");
      }
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
      final error = bindings.margaux_element_set_array_string(
        _ptr,
        namePtr.cast(),
        arrayPtr,
        values.length,
      );
      if (error != 0) {
        throw DatabaseException.fromError(error, "Failed to set string array '$name'");
      }
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
    bindings.margaux_element_clear(_ptr);
  }

  /// Frees the native memory associated with this element.
  void dispose() {
    if (_isDisposed) return;
    bindings.margaux_element_destroy(_ptr);
    _isDisposed = true;
  }
}
