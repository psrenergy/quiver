# Phase 6: Introspection & Lua - Research

**Researched:** 2026-03-10
**Domain:** Bun FFI bindings for database introspection and Lua script execution
**Confidence:** HIGH

## Summary

Phase 6 adds two small features to the JS binding: (1) four introspection methods on Database (`isHealthy`, `currentVersion`, `path`, `describe`) and (2) a standalone `LuaRunner` class with lifecycle management. Both are thin wrappers over well-established C API functions that already work in Julia, Dart, and Python.

The introspection methods follow the exact same patterns already used in `transaction.ts` (prototype augmentation with `declare module`, `Int32Array(1)` for int out-params, `BigInt64Array(1)` for int64_t out-params). The LuaRunner is a new class with its own lifecycle (constructor, `run`, `close`) -- the only novel aspect is two-source error resolution (runner-specific error first, global error fallback), which is identical across all other bindings.

**Primary recommendation:** Implement as a single plan with two tasks -- introspection augmentation (trivial) and LuaRunner class (straightforward). Total scope is approximately 8 new FFI symbol declarations and under 100 lines of binding code plus tests.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
None explicitly locked -- all areas deferred to Claude's discretion.

### Claude's Discretion
All areas deferred to Claude -- user trusts established patterns from other bindings.

**Introspection methods:**
- Follow prototype augmentation pattern (new `introspection.ts` file)
- `isHealthy()` -> int out-param, convert to boolean
- `currentVersion()` -> int64_t out-param, return as Number
- `path()` -> const char** out-param, decode to string
- `describe()` -> void, prints to stdout (pass-through like all other bindings)

**LuaRunner lifecycle:**
- Separate class in `lua-runner.ts`, exported from `index.ts`
- Constructor takes Database, calls `quiver_lua_runner_new`
- `run(script)` method calls `quiver_lua_runner_run`
- `close()` method calls `quiver_lua_runner_free` with disposed-state guard (Dart pattern)
- Hold Database reference to prevent GC during runner lifetime

**Error sourcing:**
- `run()` checks `quiver_lua_runner_get_error` first (runner-specific), falls back to `quiver_get_last_error` (global) -- matches Julia/Dart/Python pattern

**File organization:**
- `introspection.ts` -- Database augmentation for isHealthy, currentVersion, path, describe
- `lua-runner.ts` -- Standalone LuaRunner class with own lifecycle

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| JSDB-01 | User can call isHealthy, currentVersion, path, describe | Four C API functions already exist (`quiver_database_is_healthy`, `quiver_database_current_version`, `quiver_database_path`, `quiver_database_describe`). Identical patterns used in all other bindings. JS patterns for int/int64/string out-params already established in transaction.ts and time-series.ts. |
| JSLUA-01 | User can execute Lua scripts against a database | Four C API functions already exist (`quiver_lua_runner_new`, `quiver_lua_runner_run`, `quiver_lua_runner_get_error`, `quiver_lua_runner_free`). Identical class structure used in Dart/Python/Julia. Two-source error pattern documented and consistent across all bindings. |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| bun:ffi | Built-in | FFI calls to libquiver_c | Only FFI mechanism in Bun |
| bun:test | Built-in | Test framework | Already used for all JS tests |

### Supporting
No additional libraries needed. All required helpers (`check`, `allocPointerOut`, `readPointerOut`, `toCString`, `getSymbols`) already exist in the binding.

## Architecture Patterns

### Recommended File Structure
```
bindings/js/src/
  introspection.ts      # NEW - prototype augmentation for 4 introspection methods
  lua-runner.ts         # NEW - standalone LuaRunner class
  index.ts              # MODIFY - import introspection.ts, export LuaRunner
  loader.ts             # MODIFY - add 8 FFI symbol declarations
bindings/js/test/
  introspection.test.ts # NEW - tests for isHealthy, currentVersion, path, describe
  lua-runner.test.ts    # NEW - tests for LuaRunner lifecycle and script execution
```

