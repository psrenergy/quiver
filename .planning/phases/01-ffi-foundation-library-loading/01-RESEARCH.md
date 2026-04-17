# Phase 1: FFI Foundation & Library Loading - Research

**Researched:** 2026-04-17
**Domain:** Deno FFI (Deno.dlopen), native library loading, C type mapping
**Confidence:** HIGH

## Summary

Phase 1 replaces the koffi-based library loader (`bindings/js/src/loader.ts`) with Deno's built-in `Deno.dlopen` API. The current loader defines 85 symbol bindings using koffi's `lib.func()` pattern and uses `node:fs`, `node:path`, and `node:url` for library discovery. The replacement must define equivalent symbols using Deno FFI type descriptors, replace all Node.js-specific imports with Deno equivalents, and maintain the same 3-tier library search strategy.

Deno 2.7.10 is installed on the target machine. Deno FFI is a stable API with well-documented type descriptors. The type mapping from koffi to Deno is mechanical: koffi's `"void *"` becomes `"pointer"`, `"str"` becomes `"buffer"` (for input) or `"pointer"` (for output), `"int"` becomes `"i32"`, `"int64_t"` becomes `"i64"`, `"uint64_t"` becomes `"u64"`, `"double"` becomes `"f64"`. The C API headers declare 93 total functions; the current JS loader binds 85 of them (8 element introspection functions are omitted but available if needed).

**Primary recommendation:** Define all symbol descriptors as `{ parameters: [...], result: "..." }` objects grouped by domain, spread into a single symbols object passed to `Deno.dlopen()`. Use `"buffer"` for string input parameters (null-terminated via TextEncoder) and `"pointer"` for all output/opaque pointer parameters.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **D-01:** Keep all 3 tiers of library discovery: bundled `libs/{os}-{arch}/`, dev `build/bin/` walk-up (5 levels), system PATH fallback
- **D-02:** Replace `node:fs` (existsSync) with `Deno.statSync`, `node:url` (fileURLToPath) with `import.meta.dirname`, `node:path` (join/dirname) with `jsr:@std/path`
- **D-03:** Replace `process.platform`/`process.arch` with `Deno.build.os`/`Deno.build.arch`
- **D-04:** Explicitly pre-load `libquiver.dll` (core) before loading `libquiver_c.dll` using `Deno.dlopen(corePath, {})` with empty symbols. Same strategy as current koffi code -- do not rely on OS DLL search order.
- **D-05:** Group ~85 symbol definitions by domain (lifecycle, element, read, query, metadata, time-series, csv, lua, free) as separate const objects, then spread into one combined symbols object for `Deno.dlopen`. Mirrors the C API file structure (`src/c/database_*.cpp`).
- **D-06:** Define named constants for Deno FFI types at top of loader.ts: `P = 'pointer'`, `I32 = 'i32'`, `I64 = 'i64'`, `U64 = 'u64'`, `F64 = 'f64'`, `V = 'void'`. Mirrors existing koffi shorthand pattern.
- **D-07:** koffi `"str"` maps to `"pointer"` in Deno (strings are passed as pointers, encoded/decoded in ffi-helpers.ts Phase 2). koffi `"const char **"` maps to `"pointer"`. koffi `"int"` maps to `"i32"`.

### Claude's Discretion
- Error message format for library-not-found (QuiverError) -- keep consistent with current style
- Exact domain grouping boundaries for symbols (lifecycle vs element vs CRUD splits)
- Whether to keep the `_symbols` / `_coreLib` caching pattern or simplify

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| FFI-01 | Library loads via Deno.dlopen with all C API symbols defined using Deno FFI type descriptors | Deno.dlopen API documented; all 85 symbols mapped to Deno type descriptors; type mapping table provided |
| FFI-02 | Symbol type mapping covers all used koffi types (int, int64_t, uint64_t, double, str, void*, const char**) | Complete koffi-to-Deno type mapping verified: int->i32, int64_t->i64, uint64_t->u64, double->f64, str->buffer, void*->pointer, const char**->pointer |
| FFI-03 | Library search paths work for bundled libs, dev build/bin, and system PATH | Deno.statSync, import.meta.dirname, jsr:@std/path, Deno.build.os/arch all verified available |
| CONF-02 | All node: protocol imports removed and replaced with Deno equivalents | Replacement APIs identified for each node: import: node:fs->Deno.statSync, node:url->import.meta.dirname, node:path->jsr:@std/path |
</phase_requirements>

