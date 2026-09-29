# 20 — All bindings: an empty update_time_series_files map still reaches the core

**Batch** 3 · **Severity** low · **Breaking** no. This is a behaviour change for already-invalid calls only: in Julia, Dart, Python and JS, an empty map on an unknown collection, or on a collection with no `_time_series_files` table, now raises instead of silently succeeding. C++, C and Lua already raise. · **Size** S · **Layers** Julia, Dart, Python, JS wrappers (code); C++, C API, Lua (tests only); root, Dart and JS AGENTS.md; CHANGELOG
**Depends on** none · **Overlaps with** 57 (same core function `Database::update_time_series_files`: 57 deletes its unreachable `if (!table_def)` block and may reword the "files table not found" message. This plan changes no core code, and its new tests deliberately do not pin that message.) Same files but different functions: 38/24/25/31 (`database_update.dart`, the group writers and `_marshalGroupColumn`), 17/33/41 (`bindings/js/src/time-series.ts`), 18/34/56 (`bindings/julia/src/database_update.jl`), 24/25/27/28/30 (`bindings/python/src/quiverdb/database.py`), 43/44 (Lua-reference prose on `update_time_series_files` in `bindings/js/src/lua-api.ts`, not touched here), 69 (C API test leaks: the new C API test closes its handle).

## Why

The C++ core validates first and only then treats an empty map as a no-op. From `src/database_time_series.cpp`, `Database::update_time_series_files` (currently ~L387-400):

```cpp
    impl_->require_collection(collection, "update_time_series_files");

    auto tsf = impl_->schema->find_time_series_files_table(collection);
    const auto* table_def = impl_->schema->get_table(tsf);
    if (!table_def) {
        throw std::runtime_error("Time series files table not found: " + tsf);
    }

    if (paths.empty()) {
        return;
    }
```

The C API accepts an empty call (`src/c/database_time_series.cpp`, `quiver_database_update_time_series_files`, ~L441):

```cpp
    QUIVER_REQUIRE(db, collection);
    if (count > 0) {
        QUIVER_REQUIRE(columns, paths);
    }
```

Lua passes an empty table straight through (`src/lua_runner.cpp`, `update_time_series_files_lua`, ~L2260-2272).

All four FFI bindings return before the FFI call, so the core's checks never run:

- Julia, `bindings/julia/src/database_update.jl`, `update_time_series_files!` (~L311-313):
  ```julia
      if isempty(paths)
          return nothing
      end
  ```
- Dart, `bindings/dart/lib/src/database_update.dart`, `updateTimeSeriesFiles` (~L686-688):
  ```dart
        if (count == 0) {
          return;
        }
  ```
- Python, `bindings/python/src/quiverdb/database.py`, `Database.update_time_series_files` (~L1886-1887):
  ```python
          if count == 0:
              return
  ```
- JS, `bindings/js/src/time-series.ts`, `Database.prototype.updateTimeSeriesFiles` (~L429):
  ```ts
    if (entries.length === 0) return;
  ```

**Reproduction.** The C++/Lua column below was confirmed with the built `quiver_cli` running a Lua script against `tests/schemas/valid/collections.sql`. The binding column follows from the early returns above; the JS finding also reproduced it with a Bun probe:

| Call | C++ / C API / Lua | Julia / Dart / Python / JS |
| --- | --- | --- |
| `update_time_series_files("NoSuchCollection", {})` | throws `Cannot update_time_series_files: collection not found: NoSuchCollection` | returns silently |
| `update_time_series_files("Configuration", {})` (no files table) | throws `Time series files table not found for collection 'Configuration'` | returns silently |
| `update_time_series_files("Collection", {})` | no-op, existing paths kept | no-op |

This breaks two root AGENTS.md principles:
- **Intelligence**: "Logic resides in C++ layer. Bindings/wrappers remain thin." The emptiness decision belongs to the core.
- **Homogeneity**: the same call fails in three layers and passes in four.

These early returns are also the odd ones out inside their own bindings. Each binding's group writers forward an empty input to C with NULL arrays and count 0 instead of returning early (Julia `_update_group_columns`, Dart `updateVectorGroup`'s `if (data.isEmpty)` branch, Python's group writers, JS `updateGroupColumns`).

