import 'package:code_assets/code_assets.dart';
import 'package:hooks/hooks.dart';
import 'package:native_toolchain_cmake/native_toolchain_cmake.dart';
import 'package:logging/logging.dart';

void main(List<String> args) async {
  await build(args, (input, output) async {
    if (!input.config.buildCodeAssets) return;

    final logger = Logger('')
      ..level = Level.ALL
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
        // native_toolchain_cmake drives macOS through its *iOS* toolchain file, which is
        // wrong for a host-native macOS build in two ways. Both are fixed with -D cache
        // entries: they exist before the toolchain file is read, and the last -D wins.
        //  1. It does `if (NOT DEFINED CMAKE_MACOSX_BUNDLE) set(CMAKE_MACOSX_BUNDLE YES)`
        //     (ios.toolchain.cmake:790-792) as a plain directory-scope variable, so it
        //     inherits into every add_subdirectory including FetchContent's. lua-cmake then
        //     makes lua_bin/luac_bin app bundles and their RUNTIME-only install() aborts
        //     configure. We build shared libraries for FFI here, never an app bundle.
        //  2. It sets CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY unless strict try_compile
        //     is on (ios.toolchain.cmake:786-787), so check_function_exists only compiles and
        //     never links. Because it declares the symbol itself, it then reports EVERY
        //     function as present -- including Linux-only posix_fallocate, which makes
        //     sqlite3.c fail to compile on Darwin. Linking works fine for a native build.
        if (targetOS == OS.macOS) ...{
          'CMAKE_MACOSX_BUNDLE': 'OFF',
          'ENABLE_STRICT_TRY_COMPILE': 'ON',
          //  3. It defaults DEPLOYMENT_TARGET to 13 (native_toolchain_cmake passes
          //     -DDEPLOYMENT_TARGET=13), but database_csv_export.cpp calls the
          //     floating-point std::to_chars, which libc++ marks unavailable before
          //     macOS 13.3. Raise the floor to the first version that has it. Set
          //     DEPLOYMENT_TARGET, not CMAKE_OSX_DEPLOYMENT_TARGET -- the iOS toolchain
          //     file derives the latter from the former, so setting it directly is
          //     silently overwritten.
          'DEPLOYMENT_TARGET': '13.3',
        },
        // Pre-set try_run results for cross-compilation mode on Linux
        // GNU strerror_r returns char* (not int), so the test succeeds (exit code 0)
        'HAVE_GNU_STRERROR_R_EXITCODE': '0',
        'HAVE_GNU_STRERROR_R_EXITCODE__TRYRUN_OUTPUT': '',
      },
      // Dart SDK always uses release mode
      buildMode: BuildMode.release,
      generator: generator,
    );

    await builder.run(input: input, output: output, logger: logger);

    // Register built libraries as code assets
    // CMake target names (not file names) — CMake always produces lib-prefixed DLLs (libquiver.dll, libquiver_c.dll)
    // The asset-ID values are placeholders required by findAndAddCodeAssets;
    // actual library resolution happens manually in lib/src/ffi/library_loader.dart.
    await output.findAndAddCodeAssets(
      input,
      names: {
        'quiver': 'src/ffi/quiver.dart',
        'quiver_c': 'src/ffi/library_loader.dart',
      },
      logger: logger,
    );
  });
}
