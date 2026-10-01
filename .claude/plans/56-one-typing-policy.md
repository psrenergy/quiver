# 56 — One scalar typing policy: `validate_value` uses `value_matches_type`; Pattern 1 messages for unknown columns and strings on INTEGER

**Batch** 6 · **Severity** medium · **Breaking** no (the accepted values are unchanged; two error texts change) · **Size** M · **Layers** C++ core, C++ tests (+ src/AGENTS.md, root AGENTS.md typing bullet, CHANGELOG)
**Depends on** 53 (drops the `Database& db` parameter from `resolve_fk_label`/`resolve_element_fk_labels`; if 53 has not landed, keep that parameter). **Run this plan before 55**, which turns `TypeValidator` into free functions after this plan rewrites them. · **Overlaps with** 05 (moves array FK resolution into `prepare_group_data`; apply the `caller` threading there too), 55 (same file, next), 59 (`get_foreign_key` reuse in `resolve_fk_label` — do it here, see step 2)

## Why

Root AGENTS.md, Design Decision "One scalar typing policy lives in C++": an int64 is accepted for
INTEGER and REAL, a double only for REAL, and a string for TEXT / INTEGER-FK / DATE_TIME.
"`TypeValidator` (scalar create/update) and `value_matches_type` (time-series writes) share this
rule". Today the rule is **coded twice**:

- `internal::value_matches_type` (`src/database_internal.h`, ~L131-145), a four-line `std::visit`;
- `TypeValidator::validate_value` (`src/type_validator.cpp`, ~L28-70), a second copy with its own
  branches, including a **dead** string→INTEGER allowance:
  ```cpp
            } else if constexpr (std::is_same_v<T, std::string>) {
                // String can go to TEXT, INTEGER (FK label resolution), or DATE_TIME (stored as TEXT)
                if (expected_type != DataType::Text && expected_type != DataType::Integer &&
                    expected_type != DataType::DateTime) {
  ```
  It is dead because FK label strings are converted to ids by `Impl::resolve_fk_label` **before**
  validation, and a string on a non-FK INTEGER column is rejected there first
  (`src/database_impl.h`, ~L142-171).

Two create/update errors break the Pattern 1 rule that "validators thread the calling operation's
name":

1. **A string on a non-FK INTEGER column** (`resolve_fk_label`):
   ```cpp
        // String value on a non-FK INTEGER column is an error
        auto col_type = table_def.get_data_type(column);
        if (col_type && *col_type == DataType::Integer) {
            throw std::runtime_error("Cannot resolve attribute: '" + column + "' is INTEGER but received string '" +
                                     str_val + "' (not a foreign key)");
        }
   ```
   `resolve attribute` is not an operation the caller invoked.
2. **An unknown attribute name** in `create_element`/`update_element`. `TypeValidator::validate_scalar`
   calls `Schema::get_data_type(table, column)` (`src/schema.cpp`, ~L71-81), which throws an
   ad-hoc `Column 'x' not found in table 'y'` with no pattern and no operation.
   `Impl::require_column` already has the right text for that condition:
   `Cannot <op>: column '<c>' not found in table '<t>'`, pinned in `tests/test_database_update.cpp`
   (~L1529).

`resolve_fk_label` also hand-rolls the FK lookup loop that `TableDefinition::get_foreign_key`
already provides (`src/schema.cpp`; used by `update_relation`).

Principles: logic lives in one place, and the error-pattern rule holds.

## Constraints and decisions

- **Maintainer notes (binding):**
  - Keep the **early throw** in `resolve_fk_label`, threading `caller` into it and wording it like
    `validate_value`, so the string-on-INTEGER case is still rejected before any write.
  - Delete the dead string allowance.
  - The unknown-column text must be `Impl::require_column`'s exact wording.
  - Delete `Schema::get_data_type(table, column)`.
  - Pin both messages with exact-match assertions.
  - Update the src/AGENTS.md typing-policy bullet.
- Keep the DATE_TIME **content** check (`datetime::is_valid_iso8601`) as a separate guard after the
  shape check. The src/AGENTS.md "DATE_TIME content is checked by both halves" bullet explains why
  it must not move into `value_matches_type`.
