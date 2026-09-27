# 66 — Assert `current_version == 3` after `from_migrations` in the C API, Lua, Python and JS

**Batch** 7 · **Severity** low · **Breaking** no · **Size** S · **Layers** tests only (C API, Lua, Python, JS)
**Depends on** none · **Overlaps with** 67 (edits other tests in the same lifecycle files), 69 (C API test hygiene)

## Why

`tests/schemas/migrations/` has three numbered migrations (`1/`, `2/`, `3/`), so `from_migrations`
must leave `current_version() == 3`. C++ (`tests/test_database_lifecycle.cpp` ~L245/280/306), Julia
(`test_database_lifecycle.jl` ~L61/136) and Dart (`database_lifecycle_test.dart` ~L207) assert
exactly that. The other four layers only check a version-0 database, or only the value's type. A
wrapper that returned a constant 0, or read the wrong out-slot, would pass all of them.

- C API `tests/test_c_api_database_lifecycle.cpp`, `TEST_F(TempFileFixture, CurrentVersionValid)`
  (~L186): opens `:memory:` and asserts `version == 0`. There is no successful `from_migrations`
  test at all. The only ones (~L223-242) cover NULL arguments and bad paths.
- Lua `tests/test_lua_runner_query.cpp`, `TEST_F(LuaRunnerTest, CurrentVersion)` (~L110):
  `assert(type(version) == "number")`.
- Python `bindings/python/tests/test_database_lifecycle.py`: `test_from_migrations_creates_database`
  (~L25) only asserts `db is not None`, and `test_current_version_returns_int` (~L81) asserts
  `result >= 0`. The Python and JS out-buffers are zero-initialised, so a 0 proves nothing.
- JS `bindings/js/test/database-lifecycle.test.ts`, "fromMigrations opens database and returns
  Database instance" (~L62): only `instanceof`. `introspection.test.ts` asserts `version >= 0`.

Root rule: tests in every layer.

## Constraints and decisions

- Change existing tests in place where possible, and keep the version-0 checks as exact `== 0` on a
  schema-created database.
- Reuse each suite's existing migrations-path helper: `SCHEMA_PATH("schemas/migrations")` in C++,
  `migrations_path` fixture in Python, `MIGRATIONS_PATH` in JS. Check each name at the top of the
  file.

## Changes (tests only)

### C API — `tests/test_c_api_database_lifecycle.cpp`

Add next to the from_migrations tests (~L223):
```cpp
TEST_F(TempFileFixture, FromMigrationsSetsCurrentVersion) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_migrations(":memory:", SCHEMA_PATH("schemas/migrations").c_str(), &options, &db),
              QUIVER_OK)
        << quiver_get_last_error();
    int64_t version = -1;
    EXPECT_EQ(quiver_database_current_version(db, &version), QUIVER_OK);
    EXPECT_EQ(version, 3);
    quiver_database_close(db);
}
```
Check the path macro's exact name and return type in `tests/test_utils.h`
(`grep -n "define SCHEMA_PATH\|define VALID_SCHEMA" tests/test_utils.h`).

### Lua — `tests/test_lua_runner_query.cpp`, `CurrentVersion`

```cpp
TEST_F(LuaRunnerTest, CurrentVersion) {
    auto db = quiver::Database::from_migrations(":memory:", SCHEMA_PATH("schemas/migrations"));
    quiver::LuaRunner lua(db);
    lua.run(R"(
        local version = db:current_version()
        assert(version == 3, "Expected version 3 after three migrations, got " .. tostring(version))
    )");
}
```
Keep the `Configuration` element creation only if the migrations define a `Configuration` table and
the test needs it. It does not for `current_version`.

### Python — `bindings/python/tests/test_database_lifecycle.py`

```python
def test_from_migrations_creates_database(migrations_path: Path, tmp_path: Path) -> None:
    db = Database.from_migrations(str(tmp_path / "test.db"), str(migrations_path))
    try:
        assert db.current_version() == 3
    finally:
        db.close()


def test_current_version_is_zero_for_a_schema_database(db: Database) -> None:
    assert db.current_version() == 0
```
The second test replaces `test_current_version_returns_int`. Check that the `db` fixture is
`from_schema`-created, i.e. version 0 (`grep -n "def db" bindings/python/tests/conftest.py`).

### JS — `bindings/js/test/database-lifecycle.test.ts`

In "fromMigrations opens database and returns Database instance", replace the two `expect`s with:
```ts
      expect(db instanceof Database).toBeTruthy();
      expect(db.currentVersion()).toBe(3);
```
In `bindings/js/test/introspection.test.ts`, "currentVersion returns a number >= 0": change the
assertion to `expect(db.currentVersion()).toBe(0);` (the database comes from `fromSchema`).

## Docs and changelog

None.

## Verification

From the repo root:
1. `cmake --build build --config Debug`
2. `./build/bin/quiver_c_tests.exe --gtest_filter=*FromMigrationsSetsCurrentVersion*:*CurrentVersion*`
3. `./build/bin/quiver_tests.exe --gtest_filter=LuaRunnerTest.CurrentVersion`
4. `bindings/python/tests/test.bat` and `bindings/js/test/test.bat`

## Acceptance criteria

- [ ] Each of the four layers asserts `== 3` after `from_migrations` and `== 0` for a schema database.
- [ ] All four suites green.

## Pitfalls

- If `tests/schemas/migrations` gains a fourth migration later, all seven layers must change
  together. That is the point of the exact assertion.

## Out of scope

- Julia, Dart, C++ (already exact).
