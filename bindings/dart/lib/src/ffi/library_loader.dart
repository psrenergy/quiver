import 'dart:ffi';
import 'dart:io';

import 'bindings.dart';

String get _libraryName {
  if (Platform.isWindows) return 'libquiver_c.dll';
  if (Platform.isMacOS) return 'libquiver_c.dylib';
  return 'libquiver_c.so';
}

QuiverDatabaseBindings? _cachedBindings;
DynamicLibrary? _cachedLibrary;

QuiverDatabaseBindings get bindings {
  _cachedBindings ??= QuiverDatabaseBindings(library);
  return _cachedBindings!;
}

DynamicLibrary get library {
  if (_cachedLibrary != null) return _cachedLibrary!;

  // Native Assets bundles the library - just open by name
  _cachedLibrary = DynamicLibrary.open(_libraryName);
  return _cachedLibrary!;
}