- The accepted values do not change. Existing messages `Cannot <op>: type mismatch for <context>:
  expected <T>, got INTEGER|REAL|TEXT` stay byte-identical. `internal::value_type_name` (in
  `database_internal.h`, ~L149) returns the same `INTEGER`/`REAL`/`TEXT` spellings; confirm by
  reading it.

## Changes

### 1. `src/type_validator.cpp` — `validate_value` delegates the shape check

```cpp
void TypeValidator::validate_value(const std::string& caller,
                                   const std::string& context,
                                   DataType expected_type,
                                   const Value& value) {
    // The shape rule is the one typing policy (internal::value_matches_type): int64 -> INTEGER or
    // REAL, double -> REAL, string -> TEXT or DATE_TIME, NULL -> any. FK label strings never get
    // here: Impl::resolve_fk_label turns them into ids (or rejects them) first.
    if (!internal::value_matches_type(value, expected_type)) {
        throw std::runtime_error("Cannot " + caller + ": type mismatch for " + context + ": expected " +
                                 data_type_to_string(expected_type) + ", got " + internal::value_type_name(value));
    }
    // Content check, separate on purpose (see src/AGENTS.md): a DATE_TIME column is TEXT every
    // binding parses back into a date, so an unparseable value is rejected here.
    if (expected_type == DataType::DateTime) {
        if (const auto* s = std::get_if<std::string>(&value); s && !datetime::is_valid_iso8601(*s)) {
            throw std::runtime_error("Cannot " + caller + ": invalid DATE_TIME value for " + context + ": '" + *s +
                                     "' (expected YYYY-MM-DD or YYYY-MM-DDTHH:MM:SS)");
        }
    }
}
```
Add `#include "database_internal.h"` if `type_validator.cpp` does not see `internal::`. It lives
in `src/`, so the quoted include resolves. Check that `database_internal.h` does not include
`type_validator.h` (circularity).

### 2. `validate_scalar` / `validate_array` look the column up themselves

```cpp
namespace {
DataType column_type(const Schema& schema, const std::string& caller, const std::string& table,
                     const std::string& column) {
    // Same wording as Impl::require_column, so one condition has one message.
    const auto* table_def = schema.get_table(table);
    const auto type = table_def ? table_def->get_data_type(column) : std::nullopt;
    if (!type) {
        throw std::runtime_error("Cannot " + caller + ": column '" + column + "' not found in table '" + table + "'");
    }
    return *type;
}
}  // namespace

void TypeValidator::validate_scalar(const std::string& caller, const std::string& table,
                                    const std::string& column, const Value& value) const {
    validate_value(caller, "column '" + column + "'", column_type(schema_, caller, table, column), value);
}

void TypeValidator::validate_array(const std::string& caller, const std::string& table,
                                   const std::string& column, const std::vector<Value>& values) const {
    const auto expected = column_type(schema_, caller, table, column);
    for (size_t i = 0; i < values.size(); ++i) {
        validate_value(caller, "array '" + column + "' index " + std::to_string(i), expected, values[i]);
    }
}
```
Check `Schema::get_table` returns `const TableDefinition*` (`grep -n "get_table" include/quiver/schema.h`).

### 3. Delete `Schema::get_data_type(table, column)`

Remove the declaration (`include/quiver/schema.h`, ~L63-64) and the definition
(`src/schema.cpp`, ~L71-81). `TableDefinition::get_data_type(column)` stays. Check with
`grep -rn "get_data_type(" src/ include/ tests/`: only the `TableDefinition` form should remain.

### 4. `Impl::resolve_fk_label` — use `get_foreign_key`, take `caller`, Pattern 1 early throw

