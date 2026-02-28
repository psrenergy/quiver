# Phase 4: Python LuaRunner Binding - Context

**Gathered:** 2026-02-28
**Status:** Ready for planning

<domain>
## Phase Boundary

Wrap the C API `quiver_lua_runner_new/run/get_error/free` functions into a Python `LuaRunner` class in the quiverdb package. Achieves feature parity with Julia/Dart LuaRunner bindings. No new C or C++ API work required — only Python binding code and CFFI declarations.

</domain>

<decisions>
## Implementation Decisions

### Class interface
- Constructor takes a `Database` instance: `LuaRunner(db)`
- Store database reference as private attribute `self._db = db` to prevent GC from collecting the database while runner is alive
- Follow the Python `Database` class lifecycle pattern exactly:
  - `close()` method for explicit cleanup (idempotent)
  - `__enter__`/`__exit__` for context manager support (`with LuaRunner(db) as lua:`)
  - `__del__` with `ResourceWarning` if not closed explicitly
  - `_ensure_open()` guard on `run()` calls
- Track closed state with `self._closed = False`

### Error handling
- Use `QuiverError` (matching the Python Database class), not a custom LuaError or RuntimeError
- Error resolution order: `quiver_lua_runner_get_error` first (Lua-specific), then `quiver_get_last_error` (global fallback for QUIVER_REQUIRE failures)
- Generic fallback message `"Lua script execution failed"` if both error sources return empty/null
- Post-close calls raise `QuiverError("LuaRunner is closed")` — distinct from Lua execution errors

### Script loading
- Single method: `run(script: str) -> None`
- No `run_file(path)` convenience — users read files themselves
- Strings only (no bytes)
- Returns None — scripts operate via database side effects
- Multiple `run()` calls on the same runner explicitly supported (runner is long-lived)

### File and package structure
- Own file: `lua_runner.py` in `src/quiverdb/` — matches Dart (`lua_runner.dart`) and Julia (`lua_runner.jl`)
- Exported from `__init__.py`: `from quiverdb import LuaRunner`
- Added to `__all__` list
- CFFI declarations for `quiver_lua_runner_*` functions added to hand-written `_c_api.py`

### Claude's Discretion
- Exact CFFI struct/function signature formatting in `_c_api.py`
- Whether to also update `_declarations.py` (generator reference file)
- Test file organization and test case structure

</decisions>

<specifics>
## Specific Ideas

- Match the Python `Database` class lifecycle pattern exactly — same `_closed`, `_ensure_open()`, `close()`, `__enter__`/`__exit__`, `__del__` with ResourceWarning pattern
- Error resolution mirrors Dart's approach: runner-specific error -> global error -> generic fallback
- Cross-binding consistency: file naming matches Julia/Dart (`lua_runner.py`), but internal Python idioms match the existing Python `Database` class

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 04-python-luarunner-binding*
*Context gathered: 2026-02-28*
