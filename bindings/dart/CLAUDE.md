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
- **The checked-in `bindings.dart` predates the pinned ffigen (20.1.1).** Regenerating today
  rewrites the whole file and turns `quiver_data_type_t` / `quiver_error_t` / `quiver_log_level_t`
  from `abstract class` int constants into real Dart `enum`s (and the native return type from
  `Int32` to `UnsignedInt`). That is a breaking change for every downstream `== quiver_data_type_t.X`
  comparison — notably hub's `lib/models/database.dart`. The `update_vector_group` /
  `update_set_group` entries were therefore hand-added in the file's existing style, and
  `quiver_database_number_of_elements` likewise (hand-added right after
  `quiver_database_read_element_ids`, matching the C API's declaration order), as were
  `quiver_database_update_element_by_label`, the three group writers' `_by_label` forms,
  `quiver_database_upsert_time_series_row` plus its `_by_label` form, and
  `quiver_database_update_relation` plus its `_by_label` form.
  Take the generator upgrade as its own deliberate change (regenerate, then fix the enum call
  sites here and in hub) rather than as a side effect of adding a C function.
- **Native library resolution** (`lib/src/ffi/library_loader.dart`), three tiers in order:
  (1) the native-assets build output (`.dart_tool/hooks_runner/shared/quiverdb/build`) — on
  Windows it pre-loads `libquiver.dll` from there so `libquiver_c.dll`'s dependency resolves;
  (2) on macOS `@rpath/quiver_c.framework/quiver_c`, for a packaged Flutter `.app`, where there
  is no `.dart_tool` tree (flutter_tools repackages each code asset as `<name>.framework` with
  the `lib` prefix and `.dylib` suffix stripped, and rewrites the inter-asset dependency to
  `@rpath/quiver.framework/quiver`, so no core pre-open is needed there);
  (3) system PATH. In the normal dev/test flow nothing needs to be on PATH.
  Tier 1 returns **every** name match, newest first, and tries each: the build root holds one
  subtree per build config and a universal macOS build leaves an x86_64 tree beside the arm64
  one, so the first match is not necessarily loadable. It scans with `followLinks: false` to
  match the hook's own scanner. If all three tiers fail the error reports the *first* failure,
  not just the PATH one — a bundle whose framework exists but whose dependency is missing would
  otherwise be reported as a missing `libquiver_c.dylib`.
- **The macOS build hook carries four load-bearing workarounds** (`hook/build.dart`), because
  native_toolchain_cmake drives macOS through its bundled **iOS** toolchain file:
  `QUIVER_UNVERSIONED_SHARED=ON` (without it `findAndAddCodeAssets` — `followLinks: false`,
  unversioned-name match — registers **zero** assets and the hook still exits 0);
  `CMAKE_MACOSX_BUNDLE=OFF` (the toolchain's `if(NOT DEFINED ...) set(... YES)` inherits into
  FetchContent, and lua-cmake's `lua_bin` bundle + RUNTIME-only `install()` then aborts
  configure); `appleArgs: AppleBuilderArgs(enableStrictTryCompile: true)` (otherwise
  `CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY` makes every `check_function_exists` report
  success, including Linux-only `posix_fallocate`, which breaks `sqlite3.c` on Darwin); and
  `DEPLOYMENT_TARGET` floored at 13.3 (libc++ marks the floating-point `std::to_chars` used by
  `database_csv_export.cpp` / `lua_runner.cpp` unavailable below it — `cmake/Platform.cmake`
  carries the same floor for every other macOS build). Do not "simplify" these. The
  `HAVE_GNU_STRERROR_R_EXITCODE` seed is load-bearing on macOS too, and its **value** is
  per-platform: CMake hard-errors on `try_run` in cross-compiling mode without it (the iOS
  toolchain file sets `CMAKE_SYSTEM_NAME`, so macOS is in that mode), but the GNU answer is only
  right for glibc — seeding `0` on Darwin defines `STRERROR_R_CHAR_P` and makes `sqlite3.c`
  assign an `int` return to a `char*`.
  Note **no CI job runs this hook on any OS** — Dart is built and published by hand.
- **Stale native cache**: when C API struct layouts change, clear `.dart_tool/hooks_runner/` and
  `.dart_tool/lib/` to force a fresh DLL rebuild — otherwise tests run against the old layout and
  fail in confusing ways. Clear it after a **build-configuration** change too, not just an ABI
  one: the hook reuses one CMake build dir per config checksum, and that checksum does not cover
  the hook's own defines. `check_function_exists` results are cached even by a *failed* configure
  and are then skipped forever, and flipping `QUIVER_UNVERSIONED_SHARED` does not force a relink,
  so the previous build's symlinks survive and the asset scan finds nothing.
- **Marshaling idiom**: every method allocates through a `package:ffi` `Arena` and releases in
  `finally`. Typed columns go through the shared private `_marshalGroupColumn(Arena, String, List<Object?>)`
  (used by `updateTimeSeriesGroup`, `upsertTimeSeriesRow`, `upsertTimeSeriesRowByLabel`,
  `updateVectorGroup`, `updateSetGroup` and the group writers' `ByLabel` forms); query parameters
  through `_marshalParams`. Both `_marshalGroupColumn` and `Element._setMixedList` dispatch on the
  first non-null cell and then convert **every** cell individually — never `as`/`cast` the rest to
  the dispatched type, which defers the check to iteration and throws a raw `TypeError` naming
  neither the column nor the cell. An `int` (and a `bool`) is accepted into a REAL column by the
  int-for-REAL coercion. Find the dispatch cell with a plain loop, not `firstWhere(..., orElse: ()
  => null)`: `orElse` must return the list's *runtime* element type, so it throws on every
  non-nullable list — which is what a mixed literal like `[1.5, 2]` infers.
- **The group writers take columns while the group readers return rows** (`readVectorGroupById`).
  The only asymmetric reader/writer pair here — deliberate, see the root design decisions.
- **Scalar bulk NULLs**: `readScalarIntegers`/`readScalarFloats` decode a parallel `Pointer<Uint8>`
  mask into `List<int?>`/`List<double?>` (mask 0 → `null`); `readScalarStrings` returns `List<String?>`,
  null-guarding the pointer before `toDartString`; `readScalarDateTimes` maps that list while
  preserving its null slots. `bindings.dart` carries the mask arg + `quiver_database_free_mask`
  (regenerate via ffigen; clear `.dart_tool` caches on C-API changes).
- **`LuaRunner.run` owns its result**: `quiver_lua_runner_run` takes a `char** out_result` whose JSON
  string is C-heap allocated, so the `Arena` cannot own it — it is freed with
  `quiver_lua_runner_free_string` (*not* `quiver_database_free_string`) in its own nested `finally`,
  so a `toDartString` failure cannot leak it.
- **Time-series group NULLs**: `readTimeSeriesGroup`/`updateTimeSeriesGroup` use
  `Map<String, List<Object?>>` — a `null` cell is a SQL NULL. `_marshalGroupColumn` returns a
  `({int type, Pointer<Void> data, Pointer<Uint8> hasValue})` record (the per-cell mask;
  `upsertTimeSeriesRow` and `upsertTimeSeriesRowByLabel` ignore `hasValue`), dispatches on the
  first non-null element, and tags an all-null/empty column FLOAT with a zeroed placeholder. Reads
  decode the mask out-param and never `toDartString` a masked-out (NULL) pointer.
- **Query API shape**: `queryString`/`queryInteger`/`queryBoolean`/`queryFloat`/`queryDateTime`
  take an optional positional `List<Object?>? parameters` (no separate `*Params` methods).
- **`stringToDateTime` gates the shape with a regex, then range-checks the fields in UTC.**
  `DateTime.parse` is wider than the core's DATE_TIME grammar (it takes `"20240115"`, a `Z` suffix,
  a UTC offset) *and* it silently rolls an out-of-range field over instead of rejecting it —
  `"2024-02-31"` reads as March 2, `"2024-13-01"` as 2025-01-01, hour 25 as the next day — where
  Julia and Python both throw. So the regex alone is not enough: the captured fields are fed to
  `DateTime.utc` and compared with what comes back, and only a value left untouched is accepted;
  the local `DateTime(y, m, d, h, mi, s)` is then built from the validated fields. The check is in
  **UTC on purpose**: a local parse (the first version re-serialized `DateTime.parse(s)` and
  compared it with `s`) also shifts a wall-clock time the platform's zone considers nonexistent —
  a DST gap; on Windows the historical Brazilian rules put one at midnight of 2019-01-01 — so a
  valid stored `"2019-01-01T00:00:00"` came back as 01:00, failed the comparison, and every
  reader threw `Cannot convert ... expected a valid YYYY-MM-DD[THH:MM:SS]`. UTC has no gaps.
  Rejecting the offset forms also keeps every returned `DateTime` at `isUtc == false`, which
  matters because Dart's `==` compares `isUtc` as well as the instant: a list mixing the two has
  same-moment values comparing unequal, deduping to two in a `Set`, and colliding as distinct `Map`
  keys, while `compareTo` reads 0 and hides it. The residual limitation of a local `DateTime` is
  that a value inside a real DST gap is still returned an hour later (that local wall-clock time
  does not exist); it is no longer an error. `test/date_time_test.dart` pins the grammar (both
  directions) and the time-zone independence. Keep this parser accepting exactly the same set as
  Julia's `string_to_date_time` and Python's `_parse_datetime`.
- **Booleans are INTEGER 0/1 in both directions.** `_integerToBoolean` (nullable) and
  `_integerToBooleanNonNull` (group cells, which are never null) live in `database.dart` so the
  `part` files share them; both take the `collection`/`attribute` so the rejection message names
  the offending column. They throw `ArgumentError` — a locally-crafted message is unavoidable
  here, since these readers are a binding-only convenience the core never sees. On writes,
  `Element.set` maps `bool` to `setInteger` (and a `List<bool?>` through `_setMixedList`),
  `_marshalParams` binds a `bool` parameter as INTEGER, and `_marshalGroupColumn`
  (`database_update.dart`) dispatches on `first is bool || first is int` — a Dart `bool` is not an
  `int`, so without the `bool` half the group writers threw. The two are **one branch converting
  per cell**, not two branches each casting `as` their own type: dispatch reads only the first
  non-null cell, so a mixed `[true, 1]` column would otherwise throw a raw `TypeError` naming
  nothing. That branch covers all eight call sites
  (`updateVectorGroup`/`updateSetGroup`/`updateTimeSeriesGroup`/`upsertTimeSeriesRow` and every
  `ByLabel` form), which is why it takes the column name: its unsupported-type `ArgumentError`
  has to name the offending column per the root marshalling-error rule.
- **Element array NULLs**: `Element.setArray{Integer,Float,String}` take `List<T?>` and pass the
  per-cell `has_value` mask to the C setters; `Element.set` dispatches mixed lists on the first
  non-null element, and an empty or all-null list is tagged integer (valid — type is irrelevant
  when no value is read). Do not reintroduce the old "empty mixed list" rejection: an empty array
  on `updateElement` is the clear-group path.
- **Per-method FFI boilerplate is the house style** — don't collapse it into
  closure-parameterized helpers (root "Do not 'fix'" list).
