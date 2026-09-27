# 07 — list_{vector,set,time_series}_groups throw on an unknown collection

**Batch** 1 · **Severity** low · **Breaking** yes: any caller, in any layer, that passes a name that is not a table to `list_vector_groups` / `list_set_groups` / `list_time_series_groups` or to the `read_vectors_by_id` / `read_sets_by_id` composites now gets an error instead of an empty result · **Size** S · **Layers** C++ core (code); C API, Lua, Julia, Dart, Python, JS (tests only); root + `src/` CLAUDE.md; CHANGELOG

**Depends on** none · **Overlaps with**
- **57** (one `require_group_table` lookup) edits the same three function bodies later. Plan 57 must keep the `impl_->require_collection(...)` first line this plan adds. The maintainer said not to fold that refactor in here.
- **52** (Lua metadata-getter tests + C++ `list_vector/set_groups` tests) adds positive-path tests to `tests/test_database_metadata.cpp` and the Lua suites. This plan adds only the unknown-collection cases. Plan 52 must not add them again.
- **49 / 50** rewrite `list_vector_metadata_lua`, `list_set_metadata_lua`, `read_vectors_by_id_lua` and `read_sets_by_id_lua` in `src/lua_runner.cpp`. This plan does not touch that file. The Lua test added here must still pass after those plans, and it will as long as those functions still call `db.list_*_groups`.
- **02 / 03 / 04** edit other functions in `src/database_time_series.cpp`. Only the file is shared.
- **76** edits a different bullet of `src/CLAUDE.md`. Only the file is shared.
- **01–06** all append to `CHANGELOG.md` `### Changed` / `### Fixed` under 0.11.0. Append your entry, don't overwrite theirs.

## Why

`list_scalar_attributes` validates its collection. The three group listers don't.

`src/database_metadata.cpp`, `Database::list_scalar_attributes` (currently ~L70):
```cpp
std::vector<ScalarMetadata> Database::list_scalar_attributes(const std::string& collection) const {
    impl_->require_collection(collection, "list_scalar_attributes");
```
`Database::list_vector_groups` (currently ~L82) and `Database::list_set_groups` (currently ~L92):
```cpp
std::vector<GroupMetadata> Database::list_vector_groups(const std::string& collection) const {
    impl_->require_schema();
```
`src/database_time_series.cpp`, `Database::list_time_series_groups` (currently ~L57):
```cpp
std::vector<GroupMetadata> Database::list_time_series_groups(const std::string& collection) const {
    impl_->require_schema();
```
`Schema::group_names` (`src/schema.cpp`, currently ~L237) filters `table_names()` by the prefix `collection + "_vector_"` and so on. For a name that is not a table it matches nothing and returns `{}`. A typo therefore looks exactly like a collection that has no groups.

Reproduction (Python; every layer behaves the same way), with `tests/schemas/valid/collections.sql`:
```python
db = Database.from_schema(":memory:", "tests/schemas/valid/collections.sql")
db.list_scalar_attributes("Colection")   # QuiverError: Cannot list_scalar_attributes: collection not found: Colection
db.list_vector_groups("Colection")       # []   <- silent
db.list_time_series_groups("Colection")  # []   <- silent
db.read_scalars_by_id("Colection", 1)    # QuiverError (goes through list_scalar_attributes)
db.read_vectors_by_id("Colection", 1)    # {}   <- silent (loops over list_vector_groups)
db.read_sets_by_id("Colection", 1)       # {}   <- silent
```
All other collection-scoped methods start with `Impl::require_collection`: every `get_*_metadata`, every `read_*`, `has_time_series_files` and `number_of_elements`. These three listers are the only ones that skip it. That breaks the project's missing-target-throws stance (root CLAUDE.md: `update_element` / `delete_element` "throw on a missing id … not a silent no-op") and Homogeneity. It also makes `read_scalars_by_id` and `read_vectors_by_id` disagree on the same bad name.

One test pins the current behaviour. `tests/test_database_time_series_group.cpp`, `TEST(Database, TimeSeriesCollectionNotFound)` (currently ~L176):
```cpp
    // Nonexistent collection returns empty list (matches list_vector_groups behavior)
    auto groups = db.list_time_series_groups("NonexistentCollection");
    EXPECT_TRUE(groups.empty());
```
Its only justification is the sibling lister's behaviour. No CLAUDE.md, CHANGELOG entry, `lua-api.ts` passage or Design Decision records the empty result as intended.

