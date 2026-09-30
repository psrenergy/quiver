# 38 — Dart: one `_marshalGroupColumns` helper for the six group writers

**Batch** 4 · **Severity** medium · **Breaking** no (the jagged-column `ArgumentError` message gains the column name; its type is unchanged) · **Size** M · **Layers** Dart binding only (+ bindings/dart/AGENTS.md, bindings/python/AGENTS.md cross-reference)
**Depends on** 24 recommended first (it rewrites the body of `_marshalGroupColumn` in the same file; this plan only calls it) · **Overlaps with** 20 (`updateTimeSeriesFiles` in the same file — a different method), 24 (same file), 39 (Dart `element.dart`, different file)

## Why

`bindings/dart/lib/src/database_update.dart` has six columnar group writers — `updateVectorGroup`,
`updateVectorGroupByLabel`, `updateSetGroup`, `updateSetGroupByLabel`, `updateTimeSeriesGroup`,
`updateTimeSeriesGroupByLabel` (currently at ~L137, ~L209, ~L287, ~L359, ~L438, ~L510). Each
repeats the same ~40 lines before its one FFI call:

```dart
    _ensureNotClosed();

    final arena = Arena();
    try {
      if (data.isEmpty) {
        check(
          bindings.quiver_database_update_vector_group(
            _ptr,
            collection.toNativeUtf8(allocator: arena).cast(),
            group.toNativeUtf8(allocator: arena).cast(),
            id,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            0,
            0,
          ),
        );
        return;
      }

      // Validate equal lengths
      final rowCount = data.values.first.length;
      for (final entry in data.entries) {
        if (entry.value.length != rowCount) {
          throw ArgumentError('All column lists must have the same length');
        }
      }

      final columnCount = data.length;
      final columnNames = arena<Pointer<Char>>(columnCount);
      final columnTypes = arena<Int>(columnCount);
      final columnData = arena<Pointer<Void>>(columnCount);
      final columnHasValue = arena<Pointer<Uint8>>(columnCount);

      var i = 0;
      for (final entry in data.entries) {
        columnNames[i] = entry.key.toNativeUtf8(allocator: arena).cast();
        final column = _marshalGroupColumn(arena, entry.key, entry.value);
        columnTypes[i] = column.type;
        columnData[i] = column.data;
        columnHasValue[i] = column.hasValue;
        i++;
      }

      check(
        bindings.quiver_database_update_vector_group(
          _ptr, ..., columnNames, columnTypes, columnData, columnHasValue, columnCount, rowCount,
        ),
      );
    } finally {
      arena.releaseAll();
    }
```

The six copies differ only in the `bindings.quiver_database_update_*` symbol and whether the 4th
argument is an `int id` or a label string. That is roughly 280 duplicated lines, and each copy's
jagged-column error names no column. Python already has the whole-map helper:
`_marshal_group_columns(data)` in `bindings/python/src/quiverdb/database.py`, called by all six
Python writers, whose error is
`ValueError(f"All column lists must have the same length, got {len(values)} for '{name}'")`.

Principles: readability, delete duplication.

## Constraints and decisions

- **Root "Do Not Fix":** "Collapsing per-method FFI boilerplate in Dart/Python into
  closure-parameterized helpers". This plan does **not** do that. Each writer keeps its own explicit
  `check(bindings.quiver_database_update_*(...))` call with its own argument list, and nothing is
  passed a function. The helper only builds the four parallel arrays, the same kind of pure
  marshaller as the existing `_marshalGroupColumn`, which the house style already has.
- **Maintainer notes (binding):**
  - The empty map is handled inside the helper: return `nullptr`s and zero counts, and allocate
    nothing.
  - `upsertTimeSeriesRow`/`upsertTimeSeriesRowByLabel` keep calling the per-cell
    `_marshalGroupColumn` directly. The row-upsert C signature has no mask array and no row count,
    for the same reason Python keeps `_marshal_row_columns` separate.
  - Match Python's length error text.
- The error stays `ArgumentError`, like every other pre-FFI marshalling error in this file (root
  "Error Messages" exception for pre-FFI type marshalling, which should name the offending column).
- Behaviour for a named-but-empty column (`{'x': []}`) is unchanged: the helper allocates and
  marshals exactly what today's inline code does, and the C API rejects it ("named column with no
  rows is an error").

## Changes