### Pattern 1: Prototype Augmentation (introspection.ts)
**What:** Add methods to Database class via `declare module` and prototype assignment.
**When to use:** When adding Database methods in a separate file (all augmentation files follow this).
**Example:**
```typescript
// Source: bindings/js/src/transaction.ts (verified pattern)
import { CString, ptr } from "bun:ffi";
import { Database } from "./database";
import { check } from "./errors";
import { allocPointerOut, readPointerOut, toCString } from "./ffi-helpers";
import { getSymbols } from "./loader";

declare module "./database" {
  interface Database {
    isHealthy(): boolean;
    currentVersion(): number;
    path(): string;
    describe(): void;
  }
}

Database.prototype.isHealthy = function (this: Database): boolean {
  const lib = getSymbols();
  const outHealthy = new Int32Array(1);
  check(lib.quiver_database_is_healthy(this._handle, ptr(outHealthy)));
  return outHealthy[0] !== 0;
};

Database.prototype.currentVersion = function (this: Database): number {
  const lib = getSymbols();
  const outVersion = new BigInt64Array(1);
  check(lib.quiver_database_current_version(this._handle, ptr(outVersion)));
  return Number(outVersion[0]);
};

Database.prototype.path = function (this: Database): string {
  const lib = getSymbols();
  const outPath = allocPointerOut();
  check(lib.quiver_database_path(this._handle, ptr(outPath)));
  const pathPtr = readPointerOut(outPath);
  return new CString(pathPtr).toString();
};

Database.prototype.describe = function (this: Database): void {
  const lib = getSymbols();
  check(lib.quiver_database_describe(this._handle));
};
```

### Pattern 2: Standalone Class with Lifecycle (lua-runner.ts)
**What:** A class that owns a native handle, with constructor/close/ensureOpen pattern.
**When to use:** For non-Database opaque handles that need explicit lifecycle management.
**Example:**
```typescript
// Source: Dart LuaRunner pattern + JS Database.close() pattern
import { type Pointer, CString, ptr } from "bun:ffi";
import { Database } from "./database";
import { check, QuiverError } from "./errors";
import { allocPointerOut, readPointerOut, toCString } from "./ffi-helpers";
import { getSymbols } from "./loader";

export class LuaRunner {
  private _ptr: Pointer;
  private _closed = false;
  private _db: Database; // prevent GC

  constructor(db: Database) {
    this._db = db;
    const lib = getSymbols();
    const outRunner = allocPointerOut();
    check(lib.quiver_lua_runner_new(db._handle, ptr(outRunner)));
    this._ptr = readPointerOut(outRunner);
  }

  run(script: string): void {
    this.ensureOpen();
    const lib = getSymbols();
    const err = lib.quiver_lua_runner_run(this._ptr, toCString(script));
    if (err !== 0) {
      // Two-source error resolution
      const outError = allocPointerOut();
      const getErr = lib.quiver_lua_runner_get_error(this._ptr, ptr(outError));
      if (getErr === 0) {
        const errPtr = readPointerOut(outError);
        if (errPtr) {
          const msg = new CString(errPtr).toString();
          if (msg) throw new QuiverError(msg);
        }
      }
      // Fallback to global error
      check(err);
    }
  }

  close(): void {
    if (this._closed) return;
    const lib = getSymbols();
    lib.quiver_lua_runner_free(this._ptr);
    this._closed = true;
  }

  private ensureOpen(): void {
    if (this._closed) {
      throw new QuiverError("LuaRunner is closed");
    }
  }
}
```

### Pattern 3: FFI Symbol Declarations (loader.ts additions)
**What:** Add new function signatures to the symbols object in loader.ts.
**Example:**
```typescript
// Source: include/quiver/c/database.h and include/quiver/c/lua_runner.h
// Introspection
quiver_database_is_healthy: {
  args: [FFIType.ptr, FFIType.ptr],  // db, out_healthy (int*)
  returns: FFIType.i32,
},
quiver_database_current_version: {
  args: [FFIType.ptr, FFIType.ptr],  // db, out_version (int64_t*)
  returns: FFIType.i32,
},
quiver_database_path: {
  args: [FFIType.ptr, FFIType.ptr],  // db, out_path (const char**)
  returns: FFIType.i32,
},
quiver_database_describe: {
  args: [FFIType.ptr],               // db
  returns: FFIType.i32,
},

// LuaRunner
quiver_lua_runner_new: {
  args: [FFIType.ptr, FFIType.ptr],  // db, out_runner
  returns: FFIType.i32,
},
quiver_lua_runner_free: {
  args: [FFIType.ptr],               // runner
  returns: FFIType.i32,
},
quiver_lua_runner_run: {
  args: [FFIType.ptr, FFIType.ptr],  // runner, script
  returns: FFIType.i32,
},
quiver_lua_runner_get_error: {
  args: [FFIType.ptr, FFIType.ptr],  // runner, out_error
  returns: FFIType.i32,
},
```

