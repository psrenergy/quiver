# 55 — `TypeValidator` becomes free functions; `schema.h`, `schema_validator.h` and `type_validator.h` move into `src/`

**Batch** 6 · **Severity** low · **Breaking** yes, for C++ consumers only: three installed headers are removed · **Size** M · **Layers** C++ core (+ root AGENTS.md, src/AGENTS.md, CHANGELOG)
**Depends on** **56 — run plan 56 first, despite the numbering.** 56 rewrites `TypeValidator::validate_value` / `validate_scalar` / `validate_array` bodies and deletes `Schema::get_data_type`; this plan then moves the result. Also after 53 (execute into Impl) and 54. · **Overlaps with** 56 (same functions), 57/62 (edit `schema.h`/`schema.cpp`; if they land after this plan, they edit `src/schema.h`), 06 (edits `schema_validator.cpp`)

## Why

`include/quiver/schema.h`, `include/quiver/schema_validator.h` and `include/quiver/type_validator.h`
are installed, `QUIVER_API`-exported public headers, but nothing outside `src/` includes them:
`grep -rn "quiver/schema.h\|quiver/schema_validator.h\|quiver/type_validator.h" src tests include`
finds only `src/` files (`database_impl.h`, `database_internal.h`, `database_csv_import.cpp`,
`database_csv_export.cpp`, `schema.cpp`, `schema_validator.cpp`, `type_validator.cpp`). No binding
wraps them, and the C API exposes schema introspection only through `get_*_metadata` / `list_*`.
`src/AGENTS.md` nonetheless lists them under "C++ public headers".

`TypeValidator` (`include/quiver/type_validator.h`) is a class whose only state is a
`const Schema&`:

```cpp
class QUIVER_API TypeValidator {
public:
    explicit TypeValidator(const Schema& schema);
    void validate_scalar(const std::string& caller, const std::string& table, const std::string& column, const Value& value) const;
    void validate_array(const std::string& caller, const std::string& table, const std::string& column, const std::vector<Value>& values) const;
    static void validate_value(const std::string& caller, const std::string& context, DataType expected_type, const Value& value);
private:
    const Schema& schema_;
};
```

Because it holds a reference to the `Schema`, `Database::Impl` keeps **two** lazily published
members, and `load_schema_metadata` has to publish them together (`src/database_impl.h`, ~L68,
~L350-356):

```cpp
    mutable std::unique_ptr<TypeValidator> type_validator;
    ...
        // TypeValidator holds a reference to the Schema; moving the unique_ptr keeps the pointee.
        type_validator = std::make_unique<TypeValidator>(*loaded);
```

The root AGENTS.md lazy-schema decision spends a sentence on keeping that pair consistent
("`schema` and `type_validator` are `mutable` ... publishes neither until validation passes").

Principles: simple over abstract (a class with one reference member is three functions), and a
public API should be what is meant to be used.

## Constraints and decisions

- **Maintainer notes (binding):**
  - BREAKING, with a CHANGELOG entry.
  - `load_schema_metadata` publishes `schema` alone.
  - Update the root AGENTS.md lazy-schema decision text and the src/AGENTS.md file map.
  - Land after plan 56.
- The free functions take `const Schema&` plus the table name, so the missing-table/column
  behaviour (which plan 56 rewrote to Pattern 1) is unchanged.
- `data_type.h` and `value.h` stay public, because other public headers use them.
- Do not change SQLite's `PUBLIC` link visibility here. That depends on whether tests include
  `sqlite3.h`, and it is a separate decision.
- `src/` is already on `quiver`'s PRIVATE include path (`src/CMakeLists.txt`,
  `target_include_directories`), so `#include "schema.h"` resolves. The tests do not include these
  headers, so the test include path needs no change.

## Changes

### 1. Move the headers

```
git mv include/quiver/schema.h            src/schema.h
git mv include/quiver/schema_validator.h  src/schema_validator.h
git mv include/quiver/type_validator.h    src/type_validator.h
```
In each moved header:
- Drop `QUIVER_API` from the class declarations. Remove `#include "export.h"` /
  `#include "quiver/export.h"` if nothing else in the file uses it.
- Keep the includes of public headers as `#include "quiver/data_type.h"` and
  `#include "quiver/value.h"`.
- Update the include guard names if they encode the path (e.g. `QUIVER_SCHEMA_H` can stay).

### 2. Update includes

- `#include "quiver/schema.h"` becomes `#include "schema.h"` in `src/database_impl.h`,
  `src/database_internal.h`, `src/database_csv_import.cpp`, `src/database_csv_export.cpp` and
  `src/schema.cpp`.
