# Dart Binding (quiverdb)

Cross-layer naming rules (snake_case → camelCase, named constructors for factories) and the
convenience-method parity tables live in the root `CLAUDE.md`.

## Layout

```
lib/src/          # Hand-written wrappers: database.dart + part files per area, element.dart,
                  # lua_runner.dart, metadata.dart, exceptions.dart, date_time.dart,
                  # database_options.dart
lib/src/ffi/      # bindings.dart (GENERATED ffigen output — do not hand-edit) +
                  # library_loader.dart (hand-written native library resolution)
generator/        # generator.bat → runs `dart run ffigen`
hook/build.dart   # Native-assets build hook: compiles the C library via native_toolchain_cmake
test/             # Test suite (*_test.dart per area) + test.bat (plain `dart test` wrapper)
pubspec.yaml      # Version must match CMakeLists.txt (checked by scripts/assert_version.py)
```

## Rules and gotchas

- **Regenerate after C API changes**: `generator/generator.bat` rewrites
  `lib/src/ffi/bindings.dart`. The live ffigen config is the `ffigen:` block in
  **pubspec.yaml** (plain `dart run ffigen` reads only that); the sibling `ffigen.yaml` is an
  unused duplicate consulted only via an explicit `--config` flag — editing it alone changes
  nothing.
- **Native library resolution** (`lib/src/ffi/library_loader.dart`): searches the native-assets
  build output (`.dart_tool/hooks_runner/shared/quiverdb/build`) first — on Windows it pre-loads
  `libquiver.dll` from there so `libquiver_c.dll`'s dependency resolves — then falls back to
  system PATH. In the normal dev/test flow nothing needs to be on PATH.
- **Stale native cache**: when C API struct layouts change, clear `.dart_tool/hooks_runner/` and
  `.dart_tool/lib/` to force a fresh DLL rebuild — otherwise tests run against the old layout and
  fail in confusing ways.
- **Marshaling idiom**: every method allocates through a `package:ffi` `Arena` and releases in
  `finally`. Typed time-series columns go through the shared private
  `_marshalTimeSeriesColumn(Arena, List<Object?>)` (used by `updateTimeSeriesGroup` and
  `addTimeSeriesRow`); query parameters through `_marshalParams`.
- **Time-series group NULLs**: `readTimeSeriesGroup`/`updateTimeSeriesGroup` use
  `Map<String, List<Object?>>` — a `null` cell is a SQL NULL. `_marshalTimeSeriesColumn` returns a
  `({int type, Pointer<Void> data, Pointer<Uint8> hasValue})` record (the per-cell mask;
  `addTimeSeriesRow` ignores `hasValue`), dispatches on the first non-null element, and tags an
  all-null/empty column FLOAT with a zeroed placeholder. Reads decode the mask out-param and never
  `toDartString` a masked-out (NULL) pointer.
- **Query API shape**: `queryString`/`queryInteger`/`queryFloat`/`queryDateTime` take an optional
  positional `List<Object?>? parameters` (no separate `*Params` methods).
- **Per-method FFI boilerplate is the house style** — don't collapse it into
  closure-parameterized helpers (root "Do not 'fix'" list).
