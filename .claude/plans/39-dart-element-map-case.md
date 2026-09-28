# 39 — Dart `Element.set`: delete the `Map` case that silently drops its key

**Batch** 4 · **Severity** medium · **Breaking** yes — Dart callers who pass a nested `Map` as an attribute value to `createElement`/`updateElement` must pass the columns flat (or use the group writers) · **Size** S · **Layers** Dart binding (+ CHANGELOG)
**Depends on** none · **Overlaps with** 24 (edits `Element._setMixedList` in the same file; independent lines), 38 (Dart `database_update.dart`, different file)

## Why

`bindings/dart/lib/src/element.dart`, `Element.set` (currently ~L59-95), has a case that flattens a
nested map and **discards its key**:

```dart
      case Map<String, Object?> v:
        for (final entry in v.entries) {
          set(entry.key, entry.value);
        }
```

and its doc line (currently ~L58):
```dart
  /// - `Map<String, Object?>` - recursively sets each entry as a separate attribute
```

So

```dart
db.createElement('Collection', {
  'label': 'x',
  'anything_at_all': {
    'date_time': [DateTime(1990, 1, 1)],
    'some_time_series_float': [1.0],
  },
});
```

behaves exactly like passing `date_time` and `some_time_series_float` flat. The key looks like it
selects a group (`some_time_series`), but it selects nothing: a typo in it is accepted silently, and
the columns are routed **by column name** — the fan-out the root AGENTS.md warns about under
"Whole-group writers" ("an array is written to *every* matching table").

It is Dart-only. The feature came in with #71 (commit e8740a7) in both Dart and Julia
(`Base.setindex!(el::Element, value::Dict{String, <:Any}, name::String)`); Julia removed it in
#198 (commit cd8b4e5). Python's `element.py` and JS's `create.ts` have no equivalent. Its only
user in the repo is the issue-70 regression test, which has no assertion:

`bindings/dart/test/issues_test.dart`:
```dart
    test('issue 70', () {
      final db = Database.fromMigrations(
        ':memory:',
        path.join(issuesPath, 'issue70'),
      );
      try {
        db.createElement('Collection', {
          'label': 'label',
          'some_time_series': {
            'date_time': [DateTime(1990, 1, 1)],
            'some_time_series_float': [1.0],
            'some_time_series_integer': [1],
          },
        });
      } finally {
        db.close();
      }
    });
```

Principles: Homogeneity (the other bindings have no such case), delete rather than deprecate,
clean over clever.

## Constraints and decisions

- **Maintainer decision (binding):** BREAKING, CHANGELOG under 0.12.0 (already the unreleased minor;
  no manifest bump). Rewrite the issue-70 test flat, with a `readTimeSeriesGroup` assertion.
- Do not add a new "maps are not supported" message: once the case is gone, a `Map` falls through
  to the existing `default:` branch, `ArgumentError("Unsupported type ${value.runtimeType} for
  '$name'")`, which already names the attribute (root rule: pre-FFI marshalling errors name the
  offending column and type).
