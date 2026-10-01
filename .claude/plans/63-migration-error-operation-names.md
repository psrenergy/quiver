# 63 — Migration and schema-file errors name the public operation

**Batch** 6 · **Severity** low · **Breaking** no (error text only) · **Size** S · **Layers** C++ core, C++ tests, CHANGELOG
**Depends on** 60 recommended first (it rewrites the same three function bodies to use `TransactionGuard`; land 60, then reword the messages here) · **Overlaps with** 60 (same functions), 53 (no conflict: `migrate_*`/`apply_schema` stay on `Database`)

## Why

Root AGENTS.md, "C++ Error Message Patterns": "Validators thread the calling operation's name
through so the `{operation}` is the public method the user called". Three **private** helpers in
`src/database.cpp` name themselves instead:

| Site (currently) | Message today | Reached from |
|---|---|---|
| ~L421-422 | `Cannot migrate_up: migration N has no up.sql file` | `from_migrations`, `validate_migrations` |
| ~L434 | `Failed to migrate_up: migration N: <reason>` | same |
| ~L458-459 | `Cannot migrate_down: migration N has no down.sql file` | `validate_migrations` |
| ~L472 | `Failed to migrate_down: migration N: <reason>` | `validate_migrations` |
| ~L481 | `Failed to apply_schema: could not open file: <p>` | `from_schema` |
| ~L489 | `Cannot apply_schema: schema file is empty: <p>` | `from_schema` |

A `validate_migrations` caller reading `Failed to migrate_down: ...` sees a function they never
called. Three tests pin the current texts (`tests/test_migrations.cpp`):
- ~L237: `message.find("Failed to migrate_up: migration 1:")`
- ~L254: `message.find("Failed to migrate_down: migration 1:")`
- ~L288: `EXPECT_STREQ(error.what(), "Cannot migrate_down: migration 1 has no down.sql file")`

## Constraints and decisions

- Only `migrate_up` has two public callers. It gains a `const char* operation` parameter, the
  `require_collection` idiom. `migrate_down` (called only by `validate_migrations`) and
  `apply_schema` (called only by `from_schema`) write their single caller's name in directly.
- Keep the **direction word** ("up"/"down migration N"). Once the operation says
  `validate_migrations`, the direction is the useful part of the message.
- Leave `Migrations path not found: ...` (~L251) and `Schema file not found: ...` (~L295) as they
  are. They are Pattern 2 ("{Entity} not found"), which is fine.

## Changes — `src/database.cpp` (and the private declaration in `include/quiver/database.h`)

1. `void migrate_up(const std::string& migration_path);` becomes
   `void migrate_up(const std::string& migration_path, const char* operation);` in the header's
   private section, and in the definition. Callers:
   - `from_migrations`: `db.migrate_up(path, "from_migrations");`
   - `validate_migrations`: `db.migrate_up(path, "validate_migrations");`

   Find the call syntax with `grep -n "migrate_up(" src/database.cpp`.
2. In `migrate_up`:
   - `"Cannot migrate_up: migration " + v + " has no up.sql file"` becomes
     `std::string("Cannot ") + operation + ": migration " + v + " has no up.sql file"`
   - `"Failed to migrate_up: migration " + v + ": " + reason` becomes
     `std::string("Failed to ") + operation + ": up migration " + v + ": " + reason`
3. In `migrate_down`:
   - `"Cannot validate_migrations: migration " + v + " has no down.sql file"`
   - `"Failed to validate_migrations: down migration " + v + ": " + reason`
4. In `apply_schema`:
   - `"Failed to from_schema: could not open file: " + schema_path`
   - `"Cannot from_schema: schema file is empty: " + schema_path`

If plan 60 has landed, these throws sit inside `catch` blocks that re-wrap the message. Edit the
re-wrap text there.

## Tests — `tests/test_migrations.cpp`

Update the three pins. Check which public call each test makes first (from_migrations vs
validate_migrations):
- ~L237: `"Failed to migrate_up: migration 1:"` becomes `"Failed to from_migrations: up migration 1:"`
  or `"Failed to validate_migrations: up migration 1:"`, matching that test's call.
- ~L254: `"Failed to migrate_down: migration 1:"` becomes `"Failed to validate_migrations: down migration 1:"`.
- ~L288: `EXPECT_STREQ(error.what(), "Cannot validate_migrations: migration 1 has no down.sql file");`

