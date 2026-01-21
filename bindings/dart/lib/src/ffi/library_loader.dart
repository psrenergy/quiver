import 'dart:ffi';
import 'dart:io';

import 'bindings.dart';

String get _libraryName {
  if (Platform.isWindows) return 'libmargaux_c.dll';
  if (Platform.isMacOS) return 'libmargaux_c.dylib';
  return 'libmargaux_c.so'; // Linux and others
}

PsrDatabaseBindings? _cachedBindings;
DynamicLibrary? _cachedLibrary;

/// Gets the FFI bindings, loading the library if needed.
PsrDatabaseBindings get bindings {
  _cachedBindings ??= PsrDatabaseBindings(library);
  return _cachedBindings!;
}

/// Gets the native library handle.
DynamicLibrary get library {
  if (_cachedLibrary != null) return _cachedLibrary!;

  // Try custom path first
  final customPath = Platform.environment['MARGAUX_DATABASE_LIB_PATH'];
  if (customPath != null) {
    final customFile = File('$customPath/$_libraryName');
    if (customFile.existsSync()) {
      _cachedLibrary = DynamicLibrary.open(customFile.path);
      return _cachedLibrary!;
    }
  }

  // Try current working directory (for tests)
  final cwdPath = '${Directory.current.path}/$_libraryName';
  if (File(cwdPath).existsSync()) {
    _cachedLibrary = DynamicLibrary.open(cwdPath);
    return _cachedLibrary!;
  }

  // Try executable directory (Flutter bundled)
  final execDir = File(Platform.resolvedExecutable).parent.path;
  final bundledPath = '$execDir/$_libraryName';
  if (File(bundledPath).existsSync()) {
    _cachedLibrary = DynamicLibrary.open(bundledPath);
    return _cachedLibrary!;
  }

  // Fall back to system PATH
  _cachedLibrary = DynamicLibrary.open(_libraryName);
  return _cachedLibrary!;
}