### Anti-Patterns to Avoid
- **Do NOT create a LuaError subclass:** Dart has `LuaException extends DatabaseException`, but JS keeps it simple -- use `QuiverError` for everything, consistent with how errors.ts works. The error message itself distinguishes Lua errors.
- **Do NOT call quiver_database_options_default via FFI:** The function returns a struct by value which bun:ffi cannot handle. Already handled by `makeDefaultOptions()` in ffi-helpers.ts. Same logic applies -- avoid any C API function that returns a struct by value.
- **Do NOT free the path string:** `quiver_database_path` returns a `const char*` pointing to the internal `std::string` of the Database object (see `database.cpp` line 47: `*out_path = db->db.path().c_str()`). This pointer is owned by the Database and must NOT be freed.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Pointer out-params | Manual ArrayBuffer manipulation | `allocPointerOut()` + `readPointerOut()` | Already battle-tested in 5 phases |
| String marshaling | Manual null termination | `toCString()` | Handles encoding correctly |
| Error checking | Manual error code inspection | `check()` from errors.ts | Consistent error handling across all methods |
| Database handle validation | Manual null/closed checks | `this._handle` getter | Already guards with `ensureOpen()` |

**Key insight:** Every FFI pattern needed for this phase already exists in the codebase. The introspection methods are structurally identical to `inTransaction()` (int out-param -> boolean) and the time series version reads (int64_t out-param -> number).

## Common Pitfalls

### Pitfall 1: path() String Lifetime
**What goes wrong:** Calling `quiver_database_free_string` on the pointer returned by `quiver_database_path`.
**Why it happens:** Most C API read functions return allocated strings that need freeing. But `quiver_database_path` returns a `const char*` from the internal `std::string::c_str()` -- it is NOT heap-allocated.
**How to avoid:** Read the string immediately with `new CString(ptr)` and do not call any free function. The pointer is valid as long as the Database object exists.
**Warning signs:** Double-free crash or "invalid pointer" error on close.

### Pitfall 2: currentVersion() BigInt Conversion
**What goes wrong:** Returning a BigInt to the user when they expect a Number.
**Why it happens:** `BigInt64Array` stores BigInt values, but `currentVersion()` should return a plain JS `number`. The version is always a small integer.
**How to avoid:** Convert explicitly: `return Number(outVersion[0])`. This is safe because schema versions fit in Number range.
**Warning signs:** Type errors in user code comparing version to `===` a number literal.

### Pitfall 3: LuaRunner Error Resolution Order
**What goes wrong:** Using `check(err)` directly instead of checking runner-specific error first.
**Why it happens:** The standard `check()` function reads from `quiver_get_last_error()` (global), but Lua runtime errors are stored in the runner's internal `last_error` field, retrieved via `quiver_lua_runner_get_error()`.
**How to avoid:** Always try `quiver_lua_runner_get_error` first. Only fall back to `check(err)` (global error) for QUIVER_REQUIRE failures (null pointer checks).
**Warning signs:** Empty or misleading error messages from Lua script failures.

### Pitfall 4: LuaRunner Database Reference
**What goes wrong:** The Database object gets garbage collected while the LuaRunner is still alive.
**Why it happens:** JS garbage collector sees no strong reference keeping the Database alive if the user drops their reference.
**How to avoid:** Store `this._db = db` in the LuaRunner constructor. All other bindings (Julia, Python, Dart) do the same.
**Warning signs:** Segfault or access violation when calling `run()` after Database would have been collected.

## Code Examples

### Introspection Method Signatures (C API)
```c
// Source: include/quiver/c/database.h
quiver_error_t quiver_database_is_healthy(quiver_database_t* db, int* out_healthy);
quiver_error_t quiver_database_path(quiver_database_t* db, const char** out_path);
quiver_error_t quiver_database_current_version(quiver_database_t* db, int64_t* out_version);
quiver_error_t quiver_database_describe(quiver_database_t* db);
```

### LuaRunner Signatures (C API)
```c
// Source: include/quiver/c/lua_runner.h
quiver_error_t quiver_lua_runner_new(quiver_database_t* db, quiver_lua_runner_t** out_runner);
quiver_error_t quiver_lua_runner_free(quiver_lua_runner_t* runner);
quiver_error_t quiver_lua_runner_run(quiver_lua_runner_t* runner, const char* script);
quiver_error_t quiver_lua_runner_get_error(quiver_lua_runner_t* runner, const char** out_error);
```