### 1. `bindings/dart/lib/src/database_update.dart` — add `_marshalGroupColumns`

Place it directly above `_marshalGroupColumn`, whose doc comment starts
`/// Marshals one vector/set/time-series column into arena-allocated typed + mask arrays,` and sits
at about L718. It goes inside the `extension DatabaseUpdate on Database` block, like its sibling.

```dart
  /// Marshals a whole column-oriented group payload into the four parallel arena-allocated arrays
  /// the columnar C group writers take (names, type tags, data pointers, per-cell masks), plus the
  /// column and row counts. Shared by the six group writers (vector / set / time-series, by id and
  /// by label); each still makes its own C call. An empty map (clear the group) yields NULL arrays
  /// and zero counts and allocates nothing. Jagged columns throw [ArgumentError] naming the column.
  ({
    Pointer<Pointer<Char>> names,
    Pointer<Int> types,
    Pointer<Pointer<Void>> data,
    Pointer<Pointer<Uint8>> hasValue,
    int columnCount,
    int rowCount,
  })
  _marshalGroupColumns(Arena arena, Map<String, List<Object?>> data) {
    if (data.isEmpty) {
      return (
        names: nullptr,
        types: nullptr,
        data: nullptr,
        hasValue: nullptr,
        columnCount: 0,
        rowCount: 0,
      );
    }

    final rowCount = data.values.first.length;
    for (final entry in data.entries) {
      if (entry.value.length != rowCount) {
        throw ArgumentError(
          "All column lists must have the same length, got ${entry.value.length} for '${entry.key}'",
        );
      }
    }

    final columnCount = data.length;
    final names = arena<Pointer<Char>>(columnCount);
    final types = arena<Int>(columnCount);
    final columnData = arena<Pointer<Void>>(columnCount);
    final hasValue = arena<Pointer<Uint8>>(columnCount);

    var i = 0;
    for (final entry in data.entries) {
      names[i] = entry.key.toNativeUtf8(allocator: arena).cast();
      final column = _marshalGroupColumn(arena, entry.key, entry.value);
      types[i] = column.type;
      columnData[i] = column.data;
      hasValue[i] = column.hasValue;
      i++;
    }

    return (
      names: names,
      types: types,
      data: columnData,
      hasValue: hasValue,
      columnCount: columnCount,
      rowCount: rowCount,
    );
  }
```

`nullptr` is `Pointer<Never>`, assignable to every `Pointer<T>` record field. The local variable is
named `columnData`, not `data`, because the parameter is already called `data`.

### 2. Rewrite the six writers

Keep each method's doc comment, signature and `_ensureNotClosed()`. Replace the body after
`_ensureNotClosed();`. Here is the complete new `updateVectorGroup` body:

```dart
    _ensureNotClosed();

    final arena = Arena();
    try {
      final cols = _marshalGroupColumns(arena, data);
      check(
        bindings.quiver_database_update_vector_group(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          group.toNativeUtf8(allocator: arena).cast(),
          id,
          cols.names,
          cols.types,
          cols.data,
          cols.hasValue,
          cols.columnCount,
          cols.rowCount,
        ),
      );
    } finally {
      arena.releaseAll();
    }
```

The other five follow the same pattern. Only the C symbol and the 4th argument change:

| Method | C symbol | 4th argument |
|---|---|---|
| `updateVectorGroup` | `quiver_database_update_vector_group` | `id` |
| `updateVectorGroupByLabel` | `quiver_database_update_vector_group_by_label` | `label.toNativeUtf8(allocator: arena).cast()` |
| `updateSetGroup` | `quiver_database_update_set_group` | `id` |
| `updateSetGroupByLabel` | `quiver_database_update_set_group_by_label` | `label.toNativeUtf8(allocator: arena).cast()` |
| `updateTimeSeriesGroup` | `quiver_database_update_time_series_group` | `id` |
| `updateTimeSeriesGroupByLabel` | `quiver_database_update_time_series_group_by_label` | `label.toNativeUtf8(allocator: arena).cast()` |

In each method, delete:
- the `if (data.isEmpty) { check(...nullptr...); return; }` block,
- the `// Validate equal lengths` loop,
- the four `arena<...>(columnCount)` allocations,
- the `var i = 0; for (...) { ... }` fill loop.

