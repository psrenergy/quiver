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

    final builder = CMakeBuilder.create(
      name: 'quiver_native',
      sourceDir: sourceDir,
      targets: ['quiver', 'quiver_c'],
      defines: {
        'QUIVER_BUILD_C_API': 'ON',
        'QUIVER_BUILD_TESTS': 'OFF',
        'QUIVER_BUILD_SHARED': 'ON',
      },
      // Dart SDK always uses release mode
      buildMode: BuildMode.release,
      // Use Visual Studio 2019 generator on Windows
      generator: Generator.vs2019,
    );

    await builder.run(input: input, output: output, logger: logger);

    // Register built libraries as code assets
    // Library names without prefix - CMake on Windows produces quiver.dll and quiver_c.dll
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