## Standard Stack

### Core
| Library/API | Version | Purpose | Why Standard |
|-------------|---------|---------|--------------|
| Deno.dlopen | Deno 2.7.10 | Load native .dll/.so/.dylib and bind symbols | Built-in Deno FFI, no third-party dependency [VERIFIED: `deno --version` on target machine] |
| jsr:@std/path | latest (JSR) | Path join, dirname, resolve operations | Official Deno standard library replacement for node:path [CITED: https://jsr.io/@std/path] |

### Supporting
| API | Version | Purpose | When to Use |
|-----|---------|---------|-------------|
| Deno.statSync | Deno 2.7.10 | Check file/directory existence | Replace node:fs existsSync [VERIFIED: Deno built-in API] |
| import.meta.dirname | Deno 2.7.10 | Get directory of current module | Replace node:url fileURLToPath + node:path dirname [CITED: https://docs.deno.com/api/web/~/ImportMeta.dirname] |
| Deno.build.os | Deno 2.7.10 | Platform detection (windows/darwin/linux) | Replace process.platform [CITED: https://docs.deno.com/api/deno/~/Deno.build.arch] |
| Deno.build.arch | Deno 2.7.10 | Architecture detection (x86_64/aarch64) | Replace process.arch [CITED: https://docs.deno.com/api/deno/~/Deno.build.arch] |
| Deno.UnsafePointerView | Deno 2.7.10 | Read C strings from returned pointers | For quiver_version/quiver_get_last_error (Phase 2 heavy use) [CITED: https://docs.deno.com/api/deno/~/Deno.UnsafePointerView] |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| jsr:@std/path | Manual string concatenation | Error-prone on cross-platform; @std/path handles Windows/POSIX |
| Deno.statSync | jsr:@std/fs existsSync | Extra dependency; Deno.statSync with try/catch is idiomatic |

**No npm install needed.** All dependencies are Deno built-in APIs or JSR standard library (imported via `jsr:` specifier).

## Architecture Patterns

### Recommended File Structure (Phase 1 changes only)
```
bindings/js/src/
  loader.ts              # REWRITE: Deno.dlopen, symbol definitions, library search
  ffi-helpers.ts         # NO CHANGE in Phase 1 (still imports koffi -- Phase 2)
  types.ts               # NO CHANGE (pure types, no koffi dependency)
  errors.ts              # NO CHANGE (no koffi dependency)
  ...                    # All other files unchanged
```

### Pattern 1: Symbol Definition with Deno.dlopen
**What:** Define all C API function signatures as `ForeignFunction` objects
**When to use:** Every symbol binding in loader.ts

```typescript
// Source: https://docs.deno.com/runtime/fundamentals/ffi/
// Type shorthand constants (per D-06)
const P = "pointer" as const;
const BUF = "buffer" as const;
const I32 = "i32" as const;
const I64 = "i64" as const;
const U64 = "u64" as const;
const USIZE = "usize" as const;
const F64 = "f64" as const;
const V = "void" as const;

// Domain-grouped symbol definitions (per D-05)
const lifecycleSymbols = {
  quiver_version: { parameters: [], result: P },
  quiver_get_last_error: { parameters: [], result: P },
  quiver_clear_last_error: { parameters: [], result: V },
  quiver_database_options_default: { parameters: [], result: { struct: ["i32", "i32"] } },
  quiver_database_from_schema: { parameters: [BUF, BUF, BUF, BUF], result: I32 },
  quiver_database_from_migrations: { parameters: [BUF, BUF, BUF, BUF], result: I32 },
  quiver_database_close: { parameters: [P], result: I32 },
} as const;

// ... more domain groups ...

const allSymbols = {
  ...lifecycleSymbols,
  ...elementSymbols,
  ...readSymbols,
  // etc.
} as const;

const lib = Deno.dlopen(libPath, allSymbols);
```

### Pattern 2: Library Discovery with Deno APIs
**What:** 3-tier library search replacing node:fs/path/url
**When to use:** loadLibrary() function

```typescript
// Source: verified against Deno 2.7.10 APIs
import { join, dirname } from "jsr:@std/path";

const __dirname = import.meta.dirname!;

function fileExists(path: string): boolean {
  try {
    Deno.statSync(path);
    return true;
  } catch {
    return false;
  }
}

// Platform mapping (per D-03)
// Deno.build.os returns "windows" (not "win32" like Node)
// Deno.build.arch returns "x86_64" (not "x64" like Node)
const EXT = Deno.build.os === "windows" ? "dll"
  : Deno.build.os === "darwin" ? "dylib" : "so";
```

### Pattern 3: Windows DLL Pre-loading
**What:** Pre-load libquiver.dll before libquiver_c.dll (per D-04)
**When to use:** Windows only, before opening the C API library

```typescript
// Source: existing koffi pattern in loader.ts, adapted for Deno.dlopen
if (Deno.build.os === "windows" && fileExists(corePath)) {
  // Load with empty symbols just to get it into the process
  const coreLib = Deno.dlopen(corePath, {});
  // Keep reference to prevent GC/unload
  _coreLib = coreLib;
}
const lib = Deno.dlopen(cApiPath, allSymbols);
```

### Pattern 4: Singleton Caching
**What:** Cache the loaded library to avoid re-opening
**When to use:** loadLibrary() pattern -- keep existing caching approach

```typescript
// The DynamicLibrary returned by Deno.dlopen has .symbols and .close()
type DenoLib = Deno.DynamicLibrary<typeof allSymbols>;
let _lib: DenoLib | null = null;
let _coreLib: Deno.DynamicLibrary<Record<string, never>> | null = null;

export function loadLibrary(): DenoLib {
  if (_lib) return _lib;
  // ... search and load logic ...
  _lib = Deno.dlopen(path, allSymbols);
  return _lib;
}

export function getSymbols() {
  return loadLibrary().symbols;
}
```

### Anti-Patterns to Avoid
- **Using `"buffer"` for output pointer parameters:** `"buffer"` is for passing JS-owned TypedArrays into native code. For opaque native pointers (db handles, element handles), use `"pointer"` in both parameters and results.
- **Mixing `"pointer"` and `"buffer"` for the same parameter semantics:** Be consistent. String inputs use `"buffer"` (passing a Uint8Array). Opaque handles always use `"pointer"`.
- **Forgetting `as const` on symbol definition objects:** Without `as const`, TypeScript widens the type and Deno.dlopen loses type inference for `.symbols`.

## Complete Type Mapping: koffi to Deno FFI

This is the authoritative mapping for Phase 1 implementation.

| koffi Type | C Type | Deno FFI Type | JS Type (param) | JS Type (result) | Notes |
|------------|--------|---------------|------------------|-------------------|-------|
| `"void *"` / `P` | `void*`, opaque handles | `"pointer"` | `Deno.PointerValue` | `Deno.PointerValue` | Opaque pointer objects or null |
| `"str"` / `S` | `const char*` (input) | `"buffer"` | `Uint8Array` | N/A (never str result in this API) | Null-terminated via TextEncoder in Phase 2 |
| `"str"` (return) | `const char*` (return) | `"pointer"` | N/A | `Deno.PointerValue` | Read via UnsafePointerView.getCString() |
| `"int"` | `int` / `int32_t` | `"i32"` | `number` | `number` | Error codes, flags, enum values |
| `"int64_t"` / `I64` | `int64_t` | `"i64"` | `number \| bigint` | `number \| bigint` | Element IDs, version numbers |
| `"uint64_t"` | `uint64_t` / `size_t` | `"u64"` | `number \| bigint` | `number \| bigint` | Array counts, sizes |
| `"double"` | `double` | `"f64"` | `number` | `number` | Float values |
| `"const char **"` | `char**`, `const char* const*` | `"pointer"` | `Deno.PointerValue` | `Deno.PointerValue` | String arrays (Phase 2 marshaling) |
| `"void"` | `void` | `"void"` | N/A | `undefined` | quiver_clear_last_error only |
| (koffi struct) | `quiver_database_options_t` | `{ struct: ["i32", "i32"] }` | `Uint8Array` | `Uint8Array` | 8-byte struct: read_only(i32) + console_level(i32) |

[VERIFIED: Deno FFI type strings confirmed via https://docs.deno.com/runtime/fundamentals/ffi/ and https://denonomicon.deno.dev/basics]

### Critical: `size_t` Mapping Decision

The C API uses `size_t` for array counts and sizes. The current koffi loader maps these to `"uint64_t"`. Two options exist:

- **`"u64"`**: Matches the current koffi mapping exactly. Safe on 64-bit. Returns `number | bigint`.
- **`"usize"`**: More semantically correct (pointer-sized unsigned integer). On 64-bit platforms (the only target), identical to u64. Returns `number | bigint`.

**Recommendation:** Use `"usize"` for `size_t` parameters to be semantically correct. This is the direct C-to-Deno-FFI mapping. On x86_64 (the only architecture where this project runs), `usize` and `u64` are identical at runtime. [ASSUMED]

### Platform Name Mapping

| Node.js | Deno | Used For |
|---------|------|----------|
| `process.platform === "win32"` | `Deno.build.os === "windows"` | DLL extension, pre-loading |
| `process.platform === "darwin"` | `Deno.build.os === "darwin"` | dylib extension |
| `process.arch === "x64"` | `Deno.build.arch === "x86_64"` | Bundled lib dir name |
| `process.arch === "arm64"` | `Deno.build.arch === "aarch64"` | Bundled lib dir name |

[CITED: https://jcbhmr.com/2025/12/14/deno-build-possible-values/ and https://docs.deno.com/api/deno/~/Deno.build.arch]

**Impact:** The bundled library directory naming convention changes. Current: `libs/win32-x64/`. Deno: `libs/windows-x86_64/`. This requires either: (a) renaming bundled lib directories to match Deno naming, or (b) adding a platform key translation map. Since bundled libs are a packaging concern (and the binding is WIP), renaming to Deno conventions is simpler.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Path joining/dirname | Manual string concatenation | `jsr:@std/path` join/dirname | Cross-platform separator handling (Windows backslash vs POSIX forward slash) |
| File existence check | Custom stat wrapper | `Deno.statSync` with try/catch | Standard Deno idiom; handles NotFound error correctly |
| Platform detection | Environment variable parsing | `Deno.build.os` / `Deno.build.arch` | Built-in, reliable, typed |
| C string reading | Manual UTF-8 byte scanning | `Deno.UnsafePointerView.getCString()` | Handles null-termination, UTF-8 decoding, replacement chars |

## Common Pitfalls

### Pitfall 1: Forgetting `as const` on Symbol Definitions
**What goes wrong:** TypeScript widens `"i32"` to `string`, losing type safety on `lib.symbols.*` calls
**Why it happens:** Plain object literals without `as const` get widened types
**How to avoid:** Always use `as const` on symbol definition objects and the final spread object
**Warning signs:** `lib.symbols.quiver_version()` returns `any` instead of typed result

### Pitfall 2: Using `"buffer"` for Return Types
**What goes wrong:** A function declared with `result: "buffer"` still returns a `PointerValue`, NOT a TypedArray. The `"buffer"` type only changes parameter semantics.
**Why it happens:** Confusing parameter behavior (accepts TypedArray) with result behavior
**How to avoid:** Always use `result: "pointer"` for functions returning pointers. `"buffer"` is parameter-only optimization.
**Warning signs:** Expecting a Uint8Array back from a function but getting null or an opaque pointer

### Pitfall 3: Windows DLL Path Formatting
**What goes wrong:** `Deno.dlopen` fails with cryptic error on Windows when path has incorrect separators
**Why it happens:** Deno on Windows requires either forward slashes or properly escaped backslashes
**How to avoid:** Use `jsr:@std/path` join (which produces correct separators) and pass the result directly to `Deno.dlopen`
**Warning signs:** "cannot open shared object" or "access denied" errors on Windows

[CITED: https://github.com/denoland/deno/issues/21172]

### Pitfall 4: Windows DLL Dependency Resolution
**What goes wrong:** `Deno.dlopen("libquiver_c.dll", symbols)` fails because `libquiver.dll` (dependency) is not found
**Why it happens:** Deno.dlopen uses OS-level LoadLibrary which searches CWD and PATH, not the directory containing the DLL
**How to avoid:** Pre-load `libquiver.dll` using `Deno.dlopen(corePath, {})` before loading `libquiver_c.dll` (already decided in D-04)
**Warning signs:** "The specified module could not be found" error even though the DLL file exists

### Pitfall 5: Platform Key Mismatch for Bundled Libraries
**What goes wrong:** Bundled library directory not found because platform key changed from Node.js naming to Deno naming
**Why it happens:** Node uses `win32-x64`, Deno uses `windows-x86_64`
**How to avoid:** Update the platform key construction to use Deno naming: `${Deno.build.os}-${Deno.build.arch}`
**Warning signs:** Bundled mode silently fails, falls through to dev/system PATH

### Pitfall 6: Struct Return Type for quiver_database_options_default
**What goes wrong:** `quiver_database_options_default()` returns a struct by value (not a pointer). koffi handled this automatically via its struct type. Deno requires explicit struct definition.
**Why it happens:** Most C API functions return `quiver_error_t` (int). This one returns the struct directly.
**How to avoid:** Define the return type as `{ struct: ["i32", "i32"] }` to match `quiver_database_options_t` layout (8 bytes: `read_only` int32 + `console_level` int32). The result will be a `Uint8Array`.
**Warning signs:** Garbage values or crashes when reading options

### Pitfall 7: Keeping References to Prevent GC of DLL Handles
**What goes wrong:** Core DLL (pre-loaded with empty symbols) gets garbage collected, then the C API DLL loses its dependency
**Why it happens:** If the `DynamicLibrary` object from `Deno.dlopen(corePath, {})` is not stored, it may be collected
**How to avoid:** Store the core lib reference in a module-level variable (existing `_coreLib` pattern)
**Warning signs:** Intermittent crashes or "access violation" after a GC cycle

## Code Examples

### Example 1: Complete Symbol Definition for One Domain Group
```typescript
// Source: C API headers (include/quiver/c/database.h, element.h, common.h)
// Pattern per D-05 and D-06

const P = "pointer" as const;
const BUF = "buffer" as const;
const I32 = "i32" as const;
const I64 = "i64" as const;
const USIZE = "usize" as const;
const F64 = "f64" as const;
const V = "void" as const;

const lifecycleSymbols = {
  quiver_version: { parameters: [], result: P },
  quiver_get_last_error: { parameters: [], result: P },
  quiver_clear_last_error: { parameters: [], result: V },
  quiver_database_options_default: { parameters: [], result: { struct: [I32, I32] } },
  quiver_database_from_schema: { parameters: [BUF, BUF, BUF, BUF], result: I32 },
  quiver_database_from_migrations: { parameters: [BUF, BUF, BUF, BUF], result: I32 },
  quiver_database_close: { parameters: [P], result: I32 },
  quiver_database_is_healthy: { parameters: [P, BUF], result: I32 },
  quiver_database_path: { parameters: [P, BUF], result: I32 },
  quiver_database_current_version: { parameters: [P, BUF], result: I32 },
  quiver_database_describe: { parameters: [P], result: I32 },
} as const;

const elementSymbols = {
  quiver_element_create: { parameters: [BUF], result: I32 },
  quiver_element_destroy: { parameters: [P], result: I32 },
  quiver_element_set_integer: { parameters: [P, BUF, I64], result: I32 },
  quiver_element_set_float: { parameters: [P, BUF, F64], result: I32 },
  quiver_element_set_string: { parameters: [P, BUF, BUF], result: I32 },
  quiver_element_set_null: { parameters: [P, BUF], result: I32 },
  quiver_element_set_array_integer: { parameters: [P, BUF, BUF, I32], result: I32 },
  quiver_element_set_array_float: { parameters: [P, BUF, BUF, I32], result: I32 },
  quiver_element_set_array_string: { parameters: [P, BUF, P, I32], result: I32 },
} as const;
```

### Example 2: Library Search and Load
```typescript
// Source: adapted from current loader.ts patterns + Deno API docs
import { join, dirname } from "jsr:@std/path";
import { QuiverError } from "./errors.ts";

const __dirname = import.meta.dirname!;

const EXT = Deno.build.os === "windows" ? "dll"
  : Deno.build.os === "darwin" ? "dylib" : "so";
const CORE_LIB = `libquiver.${EXT}`;
const C_API_LIB = `libquiver_c.${EXT}`;

function fileExists(path: string): boolean {
  try {
    Deno.statSync(path);
    return true;
  } catch {
    return false;
  }
}

function getBundledLibDir(): string | null {
  const platformKey = `${Deno.build.os}-${Deno.build.arch}`;
  const libDir = join(__dirname, "..", "libs", platformKey);
  if (fileExists(join(libDir, C_API_LIB))) {
    return libDir;
  }
  return null;
}

function getSearchPaths(): string[] {
  const paths: string[] = [];
  let dir = __dirname;
  for (let i = 0; i < 5; i++) {
    const candidate = join(dir, "build", "bin");
    if (fileExists(join(candidate, C_API_LIB))) {
      paths.push(candidate);
    }
    dir = dirname(dir);
  }
  return paths;
}
```

### Example 3: Windows DLL Pre-loading with Deno.dlopen
```typescript
// Source: D-04 decision + Deno.dlopen API
function openLibrary(dir: string): Deno.DynamicLibrary<typeof allSymbols> {
  const corePath = join(dir, CORE_LIB);
  const cApiPath = join(dir, C_API_LIB);

  if (Deno.build.os === "windows" && fileExists(corePath)) {
    _coreLib = Deno.dlopen(corePath, {});
  }
  return Deno.dlopen(cApiPath, allSymbols);
}
```

### Example 4: Reading C Strings from Pointer Returns
```typescript
// Source: https://docs.deno.com/api/deno/~/Deno.UnsafePointerView
// Used by quiver_version() and quiver_get_last_error() which return const char*
const ptr = lib.symbols.quiver_version();
if (ptr) {
  const version = Deno.UnsafePointerView.getCString(ptr);
}
```

## Symbol Inventory

The current JS loader binds 85 of 93 total C API functions. The 8 unbound functions are:

| Function | Header | Reason Not Bound |
|----------|--------|------------------|
| `quiver_csv_options_default` | options.h | CSV options constructed differently in JS |
| `quiver_database_open` | database.h | JS uses from_schema/from_migrations only |
| `quiver_element_clear` | element.h | Not used in JS binding |
| `quiver_element_has_scalars` | element.h | Not used in JS binding |
| `quiver_element_has_arrays` | element.h | Not used in JS binding |
| `quiver_element_scalar_count` | element.h | Not used in JS binding |
| `quiver_element_array_count` | element.h | Not used in JS binding |
| `quiver_element_to_string` | element.h | Not used in JS binding |

**Recommendation:** Bind the same 85 symbols as the current loader. Do not add the 8 missing ones unless needed -- this matches the "preserve public API" project decision. [VERIFIED: grep analysis of loader.ts vs C API headers]

## Exported API Changes

The public exports from loader.ts change:

| Export | Current (koffi) | New (Deno) | Impact |
|--------|-----------------|------------|--------|
| `NativePointer` | `unknown` (koffi opaque) | `Deno.PointerValue` | Type change -- all consumers must update |
| `Symbols` | `Record<string, FFIFunction>` | `typeof lib.symbols` | More precise typing |
| `FFIFunction` | `(...args: any[]) => any` | Removed (Deno symbols are typed) | Type improvement |
| `loadLibrary()` | Returns `Symbols` | Returns typed `DynamicLibrary` | API shape preserved |
| `getSymbols()` | Returns `Symbols` | Returns typed symbols object | API shape preserved |
| `DatabaseOptionsType` | koffi struct type | Removed (struct handled as Uint8Array) | Phase 2 handles options |

## State of the Art

| Old Approach (koffi) | New Approach (Deno FFI) | When Changed | Impact |
|----------------------|------------------------|--------------|--------|
| `koffi.load(path)` then `lib.func(name, ret, params)` | `Deno.dlopen(path, symbolDefs)` one-shot | Deno 1.13+ (FFI), stable in Deno 2.x | Simpler: define all symbols upfront, no per-symbol binding |
| `koffi.decode(ptr, type)` for reading | `Deno.UnsafePointerView` methods | Deno 1.25+ | More explicit, no implicit type coercion |
| `"str"` type for automatic string marshaling | `"buffer"` + manual TextEncoder/Decoder | Deno FFI design | More control, but more verbose (Phase 2 scope) |
| Pointer represented as number/BigInt | Opaque `PointerObject` or null | Deno 1.31+ | Better type safety, prevents accidental arithmetic |
| `Buffer.alloc(8)` for out-parameters | `new Uint8Array(8)` or `new BigUint64Array(1)` | Deno (no Node Buffer) | Standard JS APIs only |

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | `"usize"` is the correct Deno FFI type for C `size_t` parameters | Complete Type Mapping | If wrong, use `"u64"` instead. Both are 8 bytes on x86_64. Zero runtime risk on 64-bit. |
| A2 | `Deno.dlopen(path, {})` with empty symbols successfully pre-loads a DLL into the process on Windows | Architecture Patterns | If Deno rejects empty symbol objects, alternative is to define a dummy symbol or use a different pre-loading strategy. |
| A3 | `import.meta.dirname` returns a string (not undefined) when running from a local .ts file | Architecture Patterns | If undefined in some Deno execution modes, need fallback using `import.meta.url` with URL parsing. Low risk for local file execution. |
| A4 | Bundled lib directory naming should change from `win32-x64` to `windows-x86_64` | Platform Name Mapping | If bundled libs already exist with old naming, need translation map instead. Since binding is WIP, renaming is acceptable. |

## Open Questions

1. **`quiver_database_options_default` struct return**
   - What we know: Returns `quiver_database_options_t` by value (8 bytes: two int32). Deno FFI returns structs as `Uint8Array`.
   - What's unclear: Whether `{ struct: ["i32", "i32"] }` correctly maps the struct layout with potential padding.
   - Recommendation: Test empirically. The struct has two consecutive `int` fields with no padding on any platform, so `["i32", "i32"]` should be correct. Fallback: skip binding this function and construct the 8-byte buffer manually in ffi-helpers.ts (already done by `makeDefaultOptions()`).

2. **`quiver_csv_options_default` struct return**
   - What we know: Returns `quiver_csv_options_t` which is a larger struct with pointers and size_t fields. Not currently bound in JS.
   - What's unclear: Not needed for Phase 1. Will be needed if CSV options binding is added later.
   - Recommendation: Do not bind in Phase 1. The JS binding already constructs CSV options manually.

3. **Whether `_coreLib` reference prevents unloading**
   - What we know: Storing the `DynamicLibrary` object should prevent Deno from closing/unloading the DLL.
   - What's unclear: Whether Deno's garbage collector can collect a `DynamicLibrary` that has no symbols accessed.
   - Recommendation: Store in module-level variable. If issues arise, call a no-op method periodically or close explicitly on module unload.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| Deno runtime | All FFI operations | Yes | 2.7.10 | -- |
| jsr:@std/path | Path operations (D-02) | Yes (JSR registry) | latest | -- |
| libquiver.dll | Core library (runtime dep) | Yes (build/bin/) | -- | -- |
| libquiver_c.dll | C API library (runtime dep) | Yes (build/bin/) | -- | -- |

[VERIFIED: `deno --version` returns 2.7.10]

**Missing dependencies with no fallback:** None
**Missing dependencies with fallback:** None

## Security Domain

### Applicable ASVS Categories

| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | No | N/A -- library loader, no auth |
| V3 Session Management | No | N/A |
| V4 Access Control | Yes (minimal) | `--allow-ffi` permission gate |
| V5 Input Validation | Yes (minimal) | Library path validation before dlopen |
| V6 Cryptography | No | N/A |

### Known Threat Patterns

| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| DLL hijacking via PATH search | Tampering | Prefer bundled/absolute paths; system PATH is last resort |
| Loading malicious library from untrusted path | Elevation of Privilege | `--allow-ffi` permission model; path-scoped `--allow-ffi=./libs` |
| Arbitrary memory read via UnsafePointerView | Information Disclosure | Phase 2+ concern; no unsafe pointer ops exposed in public API |

## Sources

### Primary (HIGH confidence)
- Deno FFI documentation: https://docs.deno.com/runtime/fundamentals/ffi/ -- type descriptors, dlopen syntax, permission model
- Deno API reference: https://docs.deno.com/api/deno/ffi -- NativeType, ForeignFunction, UnsafePointerView, UnsafePointer
- Deno.dlopen API: https://docs.deno.com/api/deno/~/Deno.dlopen -- function signature, DynamicLibrary return type
- Deno.UnsafePointerView API: https://docs.deno.com/api/deno/~/Deno.UnsafePointerView -- getCString, getArrayBuffer, all accessor methods
- Deno.UnsafePointer API: https://docs.deno.com/api/deno/~/Deno.UnsafePointer -- of, value, create, equals, offset
- import.meta.dirname: https://docs.deno.com/api/web/~/ImportMeta.dirname -- module directory path
- Deno.build: https://docs.deno.com/api/deno/~/Deno.build.arch -- os and arch values
- jsr:@std/path: https://jsr.io/@std/path -- join, dirname
- Denonomicon pointers guide: https://denonomicon.deno.dev/types/pointers -- pointer vs buffer, out-parameters
- Target machine: Deno 2.7.10 verified via `deno --version`

### Secondary (MEDIUM confidence)
- Platform naming values: https://jcbhmr.com/2025/12/14/deno-build-possible-values/ -- Deno.build.os/arch possible values
- Windows DLL path issue: https://github.com/denoland/deno/issues/21172 -- path formatting on Windows

### Tertiary (LOW confidence)
- None

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- Deno FFI is stable, well-documented, verified on target machine
- Architecture: HIGH -- type mapping is mechanical, all APIs verified against official docs
- Pitfalls: HIGH -- drawn from official docs, known issues, and existing codebase analysis

**Research date:** 2026-04-17
**Valid until:** 2026-05-17 (30 days -- Deno FFI API is stable)
