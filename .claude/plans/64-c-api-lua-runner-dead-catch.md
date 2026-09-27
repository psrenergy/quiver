# 64 — C API LuaRunner: delete the `catch (...)` branches nothing can reach

**Batch** 6 · **Severity** low · **Breaking** no · **Size** S · **Layers** C API only
**Depends on** none · **Overlaps with** none

## Why

The C API has one documented error-handling shape: `QUIVER_REQUIRE`, then `try { ... return
QUIVER_OK; } catch (const std::exception& e) { quiver_set_last_error(e.what()); return QUIVER_ERROR; }`.
See `src/c/AGENTS.md` on the single error channel. Only the two LuaRunner entry points add a
second, ad-hoc arm (`src/c/lua_runner.cpp`):

```cpp
QUIVER_C_API quiver_error_t quiver_lua_runner_new(quiver_database_t* db, quiver_lua_runner_t** out_runner) {
    ...
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    } catch (...) {
        quiver_set_last_error("Unknown error creating LuaRunner");
        return QUIVER_ERROR;
    }
}
```
and the same in `quiver_lua_runner_run` with `"Unknown error running Lua script"`.

Nothing in the wrapped code throws a non-`std::exception`:
- `quiver::LuaRunner`'s constructor and `run` raise `std::runtime_error`.
- sol2 errors are `sol::error`, which derives from `std::runtime_error`.
- `LuaRunner::run` runs the script through `safe_script`, which turns Lua errors into return codes
  that are rethrown as `std::runtime_error` (`src/lua_runner.cpp`, `LuaRunner::run`).

These arms are dead code, and their messages follow none of the three patterns.

Principle: delete unused code.

## Constraints and decisions

- Delete both `catch (...)` arms. The two functions then look like every other C API entry point.
- Do not add a `catch (...)` anywhere else "for safety". The project is clean over defensive (root
  Principles).

## Changes — `src/c/lua_runner.cpp`

In `quiver_lua_runner_new`, delete:
```cpp
    } catch (...) {
        quiver_set_last_error("Unknown error creating LuaRunner");
        return QUIVER_ERROR;
    }
```
leaving the `catch (const std::exception& e) { ... }` as the last arm, closed with `}`.

In `quiver_lua_runner_run`, do the same with the `"Unknown error running Lua script"` arm.

## Tests

None needed. The existing `tests/test_c_api_lua_runner.cpp` error tests go through the
`std::exception` arm and keep passing. `grep -rn "Unknown error creating LuaRunner\|Unknown error running Lua script" tests/ bindings/`
must print nothing. If a test pins either text, it was pinning dead code; delete that assertion.

## Docs and changelog

None.

## Verification

From the repo root:
1. `cmake --build build --config Debug`
2. `./build/bin/quiver_c_tests.exe --gtest_filter=*LuaRunner*`
3. `scripts/format.bat`

## Acceptance criteria

- [ ] `grep -n "catch (\.\.\.)" src/c/lua_runner.cpp` prints nothing.
- [ ] C API LuaRunner tests pass.

## Pitfalls

- Lua is compiled as C here (lua-cmake), so a raw `lua_error` longjmps rather than throwing a C++
  exception. That was never catchable by `catch (...)` either, so removing the arm changes nothing.
  sol2's protected calls are what keep it contained.

## Out of scope

- The `bad_alloc` arms elsewhere in the C API (an optional item, not planned).
