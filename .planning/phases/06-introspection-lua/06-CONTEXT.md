# Phase 6: Introspection & Lua - Context

**Gathered:** 2026-03-10
**Status:** Ready for planning

<domain>
## Phase Boundary

JS users can inspect database state (isHealthy, currentVersion, path, describe) and execute Lua scripts via a LuaRunner class. This completes requirements JSDB-01 and JSLUA-01.

</domain>

<decisions>
## Implementation Decisions

### Claude's Discretion

All areas deferred to Claude — user trusts established patterns from other bindings.

**Introspection methods:**
- Follow prototype augmentation pattern (new `introspection.ts` file)
- `isHealthy()` → int out-param, convert to boolean
- `currentVersion()` → int64_t out-param, return as Number
- `path()` → const char** out-param, decode to string
- `describe()` → void, prints to stdout (pass-through like all other bindings)

**LuaRunner lifecycle:**
- Separate class in `lua-runner.ts`, exported from `index.ts`
- Constructor takes Database, calls `quiver_lua_runner_new`
- `run(script)` method calls `quiver_lua_runner_run`
- `close()` method calls `quiver_lua_runner_free` with disposed-state guard (Dart pattern)
- Hold Database reference to prevent GC during runner lifetime

**Error sourcing:**
- `run()` checks `quiver_lua_runner_get_error` first (runner-specific), falls back to `quiver_get_last_error` (global) — matches Julia/Dart/Python pattern

**File organization:**
- `introspection.ts` — Database augmentation for isHealthy, currentVersion, path, describe
- `lua-runner.ts` — Standalone LuaRunner class with own lifecycle

</decisions>

<specifics>
## Specific Ideas

No specific requirements — open to standard approaches. Follow established binding patterns.

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `check()` from `errors.ts` — error handling for all C API calls
- `allocPointerOut()` / `readPointerOut()` from `ffi-helpers.ts` — pointer out-param pattern
- `toCString()` from `ffi-helpers.ts` — string marshaling for Lua scripts
- Database `_handle` getter — pass to C API functions

### Established Patterns
- Prototype augmentation via `declare module` + prototype assignment (all augmentation files)
- `Int32Array(1)` for int out-params (Phase 1 fix)
- `BigInt64Array(1)` for int64_t out-params (existing pattern in time-series.ts)
- Explicit `close()` with disposed guard for handle-based objects (Database class)

### Integration Points
- `loader.ts` — Add 8 new FFI symbol declarations (4 introspection + 4 LuaRunner)
- `index.ts` — Import `introspection.ts` augmentation + export LuaRunner class
- `database.ts` — LuaRunner constructor takes Database instance

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 06-introspection-lua*
*Context gathered: 2026-03-10*
