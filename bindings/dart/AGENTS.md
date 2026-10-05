# Dart Binding (quiverdb)

Cross-layer naming rules (snake_case → camelCase, named constructors for factories) and the
convenience-method parity tables live in the root `AGENTS.md`.

## Layout

```
lib/src/          # Hand-written wrappers: database.dart + part files per area, element.dart,
                  # sandbox.dart, metadata.dart, exceptions.dart, date_time.dart,
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
  `quiver_database_upsert_time_series_row` plus its `_by_label` form,
  `quiver_database_update_relation` plus its `_by_label` form, and the `out_mask` parameter of
  `quiver_database_read_time_series_row`. The query entry points were collapsed the same way:
  the three plain `quiver_database_query_{string,integer,float}` blocks were deleted and the
  parameterized blocks renamed onto those names. Removals are hand-deleted the same way
  (`quiver_clear_last_error` and the four `quiver_element_*` has/count accessors).
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
- **The macOS build hook carries three load-bearing workarounds** (`hook/build.dart`), because
  native_toolchain_cmake drives macOS through its bundled **iOS** toolchain file:
  `QUIVER_UNVERSIONED_SHARED=ON` (without it `findAndAddCodeAssets` — `followLinks: false`,
  unversioned-name match — registers **zero** assets and the hook still exits 0);
  `CMAKE_MACOSX_BUNDLE=OFF` (the toolchain's `if(NOT DEFINED ...) set(... YES)` inherits into
  FetchContent, and lua-cmake's `lua_bin` bundle + RUNTIME-only `install()` then aborts
  configure); and `DEPLOYMENT_TARGET` floored at 13.3 (libc++ marks the floating-point
  `std::to_chars` used by `database_csv_export.cpp` / `sandbox/return_json.cpp` / `sandbox/csv.cpp` /
  `binary/csv_converter.cpp` unavailable below it —
  `cmake/Platform.cmake` carries the same floor for every other macOS build). Do not "simplify"
  these. `appleArgs: AppleBuilderArgs(enableStrictTryCompile: true)` is kept as hygiene rather
  than a live fix: without it `CMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY` makes configure
  checks compile without linking, which once let sqlite3-cmake's `check_function_exists` report
  Linux-only `posix_fallocate` on Darwin; since v3.53.4 the fork uses `check_symbol_exists`, and
  every remaining probe reads a real header. The fork also dropped its strerror_r `try_run`, the
  only `try_run` in the dependency tree, so the hook no longer seeds try_run results.
  The Linux **Dart Coverage** CI job runs this hook through `dart test` (wrapped by
  `coverage:test_with_coverage`) on every push to master and every PR against it, so
  `QUIVER_UNVERSIONED_SHARED=ON` and the asset-count check are exercised on Linux. The
  macOS/Windows paths, including the other two macOS workarounds and `appleArgs`, remain
  unexercised in CI. Dart is published by hand.
- **Stale native cache**: when C API struct layouts change, clear `.dart_tool/hooks_runner/` and
  `.dart_tool/lib/` to force a fresh DLL rebuild — otherwise tests run against the old layout and
  fail in confusing ways. Clear it after a **build-configuration** change too, not just an ABI
  one: the hook reuses one CMake build dir per config checksum, and that checksum does not cover
  the hook's own defines. Configure-check (`check_*`) results are cached even by a *failed* configure
  and are then skipped forever, and flipping `QUIVER_UNVERSIONED_SHARED` does not force a relink,
  so the previous build's symlinks survive and the asset scan finds nothing.
- **Marshaling idiom**: every method allocates through a `package:ffi` `Arena` and releases in
  `finally`. Never request zero bytes (`arena<T>(0)`): package:ffi's allocator throws
  `ArgumentError('Could not allocate 0 bytes.')` whenever the platform returns NULL for it, which
  POSIX `malloc`/`calloc` may do (Windows' `CoTaskMemAlloc` does not, so a Windows test run will
  not catch it). Pass `nullptr` for an empty array instead, since the C API takes NULL with a zero
  count; `updateTimeSeriesFiles`, `_marshalGroupColumns` (empty map) and `_marshalParams` do this.
  The six columnar group writers (`updateVectorGroup`/`updateSetGroup`/`updateTimeSeriesGroup`
  and their `ByLabel` forms) marshal the whole payload through `_marshalGroupColumns(Arena, Map)`
  (empty map → NULL arrays; jagged columns → `ArgumentError` naming the column), which calls the
  per-column `_marshalGroupColumn(Arena, String, List<Object?>)`. The two row upserts call
  `_marshalGroupColumn` directly (no mask in the row C signature); query parameters
  go through `_marshalParams`. Both `_marshalGroupColumn` and `Element._setMixedList` take the family
  (numeric, String, DateTime) from the first non-null cell and the numeric type from all of them: a
  numeric column is INTEGER unless some cell is a `double`, which widens it to FLOAT (the rule
  Python and JS share, so `[1, 2.5]` writes 1.0 and 2.5 in every binding; it used to throw here).
  They then convert **every** cell individually — never `as`/`cast` the rest to the dispatched type, which defers the check to iteration and throws a raw `TypeError` naming
  neither the column nor the cell. An `int` (and a `bool`) is accepted into a REAL column by the
  int-for-REAL coercion. Find the dispatch cell with a plain loop, not `firstWhere(..., orElse: ()
  => null)`: `orElse` must return the list's *runtime* element type, so it throws on every
  non-nullable list — which is what a mixed literal like `[1.5, 2]` infers.
- **The group writers take columns while the group readers return rows** (`readVectorGroupById`).
  The only asymmetric reader/writer pair here — deliberate, see the root design decisions.
- **Vector/set NULL cells**: the four numeric bulk readers decode a nested
  `Pointer<Pointer<Uint8>>` — one mask per element, parallel to `outSizes` — freed by
  `quiver_database_free_masks`; the four numeric `_by_id` readers take a flat mask freed by
  `quiver_database_free_mask`. The string readers carry no mask: a NULL cell is a `nullptr` entry,
  guarded with the same `ptr == nullptr ? null : ...` form the scalar string reader uses. All twelve
  return a nullable inner element (`List<List<int?>>`, `List<int?>`, …).
- **Scalar bulk NULLs**: `readScalarIntegers`/`readScalarFloats` decode a parallel `Pointer<Uint8>`
  mask into `List<int?>`/`List<double?>` (mask 0 → `null`); `readScalarStrings` returns `List<String?>`,
  null-guarding the pointer before `toDartString`; `readScalarDateTimes` maps that list while
  preserving its null slots. `bindings.dart` carries the mask arg + `quiver_database_free_mask`
  (regenerate via ffigen; clear `.dart_tool` caches on C-API changes). `readTimeSeriesRow` decodes
  the same kind of mask, which the C API returns for every column type (mask 0 = no data at or
  before the date → `null`; the string branch never `toDartString`s a masked-out pointer).
- **`Sandbox.run` owns its result**: `quiver_lua_runner_run` takes a `char** out_result` whose JSON
  string is C-heap allocated, so the `Arena` cannot own it — it is freed with
  `quiver_lua_runner_free_string` (*not* `quiver_database_free_string`) in its own nested `finally`,
  so a `toDartString` failure cannot leak it. The columnar group decoders (`_decodeGroupRows`,
  `readTimeSeriesGroup`) free the C result in their own `finally` for the same reason. The script
  must be Lua source text: the core loads it in text mode, so a precompiled (bytecode) chunk is
  rejected with `Failed to run Lua script: ...` and surfaces like any other script error.
- **Time-series group NULLs**: `readTimeSeriesGroup`/`updateTimeSeriesGroup` use
  `Map<String, List<Object?>>` — a `null` cell is a SQL NULL. `_marshalGroupColumn` returns a
  `({int type, Pointer<Void> data, Pointer<Uint8> hasValue})` record (the per-cell mask;
  `upsertTimeSeriesRow` and `upsertTimeSeriesRowByLabel` ignore `hasValue`), types the column by
  the rule above, and tags an all-null/empty column FLOAT with a zeroed placeholder. Reads
  decode the mask out-param and never `toDartString` a masked-out (NULL) pointer.
- **Query API shape**: `queryString`/`queryInteger`/`queryBoolean`/`queryFloat`/`queryDateTime`
  take an optional positional `List<Object?>? parameters` (no separate `*Params` methods). Every
  call makes the one C call per type; `_marshalParams` returns `nullptr` arrays and a count of 0
  when `parameters` is null or empty, so a parameterless query allocates nothing.
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
- **Booleans are INTEGER 0/1 in both directions.** `_integerToBoolean` lives in
  `database.dart` so the `part` files share it; it takes the `collection`/`attribute` so the
  rejection message names the offending column. It throws `ArgumentError` — a locally-crafted
  message is unavoidable here, since these readers are a binding-only convenience the core never sees. On writes,
  `Element.set` maps `bool` to `setInteger` (and a `List<bool?>` through `_setMixedList`),
  `_marshalParams` binds a `bool` parameter as INTEGER, and `_marshalGroupColumn`
  (`database_update.dart`) counts `bool` as numeric (`first is bool || first is int || first is
  double`) — a Dart `bool` is not an `int`, so without the `bool` half the group writers threw.
  bool and int are **one branch converting per cell**, not two branches each casting `as` their own
  type: the family comes from the first non-null cell, so a mixed `[true, 1]` column would
  otherwise throw a raw `TypeError` naming nothing. That branch covers all eight call sites
  (`updateVectorGroup`/`updateSetGroup`/`updateTimeSeriesGroup`/`upsertTimeSeriesRow` and every
  `ByLabel` form; six of them via `_marshalGroupColumns`), which is why it takes the column name: its unsupported-type `ArgumentError`
  has to name the offending column per the root marshalling-error rule.
- **Element array NULLs**: `Element.setArray{Integer,Float,String}` take `List<T?>` and pass the
  per-cell `has_value` mask to the C setters; `Element.set` types mixed lists like
  `_marshalGroupColumn` (family from the first non-null element, a `double` anywhere widens to
  float), and an empty or all-null list is tagged integer (valid — type is irrelevant
  when no value is read). Do not reintroduce the old "empty mixed list" rejection: an empty array
  on `updateElement` is the clear-group path.
- **Per-method FFI boilerplate is the house style** — don't collapse it into
  closure-parameterized helpers (root "Do not 'fix'" list).
