# Phase 1: FFI Foundation and Database Lifecycle - Research

**Researched:** 2026-03-08
**Domain:** bun:ffi native library binding, C API interop, database lifecycle
**Confidence:** HIGH

## Summary

Phase 1 establishes the JS/TS binding for the Quiver C API using Bun's built-in `bun:ffi` module. The core challenge is bridging bun:ffi's pointer-centric API (no native struct support) with the C API's struct-based options and opaque handle patterns. All three existing bindings (Julia, Dart, Python) follow an identical architecture: load library, define `check(err)` helper, wrap factory methods with out-parameter allocation, track closed state. The JS binding follows this same pattern with bun:ffi-specific adaptations.

The critical technical risks are: (1) `quiver_database_options_default()` returns a struct by value, which bun:ffi cannot handle -- must construct the default struct manually via ArrayBuffer; (2) pointer-to-pointer out-parameters for `quiver_database_t**` require TypedArray-based allocation via `BigInt64Array` or `Uint8Array` + `read.ptr()`; (3) Windows DLL dependency chain (libquiver.dll must load before libquiver_c.dll) requires explicit pre-loading.

**Primary recommendation:** Use manual ArrayBuffer construction for the 8-byte `quiver_database_options_t` struct (two int32 fields), TypedArray-backed out-parameters with `read.ptr()` for pointer dereference, and `CString` for reading C error messages. Skip `quiver_database_options_default()` FFI call entirely -- hardcode the known defaults `{read_only: 0, console_level: 1}` in JS.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Auto-detect DLLs from package location first (relative to node_modules), then fall back to system PATH
- Lazy loading: DLLs loaded on first Database.fromSchema() or fromMigrations() call, not on import
- On failure, throw QuiverError with a message listing all searched paths and the missing library name
- No standalone health-check function (users discover DLL issues on first database call)
- Multi-file by topic: loader.ts, errors.ts, database.ts, ffi-helpers.ts (Phase 2-4 add create.ts, read.ts, query.ts, etc.)
- Located at bindings/js/ in the repo (follows bindings/julia/, bindings/dart/, bindings/python/ pattern)
- npm package named `quiverdb` (matches Python package name)
- Single Database class with methods added directly in later phases (no mixins or multiple inheritance)
- Track `_closed` boolean in the Database instance
- Throw QuiverError("Database is closed") on any method call after close()
- close() itself is idempotent -- second call is a silent no-op
- Symbol.dispose deferred to future milestone (EXT-07)
- Native pointer stays private -- no public getter

### Claude's Discretion
- Error handling pattern: check() helper design (matches established Julia/Dart/Python pattern)
- Out-parameter helper implementation details (FFI-03)
- Type conversion helpers for BigInt, C strings, typed arrays (FFI-04)
- Exact auto-detect search paths for DLL discovery
- Internal module structure within topic files

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| FFI-01 | Library loader detects platform and loads libquiver + libquiver_c shared libraries | DLL loading via `dlopen()`, `suffix` constant for platform detection, Windows pre-load pattern from Python/Dart bindings |
| FFI-02 | Error handler reads C API error messages and throws typed QuiverError | `quiver_get_last_error()` returns `const char*`, read via `CString`, `check()` pattern from all 3 existing bindings |
| FFI-03 | Out-parameter helpers allocate and read pointer/integer/float/string out-params | TypedArray as out-param (BigInt64Array for int64, Float64Array for double, read.ptr for pointer-to-pointer) |
| FFI-04 | Type conversion helpers handle int64 BigInt-to-number, C strings, typed arrays | `read.i64` returns BigInt, `Number()` conversion for safe integers, `CString` for char* |
| LIFE-01 | User can open database from SQL schema file via `fromSchema()` | `quiver_database_from_schema()` C API, struct options via manual buffer, pointer-to-pointer out-param |
| LIFE-02 | User can open database from migration files via `fromMigrations()` | `quiver_database_from_migrations()` C API, identical pattern to fromSchema |
| LIFE-03 | User can close database and release native resources via `close()` | `quiver_database_close()` C API, `_closed` guard pattern, idempotent close |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| bun:ffi | Built-in (Bun 1.3.10) | C library loading and function calling | Only FFI option for Bun runtime; direct C ABI calls without native addon compilation |
| bun:test | Built-in (Bun 1.3.10) | Test runner | Required by PKG-03; Jest-compatible API with describe/expect/beforeAll |
| TypeScript | Built-in (Bun 1.3.10) | Type safety | Required by PKG-02; Bun has native TS support |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `node:fs` | Built-in | File existence checks | DLL discovery path validation |
| `node:path` | Built-in | Path joining/resolution | Cross-platform path construction for DLL search |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Manual struct buffers | bun-cstruct (npm) | Adds dependency for a single 8-byte struct; not worth it |
| Manual ArrayBuffer | DataView for struct fields | DataView works but ArrayBuffer + Int32Array is simpler for aligned int32 fields |