Current behaviour confirmed with the built binary: `./build/bin/quiver_tests.exe --gtest_filter=Database.TimeSeriesCollectionNotFound` passes (empty list).

## Constraints and decisions

- **Maintainer decision (binding):** "BREAKING; CHANGELOG under 0.11.0. Do not fold in the require_group_table refactor (plan 57)." The three bodies stay as they are apart from their first line. Don't collapse them into one loop.
- **Root CLAUDE.md, Changelog + Versioning:** 0.11.0 is unreleased and is already the minor bump over 0.10.9. The entry goes under `## [0.11.0] — unreleased`, prefixed **BREAKING**, and says what a caller must do. **No manifest bump.** (Correction: the facts verifier asked for a "0.x minor version bump across all five manifests". That bump has already happened, so it is not part of this change.)
- **Root CLAUDE.md, C++ Error Message Patterns, Pattern 1:** `"Cannot {operation}: {reason}"`, where `{operation}` is the public method called. `Impl::require_collection(collection, operation)` (`src/database_impl.h`, currently ~L91) already produces exactly `Cannot <op>: collection not found: <collection>`. Reuse it; add no new helper or message.
- **Root CLAUDE.md, Error Messages:** bindings never craft their own messages. The composites `read_vectors_by_id` / `read_sets_by_id` are binding-side conveniences with no C++ counterpart, so the error they surface names the core call they delegate to (`Cannot list_vector_groups: …`). `read_scalars_by_id` already does the same today (`Cannot list_scalar_attributes: …`). Leave it; re-wrapping it in a binding would break the rule.
- **Root CLAUDE.md, "Intelligence: Logic resides in C++ layer":** the fix goes in the three C++ functions only. The C API wrappers (`src/c/database_metadata.cpp` `quiver_database_list_vector_groups` / `_list_set_groups`, `src/c/database_time_series.cpp` `quiver_database_list_time_series_groups`) already catch `std::exception` and call `quiver_set_last_error(e.what())`. The Lua wrappers (`src/lua_runner.cpp` `list_vector_metadata_lua`, `list_set_metadata_lua`, `list_time_series_groups_lua`, `read_vectors_by_id_lua`, `read_sets_by_id_lua`) and every binding's `check(...)` pass the error through. No C API signature changes, so no FFI regeneration (Julia `c_api.jl`, Dart `bindings.dart`, Python `_c_api.py`, JS `loader.ts` all stay untouched).
- **Root CLAUDE.md, tests in every layer:** the new behaviour is visible in C++, the C API, Lua and all four FFI bindings (through their listers and through their binding-side composites), so each layer gets one small test. (Correction: the policy verifier said one C API test is enough and the bindings need nothing. The bindings need no *code*, but the project rule asks for a test wherever the behaviour is visible, and each binding's composite is binding code that now fails differently.)
- **Root CLAUDE.md, Self-Updating:** update `src/CLAUDE.md` (nearest to the change) and the root Core API line that lists these functions.
- **An existing collection with no groups of that kind still returns an empty list.** The tests that pin that stay unchanged: `ListTimeSeriesGroupsEmpty` (C++/C API, `"Configuration"`), Python `test_read_vectors_by_id_no_groups` / `test_read_sets_by_id_no_groups`, Lua `ReadVectorsById` / `ReadSetsById` (`"Configuration"`), and the Dart/Julia `"Configuration"` tests.

Rejected alternatives:
- Collapse the three list bodies into one loop over a `require_group_table` helper: plan 57 owns that refactor, and the maintainer explicitly excluded it.
- Validate in each binding's composite instead: duplicates logic across four bindings plus Lua and leaves the three core listers silent.
- Have the composites catch the error and rethrow it naming `read_vectors_by_id`: that would be a binding-crafted message, which the Error Messages principle forbids.
- Add a `has_collection` API so callers can probe without catching: YAGNI. Nobody has asked for it, and the error is the probe.

## Changes

### 1. `src/database_metadata.cpp`, `Database::list_vector_groups` (currently ~L82)

Current:
```cpp
std::vector<GroupMetadata> Database::list_vector_groups(const std::string& collection) const {
    impl_->require_schema();

    std::vector<GroupMetadata> result;
```
New:
```cpp
std::vector<GroupMetadata> Database::list_vector_groups(const std::string& collection) const {
    impl_->require_collection(collection, "list_vector_groups");

    std::vector<GroupMetadata> result;
```
Why: `require_collection` calls `require_schema()` itself (see `Impl::require_collection` in `src/database_impl.h`), so lazy schema loading is unchanged. It then throws `Cannot list_vector_groups: collection not found: <c>` for a non-table.

### 2. `src/database_metadata.cpp`, `Database::list_set_groups` (currently ~L92)

Current:
```cpp
std::vector<GroupMetadata> Database::list_set_groups(const std::string& collection) const {
    impl_->require_schema();
```
New:
```cpp
std::vector<GroupMetadata> Database::list_set_groups(const std::string& collection) const {
    impl_->require_collection(collection, "list_set_groups");
```

### 3. `src/database_time_series.cpp`, `Database::list_time_series_groups` (currently ~L57)

Current:
```cpp
std::vector<GroupMetadata> Database::list_time_series_groups(const std::string& collection) const {
    impl_->require_schema();
```
New:
```cpp
std::vector<GroupMetadata> Database::list_time_series_groups(const std::string& collection) const {
    impl_->require_collection(collection, "list_time_series_groups");
```

Nothing else changes in any layer. Checked callers (all of them, from `grep -rn "list_vector_groups\|list_set_groups\|list_time_series_groups"`):
- C API: `src/c/database_metadata.cpp` (two), `src/c/database_time_series.cpp` (one). Their existing catch blocks already forward the message.
- Lua: `src/lua_runner.cpp`, `list_vector_metadata_lua` / `list_set_metadata_lua` / `list_time_series_groups_lua` / `read_vectors_by_id_lua` / `read_sets_by_id_lua`. sol2 propagates the exception as a Lua error.
- Julia `bindings/julia/src/database_metadata.jl` (`list_*_groups`) and `database_read.jl` (`read_vectors_by_id`, `read_sets_by_id`); Dart `lib/src/database_metadata.dart` and `lib/src/database_read.dart` (`readVectorsById`, `readSetsById`); Python `src/quiverdb/database.py` (`list_*_groups`, `read_vectors_by_id`, `read_sets_by_id`); JS `src/metadata.ts` and `src/composites.ts`. All go through `check(...)`.
- No other C++ core function calls these three (describe/export/import use `Schema::group_names` directly and already validate their own collection).

## Tests

Before the fix, every new test fails: the call returns an empty list / `QUIVER_OK` / `{}` instead of raising. The flipped C++ test fails with "Expected list_time_series_groups to reject an unknown collection".

### C++ — `tests/test_database_time_series_group.cpp`, flip `TEST(Database, TimeSeriesCollectionNotFound)` (currently ~L176)

Old body:
```cpp
TEST(Database, TimeSeriesCollectionNotFound) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    // Nonexistent collection returns empty list (matches list_vector_groups behavior)
    auto groups = db.list_time_series_groups("NonexistentCollection");
    EXPECT_TRUE(groups.empty());
}
```
New body:
```cpp
TEST(Database, TimeSeriesCollectionNotFound) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    try {
        (void)db.list_time_series_groups("NonexistentCollection");
        FAIL() << "Expected list_time_series_groups to reject an unknown collection";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "Cannot list_time_series_groups: collection not found: NonexistentCollection");
    }
}
```
(The try/`FAIL()`/`EXPECT_STREQ` shape copies `TEST(Database, NumberOfElementsNotFound)` in `tests/test_database_read_scalar.cpp`. `FAIL()` returns from the test and does not throw, so the `catch` cannot swallow it.)

### C++ — `tests/test_database_metadata.cpp`, new test appended at the end of the file (after `TEST(Database, GetSetMetadataNonForeignKeyColumn)`)

```cpp
// ============================================================================
// List groups: unknown collection
// ============================================================================

TEST(Database, ListGroupsCollectionNotFound) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    try {
        (void)db.list_vector_groups("Nope");
        FAIL() << "Expected list_vector_groups to reject an unknown collection";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "Cannot list_vector_groups: collection not found: Nope");
    }

    try {
        (void)db.list_set_groups("Nope");
        FAIL() << "Expected list_set_groups to reject an unknown collection";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "Cannot list_set_groups: collection not found: Nope");
    }
}
```
The existing includes (`test_utils.h`, `<gtest/gtest.h>`, `<quiver/database.h>`) are enough; other test files catch `std::runtime_error` with the same set.

### C API — `tests/test_c_api_database_metadata.cpp`, new test appended at the end of the file (after `TEST(DatabaseCApiMetadata, SummarizeCollectionNotFound)`, so the anonymous-namespace `open_collections()` helper defined above `DescribeReturnsText` is in scope)

```cpp
TEST(DatabaseCApiMetadata, ListGroupsCollectionNotFound) {
    quiver_database_t* db = open_collections();

    quiver_group_metadata_t* groups = nullptr;
    size_t count = 0;

    EXPECT_EQ(quiver_database_list_vector_groups(db, "Nope", &groups, &count), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Cannot list_vector_groups: collection not found: Nope");

    EXPECT_EQ(quiver_database_list_set_groups(db, "Nope", &groups, &count), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Cannot list_set_groups: collection not found: Nope");

    EXPECT_EQ(quiver_database_list_time_series_groups(db, "Nope", &groups, &count), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Cannot list_time_series_groups: collection not found: Nope");

    // Nothing was allocated, so there is nothing to free.
    EXPECT_EQ(groups, nullptr);

    quiver_database_close(db);
}
```
`quiver_get_last_error` is declared in `include/quiver/c/common.h`, which `quiver/c/database.h` includes.

### Lua — `tests/test_lua_runner_read.cpp`, new test directly after `TEST_F(LuaRunnerTest, NumberOfElementsUnknownCollection)` (currently ~L545)

```cpp
TEST_F(LuaRunnerTest, ListGroupsUnknownCollection) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    quiver::LuaRunner lua(db);

    expect_lua_error(lua, R"(db:list_vector_groups("Nope"))", "Cannot list_vector_groups: collection not found: Nope");
    expect_lua_error(lua, R"(db:list_set_groups("Nope"))", "Cannot list_set_groups: collection not found: Nope");
    expect_lua_error(
        lua, R"(db:list_time_series_groups("Nope"))", "Cannot list_time_series_groups: collection not found: Nope");

    // The composites inherit the throw instead of returning an empty table.
    expect_lua_error(
        lua, R"(db:read_vectors_by_id("Nope", 1))", "Cannot list_vector_groups: collection not found: Nope");
    expect_lua_error(lua, R"(db:read_sets_by_id("Nope", 1))", "Cannot list_set_groups: collection not found: Nope");
}
```
`expect_lua_error` (`tests/test_lua_runner.h`) asserts a throw *and* a substring. Existing tests call it more than once on one runner (e.g. `TEST_F(LuaRunner_ReadCsv, EscapingPathThrowsForReadCsv)` in `tests/test_lua_runner_read_csv.cpp`). Let clang-format settle the line wrapping.

### Julia — `bindings/julia/test/test_database_metadata.jl`, new `@testset` inside the outer `@testset "Metadata"`, right after `@testset "List Set Groups" … end` (before the `end` that closes `"Metadata"`, currently ~L198)

```julia
    @testset "List Groups Unknown Collection" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        exc = @test_throws Quiver.DatabaseException Quiver.list_vector_groups(db, "Nope")
        @test exc.value.msg == "Cannot list_vector_groups: collection not found: Nope"
        exc = @test_throws Quiver.DatabaseException Quiver.list_set_groups(db, "Nope")
        @test exc.value.msg == "Cannot list_set_groups: collection not found: Nope"
        exc = @test_throws Quiver.DatabaseException Quiver.list_time_series_groups(db, "Nope")
        @test exc.value.msg == "Cannot list_time_series_groups: collection not found: Nope"

        # The composites inherit the throw instead of returning an empty Dict
        exc = @test_throws Quiver.DatabaseException Quiver.read_vectors_by_id(db, "Nope", 1)
        @test exc.value.msg == "Cannot list_vector_groups: collection not found: Nope"
        exc = @test_throws Quiver.DatabaseException Quiver.read_sets_by_id(db, "Nope", 1)
        @test exc.value.msg == "Cannot list_set_groups: collection not found: Nope"

        Quiver.close!(db)
    end
```
(`read_vectors_by_id(db::Database, collection::String, id::Int64)`: the literal `1` is `Int64` on the 64-bit hosts CI uses. The `exc.value.msg` idiom is from `test_database_lifecycle.jl`.)

### Dart — `bindings/dart/test/metadata_test.dart`, new `group` right after `group('List Set Groups', …)` (the last group before `main`'s closing `}`)

```dart
  group('List Groups Unknown Collection', () {
    test('listers and composites throw instead of returning empty', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      Matcher throwsNotFound(String operation) => throwsA(
        isA<DatabaseException>().having(
          (e) => e.message,
          'message',
          equals('Cannot $operation: collection not found: Nope'),
        ),
      );
      try {
        expect(() => db.listVectorGroups('Nope'), throwsNotFound('list_vector_groups'));
        expect(() => db.listSetGroups('Nope'), throwsNotFound('list_set_groups'));
        expect(() => db.listTimeSeriesGroups('Nope'), throwsNotFound('list_time_series_groups'));
        // The composites inherit the throw instead of returning an empty map.
        expect(() => db.readVectorsById('Nope', 1), throwsNotFound('list_vector_groups'));
        expect(() => db.readSetsById('Nope', 1), throwsNotFound('list_set_groups'));
      } finally {
        db.close();
      }
    });
  });
```
`DatabaseException` is exported by `package:quiverdb/quiverdb.dart` (already imported; `database_lifecycle_test.dart` uses the same `having(... e.message ...)` matcher).

### Python — `bindings/python/tests/test_database_metadata.py`, new method at the end of `class TestListGroups` (after `test_list_time_series_groups`, before `class TestMetadataFrozen`)

```python
    def test_list_groups_unknown_collection(self, collections_db: Database) -> None:
        with pytest.raises(QuiverError, match="^Cannot list_vector_groups: collection not found: Nope$"):
            collections_db.list_vector_groups("Nope")
        with pytest.raises(QuiverError, match="^Cannot list_set_groups: collection not found: Nope$"):
            collections_db.list_set_groups("Nope")
        with pytest.raises(QuiverError, match="^Cannot list_time_series_groups: collection not found: Nope$"):
            collections_db.list_time_series_groups("Nope")
        # The composites inherit the throw instead of returning an empty dict.
        with pytest.raises(QuiverError, match="^Cannot list_vector_groups: collection not found: Nope$"):
            collections_db.read_vectors_by_id("Nope", 1)
        with pytest.raises(QuiverError, match="^Cannot list_set_groups: collection not found: Nope$"):
            collections_db.read_sets_by_id("Nope", 1)
```
`QuiverError` and `pytest` are already imported in that file. `check()` (`_helpers.py`) raises `QuiverError(detail)`, so `str(e)` is exactly the C message.

### JS — `bindings/js/test/database-metadata.test.ts`, new `test` at the end of `describe("listVectorGroups / listSetGroups / listTimeSeriesGroups", …)` (after the `listTimeSeriesGroups returns time series groups with dimensionColumn` test)

```ts
  test("an unknown collection throws in every lister and in the composites", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      expect(() => db.listVectorGroups("Nope")).toThrow(
        "Cannot list_vector_groups: collection not found: Nope",
      );
      expect(() => db.listSetGroups("Nope")).toThrow(
        "Cannot list_set_groups: collection not found: Nope",
      );
      expect(() => db.listTimeSeriesGroups("Nope")).toThrow(
        "Cannot list_time_series_groups: collection not found: Nope",
      );
      // The composites inherit the throw instead of returning an empty object.
      expect(() => db.readVectorsById("Nope", 1)).toThrow(
        "Cannot list_vector_groups: collection not found: Nope",
      );
      expect(() => db.readSetsById("Nope", 1)).toThrow(
        "Cannot list_set_groups: collection not found: Nope",
      );
    } finally {
      db.close();
    }
  });
```
`src/index.ts` imports `./composites.ts`, so `readVectorsById` / `readSetsById` exist on the `Database` imported by this file. Let biome settle the wrapping (lineWidth 100).

### Existing tests that must stay green unchanged

Every other lister/composite test passes a real table (`"Collection"`, `"Configuration"`, `"Items"`, `"AllTypes"`, `"Child"`), verified by grep across `tests/` and every binding test directory. In particular these still assert an empty result for a real collection with no groups: `Database.ListTimeSeriesGroupsEmpty`, `DatabaseCApi.ListTimeSeriesGroupsEmpty`, `LuaRunnerTest.ReadVectorsById` / `ReadSetsById`, Julia `"Metadata Empty"`, Dart `listTimeSeriesGroups returns empty for collection without time series`, Python `test_read_vectors_by_id_no_groups` / `test_read_sets_by_id_no_groups`. `DatabaseCApi` null-argument tests in `test_c_api_database_time_series_group.cpp` (~L317) are unaffected (`QUIVER_REQUIRE` fires first).

No new schema files.

## Docs and changelog

### Root `CLAUDE.md`, Core API → Database Class bullet list

Old:
```
- List groups: `list_scalar_attributes()`, `list_vector_groups()`, `list_set_groups()`, `list_time_series_groups()`
```
New:
```
- List groups: `list_scalar_attributes()`, `list_vector_groups()`, `list_set_groups()`, `list_time_series_groups()` —
  all four throw Pattern 1 `Cannot <op>: collection not found: <c>` for a name that is not a table (an existing
  collection with no groups of that kind is an empty list), and the `read_vectors_by_id` / `read_sets_by_id`
  composites inherit that throw.
```

### `src/CLAUDE.md`, Core Internals Worth Knowing → the "**Table classification has one source**" bullet (currently ~L400)

Old (last sentence of the bullet):
```
  All list/metadata/describe call sites use them — never hand-roll prefix scans.
```
New:
```
  All list/metadata/describe call sites use them — never hand-roll prefix scans. `group_names` returns an empty
  list for a name that is not a table, so each `list_{vector,set,time_series}_groups` calls
  `Impl::require_collection` first (as `list_scalar_attributes` does); without it a mistyped collection is
  indistinguishable from one that has no groups.
```

### Other docs

None. `bindings/js/src/lua-api.ts` (Lists section, currently ~L521, and Composite by-id reads, ~L372), `bindings/js/README.md`, the binding docstrings and the C/C++ header comments don't mention the unknown-collection behaviour, so none of them is wrong after this change. No binding `CLAUDE.md` mentions it either (grep `-i "unknown collection\|collection not found\|list_.*group"` over all `CLAUDE.md` files).

### `CHANGELOG.md`: append as the **last bullet of `### Changed`** under `## [0.11.0] — unreleased` (immediately before `### Fixed`; keep any bullets plans 01–06 added)

```markdown
- **BREAKING — `list_vector_groups()`, `list_set_groups()` and `list_time_series_groups()` throw
  for an unknown collection.** They returned an empty list for a name that is not a table, so a
  mistyped collection looked the same as a collection with no groups, while
  `list_scalar_attributes()` on the same name threw. All four now raise
  `Cannot <operation>: collection not found: <name>`, in the C API, Lua and every binding. The
  `read_vectors_by_id` / `read_sets_by_id` composites (Julia, Dart, Python, JS, Lua) are built on
  them and now raise `Cannot list_vector_groups: …` / `Cannot list_set_groups: …` instead of
  returning an empty map. An existing collection with no groups still returns an empty list.

  *Adapt:* a caller that used an empty result to mean "no such collection" must catch the error
  instead.
```

## Verification

From the repo root (`C:\Development\Quiver\quiver1`), in order:

1. `cmake --build build --config Debug`
2. `./build/bin/quiver_tests.exe --gtest_filter='Database.TimeSeriesCollectionNotFound:Database.ListGroupsCollectionNotFound:Database.ListTimeSeriesGroups*:LuaRunnerTest.ListGroupsUnknownCollection:LuaRunnerTest.ReadVectorsById*:LuaRunnerTest.ReadSetsById*:LuaRunnerTest.NumberOfElementsUnknownCollection'`: all pass.
3. `./build/bin/quiver_c_tests.exe --gtest_filter='DatabaseCApiMetadata.*:DatabaseCApi.ListTimeSeriesGroups*'`: all pass, including `DatabaseCApiMetadata.ListGroupsCollectionNotFound`.
4. `./build/bin/quiver_tests.exe` and `./build/bin/quiver_c_tests.exe` (full suites): no failures.
5. `bindings/julia/test/test.bat`: passes, including `List Groups Unknown Collection`. (Make sure `QUIVER_LIB_DIR` is unset so Julia loads the in-tree `build/` library.)
6. `bindings/dart/test/test.bat`: passes, including `List Groups Unknown Collection listers and composites throw instead of returning empty`.
7. `bindings/python/tests/test.bat`: passes, including `TestListGroups::test_list_groups_unknown_collection`.
8. `bindings/js/test/test.bat`: passes, including `an unknown collection throws in every lister and in the composites`.
9. No generator run: no C API signature changed.
10. `scripts/format.bat`, then `git diff --stat`. Only the files listed in this plan may change (clang-format may rewrap the new Lua/C++ test lines, and dart format / biome may rewrap the new Dart/JS tests).
11. `scripts/test-all.bat`: every suite and the CLI smoke test pass. (If the CLI smoke test fails before *and* after your change, that's the pre-existing problem plan 65 owns, not a regression.)

## Acceptance criteria

- [ ] `list_vector_groups`, `list_set_groups` and `list_time_series_groups` begin with `impl_->require_collection(collection, "<own name>")`. No `impl_->require_schema();` remains in any of the three.
- [ ] `Database.TimeSeriesCollectionNotFound` asserts the exact message `Cannot list_time_series_groups: collection not found: NonexistentCollection`.
- [ ] New tests exist and pass: `Database.ListGroupsCollectionNotFound`, `DatabaseCApiMetadata.ListGroupsCollectionNotFound`, `LuaRunnerTest.ListGroupsUnknownCollection`, Julia `List Groups Unknown Collection`, Dart `List Groups Unknown Collection`, Python `test_list_groups_unknown_collection`, JS `an unknown collection throws in every lister and in the composites`.
- [ ] Each new test fails on the pre-change code (optional check: revert the three `src/` lines locally, rebuild, run step 2/3).
- [ ] Every existing "real collection with no groups returns empty" test still passes unchanged.
- [ ] No change to any C API header, `src/c/*`, `src/lua_runner.cpp`, FFI declaration file or binding source file.
- [ ] Root `CLAUDE.md` List-groups line and `src/CLAUDE.md` Table-classification bullet updated as quoted above.
- [ ] `CHANGELOG.md` has the **BREAKING** entry as the last bullet of `### Changed` under 0.11.0. No manifest version changed.
- [ ] `scripts/format.bat` leaves no further diff. `scripts/test-all.bat` is green (apart from any pre-existing CLI smoke failure owned by plan 65).

## Pitfalls

- **Use `require_collection`, not `require_schema` plus a hand-written check.** `require_collection` already loads the schema lazily. Keep `require_schema` as a separate call and you get redundant code; drop it without `require_collection` and an `open()`ed database crashes on a null `schema`.
- **The operation string must be the method's own name**, including `list_time_series_groups` in `database_time_series.cpp`. A copy-paste of `"list_vector_groups"` into the set/time-series function yields a wrong message, and only the exact-match assertions (not substring ones) catch it.
- **Line numbers will have shifted.** Plans 02–04 edit `src/database_time_series.cpp` before this one. Find the function by name.
- **Dart runs its own native build** through the native-assets hook, not `build/bin`. If the Dart test still sees the old behaviour after the C++ change, clear `bindings/dart/.dart_tool/hooks_runner/` and `bindings/dart/.dart_tool/lib/` and rerun (see `bindings/dart/CLAUDE.md`, "Stale native cache").
- **Python and JS load `build/bin` via PATH** (their `test.bat` prepends it), so rebuild (step 1) before running them. Julia loads `build/` unless `QUIVER_LIB_DIR` is set.
- **Lua messages are substrings.** sol2 may prefix the C++ `what()` with Lua location text, which is why `expect_lua_error` does a substring match. Don't switch it to an exact comparison. The message is identical in Debug and Release.
- **`.bat` files are CRLF.** This plan edits none. Don't let an editor or sed touch the test scripts.
- **`CHANGELOG.md` is shared by many batch-1 plans.** Append your bullet and leave the entries other plans added.

## Out of scope

- Collapsing the three list bodies into one loop / a `require_group_table` helper: plan **57**.
- Positive-path C++ tests for `list_vector_groups` / `list_set_groups` and Lua tests for the metadata getters: plan **52**.
- Reworking the Lua list/composite wrappers (`list_vector_metadata_lua`, `read_vectors_by_id_lua`, …): plans **49** and **50**.
- `require_collection` only checks `has_table`, so a group-table name (e.g. `list_vector_groups("Collection_vector_values")`) passes and returns `[]`, just as `list_scalar_attributes` accepts it today. That's a separate question about what counts as a "collection". This plan doesn't change it, and no plan in the list owns it.
- Making a composite's error name the composite (`read_vectors_by_id`) instead of the core call: it would need a binding-crafted message, which the Error Messages principle rules out. `read_scalars_by_id` already behaves the same way.
- A `has_collection` probe API: not requested.