- The right tools for "write this one named group" are `updateTimeSeriesGroup` /
  `updateVectorGroup` / `updateSetGroup` (root AGENTS.md "Prefer the group writers whenever one
  group is meant").

## Changes

### `bindings/dart/lib/src/element.dart`

1. Delete the doc line (currently ~L58):
   ```dart
   /// - `Map<String, Object?>` - recursively sets each entry as a separate attribute
   ```
2. Delete the case (currently ~L83-86):
   ```dart
         case Map<String, Object?> v:
           for (final entry in v.entries) {
             set(entry.key, entry.value);
           }
   ```
   Leave every other case, the `case List v:` fallback and `default:` untouched. Order still
   matters: `case List v` must stay after the typed `List<...>` cases.

No other source file references it (`grep -rn "Map<String, Object?> v" bindings/dart/lib` → only
this line).

## Tests

### Rewrite `bindings/dart/test/issues_test.dart` "issue 70"

Replace the body with a flat write and a real assertion. The migration
`tests/schemas/issues/issue70/1/up.sql` defines
`Collection_time_series_some_time_series(id, date_time TEXT, some_time_series_float REAL,
some_time_series_integer INTEGER, PRIMARY KEY (id, date_time))`.

```dart
    test('issue 70', () {
      final db = Database.fromMigrations(
        ':memory:',
        path.join(issuesPath, 'issue70'),
      );
      try {
        final id = db.createElement('Collection', {
          'label': 'label',
          'date_time': [DateTime(1990, 1, 1)],
          'some_time_series_float': [1.0],
          'some_time_series_integer': [1],
        });

        final ts = db.readTimeSeriesGroup('Collection', 'some_time_series', id);
        expect(ts['date_time'], equals([DateTime(1990, 1, 1)]));
        expect(ts['some_time_series_float'], equals([1.0]));
        expect(ts['some_time_series_integer'], equals([1]));
      } finally {
        db.close();
      }
    });
```

(`readTimeSeriesGroup` returns the dimension as local `DateTime`s — `stringToDateTime` in
`lib/src/date_time.dart` returns `DateTime(year, ...)`, and existing tests compare against
`DateTime(2024, 2, 1, 10, 0, 0)` — so `equals([DateTime(1990, 1, 1)])` matches.)

### Add a regression test for the rejection — `bindings/dart/test/element_test.dart`

Add inside `group('Element Set Values', () { ... })` (the file's first group, currently ~L9). Each
test in that group creates `final element = Element();` and releases it with `element.dispose()` in
`finally`, and so does this one:

```dart
    test('rejects a nested map value', () {
      final element = Element();
      try {
        expect(
          () => element.set('some_time_series', {'date_time': [DateTime(1990, 1, 1)]}),
          throwsA(
            isA<ArgumentError>().having(
              (e) => e.message,
              'message',
              contains("for 'some_time_series'"),
            ),
          ),
        );
      } finally {
        element.dispose();
      }
    });
```

Before the change this test fails, because the map is accepted. After the change it passes.

## Docs and changelog

- `bindings/dart/AGENTS.md`: no passage documents the Map case (`grep -n "Map<String, Object" bindings/dart/AGENTS.md`
  shows only the unrelated time-series read type) — no edit.
- There is no `bindings/dart/README.md`, so there is nothing to update there.
- `CHANGELOG.md`, `## [0.12.0] — unreleased` → `### Changed`:
  ```markdown
  - **BREAKING — Dart: `Element.set` (and so `createElement` / `updateElement`) no longer accepts a
    nested `Map` value.** It used to flatten the map and ignore its key, so
    `{'some_group': {'date_time': [...], 'value': [...]}}` behaved exactly like passing the columns
    flat, and a misspelled key was accepted silently. A `Map` now throws `ArgumentError`
    ("Unsupported type ... for '<name>'"). *Adapt:* pass the columns flat, or use
    `updateTimeSeriesGroup` / `updateVectorGroup` / `updateSetGroup` to write one named group.
  ```

## Verification

From the repo root:
1. `bindings/dart/test/test.bat test/issues_test.dart`
2. `bindings/dart/test/test.bat test/element_test.dart`
3. `bindings/dart/test/test.bat` — full Dart suite (catches any other test that relied on the Map
   case: `grep -rn "': {" bindings/dart/test/*.dart` — only `enumLabels` CSV arguments and the
   issue-70 site should appear, and `enumLabels` does not go through `Element.set`).
4. `cd bindings/dart && dart analyze`
5. `scripts/format.bat`

## Acceptance criteria

- [ ] No `case Map<String, Object?>` in `element.dart`; doc line removed.
- [ ] Issue-70 test writes flat and asserts the stored rows.
- [ ] New rejection test passes.
- [ ] Full Dart suite and `dart analyze` clean; BREAKING CHANGELOG entry present.

## Pitfalls

- `enumLabels: {...}` maps passed to `importCSV`/`exportCSV` are CSV options, not element values —
  they never reach `Element.set`; do not touch them.
- `Map<String, List<Object?>>` arguments to the group writers are the whole payload, not an element
  value — unaffected.

## Out of scope

- Adding a group-addressed `createElement` form.
- Julia/Python/JS (no equivalent feature).