**Corrections to the review finding:**
1. For an unknown collection the core raises **Pattern 1**, `Cannot update_time_series_files: collection not found: <c>` (`Impl::require_collection`, `src/database_impl.h` ~L91-96), not Pattern 2.
2. The files-table message that actually fires is `Schema::find_time_series_files_table`'s `"Time series files table not found for collection '<c>'"` (`src/schema.cpp` ~L216-222). The `if (!table_def)` throw quoted above cannot be reached. Plan 57 owns both.
3. "The same one-line deletion in every binding" is wrong for Dart and JS. With the guard gone:
   - Dart would call `arena<Pointer<Char>>(0)`. The package:ffi 2.2.0 allocator (`lib/src/allocation.dart`, `calloc.allocate`) throws `ArgumentError('Could not allocate 0 bytes.')` whenever the platform returns NULL, and POSIX `malloc(0)`/`calloc(0, 1)` may return NULL.
   - JS would call Bun `ptr()` on zero-length `Uint8Array`s (`allocNativeStringArray` / `allocNativePtrTable`), a pattern the JS binding avoids everywhere else.

   Both must pass NULL tables instead.

## Constraints and decisions

- **Maintainer decisions for this item (binding):**
  - Dart must not allocate zero bytes; it passes `nullptr`.
  - JS passes `null` tables with `0n`.
  - Every binding gets a test that an empty map on a nonexistent collection throws the core error.
  - This is a behaviour change and gets a CHANGELOG entry.
- Root AGENTS.md, Principles:
  - "Logic resides in C++ layer. Bindings/wrappers remain thin."
  - "Homogeneity".
  - "Error Messages: … Bindings retrieve and surface them — they never craft their own." The new tests assert the core's text verbatim.
  - "Clean code over defensive code … Delete unused code."
- Root AGENTS.md, "Self-Updating": keep the nearest AGENTS.md current. The Dart zero-byte gotcha and the JS null-table convention are recorded in their binding's AGENTS.md, and the cross-layer contract goes in the root Core API list.
- Root AGENTS.md, "Changelog": user-visible changes are listed under the unreleased 0.12.0. This is not marked **BREAKING**: every call whose result changes was already rejected by C++, C and Lua, and it writes nothing either way. The maintainer note calls it a "behaviour change", not BREAKING, unlike the notes for analogous items such as 07. It goes under **Fixed**.
- No C API signature changes, so **no FFI regeneration**: Julia `c_api.jl`, Dart `bindings.dart`, Python `_c_api.py` and JS `loader.ts` are untouched.
- No Design Decision or Do-Not-Fix entry covers this early return. `git log -S` traces it to each binding's first time-series-files commit with no stated reason: 682397b (Julia, Dart), be60222 (Python), f70392c (JS).

Alternatives considered and rejected:
- **Pass explicit NULL in Julia and Python too.** This adds a branch for no behaviour gain. Their existing marshalling of an empty map is valid at `count == 0`, and I verified both:
  - Julia infers `Vector{Cstring}` for the empty comprehension and converts it to `Ptr{Ptr{Cchar}}`. This holds for `Dict{String,Optional{String}}()`, `Dict{String,String}()` and `Dict{String,Nothing}()` on Julia 1.11.
  - CFFI `ffi.new("const char*[]", 0)` returns a non-NULL 0-byte cdata.

  The C API never reads either pointer when `count == 0`.
- **Dart: copy the group writers' explicit `if (data.isEmpty) { check(call(..., nullptr, ...)); return; }` branch.** It would duplicate the FFI call. Choosing NULL at the two allocation sites keeps one call.
- **JS: a conditional at the allocation sites instead of an early call.** `allocNativeStringArray` returns a `{table, keepalive}` pair, so the conditional form is messier than the house pattern `updateGroupColumns` already uses: `if (entries.length === 0) { check(update(..., null, ..., 0n)); return; }`.
- **Have the core reject an empty map.** Rejected: a no-op on a valid collection is the intended core behaviour, and Lua and C already rely on it.

## Changes