**Installation:**
```bash
# No external dependencies -- all built-in to Bun
bun init  # creates package.json + tsconfig.json
```

## Architecture Patterns

### Recommended Project Structure
```
bindings/js/
  src/
    index.ts          # Public API exports (Database, QuiverError)
    loader.ts         # DLL discovery and lazy loading via dlopen
    errors.ts         # QuiverError class + check() helper
    database.ts       # Database class (fromSchema, fromMigrations, close)
    ffi-helpers.ts    # Out-parameter allocators, type converters, struct builders
  test/
    database.test.ts  # Lifecycle tests (fromSchema, fromMigrations, close, error handling)
    test.bat          # Test runner script (sets PATH for DLL discovery)
  package.json
  tsconfig.json
```

### Pattern 1: Library Loading (Lazy Singleton)
**What:** Load native libraries on first use, cache the handle, pre-load dependency DLL on Windows.
**When to use:** Always -- this is the entrypoint for all FFI operations.
**Example:**
```typescript
// Source: Adapted from Dart library_loader.dart and Python _loader.py patterns
import { dlopen, FFIType, suffix } from "bun:ffi";
import { existsSync } from "node:fs";
import { join, dirname } from "node:path";

const CORE_LIB = `libquiver.${suffix}`;
const C_API_LIB = `libquiver_c.${suffix}`;

let _library: ReturnType<typeof dlopen> | null = null;

function getSearchPaths(): string[] {
  const paths: string[] = [];
  // 1. Relative to this package (node_modules/quiverdb/)
  const packageDir = dirname(dirname(import.meta.dir));
  paths.push(join(packageDir, "bin"));
  paths.push(packageDir);
  // 2. Relative to build output (development mode)
  // Walk up from bindings/js/src/ to repo root, then build/bin/
  let dir = import.meta.dir;
  for (let i = 0; i < 5; i++) {
    const candidate = join(dir, "build", "bin");
    if (existsSync(join(candidate, C_API_LIB))) {
      paths.push(candidate);
    }
    dir = dirname(dir);
  }
  return paths;
}

export function loadLibrary() {
  if (_library) return _library;

  const searchPaths = getSearchPaths();

  // Try each search path
  for (const dir of searchPaths) {
    const corePath = join(dir, CORE_LIB);
    const cApiPath = join(dir, C_API_LIB);
    if (!existsSync(cApiPath)) continue;

    try {
      // Windows: pre-load core library (dependency chain)
      if (suffix === "dll" && existsSync(corePath)) {
        dlopen(corePath, {});
      }
      _library = dlopen(cApiPath, {
        // ... symbol definitions ...
      });
      return _library;
    } catch {
      continue;
    }
  }

  // Fallback: try system PATH (bare name)
  try {
    if (suffix === "dll") {
      dlopen(CORE_LIB, {});
    }
    _library = dlopen(C_API_LIB, { /* symbols */ });
    return _library;
  } catch {
    // Fall through to error
  }

  throw new QuiverError(
    `Cannot load native library '${C_API_LIB}'. ` +
    `Searched: ${searchPaths.join(", ")}, system PATH`
  );
}
```

### Pattern 2: Error Checking (check() Helper)
**What:** After every C API call, check the return code and throw QuiverError with the C-layer error message.
**When to use:** Every C API function that returns `quiver_error_t`.
**Example:**
```typescript
// Source: Identical pattern in Julia exceptions.jl, Dart exceptions.dart, Python _helpers.py
import { CString } from "bun:ffi";

export class QuiverError extends Error {
  constructor(message: string) {
    super(message);
    this.name = "QuiverError";
  }
}

export function check(err: number): void {
  if (err !== 0) {
    const lib = getSymbols();
    const errPtr = lib.quiver_get_last_error();
    const detail = errPtr ? new CString(errPtr).toString() : "";
    if (!detail) {
      console.warn("check: C API returned error but quiver_get_last_error() is empty");
      throw new QuiverError("Unknown error");
    }
    throw new QuiverError(detail);
  }
}
```