- `#include "quiver/schema_validator.h"` becomes `#include "schema_validator.h"` in
  `src/database_impl.h` and `src/schema_validator.cpp`.
- `#include "quiver/type_validator.h"` becomes `#include "type_validator.h"` in
  `src/database_impl.h` and `src/type_validator.cpp`.

Re-run the grep from "Why" afterwards. It must print nothing.

### 3. `TypeValidator` to free functions (`src/type_validator.h` / `.cpp`)

New `src/type_validator.h` body:
```cpp
namespace quiver {

// Scalar/array type validation for create_element / update_element (the scalar half of the one
// typing policy, root AGENTS.md). `caller` names the public operation for Pattern 1 messages.
void validate_scalar(const std::string& caller, const Schema& schema, const std::string& table,
                     const std::string& column, const Value& value);
void validate_array(const std::string& caller, const Schema& schema, const std::string& table,
                    const std::string& column, const std::vector<Value>& values);
void validate_value(const std::string& caller, const std::string& context, DataType expected_type, const Value& value);

}  // namespace quiver
```
In `src/type_validator.cpp`, turn each member into the matching free function. Replace
`schema_.` with `schema.`, and delete the constructor. Keep the bodies exactly as plan 56 left them.

Check for name clashes: `grep -rn "validate_scalar\|validate_array\|validate_value" src/`. If
`internal::` or `Impl` already has a function with one of these names, put the free functions in
`namespace quiver::typing` or `quiver::internal` and qualify the calls.

### 4. `src/database_impl.h` — publish `schema` alone

- Delete `mutable std::unique_ptr<TypeValidator> type_validator;` (~L68) and update the comment
  above it that mentions both members.
- In `load_schema_metadata` (~L350-356), delete the `type_validator = std::make_unique<...>` line
  and its comment. Keep "build + validate, then `schema = std::move(loaded);`" and the comment on
  why nothing is published before validation passes (a half-loaded state).
- `insert_rows_into_group_table` (~L256):
  `type_validator->validate_array(caller, table_name, col_name, *values_ptr);` becomes
  `validate_array(caller, *schema, table_name, col_name, *values_ptr);`. If plan 05 moved this
  loop into `prepare_group_data`, edit it there.

### 5. Call sites

- `src/database_create.cpp` (~L19): `impl_->type_validator->validate_scalar("create_element", collection, name, value);`
  becomes `validate_scalar("create_element", *impl_->schema, collection, name, value);`
- `src/database_update.cpp` (~L31): same, with `"update_element"`.
- Any `TypeValidator::validate_value(...)` call becomes `validate_value(...)`. Find them with
  `grep -rn "TypeValidator::" src/`.

### 6. Comments that name the class

- `src/database_csv_import.cpp` ~L229 and `src/database_time_series.cpp` ~L45 mention
  `TypeValidator`. Reword to "the scalar validators (`validate_scalar`/`validate_value`,
  `type_validator.cpp`)".
- `tests/test_database_lifecycle.cpp` ~L339: "create_element requires schema and type_validator to
  be loaded" becomes "... requires the schema to be loaded".
- `src/utils/datetime.h` ~L84, if it mentions `TypeValidator::validate_value`: reword the same way.

## Tests

No behaviour change. The full C++ and C API suites are the net, plus
`test_schema_validator.cpp` / `test_type_validator.cpp` if they exist:
`ls tests | grep -i validator`. A test that constructs `quiver::TypeValidator` directly must switch
to the free functions. It must also include the header by its new path, which needs `src/` on the
test include path. If such a test exists, either change it to exercise the behaviour through
`Database::create_element`, or add `${PROJECT_SOURCE_DIR}/src` to `quiver_tests`' private include
directories in `tests/CMakeLists.txt`. Prefer the first.

## Docs and changelog

- `src/AGENTS.md` file map: move the three entries from the `include/quiver/` block into the `src/`
  block (`schema.h`, `schema_validator.h`, `type_validator.h — scalar/array type validation (free
  functions)`). Fix the sentence claiming `csv_read` is "the first .cpp in `src/` with no
  `include/quiver/` public counterpart ... every other `QUIVER_SOURCES` entry implements a public
  header". `schema.cpp`, `schema_validator.cpp` and `type_validator.cpp` also have none now. Plan 76
  touches the same sentence; whichever lands second reconciles them.