### 1. Julia: `bindings/julia/src/database_update.jl`, `update_time_series_files!` (currently ~L310-313)

Current:
```julia
function update_time_series_files!(db::Database, collection::String, paths::AbstractDict{String, <:Optional{String}})
    if isempty(paths)
        return nothing
    end

    count = length(paths)
```
New (delete the guard and its trailing blank line; nothing else changes):
```julia
function update_time_series_files!(db::Database, collection::String, paths::AbstractDict{String, <:Optional{String}})
    count = length(paths)
```
Why: an empty `Dict` now marshals as empty `column_ptrs` / `path_ptrs` vectors with `Csize_t(0)`, the C API forwards an empty map, and the core validates.

### 2. Dart: `bindings/dart/lib/src/database_update.dart`, `updateTimeSeriesFiles` (currently ~L679-714)

Current:
```dart
    final arena = Arena();
    try {
      final count = paths.length;

      if (count == 0) {
        return;
      }

      final columns = arena<Pointer<Char>>(count);
      final pathPtrs = arena<Pointer<Char>>(count);
```
New:
```dart
    final arena = Arena();
    try {
      final count = paths.length;

      // An empty map still reaches the core, which validates the collection and its files table
      // before treating it as a no-op. NULL arrays, not arena(0): package:ffi throws when the
      // allocator returns NULL for a zero-byte request, which POSIX malloc/calloc may do.
      final columns = count == 0 ? nullptr : arena<Pointer<Char>>(count);
      final pathPtrs = count == 0 ? nullptr : arena<Pointer<Char>>(count);
```
Leave the rest of the method (the `for (final entry in paths.entries)` loop and the `check(bindings.quiver_database_update_time_series_files(..., columns, pathPtrs, count))` call) unchanged. The loop body never runs when `count == 0`, so neither `nullptr` is indexed.

Typing: `nullptr` is `Pointer<Never>`, which is a subtype of `Pointer<Pointer<Char>>`, so the conditional's static type is `Pointer<Pointer<Char>>` and `columns[i] = …` still resolves. If `dart analyze` disagrees, annotate both locals as `final Pointer<Pointer<Char>> columns = …`. No lint in `package:lints/recommended` forbids that.

### 3. Python: `bindings/python/src/quiverdb/database.py`, `Database.update_time_series_files` (currently ~L1877-1907)

Current:
```python
        self._ensure_open()
        lib = get_lib()
        count = len(data)
        if count == 0:
            return

        keepalive: list = []
```
New:
```python
        self._ensure_open()
        lib = get_lib()
        count = len(data)

        keepalive: list = []
```
Why: `ffi.new("const char*[]", 0)` is a legal zero-length array, and the C API does not read it at `count == 0`. Leave the docstring alone (plan 30 owns Python docstrings).

### 4. JS: `bindings/js/src/time-series.ts`, `Database.prototype.updateTimeSeriesFiles` (currently ~L421-455)

Current:
```ts
  const lib = getSymbols();
  const collBuf = toCString(collection);
  const entries = Object.entries(data);
  if (entries.length === 0) return;
  const keepalive: Allocation[] = [];
```
New:
```ts
  const lib = getSymbols();
  const collBuf = toCString(collection);
  const entries = Object.entries(data);
  // An empty map still reaches the core, which validates the collection and its files table
  // before the no-op. null tables, never zero-length buffers (same as updateGroupColumns).
  if (entries.length === 0) {
    check(lib.quiver_database_update_time_series_files(this._handle, collBuf.buf, null, null, 0n));
    return;
  }
  const keepalive: Allocation[] = [];
```
Why: `loader.ts` declares the two table args as `P` (`quiver_database_update_time_series_files: { args: [P, BUF, P, P, USIZE], returns: I32 }`), and Bun turns `null` into a NULL pointer for a `"pointer"` slot. `create.ts` (`setElementArray`) and `group-columns.ts` already pass `null` this way. `check` is already imported in this file.

### 5. C++ core, C API, Lua: no code change

The core order (validate, then return on empty) and the C API's `count > 0` guard are already correct. Lua already forwards `{}`. This plan only adds tests that pin them, because the four bindings now rely on that order.

## Tests