### Pattern 3: Out-Parameter Allocation for Pointer-to-Pointer
**What:** Allocate a TypedArray to receive a pointer output, pass it to FFI, then read the pointer value.
**When to use:** Every C API call with `quiver_database_t**` out-parameter.
**Example:**
```typescript
// Source: bun:ffi docs on TypedArray as pointer + read.ptr()
import { ptr, read } from "bun:ffi";

// For quiver_database_t** out_db:
// We need a buffer that can hold one pointer (8 bytes on 64-bit)
const outDbBuf = new BigInt64Array(1);  // 8 bytes, acts as Ptr<Ptr<db>>
// Pass ptr(outDbBuf) to FFI function
// Read the pointer value:
const dbPtr = read.ptr(ptr(outDbBuf), 0);  // returns number (pointer)
```

### Pattern 4: Struct Construction via ArrayBuffer
**What:** Manually construct `quiver_database_options_t` as an 8-byte buffer since bun:ffi has no struct support.
**When to use:** When calling `quiver_database_from_schema()` / `quiver_database_from_migrations()` which take `const quiver_database_options_t*`.
**Example:**
```typescript
// quiver_database_options_t layout:
//   offset 0: int32 read_only (default: 0)
//   offset 4: int32 console_level (default: 1 = QUIVER_LOG_INFO)
// Total: 8 bytes, no padding (both fields are 4-byte aligned int32)
function makeDefaultOptions(): Uint8Array {
  const buf = new ArrayBuffer(8);
  const view = new Int32Array(buf);
  view[0] = 0; // read_only = false
  view[1] = 1; // console_level = QUIVER_LOG_INFO
  return new Uint8Array(buf);
}
```

### Pattern 5: Database Class with Closed Guard
**What:** Class that wraps an opaque pointer, tracks closed state, throws on use-after-close.
**When to use:** The Database class.
**Example:**
```typescript
// Source: Pattern from all 3 existing bindings (Julia database.jl, Dart database.dart, Python database.py)
export class Database {
  private _ptr: number;  // bun:ffi pointer (number)
  private _closed = false;

  private constructor(ptr: number) {
    this._ptr = ptr;
  }

  static fromSchema(dbPath: string, schemaPath: string): Database {
    const lib = getSymbols();
    const options = makeDefaultOptions();
    const outDb = new BigInt64Array(1);

    check(lib.quiver_database_from_schema(
      Buffer.from(dbPath + "\0"),   // null-terminated
      Buffer.from(schemaPath + "\0"),
      ptr(options),
      ptr(outDb),
    ));

    const dbPtr = read.ptr(ptr(outDb), 0);
    return new Database(dbPtr);
  }

  close(): void {
    if (this._closed) return;
    const lib = getSymbols();
    check(lib.quiver_database_close(this._ptr));
    this._closed = true;
  }

  private ensureOpen(): void {
    if (this._closed) {
      throw new QuiverError("Database is closed");
    }
  }
}
```

### Anti-Patterns to Avoid
- **Calling quiver_database_options_default() from FFI:** This function returns a struct by value. bun:ffi cannot handle struct-by-value returns. Construct the default struct manually instead.
- **Using `i64` FFI type for element IDs:** Returns BigInt, which is slow and inconvenient. Use `i64_fast` for int64 return values where values fit in JS number range (all database IDs and counts do). Reserve `i64` for when exact 64-bit precision is needed.
- **Storing pointers as BigInt:** bun:ffi represents pointers as regular JS numbers (52-bit addressable). Store them as `number`, not `BigInt`.
- **Passing JS strings directly:** C API expects null-terminated `char*`. Always encode as `Buffer.from(str + "\0")` or equivalent.
- **Not pre-loading libquiver.dll on Windows:** The C API DLL depends on the core DLL. Without pre-loading, dlopen will fail with a cryptic error.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| C string reading | Manual null-byte scanning | `CString` from bun:ffi | Built-in, handles UTF-8 to UTF-16 transcoding, null-terminator search |
| Platform detection | `process.platform` checks | `suffix` from bun:ffi | Already provides "dll"/"so"/"dylib" |
| Pointer from TypedArray | Manual address calculation | `ptr()` from bun:ffi | Guaranteed correct pointer extraction |
| Pointer dereference | Raw memory reads | `read.ptr()`, `read.i32()`, `read.i64()` etc. | Type-safe, bounds-checked |
| Null-terminated strings | Manual \0 append + encode | `Buffer.from(str + "\0")` | Buffer auto-encodes UTF-8, just append null |