### Two-Source Error Resolution (verified across Dart, Python, Julia)
```typescript
// Pattern from bindings/dart/lib/src/lua_runner.dart
// and bindings/python/src/quiverdb/lua_runner.py
//
// 1. Call quiver_lua_runner_run() -> check return code
// 2. If error: try quiver_lua_runner_get_error() first (Lua-specific error)
// 3. If empty/null: fall back to quiver_get_last_error() (global error)
// 4. If still empty: throw generic "Lua script execution failed"
```

### Test Pattern (verified from Dart and Python test suites)
```typescript
// Key test cases based on Dart lua_runner_test.dart and Python test_lua_runner.py:
// 1. Create element from Lua, verify via JS reads (create + read cross-layer)
// 2. Read scalars from Lua (seed data via JS, read in Lua with assertions)
// 3. Syntax error throws QuiverError
// 4. Runtime error (undefined_variable.field) throws QuiverError
// 5. Multiple run() calls on same runner
// 6. Empty script succeeds
// 7. Comment-only script succeeds
// 8. close() is idempotent
// 9. run() after close() throws QuiverError("LuaRunner is closed")
```

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | bun:test (built-in) |
| Config file | none -- bun:test uses convention |
| Quick run command | `cd bindings/js && bun test test/introspection.test.ts test/lua-runner.test.ts` |
| Full suite command | `cd bindings/js && bun test` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| JSDB-01 | isHealthy returns boolean true | unit | `cd bindings/js && bun test test/introspection.test.ts` | No - Wave 0 |
| JSDB-01 | currentVersion returns number >= 0 | unit | `cd bindings/js && bun test test/introspection.test.ts` | No - Wave 0 |
| JSDB-01 | path returns non-empty string | unit | `cd bindings/js && bun test test/introspection.test.ts` | No - Wave 0 |
| JSDB-01 | describe runs without error | unit | `cd bindings/js && bun test test/introspection.test.ts` | No - Wave 0 |
| JSLUA-01 | Create element from Lua, verify via JS | integration | `cd bindings/js && bun test test/lua-runner.test.ts` | No - Wave 0 |
| JSLUA-01 | Syntax error throws QuiverError | unit | `cd bindings/js && bun test test/lua-runner.test.ts` | No - Wave 0 |
| JSLUA-01 | Multiple run calls on same runner | unit | `cd bindings/js && bun test test/lua-runner.test.ts` | No - Wave 0 |
| JSLUA-01 | close idempotent, run after close throws | unit | `cd bindings/js && bun test test/lua-runner.test.ts` | No - Wave 0 |

### Sampling Rate
- **Per task commit:** `cd bindings/js && bun test test/introspection.test.ts test/lua-runner.test.ts`
- **Per wave merge:** `cd bindings/js && bun test`
- **Phase gate:** Full suite green before `/gsd:verify-work`

### Wave 0 Gaps
- [ ] `bindings/js/test/introspection.test.ts` -- covers JSDB-01
- [ ] `bindings/js/test/lua-runner.test.ts` -- covers JSLUA-01

*(No framework install needed -- bun:test already in use)*

## Sources

### Primary (HIGH confidence)
- `include/quiver/c/database.h` -- C API function signatures for introspection
- `include/quiver/c/lua_runner.h` -- C API function signatures for LuaRunner
- `src/c/database.cpp` -- C API implementation showing `path()` returns internal pointer (not allocated)
- `src/c/lua_runner.cpp` -- C API implementation showing two-source error storage
- `bindings/dart/lib/src/lua_runner.dart` -- Reference Dart implementation
- `bindings/python/src/quiverdb/lua_runner.py` -- Reference Python implementation
- `bindings/julia/src/lua_runner.jl` -- Reference Julia implementation
- `bindings/js/src/transaction.ts` -- Established JS prototype augmentation pattern
- `bindings/js/src/database.ts` -- Established JS lifecycle pattern (close/ensureOpen)
- `bindings/js/src/loader.ts` -- Existing FFI symbol declarations
- `bindings/js/src/ffi-helpers.ts` -- Existing helper functions
- `bindings/dart/test/lua_runner_test.dart` -- Reference test patterns
- `bindings/python/tests/test_lua_runner.py` -- Reference test patterns

### Secondary (MEDIUM confidence)
None needed -- all information from primary sources.

### Tertiary (LOW confidence)
None.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- same bun:ffi stack used in all prior phases
- Architecture: HIGH -- every pattern verified against existing codebase
- Pitfalls: HIGH -- verified by examining C API source code directly

**Research date:** 2026-03-10
**Valid until:** 2026-04-10 (stable -- C API is frozen for v0.5)