The new Julia, Dart, Python and JS tests **fail before the fix**: the binding returns early, so nothing is thrown and the "expected throw" assertion fails. They pass after it. The C++, C API and Lua tests pass both before and after. They pin the core contract the bindings now depend on, which no test covered: no test at any layer calls `update_time_series_files` with an empty map. No existing test pins the old binding no-op, so no existing test changes.

### C++: `tests/test_database_time_series_files.cpp`

Append after `TEST(Database, TimeSeriesFilesNotFound)`:
```cpp
// An empty map is a no-op only after the collection and its files table have been validated. The
// FFI bindings forward an empty map here instead of returning early, so this order is what makes
// the call fail the same way in every layer.
TEST(Database, UpdateTimeSeriesFilesEmptyMapValidatesCollection) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    std::map<std::string, std::optional<std::string>> empty;

    try {
        db.update_time_series_files("NoSuchCollection", empty);
        FAIL() << "expected a throw";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "Cannot update_time_series_files: collection not found: NoSuchCollection");
    }

    // Configuration exists but has no _time_series_files table
    EXPECT_THROW(db.update_time_series_files("Configuration", empty), std::runtime_error);

    // On a collection that has the table, an empty map changes nothing
    std::map<std::string, std::optional<std::string>> paths;
    paths["data_file"] = "/path/to/data.csv";
    db.update_time_series_files("Collection", paths);
    db.update_time_series_files("Collection", empty);

    auto result = db.read_time_series_files("Collection");
    EXPECT_EQ(result["data_file"].value(), "/path/to/data.csv");
    EXPECT_FALSE(result["metadata_file"].has_value());
}
```
The `Configuration` assertion is a bare `EXPECT_THROW` on purpose: plan 57 rewords that message.

### C API: `tests/test_c_api_database_time_series_files.cpp`

Append after `TEST(DatabaseCApi, TimeSeriesFilesNullArguments)`:
```cpp
// count == 0 with NULL arrays is a legal call, and the core still validates the collection and its
// files table before treating the empty map as a no-op. This is how the Dart and JS bindings
// forward an empty map.
TEST(DatabaseCApi, UpdateTimeSeriesFilesEmptyMapValidatesCollection) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    EXPECT_EQ(quiver_database_update_time_series_files(db, "NoSuchCollection", nullptr, nullptr, 0), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Cannot update_time_series_files: collection not found: NoSuchCollection");

    // Configuration has no time series files table
    EXPECT_EQ(quiver_database_update_time_series_files(db, "Configuration", nullptr, nullptr, 0), QUIVER_ERROR);

    EXPECT_EQ(quiver_database_update_time_series_files(db, "Collection", nullptr, nullptr, 0), QUIVER_OK);

    quiver_database_close(db);
}
```
`quiver_get_last_error` comes from `quiver/c/common.h`, which `quiver/c/database.h` includes, so no new include is needed.

### Lua: `tests/test_lua_runner_time_series.cpp`

Insert after `TEST_F(LuaRunnerTest, UpdateTimeSeriesFilesRejectsNonStringPath)` (currently ~L604-617) and before `TEST_F(LuaRunnerTest, MultiColumnTimeSeriesUpdateAndRead)`:
```cpp
TEST_F(LuaRunnerTest, UpdateTimeSeriesFilesEmptyTableValidatesCollection) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::LuaRunner lua(db);

    expect_lua_error(lua,
                     R"(db:update_time_series_files("NoSuchCollection", {}))",
                     "Cannot update_time_series_files: collection not found: NoSuchCollection");
}
```
Use `expect_lua_error` (from `tests/test_lua_runner.h`), not bare `EXPECT_THROW`; see `tests/AGENTS.md`.

### Julia: `bindings/julia/test/test_database_time_series_files.jl`

Insert after the `@testset "Time Series Files - not found"` block (currently ~L115-122), inside the outer `@testset "Time Series Files"`:
```julia
    @testset "Time Series Files - empty update still validates the collection" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        exc = @test_throws Quiver.DatabaseException Quiver.update_time_series_files!(
            db, "NoSuchCollection", Dict{String, Quiver.Optional{String}}(),
        )
        @test exc.value.msg == "Cannot update_time_series_files: collection not found: NoSuchCollection"

        Quiver.close!(db)
    end
```