**Key insight:** bun:ffi provides `ptr()`, `read.*()`, `CString`, `toArrayBuffer()`, and `suffix` as primitives. These cover all low-level needs. The binding's job is to compose these into a clean API, not to reimplement them.

## Common Pitfalls

### Pitfall 1: Struct-by-Value Returns
**What goes wrong:** `quiver_database_options_default()` returns `quiver_database_options_t` by value (8 bytes). bun:ffi has no struct return support -- the call will crash or return garbage.
**Why it happens:** bun:ffi only supports returning scalar types and pointers, not composite types.
**How to avoid:** Do NOT bind `quiver_database_options_default()`. Hardcode the known defaults `{read_only: 0, console_level: QUIVER_LOG_INFO(1)}` in a manually constructed 8-byte buffer.
**Warning signs:** Segfaults or corrupted data when calling any function that returns a struct by value.

### Pitfall 2: Windows DLL Dependency Chain
**What goes wrong:** Loading `libquiver_c.dll` fails with "The specified module could not be found" even though the file exists.
**Why it happens:** `libquiver_c.dll` depends on `libquiver.dll`. Windows DLL loader cannot find the dependency if it's not in PATH or pre-loaded.
**How to avoid:** Always `dlopen(libquiver.dll, {})` before `dlopen(libquiver_c.dll, symbols)` on Windows. The Python binding uses `os.add_dll_directory()`, the Dart binding loads the core DLL first. JS binding must do the same.
**Warning signs:** dlopen error mentioning "module not found" when the DLL file clearly exists at the path.

### Pitfall 3: String Encoding for C API
**What goes wrong:** C API receives non-null-terminated strings, reads past buffer into garbage memory.
**Why it happens:** JavaScript strings don't have null terminators. Passing a raw JS string or a UTF-8 buffer without `\0` causes C to read beyond the buffer.
**How to avoid:** Always append `\0` when encoding strings for FFI: `Buffer.from(str + "\0")`. The `buffer` FFI type passes the pointer to the TypedArray's underlying memory.
**Warning signs:** Garbled text, random crashes, or reads returning unexpected extra characters.

### Pitfall 4: BigInt vs Number for int64
**What goes wrong:** FFI returns BigInt for i64 values, but downstream code expects number. Arithmetic operations fail (`BigInt + number` throws TypeError).
**Why it happens:** bun:ffi `i64` type always returns BigInt. Database IDs and counts fit in JS safe integer range but still come back as BigInt.
**How to avoid:** Use `i64_fast` FFI type for return values where values are expected to fit in 53-bit range (database IDs, counts, sizes). This returns `number` when value fits, `BigInt` only when it doesn't. For args, `i64` accepts both number and BigInt.
**Warning signs:** TypeErrors at `BigInt + number` operations, or `typeof value === "bigint"` when `number` was expected.

### Pitfall 5: Pointer Lifetime and GC
**What goes wrong:** TypedArray used as out-parameter gets garbage collected while C code still holds a reference to the underlying memory.
**Why it happens:** JavaScript GC doesn't know C code references the buffer.
**How to avoid:** Keep references to all TypedArrays alive until after the FFI call returns. For out-parameters, allocate them as local variables in the same function scope as the FFI call -- they won't be collected during a synchronous call.
**Warning signs:** Sporadic crashes or corrupted pointer values, especially under memory pressure.

### Pitfall 6: size_t Mapping
**What goes wrong:** Using wrong FFI type for `size_t` parameters causes misaligned reads or truncated values.
**Why it happens:** `size_t` is platform-dependent (8 bytes on 64-bit). bun:ffi does not have a dedicated `size_t` type.
**How to avoid:** Use `u64` (or `u64_fast` for returns) for `size_t` parameters on 64-bit platforms. On Windows x64, `size_t` is always 8 bytes.
**Warning signs:** Incorrect counts or sizes when reading arrays, especially values > 2^32.

