import 'dart:ffi';
import 'dart:io';

import 'bindings.dart';

String get _libraryName {
  if (Platform.isWindows) return 'libquiver_c.dll';
  if (Platform.isMacOS) return 'libquiver_c.dylib';
  return 'libquiver_c.so';
}

String get _coreLibraryName {
  // Core library that the C API library depends on
  if (Platform.isWindows) return 'libquiver.dll';
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
  // 2. The app bundle's framework, on macOS (for a packaged Flutter app)
  // 3. System PATH (for installed/deployed apps)
  //
  // Each tier remembers its first failure, so if every tier fails the error names the real
  // loader problem instead of only the last thing tried.
  Object? firstError;

  // The build root holds one subtree per build config, and a universal macOS build leaves an
  // x86_64 tree next to the arm64 one — so a name match is not necessarily a *loadable*
  // library. Try every candidate rather than betting on directory enumeration order.
  for (final path in _findNativeAssetsLibraries()) {
    // On Windows, load the core library first so the C API library can find it
    if (Platform.isWindows) {
      final corePath = '${File(path).parent.path}/$_coreLibraryName';
      if (File(corePath).existsSync()) {
        try {
          DynamicLibrary.open(corePath);
        } catch (_) {
          // Best effort — let the C API library's own load report the failure.
        }
      }
    }

    try {
      _cachedLibrary = DynamicLibrary.open(path);
      return _cachedLibrary!;
    } catch (e) {
      firstError ??= e;
    }
  }

  // Inside a macOS .app there is no .dart_tool tree, so the scan above finds nothing.
  // flutter_tools repackages every code asset into its own framework and rewrites the
  // install name to @rpath/<name>.framework/<name>, where <name> is the file name with the
  // `lib` prefix and `.dylib` suffix stripped -- so libquiver_c.dylib is loaded as
  // quiver_c.framework/quiver_c. That stripping is why the build hook turns
  // QUIVER_UNVERSIONED_SHARED on: a versioned libquiver_c.0.10.5.dylib would become
  // quiver_c0105.framework instead. The framework is internally versioned
  // (Versions/A/quiver_c) but carries a top-level symlink, so this path resolves. The
  // dependency on libquiver is rewritten to its own framework at the same time and is
  // resolved by dyld through the app's @executable_path/../Frameworks rpath, so there is
  // no need to pre-open the core library the way Windows does above.
  if (Platform.isMacOS) {
    try {
      _cachedLibrary = DynamicLibrary.open('@rpath/quiver_c.framework/quiver_c');
      return _cachedLibrary!;
    } catch (e) {
      // Not running from an app bundle - fall through to the bare library name.
      firstError ??= e;
    }
  }

  // Fallback to system PATH
  try {
    _cachedLibrary = DynamicLibrary.open(_libraryName);
    return _cachedLibrary!;
  } catch (e) {
    if (firstError == null) rethrow;
    throw ArgumentError(
      'Failed to load $_libraryName. First failure: $firstError. PATH fallback: $e',
    );
  }
}

/// Searches for the library in Native Assets build output directories.
///
/// Returns every match, newest first: the build root holds one subtree per build config
/// (including one per architecture for a universal macOS build) and hooks_runner does not
/// purge stale ones, so the first name match is not necessarily the right library.
Iterable<String> _findNativeAssetsLibraries() {
  // Find the package root by looking for pubspec.yaml
  final Directory? packageRoot = _findPackageRoot();
  if (packageRoot == null) return const [];

  // Search in .dart_tool/hooks_runner/shared/quiverdb/build/
  final hooksDir = Directory(
    '${packageRoot.path}/.dart_tool/hooks_runner/shared/quiverdb/build',
  );

  if (!hooksDir.existsSync()) return const [];

  // Search recursively for the library
  try {
    // followLinks: false matches the build hook's own asset scan, which registers only real
    // files -- an unversioned symlink here is a leftover from a pre-QUIVER_UNVERSIONED_SHARED
    // build and points at an ABI the hook deliberately refuses to register.
    // ponytail: recursive walk over the whole build tree (~85% of it FetchContent sources);
    // it runs once per isolate. Narrow to <config>/lib/ if it ever shows up in startup cost.
    final matches =
        hooksDir
            .listSync(recursive: true, followLinks: false)
            .whereType<File>()
            .where((entity) => entity.path.endsWith(_libraryName))
            .toList()
          ..sort((a, b) => b.statSync().modified.compareTo(a.statSync().modified));
    return matches.map((entity) => entity.path);
  } catch (_) {
    // Ignore errors during search
    return const [];
  }
}

/// Finds the package root directory by searching for pubspec.yaml.
Directory? _findPackageRoot() {
  // Start from the script's directory and search upward, then from the current working
  // directory. Platform.script is not always a file URI (an embedder can hand us http: or
  // data:), and toFilePath() throws UnsupportedError for those -- so check before using it.
  final candidates = <Directory>[
    if (Platform.script.scheme == 'file') Directory(Platform.script.toFilePath()).parent,
    Directory.current,
  ];

  for (final startDir in candidates) {
    var dir = startDir;
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