New:
```cpp
    Value resolve_fk_label(const char* caller, const TableDefinition& table_def, const std::string& column,
                           const Value& value) const {
        if (!std::holds_alternative<std::string>(value)) {
            return value;
        }
        const auto& label = std::get<std::string>(value);

        if (const auto* fk = table_def.get_foreign_key(column)) {
            auto id = lookup_id_by_label(fk->to_table, label);
            if (!id) {
                throw std::runtime_error("Failed to resolve label '" + label + "' to ID in table '" + fk->to_table + "'");
            }
            return *id;
        }

        // A string on a non-FK INTEGER column is rejected here, before any write, with the same
        // wording validate_value uses for every other mismatch.
        if (const auto type = table_def.get_data_type(column); type && *type == DataType::Integer) {
            throw std::runtime_error(std::string("Cannot ") + caller + ": type mismatch for column '" + column +
                                     "': expected INTEGER, got TEXT");
        }
        return value;
    }
```
(Drop `Database& db` and the `db` argument to `lookup_id_by_label` if plan 53 has landed. Otherwise
keep them.) Check `TableDefinition::get_foreign_key(column)` returns `const ForeignKey*`
(`grep -n "get_foreign_key" include/quiver/schema.h`).

Thread `caller` through the callers:
- `resolve_element_fk_labels` (~L173-211) calls it for scalars (~L179) and arrays (~L202). Give
  `resolve_element_fk_labels` a `const char* caller` parameter and pass it on. Its callers are
  `create_element` (`"create_element"`) and `update_element` (`"update_element"`); check with
  `grep -n "resolve_element_fk_labels" src/*.cpp`. If plan 05 moved the array resolution into
  `prepare_group_data`, that function already has `caller`.
- `update_group_rows` (`src/database_update.cpp`, ~L192):
  `value = resolve_fk_label(*table_def, col_name, value, db);` becomes
  `value = resolve_fk_label(caller, *table_def, col_name, value);`. `update_group_rows` already
  receives `caller`.

Keep the "Failed to resolve label" message. src/AGENTS.md records it as Pattern 3.

## Tests

### `tests/test_database_create.cpp`

