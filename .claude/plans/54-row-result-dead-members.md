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
- `CHANGELOG.md`, under `## [0.11.0] — unreleased` → `### Removed` (create the heading if absent):
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

- [ ] The listed members are gone from the headers and `.cpp` files.
- [ ] Everything builds, and both C++ suites pass.
- [ ] CHANGELOG `### Removed` entry added.

## Pitfalls

- `Row::is_null` may be used by a C API helper. The grep says no, but the build decides.
- `database_internal.h`'s `read_single_value` may check `result.empty()`. That member is kept.

## Out of scope

- Moving `row.h`/`result.h` to `src/`, and `TypeValidator` (plan 55).