Do not touch `upsertTimeSeriesRow`/`upsertTimeSeriesRowByLabel` (currently ~L580-670). They keep
calling `_marshalGroupColumn(arena, entry.key, [entry.value])` directly.

Sanity check after editing: `grep -n "_marshalGroupColumn(" bindings/dart/lib/src/database_update.dart`
must show exactly three calls: one inside `_marshalGroupColumns` and two in the upserts. It must
also show the definition line. `grep -c "All column lists must have the same length"` must print 1.

## Tests

### `bindings/dart/test/database_update_test.dart` — the length error names the column

Tighten the existing test `test('rejects columns of differing length', ...)` (currently ~L1741) to
pin the new message. Current:

```dart
        expect(
          () => db.updateVectorGroup('Child', 'refs', 1, {
            'parent_ref': [1, 2],
            'vector_index': [1],
          }),
          throwsA(isA<ArgumentError>()),
        );
```

New:

```dart
        expect(
          () => db.updateVectorGroup('Child', 'refs', 1, {
            'parent_ref': [1, 2],
            'vector_index': [1],
          }),
          throwsA(
            isA<ArgumentError>().having(
              (e) => e.message,
              'message',
              "All column lists must have the same length, got 1 for 'vector_index'",
            ),
          ),
        );
```

(Map literal iteration order is insertion order in Dart, so `parent_ref` sets `rowCount = 2` and
`vector_index` is the jagged one.)

### `bindings/dart/test/database_time_series_group_test.dart`

Tighten `test('rejects mismatched list lengths', ...)` (currently ~L370) the same way. `date_time`
has length 2 and `temperature` has length 1, so the message is
`"All column lists must have the same length, got 1 for 'temperature'"`.

### Coverage of the empty-map and by-label paths

These already exist and must pass unchanged:
- The clear tests for each writer. Find them with
  `grep -n "{})" bindings/dart/test/database_update_test.dart bindings/dart/test/database_time_series_group_test.dart`,
  for example `updateVectorGroupByLabel writes and clears only that element`.
- The set-group and time-series-by-label tests in `database_update_test.dart` and
  `database_time_series_group_test.dart`.

If any of the six writers lacks an empty-map clear test, add one following its neighbours' pattern:
write one row, call with `{}`, and assert that the reader returns empty. Check with
`grep -n "ByLabel(.*{})\|Group(.*{})" bindings/dart/test/*.dart`.

## Docs and changelog

- `bindings/dart/AGENTS.md`, "Marshaling idiom" bullet (currently ~L77). Current text:
  > Typed columns go through the shared private `_marshalGroupColumn(Arena, String, List<Object?>)`
  > (used by `updateTimeSeriesGroup`, `upsertTimeSeriesRow`, `upsertTimeSeriesRowByLabel`,
  > `updateVectorGroup`, `updateSetGroup` and the group writers' `ByLabel` forms);

  New text:
  > The six columnar group writers (`updateVectorGroup`/`updateSetGroup`/`updateTimeSeriesGroup`
  > and their `ByLabel` forms) marshal the whole payload through `_marshalGroupColumns(Arena, Map)`
  > (empty map → NULL arrays; jagged columns → `ArgumentError` naming the column), which calls the
  > per-column `_marshalGroupColumn(Arena, String, List<Object?>)`. The two row upserts call
  > `_marshalGroupColumn` directly (no mask in the row C signature);

  Also update the sentence further down (currently ~L138-141) that says "That branch covers all
  eight call sites (...)". It stays true, because `_marshalGroupColumn` still serves all eight
  paths, six of them through `_marshalGroupColumns`. Append "(six of them via
  `_marshalGroupColumns`)".
- `bindings/python/AGENTS.md`, the `_marshal_group_columns` bullet: "same name as Dart's
  `_marshalGroupColumn`" becomes "Dart's counterpart is `_marshalGroupColumns`".
- `CHANGELOG.md`: no entry needed. The only user-visible difference is that the message now names
  the column, which does not break anyone. If you want to mention it, add one line under
  `## [0.12.0] — unreleased` → `### Changed`: "Dart: the group writers' jagged-column
  `ArgumentError` names the offending column."

## Verification

From the repo root:
1. `cmake --build build --config Debug`. The Dart hook builds its own copy, so if the C API has
   not changed this only confirms the core builds.
