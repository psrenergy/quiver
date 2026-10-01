# 62 — Schema-loading errors name the table and column (unsupported type, invalid table name)

**Batch** 6 · **Severity** medium · **Breaking** no (error text only) · **Size** S · **Layers** C++ core, C++ + C API tests, test schema (+ tests/AGENTS.md schema list, CHANGELOG)
**Depends on** none · **Overlaps with** 55 (moves `schema.h`/`schema.cpp`-adjacent headers into `src/`; `data_type.h` stays public), 06 (adds invalid schemas and validator tests next to the ones added here), 57 (other `schema.cpp` edits)

## Why

`Schema` loading runs **before** `SchemaValidator`. Two of its failures produce messages that name
neither the table nor the column, and neither follows the root error patterns.

1. **Unsupported column type.** `data_type_from_string` (`include/quiver/data_type.h`,
   ~L11-21):
   ```cpp
   inline DataType data_type_from_string(const std::string& type_str) {
       if (type_str == "INTEGER") return DataType::Integer;
       ...
       throw std::runtime_error("Unknown data type: " + type_str);
   }
   ```
   Its only caller is `Schema::query_columns` (`src/schema.cpp`, ~L373:
   `col.type = data_type_from_string(type_str);`). A schema with `payload BLOB` fails with
   `Unknown data type: BLOB`, and the user cannot tell which column in which table.
2. **Invalid table name.** Three per-function guards throw ad-hoc texts, e.g. (~L354-356):
   ```cpp
    if (!is_safe_identifier(table)) {
        throw std::runtime_error("Cannot query columns: invalid table name: " + table);
    }
   ```
   There are matching guards in `query_foreign_keys` (~L392) and `query_indexes` (~L424).
   `query columns` is not an operation the caller invoked.

`SchemaValidator::validation_error` already produces `Failed to validate schema: ...`
(`src/schema_validator.cpp`, ~L11-13). Load-time rejections should read the same way.

## Constraints and decisions

- `data_type_from_string` returns `std::optional<DataType>`. It lives in a public header, and
  changing its return type is acceptable (breaking changes OK). `query_columns` is its only caller:
  `grep -rn "data_type_from_string" src include bindings tests`.
- One `is_safe_identifier` check at the top of the per-table loop in `Schema::load_from_database`
  (~L334) replaces the three per-function guards. The table names come from `sqlite_master`, and
  that loop is the only path into the three functions. **Keep** the index-name `continue` (~L442),
  because an index with an unsafe name is skipped, not fatal. Keep `is_safe_identifier` itself; it
  protects the concatenated `PRAGMA ...('<table>')` SQL.
- Spell an empty declared type (an untyped column) as `(none)` in the message.

## Changes

### 1. `include/quiver/data_type.h`

```cpp
// nullopt for a declared type Quiver does not support (e.g. BLOB, NUMERIC, or no type at all);
// Schema::query_columns turns that into an error naming the table and column.
inline std::optional<DataType> data_type_from_string(const std::string& type_str) {
    if (type_str == "INTEGER") return DataType::Integer;
    if (type_str == "REAL") return DataType::Real;
    if (type_str == "TEXT") return DataType::Text;
    if (type_str == "DATE_TIME") return DataType::DateTime;
    return std::nullopt;
}
```
Add `#include <optional>` to that header if it is missing. Keep the existing formatting style
(braces or no braces) to match `scripts/format.bat`.

### 2. `src/schema.cpp`, `Schema::query_columns` (~L373)

```cpp
        const auto type = data_type_from_string(type_str);
        if (!type) {
            throw std::runtime_error("Failed to validate schema: column '" + col.name + "' in table '" + table +
                                     "' has unsupported type '" + (type_str.empty() ? "(none)" : type_str) + "'");
        }
        col.type = *type;
```
Check the local variable names in that loop (`col`, `type_str`, `table`) and keep any existing
post-processing, such as the retyping of TEXT `date_` columns to DateTime (~L381), after this
assignment.

### 3. One table-name check (`Schema::load_from_database`, ~L334)

At the top of `for (const auto& name : names) {`:
```cpp
        if (!is_safe_identifier(name)) {
            throw std::runtime_error("Failed to validate schema: invalid table name '" + name + "'");
        }
```
Delete the three guards in `query_columns`, `query_foreign_keys` and `query_indexes`
(~L354-356, ~L392-394, ~L424-426). Before deleting, confirm those three functions have no other
caller than this loop: `grep -n "query_columns(\|query_foreign_keys(\|query_indexes(" src/schema.cpp`.
If one does, keep its guard and reword it to the same message.

## Tests

### New schema `tests/schemas/invalid/unsupported_type.sql` (LF line endings)

```sql
PRAGMA foreign_keys = ON;

CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Items (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL,
    payload BLOB
) STRICT;
```

### C++ — `tests/test_schema_validator.cpp`

Add a test next to the other invalid-schema tests, using that file's fixture or helper (e.g.
`INVALID_SCHEMA("...")` or a `SchemaValidatorFixture`; check the top of the file):
```cpp
TEST_F(SchemaValidatorFixture, UnsupportedColumnTypeNamesTableAndColumn) {
    try {
        quiver::Database::from_schema(":memory:", INVALID_SCHEMA("unsupported_type.sql"),
                                      {.read_only = false, .console_level = quiver::LogLevel::Off});
        FAIL() << "expected a throw";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "Failed to validate schema: column 'payload' in table 'Items' has unsupported type 'BLOB'");
    }
}
```