### Dart: `bindings/dart/test/database_time_series_files_test.dart`

Insert after the `'listTimeSeriesFilesColumns throws for collection without files table'` test (currently ~L132-148), inside `group('Time Series Files', ...)`:
```dart
    test('updateTimeSeriesFiles with an empty map still validates the collection', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        expect(
          () => db.updateTimeSeriesFiles('NoSuchCollection', {}),
          throwsA(
            isA<DatabaseException>().having(
              (e) => e.message,
              'message',
              equals('Cannot update_time_series_files: collection not found: NoSuchCollection'),
            ),
          ),
        );
      } finally {
        db.close();
      }
    });
```

### Python: `bindings/python/tests/test_database_time_series_files.py`

Append to `class TestUpdateTimeSeriesFiles` (after `test_update_time_series_files_overwrite`, currently the last method, ~L91-100):
```python
    def test_update_time_series_files_empty_map_validates_collection(self, collections_db: Database) -> None:
        """An empty map still reaches the core, which rejects an unknown collection."""
        with pytest.raises(
            QuiverError, match="Cannot update_time_series_files: collection not found: NoSuchCollection"
        ):
            collections_db.update_time_series_files("NoSuchCollection", {})
```
`pytest` and `QuiverError` are already imported in this file, and `collections_db` is the conftest fixture.

### JS: `bindings/js/test/database-time-series-files.test.ts`

Add as the last test inside `describe("time series files", ...)`, after `"updateTimeSeriesFiles and readTimeSeriesFiles round-trip"`:
```ts
  test("updateTimeSeriesFiles with {} still validates the collection", () => {
    const db = Database.fromSchema(":memory:", COLLECTIONS_SCHEMA);
    try {
      expect(() => db.updateTimeSeriesFiles("NoSuchCollection", {})).toThrow(
        /Cannot update_time_series_files: collection not found: NoSuchCollection/,
      );
    } finally {
      db.close();
    }
  });
```

No new schema files: every test uses `tests/schemas/valid/collections.sql`, which has `Collection_time_series_files` and a `Configuration` table without one.

## Docs and changelog

1. **Root `AGENTS.md`**, Core API list, "### Database Class" (currently ~L612).

   Old line:
   ```markdown
   - Time series files: `has_time_series_files()`, `list_time_series_files_columns()`, `read_time_series_files()`, `update_time_series_files()`
   ```
   New line:
   ```markdown
   - Time series files: `has_time_series_files()`, `list_time_series_files_columns()`, `read_time_series_files()`, `update_time_series_files()`. An empty `update_time_series_files` map still validates the collection and its files table (an unknown collection, or one with no `_time_series_files` table, throws) and then changes nothing — in every layer: the FFI bindings forward it as `count == 0` with NULL or zero-length arrays and never return early.
   ```

2. **`bindings/dart/AGENTS.md`**, the "Marshaling idiom" bullet (currently ~L77-78).

   Old opening:
   ```markdown
   - **Marshaling idiom**: every method allocates through a `package:ffi` `Arena` and releases in
     `finally`. Typed columns go through the shared private `_marshalGroupColumn(Arena, String, List<Object?>)`
   ```
   New opening (one sentence inserted after "releases in `finally`."; re-wrap to ~100 columns; keep the rest of the bullet unchanged):
   ```markdown
   - **Marshaling idiom**: every method allocates through a `package:ffi` `Arena` and releases in
     `finally`. Never request zero bytes (`arena<T>(0)`): package:ffi's allocator throws
     `ArgumentError('Could not allocate 0 bytes.')` whenever the platform returns NULL for it, which
     POSIX `malloc`/`calloc` may do (Windows' `CoTaskMemAlloc` does not, so a Windows test run will
     not catch it). Pass `nullptr` for an empty array instead, since the C API takes NULL with a zero
     count; `updateTimeSeriesFiles` and the group writers' clear paths do this. Typed columns go
     through the shared private `_marshalGroupColumn(Arena, String, List<Object?>)`
   ```