## Code Examples

Verified patterns from official sources and existing bindings:

### dlopen Symbol Definition
```typescript
// Source: bun:ffi docs (https://bun.com/docs/runtime/ffi)
import { dlopen, FFIType, ptr, read, CString, suffix } from "bun:ffi";

const lib = dlopen(path, {
  // Lifecycle functions
  quiver_get_last_error: {
    args: [],
    returns: FFIType.ptr,  // const char* (not cstring -- we want the pointer, not auto-conversion)
  },
  quiver_clear_last_error: {
    args: [],
    returns: FFIType.void,
  },
  quiver_version: {
    args: [],
    returns: FFIType.cstring,  // Auto-converts to JS string
  },
  quiver_database_from_schema: {
    args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr],
    // (db_path: char*, schema_path: char*, options: options_t*, out_db: database_t**)
    returns: FFIType.i32,  // quiver_error_t (enum = uint32, but i32 works for 0/1)
  },
  quiver_database_from_migrations: {
    args: [FFIType.ptr, FFIType.ptr, FFIType.ptr, FFIType.ptr],
    returns: FFIType.i32,
  },
  quiver_database_close: {
    args: [FFIType.ptr],
    returns: FFIType.i32,
  },
});
```

### Reading a C String from Error Pointer
```typescript
// Source: bun:ffi CString docs + Julia/Dart/Python check() pattern
const errPtr = lib.symbols.quiver_get_last_error();
// errPtr is a number (pointer value)
if (errPtr) {
  const message = new CString(errPtr);
  // CString extends String, use .toString() or template literals
  throw new QuiverError(`${message}`);
}
```

### Out-Parameter for quiver_database_t**
```typescript
// Source: bun:ffi docs on TypedArray -> pointer conversion + read.ptr()
// Allocate space for one pointer (8 bytes)
const outDb = new BigInt64Array(1);
// Pass to FFI -- bun auto-converts TypedArray to pointer
const err = lib.symbols.quiver_database_from_schema(
  Buffer.from(dbPath + "\0"),
  Buffer.from(schemaPath + "\0"),
  ptr(optionsBuf),
  ptr(outDb),
);
check(err);
// Read the pointer value written by C
const dbPtr = read.ptr(ptr(outDb), 0);
// dbPtr is a number representing the quiver_database_t* handle
```

