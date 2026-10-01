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

- [x] `grep -n "catch (\.\.\.)" src/c/lua_runner.cpp` prints nothing.
- [x] C API LuaRunner tests pass.

## Pitfalls

- Lua is compiled as C here (lua-cmake), so a raw `lua_error` longjmps rather than throwing a C++
  exception. That was never catchable by `catch (...)` either, so removing the arm changes nothing.
  sol2's protected calls are what keep it contained.

## Out of scope

- The `bad_alloc` arms elsewhere in the C API (an optional item, not planned).

## Implementation notes

- **Landed as written, no drift.** Both excerpts matched `src/c/lua_runner.cpp` exactly, and
  `rs/plan64` was already level with `master` (merge was a no-op). Six lines deleted, nothing else
  in the source changed.
- **The arms were checked dead before deleting them**:
  - `src/lua_runner.cpp` throws only `std::runtime_error`.
  - sol2's `error` / `dump_error` derive from `std::runtime_error`, and `bad_optional_access` from
    `std::exception`.
  - `SOL_EXCEPTIONS_SAFE_PROPAGATION` is not defined, so sol2's trampolines turn any C++ exception
    into a Lua error that `run` rethrows as `std::runtime_error`.
  - Lua builds as C (`LUA_LANGUAGE:STRING=C`, the lua-cmake default; nothing in the repo
    overrides it).
  - Nothing compiles with `/EHa`, so the arm never caught SEH either.
- **No test, CHANGELOG or AGENTS.md change.** This is not user-visible, so there was no
  regression test to show failing first. `src/c/AGENTS.md` ("Error Handling") already documents
  the single `std::exception` arm, which the two functions now match.
- **Left alone, deliberately.**
  - `#include <new>` in `src/c/lua_runner.cpp`: clangd flags it unused, and it was unused before
    this change too. That is include hygiene, not this plan.
  - The `catch (...)` at `src/c/database_helpers.h` (`marshal_group_rows_to_c`): it is a
    cleanup-and-`throw;`, not an error-reporting arm.
- **Verification.**
  - `quiver_c_tests --gtest_filter=*LuaRunner*`: 27/27 passed.
  - Full `quiver_c_tests`: 572/572 passed. Full `quiver_tests`: 1402/1402 passed.
  - The acceptance grep over text files prints nothing. A plain `grep -rn` still hits the stale,
    gitignored Dart native-assets build (`bindings/dart/.dart_tool/**/libquiver_c.dll`, `.obj`),
    which the next hook build regenerates.
- **For later plans.**
  - `scripts/format.bat` hit plan 63's stale-glob failure again (`include/quiver/schema.h: no
    such file or directory`). `cmake -S . -B build` fixed it, which is more evidence for plan 86.
  - Biome again rewrote 43 untouched CRLF JS files to LF. I reverted them with
    `git checkout -- bindings/js`.