3. **`bindings/js/AGENTS.md`**, the "A nullable scalar string argument passes literal `null`" bullet (currently ~L78-81).

   Old:
   ```markdown
   - **A nullable scalar string argument passes literal `null`, never `""`**
     (`updateRelation`/`updateRelationByLabel`) — Bun turns `null` into a NULL pointer for a
     `"pointer"` slot, the same way `group-columns.ts` passes `null` for the array pointers when
     clearing. The C API reads NULL as "clear the relation" and an empty string as a label to look up.
   ```
   New:
   ```markdown
   - **A nullable scalar string argument passes literal `null`, never `""`**
     (`updateRelation`/`updateRelationByLabel`) — Bun turns `null` into a NULL pointer for a
     `"pointer"` slot, the same way `group-columns.ts` (clearing a group) and `updateTimeSeriesFiles`
     (an empty map) pass `null` for the array pointers instead of building zero-length tables. The
     C API reads NULL as "clear the relation" and an empty string as a label to look up.
   ```

4. No edits to `bindings/julia/AGENTS.md` or `bindings/python/AGENTS.md`: they state nothing about this path, and deleting a guard adds no rule worth recording. No edits to `bindings/js/src/lua-api.ts` either: plans 43/44 own the `update_time_series_files` prose there, and Lua behaviour does not change. No README or `docs/*.md` mentions an empty map.

5. **`CHANGELOG.md`**, under `## [0.12.0] — unreleased` → `### Fixed` (currently ~L67), appended as the last bullet of that list:
   ```markdown
   - **Julia, Dart, Python, JS: `update_time_series_files` with an empty map validates the
     collection.** The four bindings returned before calling the core when the map was empty, so
     `update_time_series_files("NoSuchCollection", {})`, or the same call on a collection with no
     `_time_series_files` table, succeeded silently where C++, the C API and Lua raised. The empty
     map now reaches the core in every binding and raises the same error
     (`Cannot update_time_series_files: collection not found: <collection>`); on a collection that
     has the table it still changes nothing. A caller that made this call on a collection without a
     files table should check `has_time_series_files` first.
   ```

## Verification

Run from the repo root (`C:\Development\Quiver\quiver1`). Use Git Bash for the executables and a Windows shell (cmd or PowerShell) for the `.bat` scripts.

