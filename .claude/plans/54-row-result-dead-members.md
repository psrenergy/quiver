# 54 — Delete the dead `Row`/`Result` members and forward-declare `Result` in `database.h`

**Batch** 6 · **Severity** low · **Breaking** yes, for C++ consumers only: installed-header members are removed; no binding uses them · **Size** S · **Layers** C++ core, C++ tests
**Depends on** 53 (moves `execute` into `Impl`, so `database.h` no longer declares a `Result`-returning member) · **Overlaps with** 55 (moves `schema.h`/`schema_validator.h`/`type_validator.h` to `src/` and replaces `TypeValidator`; this plan does not touch those)

## Why

`include/quiver/row.h` and `include/quiver/result.h` are installed, `QUIVER_API`-exported headers.
Nothing outside the core uses them: no binding, no C API function, and no public `Database`
method returns a `Row`/`Result` (`execute` is private, and after plan 53 it lives on `Impl`).
Several of their members are called only by their own unit tests in `tests/test_row_result.cpp`:

- `Row`: `size()`, `column_count()`, `empty()`, `at(size_t)`, `is_null(size_t)`, `begin()`, `end()`.
- `Result`: `Result()` (the default constructor, `src/result.cpp:7`), `column_count()`, `at(size_t)`.

`src/` never calls them. The only `.at(` hits in `src/` are on `std::map`s, and
`grep -rn "\.column_count()\|\.is_null(" src/` finds nothing. The core uses `Row::operator[]` /
`get_integer` / `get_float` / `get_string`, and `Result::operator[]` / `row_count` / `empty` /
`columns` / `begin` / `end` (range-for in `src/database_internal.h`).

Principle: delete unused code, do not deprecate.

## Constraints and decisions

- **Maintainer note (binding):** delete only the Row/Result half. Moving headers and replacing
  `TypeValidator` is plan 55. Keep `row.h`/`result.h` public (the minimal option). Keep the `get_*`
  tests.
- Keep `Result::begin/end`, because `database_internal.h` iterates results. Keep `Result::empty`,
  `row_count`, `columns` and `operator[]`, and `Row::operator[]` plus the `get_*` getters.
- `Row::get_float` widening an INTEGER is load-bearing (root AGENTS.md), so do not touch it.

## Changes

### `include/quiver/row.h`

Delete `size()`, `column_count()`, `empty()`, `at(size_t)`, `is_null(size_t)`, and the
`// Iterator support` `begin()`/`end()` pair. What remains:
```cpp
class QUIVER_API Row {
public:
    explicit Row(std::vector<Value> values);

    const Value& operator[](size_t index) const;

    // Type-specific getters (return optionals for safe access)
    std::optional<int64_t> get_integer(size_t index) const;
    std::optional<double> get_float(size_t index) const;
    std::optional<std::string> get_string(size_t index) const;

private:
    std::vector<Value> values_;
};
```
Check the private member name in the header before pasting.

### `src/row.cpp`

Delete the definitions of `Row::size`, `Row::empty`, `Row::at` and `Row::is_null`
(`column_count`/`begin`/`end` are inline in the header). If `operator[]` or a getter calls
`at(...)` or `size()` internally, inline that logic, e.g. `values_[index]` or
`values_.at(index)`. Check with `grep -n "at(\|size()" src/row.cpp`.

### `include/quiver/result.h`

Delete `Result();`, `column_count()` and `at(size_t)`. Keep
`Result(std::vector<std::string> columns, std::vector<Row> rows)`, `columns()`, `row_count()`,
`empty()`, `operator[]`, `begin()` and `end()`.

### `src/result.cpp`

Delete `Result::Result() = default;` (~L7), `Result::column_count` and `Result::at` (~L29). If
`operator[]` delegates to `at`, write `return rows_[index];` or keep `rows_.at(index)` inline.
Check whether any code default-constructs a `Result`, e.g. `Result r;` or `return {};` in a
function returning `Result`:
`grep -rn "Result{}\|Result()\|return {};" src/database*.cpp src/database_impl.h`. Fix any hit to
construct with `Result({}, {})`.