Add one pin for `from_schema` with an empty schema file, next to the existing from_schema error
tests (`grep -rn "schema file is empty\|could not open file" tests/`). If a test already pins
either text, update it to the `from_schema` wording. Also search bindings for the old texts:
`grep -rn "migrate_up\|migrate_down\|apply_schema" bindings/*/test* bindings/python/tests`.

## Docs and changelog

- `CHANGELOG.md`, `## [0.12.0] — unreleased` → `### Changed`:
  ```markdown
  - **Migration and schema-file errors name the method you called.** `from_migrations`,
    `validate_migrations` and `from_schema` now report e.g. `Failed to validate_migrations: down
    migration 2: ...` and `Cannot from_schema: schema file is empty: ...` instead of the private
    helper names `migrate_up` / `migrate_down` / `apply_schema`.
  ```

## Verification

From the repo root:
1. `cmake --build build --config Debug`
2. `./build/bin/quiver_tests.exe --gtest_filter=*Migration*:*FromSchema*`, then the full suite
3. `./build/bin/quiver_c_tests.exe`
4. `scripts/format.bat`

## Acceptance criteria

- [ ] No thrown message contains `migrate_up`, `migrate_down` or `apply_schema`
      (`grep -n "migrate_up:\|migrate_down:\|apply_schema:" src/database.cpp` prints nothing).
- [ ] Pins updated; suites green; CHANGELOG entry.

## Pitfalls

- `validate_migrations` runs both `migrate_up` and `migrate_down`. Pass `"validate_migrations"` to
  `migrate_up` from there, or its up-direction failures will say `from_migrations`.

## Out of scope

- Moving these helpers into `Impl` (not planned).

## Implementation notes

- **Landed as written.** `migrate_up(path, operation)` gets `"from_migrations"` from
  `from_migrations` and `"validate_migrations"` from `validate_migrations` (see the pitfall).
  `migrate_down` and `apply_schema` hard-code their only caller. The "up"/"down" direction word
  is kept. Both Pattern 2 `... not found: ...` messages are unchanged.
- **Drift fixed in place.** The line numbers had moved (`migrate_up` ~L379, `migrate_down`
  ~L423, `apply_schema` ~L457; the test pins were at L253/L270/L304). After plan 60, both
  `Failed to ...` throws are the re-wraps in the `catch` blocks, and the `Cannot ...` throws
  sit before the `try`, as this plan anticipated. The L237 pin was
  `ValidateMigrationsExecutesUpSql`, so it became `Failed to validate_migrations: up migration 1:`.
- **CHANGELOG retarget.** The entry is under `## [0.12.8] — unreleased` → `### Changed`, not
  `[0.12.0]` (the newest tag is `v0.12.7`). The manifests were not bumped.
- **Tests beyond the three pins.** `DatabaseMigrationWithEmptyUpSql` and
  `DatabaseMigrationWithInvalidSQL` were bare `EXPECT_THROW`s. They now pin
  `Cannot from_migrations: migration 1 has no up.sql file` and
  `Failed to from_migrations: up migration 1:`; before this, nothing checked the
  `"from_migrations"` argument. I added `TempFileFixture.FromSchemaEmptyFile`
  (`tests/test_database_lifecycle.cpp`). It is a full-string pin of
  `Cannot from_schema: schema file is empty: <path>`. I did not pin
  `Failed to from_schema: could not open file:`: the only way to reach it past the `fs::exists`
  check is a directory path, which fails the open on Windows but opens and reads as empty on Linux.
  All six tests failed against the old build before the fix (old helper names in the messages).
- **No binding changes.** No binding, C API, Lua or docs test quotes these texts. I grepped
  `bindings/**/test*`, `bindings/python/tests`, `tests/test_c_api*`, `tests/test_lua*`,
  `bindings/js/src/lua-api.ts` and `docs/`.
- **AGENTS.md.** The plan listed no AGENTS.md edit. Per the Self-Updating rule, I added one
  sentence to `src/AGENTS.md` ("Factory Methods"): the private helpers name the public caller.
- **For later plans.**
  - `scripts/format.bat` failed at first with `include/quiver/schema.h: no such file or
    directory` (also `schema_validator.h` and `type_validator.h`). The cause: the root
    `ALL_SOURCE_FILES` `GLOB_RECURSE` has no `CONFIGURE_DEPENDS`, so the `build/` tree still
    listed the headers plan 55 moved into `src/`. `cmake -S . -B build` fixed it, and no file
    changed. This is relevant to plan 86 (format glob).
  - A known quirk, left alone: an *existing but empty* `up.sql` also reports "has no up.sql
    file", because `Migration::up_sql()` returns `""` in both cases.
