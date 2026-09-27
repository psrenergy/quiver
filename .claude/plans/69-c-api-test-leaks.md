# 69 — C API tests: fix the handle leaks and the raw `delete[]` frees

**Batch** 7 · **Severity** medium (a leaked binary writer breaks every later test in the fixture) · **Breaking** no · **Size** S · **Layers** C API tests only
**Depends on** none · **Overlaps with** 71 (deletes `UpdateScalarInteger/Float/String` in `test_c_api_database_update.cpp`, ~L15-104, which contain three of the five leaks and one of the `delete[]` sites — if 71 lands first, skip those; if 69 lands first, 71 deletes the fixed tests), 70 (replaces four other tests in the same file)

## Why

The C API test suites manage every handle by hand, and several tests leak.

1. **Five update tests never destroy the update element**, in
   `tests/test_c_api_database_update.cpp`: `UpdateScalarInteger` (~L29), `UpdateScalarFloat`
   (~L59), `UpdateScalarString` (~L89), `UpdateScalarStringTrimsWhitespace` (~L812) and
   `UpdateDateTimeScalar` (~L900). Each builds `quiver_element_t* update`, calls
   `quiver_database_update_element(...)` and never calls `quiver_element_destroy(update)`. Their
   `create_element` result is unchecked too.
2. **An `ASSERT` inside an open-for-write span skips the close.** In
   `tests/test_c_api_expression.cpp` (~L88 `write_fixture_with_metadata`, ~L167 `write_one_cell`,
   ~L189 `write_dense`, ~L1468 `ApplyTernaryShapeMismatch`), `ASSERT_EQ(quiver_binary_file_write(...), QUIVER_OK)`
   sits between `quiver_binary_file_open_file(..., 'w', ...)` and `quiver_binary_file_close(f)`.
   When a write fails, `ASSERT` returns early and the writer stays open. Its path stays in the
   process-wide binary write registry (`src/binary/binary_file.cpp`), and **every later test** that
   reads that fixture file fails too. One failure cascades into many.
3. **Raw `delete[]` instead of the C API's free function**, at 12 sites:
   - `test_c_api_database_update.cpp` ~L102, 372, 422, 430, 825, 914, 1404
   - `test_c_api_database_read_scalar.cpp` ~L406, 987
   - `test_c_api_database_query.cpp` ~L35, 286
   - `test_c_api_database_create.cpp` ~L312

   Strings returned by the C API should be freed with `quiver_database_free_string`, as the describe
   and element C API tests already do. `test_c_api_database_lifecycle.cpp` (~L386, ~L450) also wraps
   `quiver_database_free_integer_array(ids)` in `if (ids != nullptr)`. The free functions accept
   NULL, so the guard is noise.

Principles: RAII and ownership ("explicit and unambiguous"), and tests that fail independently.

## Constraints and decisions

- **Maintainer decision (policy verifier, binding):** keep the scope narrow. Fix these specific
  leaks and frees. **No** suite-wide RAII rewrite and no new handle helper types in
  `tests/test_utils.h`.
- Use `EXPECT_EQ` for the write inside open spans, so the close always runs. If the rest of the
  test is meaningless after a failed write, `break` out of the loop but still close.

## Changes (tests only)

1. `tests/test_c_api_database_update.cpp`, for each of the five tests: after the
   `quiver_database_update_element(...)` call, add
   `EXPECT_EQ(quiver_element_destroy(update), QUIVER_OK);`. Where the test ignores the
   `quiver_database_create_element(...)` return code, wrap it in
   `ASSERT_EQ(..., QUIVER_OK) << quiver_get_last_error();`. Skip the three at ~L29/59/89 if plan 71
   has already deleted those tests.
2. `tests/test_c_api_expression.cpp` at the four sites: change `ASSERT_EQ(quiver_binary_file_write(` to
   `EXPECT_EQ(quiver_binary_file_write(`. Check that the `quiver_binary_file_close(f)` call after
   each loop is not itself behind an `ASSERT` that could return early. If it is, make that one
   `EXPECT` too.
3. The 12 `delete[] <var>;` lines: replace each with `quiver_database_free_string(<var>);`. First
   check that each freed pointer is a C-API-returned string (`char*`) and not an array of pointers.
   An array needs `quiver_database_free_string_array(ptr, count)` instead:
   `grep -n "free_string_array\|free_string(" include/quiver/c/database.h`.
4. `tests/test_c_api_database_lifecycle.cpp` ~L386, ~L450: replace
   `if (ids != nullptr) { quiver_database_free_integer_array(ids); }` with
   `quiver_database_free_integer_array(ids);`.

Afterwards, `grep -n "delete\[\]" tests/test_c_api_*.cpp` must print nothing, unless a test
allocates its own buffer with `new[]`. Leave those.

## Tests

These are test edits. Run the C API suite. To see the cascade fix work, temporarily make one
fixture write fail (e.g. pass a bad dims map in `write_one_cell`). Before the change many tests
fail; after it only the tests that use that fixture's data fail. Revert the experiment.

## Docs and changelog

- `tests/CLAUDE.md`: add one line in the C API section: "Free C API strings with
  `quiver_database_free_string`, never `delete[]`. Inside an open-for-write binary span use `EXPECT`,
  not `ASSERT`, so the writer is always closed."
- No CHANGELOG entry.

## Verification

1. `cmake --build build --config Debug`
2. `./build/bin/quiver_c_tests.exe` (full)
3. If you have a leak checker (e.g. Dr. Memory on Windows, or ASan/valgrind under WSL), run it on
   `quiver_c_tests.exe --gtest_filter=*UpdateScalar*:*UpdateDateTimeScalar*` before and after.
   This is optional.

## Acceptance criteria

- [ ] The five update tests destroy their update element.
- [ ] There is no `ASSERT` between a binary writer's open and close in `test_c_api_expression.cpp`.
- [ ] There are no `delete[]` frees of C-API-returned strings, and no `if (ids != nullptr)` guards
      around free calls.
- [ ] The C API suite is green.

## Pitfalls

- `quiver_database_free_string` and `delete[]` are only interchangeable today by accident: the C
  API allocates with `new char[]` (`quiver::string::new_c_str`). Using the API's own free is the
  contract.

## Out of scope

- A RAII handle layer for the C API tests (explicitly rejected).
- Deleting the per-type update leftovers (plan 71).