### C API — `tests/test_c_api_database_lifecycle.cpp` (or wherever invalid `from_schema` is tested)

`quiver_database_from_schema(":memory:", <path to unsupported_type.sql>, &opts, &db)` returns
`QUIVER_ERROR`, and `quiver_get_last_error()` contains
`"column 'payload' in table 'Items' has unsupported type 'BLOB'"`. Copy the neighbouring
invalid-schema C test for the path macro and option setup.

The invalid-table-name path is not reachable from a `.sql` file without quoting tricks. Skip a
dedicated test for it; the code is a one-line guard.

Search the tests for the old texts and update any hit:
`grep -rn "Unknown data type\|Cannot query columns\|invalid table name" tests/ bindings/`.

## Docs and changelog

- `tests/AGENTS.md`: add `unsupported_type.sql` to the `invalid/` schema list
  (`grep -n "invalid/" tests/AGENTS.md`).
- `CHANGELOG.md`, under `## [0.12.0] — unreleased` → `### Changed`:
  ```markdown
  - **Schema errors name the offending column.** A column type Quiver does not support now fails
    with `Failed to validate schema: column 'payload' in table 'Items' has unsupported type 'BLOB'`
    (was `Unknown data type: BLOB`), and an unsafe table name with `Failed to validate schema:
    invalid table name '...'`.
  ```

## Verification

From the repo root:
1. `cmake --build build --config Debug`
2. `./build/bin/quiver_tests.exe --gtest_filter=*SchemaValidator*` and then the full suite
3. `./build/bin/quiver_c_tests.exe`
4. `scripts/format.bat`

## Acceptance criteria

- [x] `data_type_from_string` returns `std::optional`, and the unsupported-type message names the
      table, the column and the type.
- [x] One `is_safe_identifier` table check at load. The three per-function guards are gone (or
      reworded if they have another caller).
- [x] New schema and tests are green, and the CHANGELOG and tests/AGENTS.md are updated.

## Pitfalls

- SQLite reports a declared type verbatim (e.g. `DATE_TIME` is not a SQLite type but is accepted in
  STRICT tables? No: STRICT tables allow only INT, INTEGER, REAL, TEXT, BLOB, ANY). Check how the
  repo's schemas declare date columns (`TEXT` plus a `date_` prefix) before assuming `DATE_TIME`
  appears. Keep the branch either way.
- A `STRICT` table accepts `BLOB`, so the new schema is valid SQL. That is the point: it passes
  SQLite and fails Quiver.

## Out of scope

- Supporting BLOB columns.
- Validator (post-load) messages.

## Implementation notes

- **Landed as planned.** `data_type_from_string` returns `std::optional<DataType>`, and
  `Schema::query_columns` throws `Failed to validate schema: column '<c>' in table '<t>' has
  unsupported type '<type>'` (`(none)` for an untyped column). One `is_safe_identifier` check at
  the top of the per-table loop in `Schema::load_from_database` throws `Failed to validate
  schema: invalid table name '<t>'`. The three per-function guards are deleted, since the loop was
  their only caller. The index-name `continue` and `is_safe_identifier` itself stay.
- **Leak fixed on the new throw path.** The throw sits inside the `sqlite3_step` loop, so
  `query_columns` now calls `sqlite3_finalize(stmt)` before throwing. The old throw from inside
  `data_type_from_string` leaked the statement, and an unfinalized statement keeps both
  `sqlite3_close` (`Database::~Database`) and `sqlite3_close_v2` (`Impl`) from releasing the
  connection. That held a file database open (and locked on Windows) after the failed open.
- **Tests (red first, then green).** `SchemaValidatorFixture.UnsupportedColumnTypeNamesTableAndColumn`
  (`tests/test_schema_validator.cpp`) uses the file's `EXPECT_THAT(..., ThrowsMessage(StrEq(...)))`
  style instead of the plan's try/catch, and still asserts the full message.
  `TempFileFixture.FromSchemaRejectsUnsupportedColumnType` (`tests/test_c_api_database_lifecycle.cpp`)
  asserts the full message through `quiver_get_last_error`. Before the fix both failed with
  `Unknown data type: BLOB`. The invalid-table-name path has no test, as the plan decided.
- **Drift fixed.** Line numbers had moved (call at `schema.cpp` ~L341, loop ~L302). `schema.h` now
  lives in `src/` (plan 55). The CHANGELOG section is `[0.12.8] — unreleased`, not `[0.12.0]`.
  The local `const char* type` already existed in `query_columns`, so the optional is named
  `data_type`.
- **Deviation: CHANGELOG BREAKING (C++ only) line.** The plan header says "Breaking: no", but
  `quiver/data_type.h` is an installed header and the return type changed. Following plans 54 and
  55, I added a `BREAKING (C++ only)` bullet with an *Adapt* under `### Changed`. No binding or C
  API symbol changed, so no FFI regeneration was needed.
- **For later plans.** `scripts/format.bat` (biome) rewrites every JS file in a CRLF working tree
  to LF. That is line-ending churn only (`git diff --ignore-cr-at-eol` is empty), so revert it
  with `git checkout -- bindings/js` before committing.