### `include/quiver/database.h`

Once plan 53 has landed, nothing in the header refers to `Result`. Replace
`#include "quiver/result.h"` with nothing if it is unused. Otherwise use a forward declaration
`class Result;` (check with `grep -n "Result" include/quiver/database.h`). Any `src/` file that
needs the full type must include `quiver/result.h` itself. `database_impl.h` does, for `execute`.

## Tests — `tests/test_row_result.cpp`

Delete the tests that exercise only the removed members:
- `Row.EmptyRow` (~L13)
- `Row.AtOutOfBounds` (~L21)
- `Row.IsNullTrueForNullValue` (~L37)
- `Row.IsNullFalseForNonNull` (~L43)
- `Row.IteratorSupport` (~L91)
- `Result.DefaultConstructor` (~L107)
- `Result.AtOutOfBounds` (~L128)
- `Result.EmptyResult` (~L135), if it uses `Result()` or `column_count`. If it only checks
  `empty()`/`row_count()` on a `Result({}, {})`, rewrite it that way instead.
- `Result.IteratorOnEmpty` (~L146), if it default-constructs. Otherwise rewrite it with
  `Result({}, {})`.
- In `Result.MixedValueTypes` (~L185), delete only the `column_count()`/`is_null()` assertions.

Keep `Row.OperatorBracketValidIndex`, the `GetInt/GetFloat/GetString` wrong-type and from-null
tests, `Result.ColumnsAccessor`, `Result.IteratorOnNonEmpty` (Result iteration is kept),
`Result.OperatorBracketValid` and the three `RowResult.*` Database-level tests.

Compiling the test file is the check. Any leftover use of a deleted member fails to build.

## Docs and changelog

- `src/AGENTS.md` file map line `row.h / result.h  # Row and Result query-result types`: no change
  needed.
- `CHANGELOG.md`, under `## [0.12.0] — unreleased` → `### Removed` (create the heading if absent):
  ```markdown
  - **BREAKING (C++ only) — unused `Row`/`Result` members removed:** `Row::size`, `column_count`,
    `empty`, `at`, `is_null`, `begin`, `end` and `Result::Result()`, `column_count`, `at`. They were
    reachable only from the installed headers, and no binding used them. *Adapt:* use
    `operator[]` and the `get_*` getters.
  ```

## Verification

From the repo root:
1. `cmake --build build --config Debug`. This compiles the core, the C API and both test binaries.
   A remaining use of a deleted member is a build error.
2. `./build/bin/quiver_tests.exe` and `./build/bin/quiver_c_tests.exe`
3. `scripts/format.bat`

## Acceptance criteria

- [x] The listed members are gone from the headers and `.cpp` files (all except `Row::is_null`,
  which stays; see Implementation notes).
- [x] Everything builds, and both C++ suites pass.
- [x] CHANGELOG `### Removed` entry added.

## Pitfalls

- `Row::is_null` may be used by a C API helper. The grep says no, but the build decides.
- `database_internal.h`'s `read_single_value` may check `result.empty()`. That member is kept.

## Out of scope

- Moving `row.h`/`result.h` to `src/`, and `TypeValidator` (plan 55).

## Implementation notes

Implemented on `rs/plan54` at `5b12950` (plan 53 merged). `git merge origin/master` was a no-op.

### What changed

- **`include/quiver/row.h`, `src/row.cpp`.** `size`, `column_count`, `empty`, `at`, and the
  `begin`/`end` pair are gone. `Row` keeps its constructor, `operator[]`, `is_null` and the three
  `get_*` getters. `get_float`'s widening is untouched.
- **`include/quiver/result.h`, `src/result.cpp`.** `Result()`, `column_count` and `at` are gone.
  `Result` keeps the 2-arg constructor, `columns`, `row_count`, `empty`, `operator[]` and
  `begin`/`end`.