### Encoding Strings for C API
```typescript
// Null-terminated UTF-8 string for C
const cStr = Buffer.from("Hello World\0");
// Pass as FFIType.ptr argument
lib.symbols.some_function(cStr);
// Buffer's underlying ArrayBuffer memory is passed as pointer
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| N-API / node-ffi-napi | bun:ffi built-in | Bun 0.1+ (2022) | No native compilation needed, direct C ABI calls |
| Struct return by value | Manual buffer construction | Ongoing (still no struct support) | Must manually lay out struct bytes |
| Always BigInt for i64 | i64_fast coerces to number | Bun 0.6+ | Better ergonomics for typical int64 values that fit in 53 bits |

**Deprecated/outdated:**
- `bun:ffi` docs still carry the "experimental" label but the core API (`dlopen`, `FFIType`, `ptr`, `read`, `CString`) is stable and widely used as of Bun 1.3.x.

## Open Questions

1. **read.ptr() on out-parameter TypedArray**
   - What we know: `read.ptr(ptr(typedArray), 0)` should dereference the pointer stored at offset 0 of the TypedArray
   - What's unclear: Empirical validation needed -- does this work reliably for `quiver_database_t**` pattern? (Flagged in STATE.md)
   - Recommendation: First implementation task should include a minimal smoke test that opens and closes a database to validate the out-parameter flow end-to-end

2. **quiver_error_t return type mapping**
   - What we know: `quiver_error_t` is a C enum `{ QUIVER_OK = 0, QUIVER_ERROR = 1 }` backed by `uint32`
   - What's unclear: Whether `FFIType.i32` or `FFIType.u32` is more appropriate
   - Recommendation: Use `FFIType.i32` -- both map to JS `number`, and `0`/`1` are identical in signed/unsigned interpretation. All other bindings treat the return as a plain integer.

3. **size_t / usize availability**
   - What we know: STATE.md flagged `usize` as unconfirmed. WebSearch results suggest bun:ffi does support `"usize"` as a string type.
   - What's unclear: Whether `usize` is documented or experimental
   - Recommendation: Test `usize` in implementation. If unavailable, use `u64` as fallback (both are 8 bytes on 64-bit platforms, which is the only target).

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | bun:test (built-in, Bun 1.3.10) |
| Config file | none -- Wave 0 |
| Quick run command | `bun test bindings/js/test/` |
| Full suite command | `bun test bindings/js/test/` |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| FFI-01 | Library loads without error | smoke | `bun test bindings/js/test/database.test.ts -t "loads library"` | Wave 0 |
| FFI-02 | Error from C API surfaces as QuiverError | unit | `bun test bindings/js/test/database.test.ts -t "error handling"` | Wave 0 |
| FFI-03 | Out-parameter returns valid pointer | integration | `bun test bindings/js/test/database.test.ts -t "fromSchema"` | Wave 0 |
| FFI-04 | BigInt/CString conversions work | unit | `bun test bindings/js/test/database.test.ts -t "conversions"` | Wave 0 |
| LIFE-01 | fromSchema opens database | integration | `bun test bindings/js/test/database.test.ts -t "fromSchema"` | Wave 0 |
| LIFE-02 | fromMigrations opens database | integration | `bun test bindings/js/test/database.test.ts -t "fromMigrations"` | Wave 0 |
| LIFE-03 | close releases resources, subsequent ops throw | unit | `bun test bindings/js/test/database.test.ts -t "close"` | Wave 0 |

### Sampling Rate
- **Per task commit:** `bun test bindings/js/test/`
- **Per wave merge:** `bun test bindings/js/test/`
- **Phase gate:** Full suite green before `/gsd:verify-work`

### Wave 0 Gaps
- [ ] `bindings/js/test/database.test.ts` -- covers FFI-01 through LIFE-03
- [ ] `bindings/js/test/test.bat` -- test runner with PATH setup (sets `build/bin/` in PATH for DLL discovery)
- [ ] `bindings/js/package.json` -- package metadata
- [ ] `bindings/js/tsconfig.json` -- TypeScript config

## Sources

### Primary (HIGH confidence)
- [bun:ffi official docs](https://bun.com/docs/runtime/ffi) - dlopen, FFIType, ptr, read, CString, suffix
- [bun:ffi API reference](https://bun.com/reference/bun/ffi) - Complete module exports and type mappings
- [bun:ffi read reference](https://bun.com/reference/bun/ffi/read) - read.ptr, read.i32, read.i64 etc.
- C API headers (`include/quiver/c/common.h`, `database.h`, `options.h`) - Function signatures, struct layouts, return types
- Existing bindings (Julia `c_api.jl`/`database.jl`, Dart `library_loader.dart`/`database.dart`/`exceptions.dart`, Python `_loader.py`/`_helpers.py`/`database.py`) - Proven patterns for all operations

### Secondary (MEDIUM confidence)
- [GitHub oven-sh/bun#6139](https://github.com/oven-sh/bun/issues/6139) - Struct support NOT available (open issue)
- [GitHub oven-sh/bun#6369](https://github.com/oven-sh/bun/discussions/6369) - Complex return types workarounds
- [Bun FFI blog/community posts](https://dev.to/calumk/bun-ffi-getting-started-with-c-and-bun-47ea) - Practical patterns

### Tertiary (LOW confidence)
- `usize` FFI type availability -- mentioned in search results but not confirmed in official docs. Needs empirical validation.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - bun:ffi is the only option, API is well-documented
- Architecture: HIGH - Identical pattern proven in 3 existing bindings, adapted for bun:ffi specifics
- Pitfalls: HIGH - Struct-by-value and DLL dependency issues are confirmed by official sources and existing binding code
- Out-parameter pattern: MEDIUM - read.ptr() approach is documented but needs empirical validation for pointer-to-pointer
- usize availability: LOW - Not confirmed in official docs

**Research date:** 2026-03-08
**Valid until:** 2026-04-08 (30 days -- bun:ffi API is stable)
