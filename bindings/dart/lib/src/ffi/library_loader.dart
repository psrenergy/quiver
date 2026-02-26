import 'dart:ffi';
import 'dart:io';

import 'bindings.dart';

String get _libraryName {
  // Windows CMake doesn't add 'lib' prefix by default
  if (Platform.isWindows) return 'quiver_c.dll';
  if (Platform.isMacOS) return 'libquiver_c.dylib';
  return 'libquiver_c.so';
}

String get _coreLibraryName {
  // Core library that the C API library depends on
  if (Platform.isWindows) return 'quiver.dll';
  if (Platform.isMacOS) return 'libquiver.dylib';
  return 'libquiver.so';
}

QuiverDatabaseBindings? _cachedBindings;
DynamicLibrary? _cachedLibrary;

QuiverDatabaseBindings get bindings {
  _cachedBindings ??= QuiverDatabaseBindings(library);
  return _cachedBindings!;
}

DynamicLibrary get library {
  if (_cachedLibrary != null) return _cachedLibrary!;

  // Try to find the library in order of priority:
  // 1. Native Assets build output (for dart test/run)
  // 2. System PATH (for installed/deployed apps)

  final nativeAssetsPath = _findNativeAssetsLibrary();
  if (nativeAssetsPath != null) {
    final libDir = File(nativeAssetsPath).parent.path;

    // On Windows, load the core library first so the C API library can find it
    if (Platform.isWindows) {
      final corePath = '$libDir/$_coreLibraryName';
      final libCorePath = '$libDir/lib$_coreLibraryName';
      if (File(corePath).existsSync()) {
        DynamicLibrary.open(corePath);
      } else if (File(libCorePath).existsSync()) {
        DynamicLibrary.open(libCorePath);
      }
    }

    _cachedLibrary = DynamicLibrary.open(nativeAssetsPath);
    return _cachedLibrary!;
  }

  // Fallback to system PATH
  _cachedLibrary = DynamicLibrary.open(_libraryName);
  return _cachedLibrary!;
}

/// Searches for the library in Native Assets build output directories.
String? _findNativeAssetsLibrary() {
  // Find the package root by looking for pubspec.yaml
  Directory? packageRoot = _findPackageRoot();
  if (packageRoot == null) return null;

  // Search in .dart_tool/hooks_runner/shared/quiverdb/build/
  final hooksDir = Directory(
    '${packageRoot.path}/.dart_tool/hooks_runner/shared/quiverdb/build',
  );

  if (!hooksDir.existsSync()) return null;

  // Search recursively for the library
  try {
    for (final entity in hooksDir.listSync(recursive: true)) {
      if (entity is File && entity.path.endsWith(_libraryName)) {
        return entity.path;
      }
    }
  } catch (_) {
    // Ignore errors during search
  }

  return null;
}

/// Finds the package root directory by searching for pubspec.yaml.
Directory? _findPackageRoot() {
  // Start from the script's directory and search upward
  var dir = Directory(Platform.script.toFilePath()).parent;

  // Also try current working directory
  final candidates = [dir, Directory.current];

  for (final startDir in candidates) {
    dir = startDir;
    for (var i = 0; i < 10; i++) {
      final pubspec = File('${dir.path}/pubspec.yaml');
      if (pubspec.existsSync()) {
        return dir;
      }
      final parent = dir.parent;
      if (parent.path == dir.path) break;
      dir = parent;
    }
  }

  return null;
}
