# 72 — Convert 17 bare `EXPECT_THROW` Lua tests to `expect_lua_error` with real substrings

**Batch** 7 · **Severity** medium · **Breaking** no · **Size** S · **Layers** C++ Lua tests only
**Depends on** 47 (renames the helper-named Lua messages; take substrings from the messages as they are **after** 47) · **Overlaps with** 44, 48 (add new `expect_lua_error` tests in the same files), 63/56/57 (change some core messages — re-run each converted test after those land)

## Why

`tests/CLAUDE.md` warns that a bare `EXPECT_THROW` on a `db:` call passes vacuously: a missing
binding, a typo in the method name or any Lua error at all satisfies it. The file already provides
the right helper, `expect_lua_error(lua, script, substring)` (`tests/test_lua_runner.h` ~L41). These
tests call `db:` methods but only assert "something threw":

- `tests/test_lua_runner_errors.cpp` ~L156, ~L167, ~L176, ~L186, ~L196, ~L244
- `tests/test_lua_runner_create.cpp` ~L88, ~L134
- `tests/test_lua_runner_fk.cpp` ~L42, ~L58, ~L211, ~L343
- `tests/test_lua_runner_time_series.cpp` ~L490
- `tests/test_lua_runner_transaction.cpp` ~L42, ~L58, ~L67, ~L110

Keep the bare `EXPECT_THROW` for the generic Lua syntax/runtime tests in
`test_lua_runner_errors.cpp` (~L9-104), which exercise no binding. Leave
`test_lua_runner_write_csv.cpp` ~L1437 alone too; check what it asserts.

## Constraints and decisions

- Use each call's **actual** message, and do not assume every one is Pattern 1. Expected
  substrings at HEAD (confirm each by running the test):
  - errors.cpp:156 `Cannot read_scalar_strings: collection not found`
  - errors.cpp:167 `Cannot read_scalar_strings: column 'nonexistent' not found`
  - errors.cpp:176 `Cannot update_element: collection not found`
  - errors.cpp:186 `Cannot delete_element: collection not found`
  - errors.cpp:196 `Cannot create_element: collection not found` (keep its existing extra check)
  - errors.cpp:244 `Cannot read_element_ids: collection not found`
  - create.cpp:88 `NOT NULL constraint failed` (a missing label is caught by SQLite, via
    `Failed to execute statement`)
  - create.cpp:134 `Cannot create_element: collection not found`
  - fk.cpp:42, 211, 343 `Failed to resolve label` (Pattern 3). Use the specific label each test
    passes, e.g. `Failed to resolve label 'Nonexistent Parent'`.
  - fk.cpp:58: read the test. The review noted `'score' is INTEG...`, which is the old "Cannot
    resolve attribute" text. After plan 56 this becomes
    `Cannot create_element: type mismatch for column 'score': expected INTEGER, got TEXT`.
  - time_series.cpp:490 `row missing required 'date_time'`. Get the exact text with
    `grep -n "missing required" src/`.
  - transaction.cpp:42 `Cannot begin_transaction: transaction already active`
  - transaction.cpp:58 `Cannot commit: no active transaction`
  - transaction.cpp:67 `Cannot rollback: no active transaction`
  - transaction.cpp:110 `intentional error`. The script's own error must come back out through
    `db:transaction`.
- Keep substrings short but specific: the operation plus the condition, not the whole message. That
  way wording tweaks elsewhere don't churn these tests.

## Changes (tests only)

For each site, replace
```cpp
    EXPECT_THROW({ lua.run(R"(...)"); }, std::runtime_error);
```
with
```cpp
    expect_lua_error(lua, R"(...)", "<substring>");
```
Keep the script text byte-identical. Multi-line `EXPECT_THROW(\n lua.run(...), ...)` forms get the
same treatment. Any assertion that follows (e.g. errors.cpp:196's `labels == 1` check) stays after
the call. Check `expect_lua_error`'s signature in `tests/test_lua_runner.h`: it takes
`(quiver::LuaRunner&, const std::string&, const std::string&)`. A test that builds `script` in a
variable passes the variable.

## Tests

To prove each converted test is no longer vacuous, temporarily change one substring to nonsense and
check that the test fails. Revert that before committing.

## Docs and changelog

None. The tests/CLAUDE.md rule already exists.

## Verification

1. `cmake --build build --config Debug`
2. `./build/bin/quiver_tests.exe --gtest_filter=LuaRunner*`
3. Run it on the Release tree as well if you have one (`build-release`). Some messages differ when
   sol2 safety checks are off; they should not for these core-originated messages.

## Acceptance criteria

- [ ] The 17 sites use `expect_lua_error` with a meaningful substring.
- [ ] The generic Lua error tests keep their bare `EXPECT_THROW`.
- [ ] The Lua suites are green.

## Pitfalls

- Land after 47, 56, 57 and 63. Each changes messages some of these tests will pin. If a test fails
  after a later plan lands, update its substring to the new message rather than loosening it.

## Out of scope

- The non-binding Lua syntax/runtime tests.
