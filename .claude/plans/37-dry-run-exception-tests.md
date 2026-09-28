# 37 — Dart and Julia: test the dry-run wrapper's exception path

**Batch** 4 · **Severity** low · **Breaking** no · **Size** S · **Layers** Dart tests, Julia tests (no production code)
**Depends on** none · **Overlaps with** none (Python already has this test; plan 05 adds dry-run regression tests at the C++ layer only)

## Why

Both bindings hand-write the rethrow branch of their dry-run block wrapper, and neither tests it.

Dart, `bindings/dart/lib/src/database_transaction.dart`, `dryRun` (currently ~L85-101):
```dart
  T dryRun<T>(T Function(Database) fn) {
    beginDryRun();
    final T result;
    try {
      result = fn(this);
    } catch (_) {
      try {
        endDryRun();
      } catch (_) {
        // Best-effort only while an exception is already in flight, mirroring transaction()
      }
      rethrow;
    }
    // On the success path the rollback is this wrapper's whole promise -- let a failure surface.
    endDryRun();
    return result;
  }
```

Julia, `bindings/julia/src/database_transaction.jl`, `dry_run` (currently ~L62-77):
```julia
function dry_run(fn, db::Database)
    begin_dry_run!(db)
    result = try
        fn(db)
    catch
        # Best-effort only while an exception is already in flight, mirroring `transaction`.
        try
            end_dry_run!(db)
        catch
        end
        rethrow()
    end
    # On the success path the rollback is the wrapper's whole promise -- let a failure surface.
    end_dry_run!(db)
    return result
end
```

Existing coverage:
- Dart: `bindings/dart/test/lua_runner_test.dart`, `group('Database dry run', ...)` — the success
  path ("rolls back a script") and "endDryRun without a dry run throws". Nothing makes `fn` throw
  (`grep -n "dryRun" bindings/dart/test/*.dart`).
- Julia: `bindings/julia/test/test_lua_runner.jl`, `@testset "Dry Run"` — success path and
  `end_dry_run!` without a dry run. Nothing makes `fn` throw.
- Python covers it (`bindings/python/tests/test_lua_runner.py` `test_rolls_back_on_exception`,
  which raises inside `with db.dry_run()` and asserts the writes are gone and `in_dry_run()` is
  `False`).

If the catch branch broke (e.g. forgot `endDryRun()`), the handle would stay inside a dry run after
an exception: every later write in the process would be silently rolled back at the next
`end_dry_run`, and `in_dry_run()` would report `true`. Root AGENTS.md "Tests must exist in all
layers".

## Constraints and decisions

- Test-only. No production code changes.
- Put each test next to its binding's `transaction` exception test, where the other block wrapper
  is tested: Dart `bindings/dart/test/database_transaction_test.dart`
  (`test('transaction block rollback on exception', ...)`, currently ~L132), Julia
  `bindings/julia/test/test_database_transaction.jl` (`@testset "Transaction block rollback on
  exception"`, currently ~L107). Keeping the Lua-driven dry-run tests where they are is fine.
- Mirror the existing transaction tests' style (fixture paths, `Configuration` row, `try/finally
  db.close()` in Dart, `Quiver.close!(db)` in Julia).

## Changes

None to production code.

## Tests

### Dart — `bindings/dart/test/database_transaction_test.dart`

Add inside `group('Transaction', () { ... })`, right after `test('transaction block rollback on
exception', ...)`:

```dart
    test('dryRun block rolls back and ends the dry run on exception', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Config'});

        expect(
          () => db.dryRun((db) {
            db.createElement('Collection', {'label': 'Preview'});
            throw StateError('boom');
          }),
          throwsStateError,
        );

        expect(db.inDryRun(), isFalse);
        expect(db.readScalarStrings('Collection', 'label'), isEmpty);

        // The handle is usable normally afterwards: a plain write commits.
        db.createElement('Collection', {'label': 'After'});
        expect(db.readScalarStrings('Collection', 'label'), equals(['After']));
      } finally {
        db.close();
      }
    });
```

`throwsStateError` is a `package:test` matcher (already imported via `package:test/test.dart`).
The error thrown by `fn` must be the one that surfaces — `rethrow` preserves it.

### Julia — `bindings/julia/test/test_database_transaction.jl`

Add inside `@testset "Transaction" begin ... end`, right after `@testset "Transaction block rollback
on exception"`:

```julia
    @testset "Dry run block rolls back and ends on exception" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Config")

        @test_throws ErrorException begin
            Quiver.dry_run(db) do db
                Quiver.create_element!(db, "Collection"; label = "Preview")
                return error("boom")
            end
        end

        @test Quiver.in_dry_run(db) == false
        @test isempty(Quiver.read_scalar_strings(db, "Collection", "label"))

        # The handle is usable normally afterwards: a plain write commits.
        Quiver.create_element!(db, "Collection"; label = "After")
        @test Quiver.read_scalar_strings(db, "Collection", "label") == ["After"]

        Quiver.close!(db)
    end
```

Both tests pass at HEAD (the branch works today); they pin it. To see them fail, temporarily delete
the `endDryRun()` / `end_dry_run!(db)` call inside the catch branch: `inDryRun()` then stays `true`
and the "After" write is rolled back later, failing the last assertion. Do not commit that
experiment.

`read_scalar_strings` on a `NOT NULL` label column returns a concrete `Vector{String}` in Julia, so
`== ["After"]` compares cleanly.

## Docs and changelog

- No documentation change (the wrappers' behaviour is already documented in root AGENTS.md
  "Transaction block wrappers").
- No CHANGELOG entry (test-only).

## Verification

From the repo root:
1. `cmake --build build --config Debug`
2. `bindings/dart/test/test.bat test/database_transaction_test.dart`
3. `bindings/julia/test/test.bat test_database_transaction.jl`
4. Full suites: `bindings/dart/test/test.bat`, `bindings/julia/test/test.bat`.
5. `scripts/format.bat` (dart format + Julia formatter).

## Acceptance criteria

- [ ] One new Dart test and one new Julia testset exercising a throwing `fn` inside the dry-run
      wrapper, asserting the error surfaces, `in_dry_run` is false, the write is gone, and the
      handle works afterwards.
- [ ] Both suites green.

## Pitfalls

- Dart's `test.bat` runs from `bindings/dart` (`pushd %~dp0..`), so pass `test/<file>.dart`.
- Julia's `test.bat` takes a path relative to `bindings/julia/test/`.
- Do not catch the error inside `fn`; the wrapper must see it.

## Out of scope

- Any change to the wrappers themselves.
- Lua `db:dry_run` (covered by the C++ Lua suite).