- `src/AGENTS.md`, "Schema metadata loads lazily" bullet: "`schema` and `type_validator` are
  `mutable` ... publishes **neither** member until `SchemaValidator::validate()` passes —
  assigning `schema` first would leave a half-loaded state (schema set, `type_validator` null)"
  becomes "`schema` is `mutable` (const readers trigger the load), and `load_schema_metadata()`
  publishes it only after `SchemaValidator::validate()` passes, so a failed lazy load leaves no
  half-loaded state for the next call." Also fix the "TypeValidator threads the caller's name"
  bullet: "`validate_scalar`/`validate_array` thread the caller's name".
- Root `AGENTS.md`, Design Decision "Schema metadata loads lazily": "`schema` and `type_validator`
  are `mutable` so the const readers can trigger it, and `load_schema_metadata` publishes neither
  until validation passes" becomes "`schema` is `mutable` so the const readers can trigger it, and
  `load_schema_metadata` publishes it only after validation passes". Also update the typing-policy
  decision's mention of `TypeValidator` ("`TypeValidator` (scalar create/update) and
  `value_matches_type`") to "`validate_scalar`/`validate_value` (`src/type_validator.cpp`) and
  `value_matches_type`".
- `CHANGELOG.md`, under `## [0.12.0] — unreleased` → `### Removed`:
  ```markdown
  - **BREAKING (C++ only) — `quiver/schema.h`, `quiver/schema_validator.h` and
    `quiver/type_validator.h` are no longer installed.** They were internal (no binding and no C
    API used them). *Adapt:* use `Database::get_*_metadata` / `list_*` for schema introspection.
  ```

## Verification

From the repo root:
1. `cmake --build build --config Debug`
2. `./build/bin/quiver_tests.exe` and `./build/bin/quiver_c_tests.exe`
3. `cmake --install build --prefix %TEMP%\quiver-install` and check that the three headers are
   absent from `%TEMP%\quiver-install\include\quiver\`. Skip this step if the project has no install
   rules (`grep -n "install(" src/CMakeLists.txt CMakeLists.txt`).
4. `scripts/test-all.bat`
5. `scripts/format.bat`

## Acceptance criteria

- [x] The three headers live in `src/` without `QUIVER_API`, and `grep -rn "quiver/schema.h\|quiver/schema_validator.h\|quiver/type_validator.h" .` (excluding `build/`) finds nothing. (Checked with the scoped `git grep`; see Implementation notes.)
- [x] No `TypeValidator` class remains, and `Impl` has no `type_validator` member.
- [x] All suites green. Both AGENTS.md files and the CHANGELOG are updated.

## Pitfalls

- `quiver.h` (the umbrella header) must not include the moved headers. It does not at HEAD (check
  `include/quiver/quiver.h`).
- `database_internal.h` is included by `lua_runner.cpp`? Check with
  `grep -n "database_internal.h\|schema.h" src/lua_runner.cpp`. Both are in `src/`, so the
  quoted include still resolves.

## Out of scope

- The typing-policy rewrite (plan 56).
- `Row`/`Result` (plan 54, which keeps them public).

## Implementation notes

I implemented this on `rs/plan55`, merging `origin/master` at `53300d9` first. That merge
carries plans 53 (`5b12950`), 54 (`96c205e`) and 56 (`53300d9`), so all three dependencies had
landed. The user confirmed 56 should run before 55. I verified the plan against the code twice:
once at `3ca11b8` in plan mode, with a read-only adversarial pass, and again after the merge. Every
edit is re-anchored by function name.

### Deviations and drift

- **Array validation call site.** It is in `Impl::validate_group_columns`, not
  `insert_rows_into_group_table`; plan 05 moved it. It now reads
  `validate_array(caller, *schema, table_name, col_name, values)`, with the
  `if (!values.empty())` guard kept. `validate_group_columns` stays `const` and non-static.
- **Bodies.** I kept them exactly as plan 56 left them, including its anonymous
  `column_type(schema, caller, table, column)` helper, which the free functions now pass `schema`
  to directly. The constructor and the `schema_` member are gone. clang-format joined the
  `validate_value` definition onto one line.
- **The comment above `Impl::schema` needed no change.** It already describes one member. Only the
  `type_validator` line went. I also changed "unlike schema/type_validator above" above
  `ui_metadata`, and `resolve_fk_label`'s comment naming `TypeValidator::validate_value`, which
  plan 56 added.
- **`load_schema_metadata`'s comment and both AGENTS.md lazy-schema texts name the real hazard.**
  With one member, the old "half-loaded state ... crash the next call" is no longer what can go
  wrong. A schema published before `validate()` would stay published, and `require_schema` (which
  loads only while `schema` is null) would never validate it again. The plan's replacement for the
  root AGENTS.md text stopped at "passes" and left the stale "crash the next call" tail, so I
  rewrote the tail as well.
- **More `TypeValidator` mentions than the plan listed.** Besides its own anchors, I changed
  `tests/test_database_csv_import.cpp` (~L591), root `AGENTS.md` L117 ("never sees") and the
  `src/AGENTS.md` mentions at ~L483/485/495/504/515. I also changed the file-map `database_impl.h`
  line from "schema/type validators" to "lazy schema load". Plan 56 had already rewritten the
  root AGENTS.md typing sentence, which now reads "`validate_value` (`src/type_validator.cpp`,
  scalar create/update) delegates the shape check to `value_matches_type`".
- **`src/AGENTS.md` "first `.cpp` with no `include/quiver/` counterpart" paragraph.** Both of its
  clauses became false: "every other internal helper ... is header-only inline" and "every other
  `QUIVER_SOURCES` entry implements a public header". I rewrote both and named `schema.cpp`,
  `schema_validator.cpp` and `type_validator.cpp`.
- **CHANGELOG.** The entry is under `## [0.12.8] — unreleased` → `### Removed`, a section plan 54
  opened; there is no 0.12.0 section. The entry also says that it supersedes the *Adapt* line of
  plan 56's `Schema::get_data_type` entry just above it. That line told C++ users to call
  `schema.get_table(...)`, which no outside code can do any more. No manifest bump.
