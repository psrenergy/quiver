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
- **Native library resolution** (`lib/src/ffi/library_loader.dart`): searches the native-assets
  build output (`.dart_tool/hooks_runner/shared/quiverdb/build`) first — on Windows it pre-loads
  `libquiver.dll` from there so `libquiver_c.dll`'s dependency resolves — then falls back to
  system PATH. In the normal dev/test flow nothing needs to be on PATH.
- **Stale native cache**: when C API struct layouts change, clear `.dart_tool/hooks_runner/` and
  `.dart_tool/lib/` to force a fresh DLL rebuild — otherwise tests run against the old layout and
  fail in confusing ways.
- **Marshaling idiom**: every method allocates through a `package:ffi` `Arena` and releases in
  `finally`. Typed columns go through the shared private `_marshalGroupColumn(Arena, List<Object?>)`
  (used by `updateTimeSeriesGroup`, `upsertTimeSeriesRow`, `upsertTimeSeriesRowByLabel`,
  `updateVectorGroup`, `updateSetGroup` and the group writers' `ByLabel` forms); query parameters
  through `_marshalParams`.
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
- **`stringToDateTime` gates the shape, then re-serializes to catch a rolled-over field.**
  `DateTime.parse` is wider than the core's DATE_TIME grammar (it takes `"20240115"`, a `Z` suffix,
  a UTC offset) *and* it silently rolls an out-of-range field over instead of rejecting it —
  `"2024-02-31"` reads as March 2, `"2024-13-01"` as 2025-01-01, hour 25 as the next day — where
  Julia and Python both throw. So the regex alone is not enough: the parsed value is fed back
  through `dateTimeToString` and compared, and only a value the parse left untouched is accepted.
  Rejecting the offset forms also keeps every returned `DateTime` at `isUtc == false`, which
  matters because Dart's `==` compares `isUtc` as well as the instant: a list mixing the two has
  same-moment values comparing unequal, deduping to two in a `Set`, and colliding as distinct `Map`
  keys, while `compareTo` reads 0 and hides it. Keep this parser accepting exactly the same set as
  Julia's `string_to_date_time` and Python's `_parse_datetime`.
- **Booleans are INTEGER 0/1 in both directions.** `_integerToBoolean` (nullable) and
  `_integerToBooleanNonNull` (group cells, which are never null) live in `database.dart` so the
  `part` files share them; both take the `collection`/`attribute` so the rejection message names
  the offending column. They throw `ArgumentError` — a locally-crafted message is unavoidable
  here, since these readers are a binding-only convenience the core never sees. On writes,
  `Element.set` maps `bool` to `setInteger` (and a `List<bool?>` through `_setMixedList`),
  `_marshalParams` binds a `bool` parameter as INTEGER, and `_marshalGroupColumn`
  (`database_update.dart`) carries a `case bool` before `case int` — a Dart `bool` is not an
  `int`, so without it the group writers threw. That one branch covers all eight call sites
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