- **`include/quiver/database.h`.** `#include "quiver/result.h"` is deleted, and no forward
  declaration replaces it, because after plan 53 nothing in the header names `Result`.
  `src/database_impl.h` already included `quiver/result.h` (plan 53 added it), so no other
  include changed.
- **`tests/test_row_result.cpp`.**
  - Deleted: `Row.EmptyRow`, `Row.AtOutOfBounds`, `Row.IteratorSupport`,
    `Result.DefaultConstructor`, `Result.AtOutOfBounds`.
  - `Result.EmptyResult`: the column check now reads `columns().size()` instead of
    `column_count()`.
  - `Result.IteratorOnEmpty`: builds its empty result with `Result({}, {})`.
  - `Result.IteratorOnNonEmpty`: checks `row.get_integer(0).value() == count + 1` instead of
    `row.size() == 1`.
  - `Result.MixedValueTypes`: only the `column_count()` assertion was removed.
- **`CHANGELOG.md`.** One `### Removed` entry under `## [0.12.8] — unreleased`, placed before plan
  53's `### Fixed` (Keep a Changelog order).

### Deviations and drift

- **`Row::is_null` is kept (user decision).** The "Why" grep is wrong:
  `src/database_internal.h` `read_grouped_values_all` calls `!result[i].is_null(1)`. That is the
  LEFT JOIN presence test behind all six bulk vector/set readers, and `src/AGENTS.md` documents it.
  So `is_null` stays, and so do its two tests (`Row.IsNullTrueForNullValue`,
  `Row.IsNullFalseForNonNull`) and the `MixedValueTypes` `is_null(3)` assertion. It is also left off
  the CHANGELOG list, whose *Adapt* line names `is_null` as one of the kept accessors.
- **`Result.IteratorOnNonEmpty` needed an edit.** The plan listed it as "keep", but it called
  `row.size()`. The new assertion also pins row order.
- **`Result.EmptyResult` and `Result.IteratorOnEmpty` were rewritten, not deleted.** Both test kept
  members: `empty()`/`row_count()` with columns present, and `begin`/`end` on an empty result.
  `EmptyResult` is the only unit test of `Result::empty()`.
- **CHANGELOG section.** The entry is under 0.12.8, not 0.12.0. Plan 53 opened that section.
- **Commit type.** The commit is `refactor!:`, matching the `fix!:` convention plans 45/48 used for
  breaking changes. No manifest bump.

### Verification

- **Build.** `cmake --build build --config Debug` exited 0. The only warnings are ones that were
  already there: C4715 in `time_properties.cpp`, C4701 `group_type` in `import_csv`, and C4100 in
  `test_c_api_expression.cpp`.
- **`quiver_tests`.** 1392/1392 passed. That is 1397 at plan 53 minus the 5 deleted tests, and all
  18 `Row.*`/`Result.*`/`RowResult.*` tests pass.
- **`quiver_c_tests`.** 571/571 passed.
- **`scripts/format.bat`.** Exited 0, and clang-format changed nothing.
  - Biome again rewrote 43 JS files CRLF→LF. `git diff --ignore-cr-at-eol bindings/` was empty, and
    `git checkout -- bindings/js` reverted them.
- **Line endings.** Checked by byte count: `CHANGELOG.md` and this plan are still CRLF, and the
  C++ files are still LF.

### For later plans

- **Plan 55.** `quiver/database.h` no longer pulls in `result.h`/`row.h`. A `src/` file that needs
  `Result`/`Row` gets them from `database_impl.h`, `database_internal.h`, or its own include
  (`database.cpp`). The umbrella `quiver/quiver.h` never included `result.h`, so C++ consumers now
  reach these types only by including the header directly.
- **Plan 59.** `Row::is_null` is still the presence test in `read_grouped_values_all`. Keep it if
  that function is touched.