1. Optional TDD check: add the four binding tests before the four binding edits, then run them (steps 5-8 below). Each of the four new binding tests must fail with "expected throw"/"did not throw".
2. `cmake --build build --config Debug`
3. `./build/bin/quiver_tests.exe --gtest_filter="*UpdateTimeSeriesFilesEmpty*"` must run and pass 2 tests: `Database.UpdateTimeSeriesFilesEmptyMapValidatesCollection` and `LuaRunnerTest.UpdateTimeSeriesFilesEmptyTableValidatesCollection`.
4. `./build/bin/quiver_c_tests.exe --gtest_filter="*UpdateTimeSeriesFilesEmpty*"` must pass 1 test: `DatabaseCApi.UpdateTimeSeriesFilesEmptyMapValidatesCollection`. Then `./build/bin/quiver_tests.exe --gtest_filter="*TimeSeriesFiles*"` (14 tests; 12 today) and `./build/bin/quiver_c_tests.exe --gtest_filter="*TimeSeriesFiles*"` (9 tests; 8 today) must all pass.
5. `bindings\julia\test\test.bat test_database_time_series_files.jl` must pass, including "Time Series Files - empty update still validates the collection". Then run the full `bindings\julia\test\test.bat`.
6. `cd bindings/dart && dart analyze` must be clean, which checks the conditional `nullptr` typing. Then `bindings\dart\test\test.bat test/database_time_series_files_test.dart` must pass, then the full `bindings\dart\test\test.bat`. No C API change, so the native-assets cache does not need clearing.
7. `bindings\python\tests\test.bat -k empty_map` must pass `test_update_time_series_files_empty_map_validates_collection`. Then run the full `bindings\python\tests\test.bat`.
8. `bindings\js\test\test.bat` must pass everything, including "updateTimeSeriesFiles with {} still validates the collection". Then `cd bindings/js && bunx biome check src/time-series.ts test/database-time-series-files.test.ts` must be clean.
9. `scripts\format.bat`, then `git diff --stat`. Only the files named in this plan may change: 4 binding sources, 7 test files, 3 AGENTS.md files and `CHANGELOG.md`. Revert anything else the formatters touched.
10. `scripts\test-all.bat`: all six suites plus the CLI smoke test must be green (the CLI smoke test is subject to plan 65's state).

## Acceptance criteria

- [x] `update_time_series_files!` (Julia) and `update_time_series_files` (Python) have no empty-map early return.
- [x] Dart `updateTimeSeriesFiles` has no early return and passes `nullptr` for both arrays when `count == 0`. It never calls `arena<…>(0)`.
- [x] JS `updateTimeSeriesFiles` calls `quiver_database_update_time_series_files(this._handle, collBuf.buf, null, null, 0n)` on `{}` and does not build zero-length tables.
- [x] Each binding raises `Cannot update_time_series_files: collection not found: NoSuchCollection` for an empty map on an unknown collection, pinned by one new test per binding.
- [x] New C++, C API and Lua tests pin the core order (validate, then no-op on empty) and pass.
- [x] No C API, `c_api.jl`, `bindings.dart`, `_c_api.py` or `loader.ts` change.
- [x] Root, Dart and JS AGENTS.md updated as specified. CHANGELOG `### Fixed` entry under 0.12.0 added. *(Landed under `## [0.12.5] — unreleased`, the open section. See Implementation notes.)*
- [x] `scripts\format.bat` leaves no diff outside the listed files. `scripts\test-all.bat` is green. *(After restoring 40 JS files biome rewrote CRLF→LF only. See Implementation notes.)*

## Pitfalls

- **Windows hides the Dart bug.** `CoTaskMemAlloc(0)` returns a valid pointer, so a Windows run of the old or a half-fixed Dart code passes. The failure is POSIX-only. Keep the `nullptr` form even though no local test distinguishes it.
- **Do not route JS `{}` through `allocNativeStringArray([])` / `allocNativePtrTable([])`.** That calls Bun `ptr()` on a zero-length `Uint8Array`, which the JS house style avoids.
- **Do not pin the "files table not found" text** in any new test. Plan 57 rewords it (`find_time_series_files_table` → Pattern 2) and deletes the unreachable `if (!table_def)` throw in the same function. The new C++/C API tests assert only that a throw or `QUIVER_ERROR` occurs for `Configuration`.
- **Dart conditional typing.** If the analyzer infers something other than `Pointer<Pointer<Char>>` for `count == 0 ? nullptr : arena<Pointer<Char>>(count)`, annotate the two locals explicitly. Do not fall back to `arena(count == 0 ? 1 : count)`: that allocates a slot the C API never reads, just to dodge the allocator.
- **Julia `@test_throws` returns the `Test.ExceptionInfo`.** Read the message through `exc.value.msg`, as `test_database_lifecycle.jl` does.
- **Python line length.** The `pytest.raises(...)` line is over ruff's 120 columns on one line, so keep the wrapped form shown (it is what `ruff format` produces).
- Line numbers above will have shifted if earlier plans (17, 18, 24, 25, 33, 34, 38) landed first. Anchor on the function names and the quoted excerpts.
- No `.bat` file is edited here. If an editor or tool touches one, restore its CRLF line endings.

## Out of scope

- The unreachable `if (!table_def)` throw in `Database::update_time_series_files`, and the `find_time_series_files_table` / `list_time_series_files_columns` message wording: plan 57.
- The Lua-reference claims about `update_time_series_files` (whole-row replace, `nil` semantics) in `bindings/js/src/lua-api.ts`: plans 43 and 44.
- Python docstring wording for `update_time_series_files`: plan 30.
- Any change to the core's empty-map no-op or to the whole-row-replace semantics of a non-empty map: not planned, and the current behaviour is intended.

## Implementation notes

Implemented on `rs/plan20`. Before any edit the branch fast-forwarded from `afa5fea` to `35b7fba`, which brought in plans 17 (`28d6e1f`), 18 (`42790be`) and 19 (`063cfe0`). After that, `git fetch origin && git merge origin/master` reported "Already up to date". Every anchor was re-checked after the merge: the four binding early returns, `Database::update_time_series_files` (validate, then `if (paths.empty()) return;`, now at `src/database_time_series.cpp:401-414`), the C API `count > 0` guard, `update_time_series_files_lua`, all seven test anchors, and the three AGENTS.md passages. None changed apart from line numbers. Every code and test edit is the plan's text verbatim. `dart analyze` accepted the untyped `count == 0 ? nullptr : arena<Pointer<Char>>(count)` conditional, so no annotation was needed.

**Results.**
- TDD: the four new binding tests failed before the fix, each for the expected reason:
  - Julia: `No exception thrown` (16 pass / 1 fail in the file).
  - Dart: `Actual: <Closure: () => void> Which: returned <null>`.
  - Python: `Failed: DID NOT RAISE QuiverError`.
  - JS: `Received function did not throw`.

  All four pass after the fix. The C++, C API and Lua pinning tests passed before the fix, as the plan predicts.
- `*UpdateTimeSeriesFilesEmpty*`: 2/2 in `quiver_tests` and 1/1 in `quiver_c_tests`. `*TimeSeriesFiles*`: 14/14 and 9/9, exactly the plan's counts.
- File-level runs: Julia 18/18, Dart 9/9. Python `-k empty_map` 1 passed, and JS 229/229.
- `scripts/test-all.bat`: all six suites PASS (C++ 1375, C API 572, Julia 1558, Dart 436, JS 229, Python 325). That is plan 19's recorded totals plus exactly this plan's new tests. The script has six steps; the CLI smoke step is gone (see plan 65).
- `dart analyze`: no issue in the touched files, only pre-existing `info`s elsewhere. biome, on the LF-normalized content that is what gets committed: both files are format-clean, and the only lint hits are the pre-existing unused `MIXED_TS_SCHEMA` / `NULLABLE_TS_SCHEMA` constants in the test file, left alone per the no-drive-by rule.

**Drift fixed or noted:**
1. The repo is `quiver4`, not `quiver1`, and line numbers moved as the Pitfalls anticipate.
2. The CHANGELOG entry went under `## [0.12.5] — unreleased` → `### Fixed`, the open section plans 17/18 created, not under `0.12.0` (released). This is a patch bump for a non-breaking fix, so no manifest changes.
3. `scripts/format.bat`: clang-format, JuliaFormatter, dart format and ruff changed nothing. biome rewrote 42 JS files CRLF→LF, as in plans 08-19; this checkout has `core.autocrlf=true`, so the working tree is CRLF and the index LF. `git diff --stat` showed content changes only in the 15 planned files, so the other 40 were restored with `git checkout --`. No `.bat` file changed.
4. **The CHANGELOG wording deviates from §Docs 5.** The review found that the plan's text says both cases raise `collection not found`. The no-files-table case actually raises `find_time_series_files_table`'s own error. The entry now quotes the message only for the unknown collection and names the other as "the files-table-not-found error" without quoting it, since plan 57 rewords it.

**Review.** A three-lens adversarial workflow read the diff: FFI marshalling of the empty path per binding, scope and doc accuracy, and completeness (any other early return, doc comment or test that pinned the old no-op). It confirmed only drift item 4, which is fixed. The FFI and completeness lenses raised nothing.

**For later plans:**
- **Plan 57** must keep the core's order, validate and only then `if (paths.empty()) return;`. The four bindings now depend on it, and `UpdateTimeSeriesFilesEmptyMapValidatesCollection` (C++ and C API) plus the Lua test pin it. Those tests assert only the `collection not found` text. The `Configuration` case is a bare `EXPECT_THROW` / `QUIVER_ERROR`, so 57 can reword the files-table message freely.
- **Plan 43** (Lua reference prose on `update_time_series_files`) is unaffected: Lua behaviour did not change.
- **Teammate branch `origin/db/patch-ts-files`** (unmerged, not a plan) rewrites the same core function to write only the named columns. It appends tests to the same seven test files, so merging it will conflict textually with this commit's appended tests; the conflicts are additive. Its semantics are compatible: an empty map stays a no-op after validation.