- **Acceptance grep.** The plan's `grep -rn ... .` can never come back empty: these plan files,
  `.cache/clangd`, `bindings/dart/.dart_tool` build trees and the new CHANGELOG entry all contain
  the strings. I used
  `git grep -n "quiver/schema.h\|quiver/schema_validator.h\|quiver/type_validator.h" -- . ':!.claude/plans' ':!CHANGELOG.md'`
  instead, and it is empty.
- **Commit type.** `refactor!:`, as for plans 42 and 54. It is breaking for C++ consumers only.

### Verification

- **Baseline.** Before any edit, the build was green, with 1395 `quiver_tests` and 571
  `quiver_c_tests`.
- **Configure.** I ran `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
  -DQUIVER_BUILD_TESTS=ON -DQUIVER_BUILD_C_API=ON` first. The root `GLOB_RECURSE ALL_SOURCE_FILES`
  has no `CONFIGURE_DEPENDS`, so without a reconfigure the format target would still list
  `include/quiver/schema.h`.
- **Build.** `cmake --build build --config Debug` exited 0. The only warning is the existing C4701
  `group_type` in `database_csv_import.cpp`, a file where this plan changed only the include line.
- **`quiver_tests`.** 1395/1395 passed.
- **`quiver_c_tests`.** 571/571 passed.
- **`cmake --install build --prefix %TEMP%\quiver-install`.** `include/quiver/` no longer has
  `schema.h`, `schema_validator.h` or `type_validator.h`. I deleted the prefix afterwards.
- **`scripts/test-all.bat`.**
  - The first run failed before running any Python test: `uv` could not install the
    `scikit-build-core` build dependency ("Failed to update Windows PE resources:
    ...uv-trampoline...exe — The system cannot open the device or file specified"). That is an
    environment problem. `bindings/python/tests/test.bat` alone then passed, 350 tests.
  - The final run, after formatting, exited 0 with every suite passing: C++, C API, Julia,
    Dart (445), JS (242 pass, 0 fail) and Python (350).
- **`scripts/format.bat`.** Exited 0.
  - clang-format reflowed only the `validate_value` signature.
  - Biome again rewrote 43 JS files CRLF→LF. `git diff --ignore-cr-at-eol -- bindings/js` was
    empty, and I reverted them with `git checkout -- bindings/js`.

### For later plans

- **57** (L73, L111, L126) and **58** (L64) cite `include/quiver/schema.h`. The file is now
  `src/schema.h`, without `QUIVER_API`. **62** cites only `src/schema.cpp`, so it needs no path
  change.
- **76** (`src/AGENTS.md` ~L91-93): this plan already rewrote the paragraph's two false clauses
  for the three moved pairs. Step 2 still has to add `csv/csv_write.cpp`/`ui_metadata.cpp`
  and change "is the first" to "was the first".
- **README.** The batch-6 table lists 56 as depending on "53, 55". That contradicts the order
  note "56 before 55" and both plan headers, which form the cycle with 55's "56". The
  order that actually ran was 53, 54, 56, 55.
- **`validate_scalar`/`validate_array`/`validate_value`** are free functions in namespace `quiver`
  (`src/type_validator.h`). Any later plan that validates a value calls them with the `Schema`
  (`*impl_->schema` or `*schema` inside `Impl`).