- Tighten `TEST(Database, RejectStringForNonFkIntegerColumn)` (~L500). It uses `EXPECT_THROW`
  today. Keep its setup, and assert the exact message in the file's style (e.g. `try { ... FAIL(); }
  catch (const std::runtime_error& e) { EXPECT_STREQ(e.what(), "..."); }`, or the helper the file
  already uses). The expected text is
  `"Cannot create_element: type mismatch for column '<col>': expected INTEGER, got TEXT"`, where
  `<col>` is the column that test writes.
- Add `TEST(Database, CreateElementUnknownScalarAttribute)`: `create_element("Configuration",
  Element().set("label", "x").set("no_such_column", int64_t{1}))` throws exactly
  `"Cannot create_element: column 'no_such_column' not found in table 'Configuration'"`. Use
  `basic.sql` via `VALID_SCHEMA`.

### `tests/test_database_update.cpp`

- Add the update twin: `update_element("Configuration", 1, Element().set("no_such_column", int64_t{1}))`
  throws `"Cannot update_element: column 'no_such_column' not found in table 'Configuration'"`.
  Only if `update_element` validates scalars before `require_element`: check the order in
  `src/database_update.cpp`. Otherwise create the element first.

### Existing tests that must still pass

All type-mismatch tests (`grep -rn "type mismatch" tests/*.cpp`), the DATE_TIME content tests
(`grep -rn "invalid DATE_TIME" tests/*.cpp`), the FK label tests (`grep -rn "Failed to resolve label" tests/*.cpp`)
and the Lua FK tests (`tests/test_lua_runner_fk.cpp`). Search every test for the old texts and
update each hit: `grep -rn "Cannot resolve attribute\|not found in table '" tests/ bindings/`. Any
binding test that pinned `Cannot resolve attribute` changes to the new message.

## Docs and changelog

- `src/AGENTS.md`, "One scalar typing policy" bullet. Replace "string matches `TEXT`/`INTEGER`(FK
  label)/`DATE_TIME`. Keep the two in sync (root design decision)." with: "string matches `TEXT` /
  `DATE_TIME`. `TypeValidator::validate_value` *calls* `value_matches_type`, so the rule lives in
  one function. An FK label string never reaches it: `Impl::resolve_fk_label` turns it into an id
  first, and rejects a string on a non-FK INTEGER column itself (Pattern 1, naming the caller)."
  Also fix any src/AGENTS.md sentence calling `resolve_fk_label`'s throw Pattern 3. Only the
  label-miss is Pattern 3.
- Root AGENTS.md typing-policy decision: "a string for TEXT / INTEGER-FK / DATE_TIME" stays true as
  a policy statement, so no edit is needed. If it says "`TypeValidator` and `value_matches_type`
  share this rule", make it "`TypeValidator` delegates to `value_matches_type`".
- `CHANGELOG.md`, under `## [0.12.0] — unreleased` → `### Changed`:
  ```markdown
  - **`create_element`/`update_element` errors name the operation.** A string written to a non-FK
    INTEGER column now reports `Cannot create_element: type mismatch for column 'x': expected
    INTEGER, got TEXT` (was `Cannot resolve attribute: ...`), and an unknown attribute reports
    `Cannot create_element: column 'x' not found in table 'T'` (was `Column 'x' not found in table
    'T'`). The accepted values are unchanged.
  ```

## Verification

From the repo root:
1. `cmake --build build --config Debug`
2. `./build/bin/quiver_tests.exe` and `./build/bin/quiver_c_tests.exe`
3. `scripts/test-all.bat` (binding suites may pin the old messages)
4. `scripts/format.bat`

## Acceptance criteria

- [x] `validate_value` has no per-type shape branches. It calls `value_matches_type`, then the
      DATE_TIME content check.
- [x] `Schema::get_data_type(table, column)` is gone.
- [x] Both new messages are Pattern 1, name the caller, and are pinned exactly.
- [x] All suites green, docs and CHANGELOG updated.

## Pitfalls

- `value_matches_type` treats a string as matching TEXT and DATE_TIME only. That is correct here,
  because FK strings are already ids by the time validation runs. If a path validates *before*
  resolving FKs, it now rejects FK labels. Check the order in `create_element`/`update_element`
  (resolve, then validate) and in plan 05's `prepare_group_data`.
- `data_type_to_string(expected_type)` is the public one in `quiver/data_type.h`. It is not the Lua
  file's local shadow (plan 49).

## Out of scope

- Turning `TypeValidator` into free functions and moving headers (plan 55).
- `import_csv`'s own parse rules (`parse_integer`/`parse_float`).

## Implementation notes

Implemented on `rs/plan56`. At planning time the branch sat at master `3ca11b8`. By the time implementation started it had been fast-forwarded to `96c205e`, which brought in plans 53 (#368) and 54 (#369). So `git fetch origin && git merge origin/master` printed "Already up to date." 53 had landed. 55 had not, so this plan ran before 55, as the README order note says.

**Verdict: implement.** It removes a duplicated rule and a dead branch, deletes one method, and fixes two error texts that broke Pattern 1. The values `Database` accepts do not change.

### Drift fixed

- **Plan 05 had landed.** `resolve_element_fk_labels` is now `resolve_scalar_fk_labels`, which handles scalars only. Arrays are resolved in `prepare_group_data`, which already had `caller`. `caller` was threaded through three paths:
  - `resolve_scalar_fk_labels` (new first parameter): `create_element` / `update_element`;
  - `prepare_group_data`;
  - `Impl::update_group_rows`.
- **Plan 53 had landed.** There is no `Database& db` anywhere. `resolve_fk_label` and `resolve_scalar_fk_labels` are now `const`, since `lookup_id_by_label` is a const member.
- **The existing test writes an array, not a scalar.** `RejectStringForNonFkIntegerColumn` writes `score`, which is a column of the set table `Child_set_scores`. The early throw always says `column '<c>'`, so the pinned text is `type mismatch for column 'score'`. Every other array mismatch says `array '<c>' index <i>`. That small difference was kept on purpose: the maintainer note keeps the early throw, and the `resolve_fk_label` comment says so.
- **CHANGELOG.** The plan says `[0.12.0]`. Plan 54 had already opened `## [0.12.8] — unreleased` with `### Removed`. `### Changed` went in before it. The entry also says that `update_vector_group` / `update_set_group` name themselves.
- **One more CHANGELOG line**, beyond the plan, added after review: `### Removed` → **BREAKING (C++ only)** `Schema::get_data_type(table, column)` removed.
  - It was a method of `QUIVER_API Schema` in an installed header. Plan 54 logged the same kind of removal in this section.
  - The line also notes that the public static `TypeValidator::validate_value` no longer accepts a string for INTEGER. No `Database` path passed it one.
  - The plan's "Breaking no" holds for every binding and every `Database` method.
- **Docs the plan missed:**
  - **src/AGENTS.md "DATE_TIME content" bullet.** It said `validate_value` "calls it in its string branch" and argued against "restoring symmetry". Both halves now have the same shape, a separate content guard after the `value_matches_type` shape check, so the bullet now says that.
  - **src/AGENTS.md typing bullet and root AGENTS.md decision.** Both now say that the time-series writers resolve no labels (existing behaviour; the old text implied they did).
  - **Root AGENTS.md** now says that `TypeValidator` delegates the shape check to `value_matches_type`.
  - **src/AGENTS.md "Label→id resolution" bullet.** It now says `resolve_fk_label` reports "a miss is Pattern 3".
- **Small addition to the tests.** `UpdateElementStringForNonFkIntegerScalar` pins the new `caller` on the scalar path, which the existing array test does not reach.
- **Unknown-column test for update.** `update_element` runs `require_element` before validation, so `UpdateElementUnknownScalarAttribute` creates the element first.

### Results

- **Red first.** All four tests failed on the old code, each with its old message:
  - `Cannot resolve attribute: 'score' is INTEGER but received string 'not_a_label' (not a foreign key)`
  - `Column 'no_such_column' not found in table 'Configuration'` (create and update)
  - `Cannot resolve attribute: 'integer_attribute' is INTEGER but received string 'abc' (not a foreign key)`
- **Green after the change.** `quiver_tests` 1395/1395 and `quiver_c_tests` 571/571.
- **`scripts/test-all.bat`** exited 0:

  | Suite | Result |
  |---|---|
  | C++ | 1395 |
  | C API | 571 |
  | Julia | 1575 |
  | Dart | 445 |
  | JS | 242 |
  | Python | 350 |

  No binding pinned an old message; each binding's test for this case only asserts that it throws.
- **`scripts/format.bat`** exited 0:
  - clang-format changed nothing in the diff;
  - Biome again rewrote 43 JS files CRLF→LF. `git diff --ignore-cr-at-eol bindings/js` was empty, and `git checkout -- bindings/js` reverted them.
- **Greps.**
  - `git grep -n "Cannot resolve attribute" -- src tests bindings docs` prints nothing. Plain `grep -r` would still match stale ignored DLLs in `bindings/python/.venv` and `bindings/dart/.dart_tool`.
  - `git grep -n "get_data_type(" -- src include tests` shows only the one-argument `TableDefinition` form.
- **Adversarial review.** Two read-only workflows ran: one on the execution plan and one on the diff. Each finding was verified independently.
  - The plan review led to the time-series wording, the DATE_TIME bullet rewrite, the `git grep` form and the 55-first fallback.
  - The diff review led to the `### Removed` line and the more precise `resolve_fk_label` comment.
  - The diff review confirmed that `validate_value` accepts the same values and produces byte-identical messages for every `DataType` × variant pair, except the dropped string→INTEGER pair, which nothing reached.

### For later plans

- **Plan 55.**
  - The `TypeValidator` bodies are final. `column_type` is an anonymous-namespace helper in `type_validator.cpp` and moves with them unchanged.
  - `Schema::get_data_type(table, column)` is gone, so nothing in `schema.h` refers to it.
  - The root AGENTS.md typing sentence now reads "`TypeValidator` (scalar create/update) delegates the shape check to `value_matches_type`, which the time-series writers call directly". 55's quoted anchor ("... and `value_matches_type` (time-series writes) share this rule") no longer matches. Rewrite the new sentence with the free-function names.
  - `TypeValidator::validate_value` is also named in the src/AGENTS.md typing and DATE_TIME bullets and in the `resolve_fk_label` comment (`src/database_impl.h`).
  - 55's CHANGELOG entry (headers no longer installed) can absorb this plan's `Schema::get_data_type` `### Removed` line.
- **Plan 59.** Step 2's `resolve_fk_label` half is done: it uses `TableDefinition::get_foreign_key`. The `database_csv_export.cpp` FK loops are still there.
- **Plan 75.** Unchanged from plan 52's note: `tests/AGENTS.md` still describes a seven-step `test-all.bat` with a CLI smoke test, but the script runs six suites.