2. `bindings/dart/test/test.bat test/database_update_test.dart`
3. `bindings/dart/test/test.bat test/database_time_series_group_test.dart`
4. `bindings/dart/test/test.bat`, the full Dart suite.
5. `cd bindings/dart && dart analyze`: no new warnings.
6. `scripts/format.bat` (dart format).

## Acceptance criteria

- [x] `_marshalGroupColumns` exists, and the six writers call it. Each still has its own explicit
      `check(bindings.quiver_database_update_*(...))`.
- [x] No `data.isEmpty` branch and no `// Validate equal lengths` loop remain in the six writers.
- [x] The upserts still call `_marshalGroupColumn` directly.
- [x] Two tightened message tests pass. The full Dart suite is green. `dart analyze` is clean.
- [x] Both AGENTS.md files are updated.

## Pitfalls

- Name the local array `columnData`. The parameter is `data`, and shadowing it breaks the fill loop.
- The record type's field named `data` does not clash with the parameter, because record fields
  are accessed as `cols.data`.
- If plan 24 has landed, `_marshalGroupColumn`'s internals changed but its signature
  `(Arena, String, List<Object?>)` and its return record did not. This plan is unaffected.
- Do not allocate `arena<...>(0)` for the empty map. `package:ffi`'s allocator can throw on a zero
  size on some platforms. That is why the helper returns `nullptr` before any allocation.

## Out of scope

- The upsert marshalling.
- Any change to `_marshalGroupColumn`'s typing rules (plan 24).
- Python/JS/Julia.

## Implementation notes

- **Master merge**: `git fetch origin` + `git merge origin/master` reported "Already up to date";
  `rs/plan38` was at `origin/master` (`9a0e651`, plans 32-37 landed). None of those touched
  `database_update.dart`, the two test files or either AGENTS.md; the edits applied at the function
  anchors above. Plan 24 (the dependency) is `ac3d70e`.
- **Test first**: with only the two message tests tightened, both failed against the old code, e.g.
  `Which: threw ArgumentError:<Invalid argument(s): All column lists must have the same length>
  which has 'message' with value 'All column lists must have the same length' which is different`
  (`database_update_test.dart` and `database_time_series_group_test.dart`, `+92 -2` combined).
  After the change: `database_update_test.dart` 78/78, `database_time_series_group_test.dart` 16/16,
  full Dart suite 442 passed. `dart analyze`: 7 pre-existing infos, none in files touched here.
  `cmake --build` had no work to do (no C API change).
- **Drift fixed**:
  - The CHANGELOG's current section is `## [0.12.6] — unreleased`, not `[0.12.0]`. The optional
    one-line entry went there under `### Changed`, non-breaking, with no version bump.
  - `updateSetGroup` (by id) was the only one of the six writers with no empty-map clear test. Added
    `updateSetGroup replaces rows and clears on an empty map` next to the vector version, per the
    plan's "if any writer lacks one" step.
  - In `bindings/dart/AGENTS.md`'s marshaling bullet, "the group writers' clear paths" (the writers
    that pass `nullptr` for an empty array) now reads "`_marshalGroupColumns` (empty map)", since
    that is where the clear path lives now. The "eight call sites" parenthesis reads "…every
    `ByLabel` form; six of them via `_marshalGroupColumns`".
- **Adversarial review** (3 read-only agents: behaviour parity, house style / doc accuracy, test
  coverage): no bugs, no doc inaccuracies. One nit, left as is: the jagged-length path is tested
  through `updateVectorGroup` and `updateTimeSeriesGroup` only, and all six writers now share that
  one code path.
- **`scripts/format.bat`**: `dart format` changed 0 files. As in plan 37, the biome step rewrote 43
  untouched `bindings/js` files from CRLF to LF with no content diff (`core.autocrlf=true`). They
  were restored and are not in this commit.
- **For later plans**: `_marshalGroupColumn` still requests `arena<Uint8>(0)` (the mask) for a
  named-but-empty column (`{'x': []}`). On POSIX, package:ffi can throw
  `ArgumentError('Could not allocate 0 bytes.')` there before the C API's "named column with no
  rows" error is reached, and Windows test runs do not see it. This predates plan 38 and is out of its
  scope; the fix is to skip the allocation (or use `nullptr`) when `values` is empty. The same
  applies to `upsertTimeSeriesRow*` with an empty `row` map, which calls `arena(0)` for its three
  arrays.
