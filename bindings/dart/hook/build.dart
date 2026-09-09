import 'package:code_assets/code_assets.dart';
import 'package:hooks/hooks.dart';
import 'package:native_toolchain_cmake/native_toolchain_cmake.dart';
import 'package:logging/logging.dart';

Future<void> main(List<String> args) async {
  await build(args, (input, output) async {
    if (!input.config.buildCodeAssets) return;

    // Logger('') is Logger.root, so Level.ALL there would print every record from every
    // logger in the process -- including native_toolchain_cmake's per-chunk `fine` logging of
    // the whole CMake configure and compiler output. A detached logger at INFO keeps the
    // "Running <cmd>" lines and stderr (logged at severe) without the firehose.
    final logger = Logger.detached('quiver_native')
      ..level = Level.INFO
      ..onRecord.listen((record) => print(record.message));

    // CMake source is at project root (two levels up from bindings/dart)
    final sourceDir = input.packageRoot.resolve('../../');

    // Select generator based on target OS
    final targetOS = input.config.code.targetOS;
    final generator = switch (targetOS) {
      OS.windows => Generator.ninja,
      OS.macOS => Generator.xcode,
      _ => Generator.ninja,
    };

    final builder = CMakeBuilder.create(
      name: 'quiver_native',
      sourceDir: sourceDir,
      targets: ['quiver', 'quiver_c'],
      defines: {
        'QUIVER_BUILD_C_API': 'ON',
        'QUIVER_BUILD_TESTS': 'OFF',
        'QUIVER_BUILD_SHARED': 'ON',
        // findAndAddCodeAssets walks the build dir with followLinks:false, skips
        // non-File entities, and matches a path ending in the unversioned library name
        // (add_assets.dart:107-112). With VERSION/SOVERSION set, the real files carry a
        // version infix and the unversioned names are symlinks, so NOTHING registers and
        // the hook silently reports zero assets. Drop the versioning for this build.
        'QUIVER_UNVERSIONED_SHARED': 'ON',
        // native_toolchain_cmake drives macOS through its *iOS* toolchain file, which is wrong
        // for a host-native macOS build. It does
        // `if (NOT DEFINED CMAKE_MACOSX_BUNDLE) set(CMAKE_MACOSX_BUNDLE YES)` as a plain
        // directory-scope variable, so it inherits into every add_subdirectory including
        // FetchContent's; lua-cmake then makes lua_bin/luac_bin app bundles and their
        // RUNTIME-only install() aborts configure. We build shared libraries for FFI here,
        // never an app bundle. A -D cache entry is the only lever (the toolchain's guard is
        // `NOT DEFINED`, and a cache entry counts as defined); there is no builder parameter
        // for it, unlike the strict-try-compile flag passed via appleArgs below.
        if (targetOS == OS.macOS) ...{
          'CMAKE_MACOSX_BUNDLE': 'OFF',
          // The floating-point std::to_chars (src/database_csv_export.cpp, src/lua_runner.cpp)
          // is marked unavailable by libc++ before macOS 13.3, so the build floor cannot go
          // lower than that. Honour a higher floor if the SDK asks for one rather than
          // silently overriding the consumer's deployment target in both directions. Set
          // DEPLOYMENT_TARGET, not CMAKE_OSX_DEPLOYMENT_TARGET -- the iOS toolchain file
          // derives the latter from the former with CACHE INTERNAL (which implies FORCE), so
          // setting it directly is silently overwritten.
          'DEPLOYMENT_TARGET': input.config.code.macOS.targetVersion > 13
              ? '${input.config.code.macOS.targetVersion}'
              : '13.3',
        },
        // Pre-set try_run results: sqlite3-cmake probes for the GNU strerror_r with
        // check_c_source_runs, and CMake hard-errors on try_run in cross-compiling mode
        // without these. Both Linux (explicit toolchain file) and macOS (the iOS toolchain
        // file sets CMAKE_SYSTEM_NAME, so CMAKE_CROSSCOMPILING is true even for a host build)
        // land in that mode, so the value has to be per-platform rather than blanket 0:
        // glibc's strerror_r is the GNU char*-returning form (probe succeeds, exit 0), while
        // Darwin's is the XSI int-returning form (probe must fail; any non-zero exit sends
        // CheckSourceRuns down its falsy path). Getting this wrong is not cosmetic --
        // sqlite3-cmake turns a true answer into STRERROR_R_CHAR_P=1, and sqlite3.c then
        // assigns the int return to a char* and logs it ("Incorrectly concluding that the GNU
        // version is available could lead to a segfault", sqlite3.c:40139); that branch is
        // dead only while sqlite3_ENABLE_THREADSAFE is OFF.
        'HAVE_GNU_STRERROR_R_EXITCODE': targetOS == OS.linux ? '0' : '1',
        'HAVE_GNU_STRERROR_R_EXITCODE__TRYRUN_OUTPUT': '',
      },
      // Dart SDK always uses release mode
      buildMode: BuildMode.release,
      generator: generator,
      // The iOS toolchain file sets CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY unless strict
      // try_compile is on, so every check_function_exists only compiles and never links.
      // Because the probe declares the symbol itself, it then reports EVERY function as
      // present -- including Linux-only posix_fallocate, which makes sqlite3.c fail to compile
      // on Darwin. Linking works fine for a native build. Passed through appleArgs rather than
      // a raw -D: the raw form would only win because native_toolchain_cmake happens to append
      // user defines after its own, which is not part of its API.
      appleArgs: const AppleBuilderArgs(enableStrictTryCompile: true),
    );

    await builder.run(input: input, output: output, logger: logger);

    // Register built libraries as code assets. Each key is turned into the platform's library
    // file name (libquiver.dylib / .so / .dll) and matched against the build tree; each value
    // becomes the asset id (`package:quiverdb/<value>`). Nothing resolves those ids today —
    // library resolution happens manually in lib/src/ffi/library_loader.dart, and on macOS the
    // framework name flutter_tools derives comes from the *file* name, not the id — so
    // `src/ffi/quiver.dart` naming no real library is inert rather than a placeholder
    // requirement. Registering both is what gives the .app its quiver.framework.
    final names = {
      'quiver': 'src/ffi/quiver.dart',
      'quiver_c': 'src/ffi/library_loader.dart',
    };
    final added = await output.findAndAddCodeAssets(input, names: names, logger: logger);

    // findAndAddCodeAssets reports "found nothing" by returning an empty list, not by failing,
    // so an unregistered library would otherwise surface as a green build that dies at the
    // first FFI call. Fail here instead, where the cause is still visible.
    if (added.length != names.length) {
      throw StateError(
        'Expected ${names.length} code assets, registered ${added.length} '
        '(${added.map((asset) => asset.id).join(', ')}). '
        'Searched ${input.outputDirectory} for ${names.keys.join(', ')}.',
      );
    }
  });
}
