# Technology Stack

**Project:** Quiver JS/TS Binding (v0.5)
**Researched:** 2026-03-08

## Recommended Stack

### Runtime

| Technology | Version | Purpose | Why |
|------------|---------|---------|-----|
| Bun | >= 1.3.0 | Runtime, package manager, test runner, bundler | Project requirement. Provides `bun:ffi` for direct C library calls without native addon compilation. All-in-one: no separate test framework, bundler, or package manager needed. |

### FFI Layer

| Technology | Version | Purpose | Why |
|------------|---------|---------|-----|
| `bun:ffi` (built-in) | Ships with Bun | C shared library binding via `dlopen` | Built into Bun. Calls C functions directly with JIT-compiled bindings. No compilation step (unlike N-API/node-gyp). Supports all types needed by libquiver_c: `ptr` (opaque handles), `i32` (error codes, booleans), `i64` (int64_t IDs/values), `f64` (doubles), `cstring` (string args), `u64`/`usize` (size_t). |

### TypeScript

| Technology | Version | Purpose | Why |
|------------|---------|---------|-----|
| TypeScript | >= 5.5 | Type checking and declaration generation | Bun transpiles TS natively at runtime (no tsc build step for execution). TypeScript is used for type checking during development and `.d.ts` generation for consumers. Version 5.5+ for `isolatedDeclarations` support. |
| `@types/bun` | latest | Bun runtime type definitions | Provides types for `bun:ffi`, `bun:test`, and all Bun-specific APIs. Shim that loads `bun-types` under the hood. |

### Testing

| Technology | Version | Purpose | Why |
|------------|---------|---------|-----|
| `bun:test` (built-in) | Ships with Bun | Unit and integration testing | Built into Bun. Jest-compatible API (`describe`, `test`, `expect`). Runs TypeScript natively without transpilation. 10-30x faster than Jest. Supports lifecycle hooks, mocking, coverage (`bun test --coverage`). Zero external dependencies. |

### Package Distribution

| Technology | Version | Purpose | Why |
|------------|---------|---------|-----|
| npm registry | N/A | Package distribution | Standard JS package registry. Bun uses npm natively (`bun add`). |
| `bun publish` | Ships with Bun | Publishing to npm | Built-in command. Handles tarball creation, workspace protocol stripping, registry auth. No need for separate `npm publish`. |

### Code Quality

| Technology | Version | Purpose | Why |
|------------|---------|---------|-----|
| Biome | >= 2.0 | Linting and formatting | Single tool for both lint and format. Fast (Rust-based). No ESLint + Prettier configuration complexity. Opinionated defaults reduce bikeshedding. |

## What NOT to Use

These are explicit exclusions to prevent wrong turns:

| Technology | Why Excluded |
|------------|-------------|
| Node.js / N-API / node-gyp | Project is Bun-only. N-API requires compilation. `bun:ffi` is simpler and faster for this use case. |
| `node-ffi-napi` / `ffi-napi` | Node.js FFI packages. Incompatible with `bun:ffi`. |
| `bun-ffi-gen` | FFI binding generator from C headers. Requires clang in PATH, adds complexity. Quiver's C API is small and stable enough for hand-written bindings (same approach as Julia/Dart/Python). Hand-written bindings match the project's existing generator pattern. |
| Jest / Vitest / Mocha | External test frameworks. `bun:test` is built-in, faster, and Jest-compatible. Zero-config. |
| ESLint + Prettier | Two tools, complex configuration. Biome does both in one tool. |
| Webpack / Rollup / esbuild | External bundlers. This is a Bun-native library, not a browser bundle. Bun's built-in bundler handles anything needed. |
| `bunup` | Build tool for TypeScript libraries targeting ESM/CJS dual output. Overkill for a Bun-only package that ships `.ts` source (Bun runs TS natively). |

## Core FFI Concepts for libquiver_c

### Type Mapping: C to bun:ffi

| C Type | FFI Type | JS Type | Notes |
|--------|----------|---------|-------|
| `quiver_error_t` (int enum) | `"i32"` | `number` | QUIVER_OK=0, QUIVER_ERROR=1 |
| `quiver_database_t*` | `"ptr"` | `number` (pointer) | Opaque handle, store as number |
| `quiver_element_t*` | `"ptr"` | `number` (pointer) | Opaque handle, store as number |
| `const char*` (input) | `"cstring"` | `string` via `Buffer.from()` | Must null-terminate; use `Buffer.from(str + "\0")` |
| `const char*` (return) | `"ptr"` | Read with `new CString(ptr)` | Clone immediately; C side may free |
| `int64_t` | `"i64"` | `number \| bigint` | Bun coerces to `number` if fits in safe integer range |
| `int64_t*` (out param) | `"ptr"` | `BigInt64Array` | Allocate `new BigInt64Array(1)`, pass via `ptr()` |
| `double` | `"f64"` | `number` | Direct mapping |
| `double*` (out param) | `"ptr"` | `Float64Array` | Allocate `new Float64Array(1)`, pass via `ptr()` |
| `size_t` | `"usize"` | `number` | Platform-sized unsigned integer |
| `size_t*` (out param) | `"ptr"` | Typed array | Use platform-appropriate typed array |
| `int` / `int*` (bool out) | `"i32"` / `"ptr"` | `number` / `Int32Array` | For `out_has_value` style parameters |
| `void**` (out ptr-to-ptr) | `"ptr"` | Pointer buffer | Allocate buffer, read back with `read.ptr()` |
| `bool` | `"bool"` | `boolean` | Direct mapping (used rarely in quiver C API) |

### Library Loading Pattern

```typescript
import { dlopen, suffix, ptr, CString, read, toArrayBuffer } from "bun:ffi";

// suffix = "dll" on Windows, "so" on Linux, "dylib" on macOS
const LIB_NAME = `libquiver_c.${suffix}`;
const CORE_LIB_NAME = `libquiver.${suffix}`;

// Load core library first (dependency of C API library)
// On Windows, libquiver_c.dll depends on libquiver.dll
dlopen(CORE_LIB_NAME, {});

// Load C API library with function signatures
const lib = dlopen(LIB_NAME, {
  quiver_version: { args: [], returns: "ptr" },
  quiver_get_last_error: { args: [], returns: "ptr" },
  quiver_database_from_schema: {
    args: ["cstring", "cstring", "ptr", "ptr"],
    returns: "i32",
  },
  quiver_database_close: {
    args: ["ptr"],
    returns: "i32",
  },
  // ... all other functions
});
```

### Out-Parameter Pattern (critical for quiver C API)

The quiver C API returns values via out-parameters (e.g., `quiver_database_t** out_db`). This requires allocating buffers in JS and reading back pointer values:

```typescript
// Allocate a pointer-sized buffer for the out parameter
const outDb = new BigInt64Array(1);  // holds a pointer

const err = lib.symbols.quiver_database_from_schema(
  Buffer.from(dbPath + "\0"),
  Buffer.from(schemaPath + "\0"),
  ptr(optionsBuffer),  // quiver_database_options_t*
  ptr(outDb),          // quiver_database_t** out_db
);

if (err !== 0) {
  const errPtr = lib.symbols.quiver_get_last_error();
  throw new Error(new CString(errPtr));
}

// Read the pointer value back
const dbPtr = read.ptr(outDb, 0);
```

### String Return Pattern

```typescript
// For functions returning strings via char** out_value
const outStr = new BigInt64Array(1);
const outHas = new Int32Array(1);

lib.symbols.quiver_database_read_scalar_string_by_id(
  dbPtr,
  Buffer.from(collection + "\0"),
  Buffer.from(attribute + "\0"),
  id,
  ptr(outStr),
  ptr(outHas),
);

if (outHas[0]) {
  const strPtr = read.ptr(outStr, 0);
  const value = new CString(strPtr);
  // CString clones the string, safe to free the C pointer
  lib.symbols.quiver_database_free_string(strPtr);
  return String(value);
}
return null;
```

### Memory Management Strategy

bun:ffi does NOT manage memory. The binding must explicitly call every `quiver_database_free_*` function. Pattern:

1. Call C function that allocates (read, query)
2. Copy data into JS values (CString clones strings; TypedArray copies for numeric arrays)
3. Call matching `quiver_database_free_*` immediately
4. Return JS values

Use `try/finally` to ensure cleanup:

```typescript
const outValues = new BigInt64Array(1);  // int64_t** out_values
const outCount = new BigInt64Array(1);   // size_t* out_count

lib.symbols.quiver_database_read_scalar_integers(
  dbPtr, collection, attribute, ptr(outValues), ptr(outCount)
);

const arrayPtr = read.ptr(outValues, 0);
const count = Number(outCount[0]);
try {
  // Convert to JS array via toArrayBuffer
  const buffer = toArrayBuffer(arrayPtr, 0, count * 8);  // 8 bytes per int64
  return Array.from(new BigInt64Array(buffer), Number);
} finally {
  lib.symbols.quiver_database_free_integer_array(arrayPtr);
}
```

## TypeScript Configuration

### tsconfig.json

```json
{
  "compilerOptions": {
    "lib": ["ESNext"],
    "target": "ESNext",
    "module": "Preserve",
    "moduleDetection": "force",
    "moduleResolution": "bundler",
    "allowImportingTsExtensions": true,
    "verbatimModuleSyntax": true,
    "noEmit": true,
    "strict": true,
    "skipLibCheck": true,
    "noFallthroughCasesInSwitch": true,
    "noUncheckedIndexedAccess": true,
    "noImplicitOverride": true,
    "isolatedDeclarations": true,
    "declaration": true,
    "types": ["bun-types"]
  },
  "include": ["src/**/*.ts"],
  "exclude": ["node_modules", "test"]
}
```

Key decisions:
- **`"module": "Preserve"`** -- Bun's recommended setting. Does not transform imports.
- **`"moduleResolution": "bundler"`** -- Bun's module resolution strategy.
- **`"noEmit": true`** -- Bun runs `.ts` directly; tsc is only for type checking.
- **`"isolatedDeclarations": true`** -- Faster `.d.ts` generation, forces explicit types on exports.
- **`"types": ["bun-types"]`** -- Provides `bun:ffi`, `bun:test` type definitions.
- **`"strict": true`** -- Catch pointer/null errors early (critical for FFI safety).

### For .d.ts Generation (npm consumers)

Run `tsc --declaration --emitDeclarationOnly` to generate `.d.ts` files for npm distribution. This is needed because npm consumers may use editors that need declaration files, even though Bun runs `.ts` natively.

## Package Configuration

### package.json

```json
{
  "name": "quiverdb",
  "version": "0.5.0",
  "type": "module",
  "main": "src/index.ts",
  "types": "src/index.ts",
  "files": [
    "src/**/*.ts"
  ],
  "scripts": {
    "test": "bun test",
    "typecheck": "tsc --noEmit",
    "lint": "biome check src/ test/",
    "format": "biome format --write src/ test/"
  },
  "devDependencies": {
    "@types/bun": "latest",
    "typescript": "^5.5.0",
    "@biomejs/biome": "^2.0.0"
  },
  "engines": {
    "bun": ">=1.3.0"
  },
  "os": ["win32", "linux", "darwin"],
  "peerDependencies": {}
}
```

Key decisions:
- **`"main": "src/index.ts"`** -- Ship TypeScript source. Bun consumers run `.ts` directly. No build step needed for the primary use case.
- **`"type": "module"`** -- ESM only. Bun is ESM-first.
- **`"files": ["src/**/*.ts"]`** -- Only ship source. Native `.dll`/`.so`/`.dylib` are NOT bundled in npm package (see Distribution section).
- **No `"exports"` field** -- Single entry point, no subpath exports needed for v0.5.
- **No build step** -- Bun runs TypeScript natively. No compilation, no bundling.

### Directory Structure

```
bindings/js/
  package.json
  tsconfig.json
  biome.json
  src/
    index.ts              # Public API re-exports
    ffi/
      symbols.ts          # dlopen declarations (all C function signatures)
      loader.ts           # Library loading (platform detection, dependency chain)
      types.ts            # FFI type constants and helpers
    database.ts           # Database class (main public API)
    element.ts            # Element builder
    errors.ts             # QuiverError class, error checking helper
    options.ts            # DatabaseOptions, CSVOptions types
  test/
    test.bat              # Test runner script (sets PATH for DLL discovery)
    database.test.ts      # Database lifecycle tests
    create.test.ts        # Create element tests
    read.test.ts          # Read operations tests
    query.test.ts         # Query operations tests
    transaction.test.ts   # Transaction tests
```

This mirrors the existing binding structure: `bindings/julia/`, `bindings/dart/`, `bindings/python/` each have a `src/`, `test/`, and `generator/` directory.

## Distribution Strategy

### Native Library Discovery (NOT bundled in npm)

The npm package ships TypeScript source only. The native shared libraries (`libquiver.dll`/`.so`/`.dylib` and `libquiver_c.dll`/`.so`/`.dylib`) must be available at runtime via one of:

1. **Development mode**: Libraries on system PATH (e.g., `build/bin/` added to PATH by test scripts)
2. **Installed mode**: Libraries in a known location, path configured by user

This matches the existing bindings' approach:
- **Python**: `_loader.py` checks `_libs/` subdirectory first, then falls back to system PATH
- **Dart**: `library_loader.dart` checks Native Assets build output, then system PATH
- **Julia**: `BinaryBuilder` package handles native library distribution

For v0.5, development-mode PATH-based loading is sufficient. A bundling/distribution mechanism for production use can be added in a future milestone.

### Library Loading Order

On Windows, `libquiver_c.dll` depends on `libquiver.dll`. The loader must:
1. Load `libquiver.{suffix}` first (no symbols needed, just resolve dependency)
2. Load `libquiver_c.{suffix}` with full symbol definitions

This is the same pattern used by Python's `_loader.py` and Dart's `library_loader.dart`.

## Alternatives Considered

| Category | Recommended | Alternative | Why Not |
|----------|-------------|-------------|---------|
| FFI mechanism | `bun:ffi` | N-API (Node.js native addons) | Requires C++ compilation at install time, node-gyp dependency, not Bun-native |
| FFI mechanism | `bun:ffi` | WebAssembly | Would require recompiling C++ core to WASM; loses SQLite's native file I/O |
| Test framework | `bun:test` | Jest | External dependency, slower, requires config for TS |
| Test framework | `bun:test` | Vitest | External dependency, designed for Vite ecosystem |
| Linter/formatter | Biome | ESLint + Prettier | Two tools, complex config, slower |
| Package format | Ship `.ts` source | Compile to `.js` + `.d.ts` | Bun runs TS natively; shipping source is simpler and matches Bun ecosystem norms |
| Binding generator | Hand-written | `bun-ffi-gen` | Requires clang in PATH, adds build complexity. C API surface is small (~50 functions). Other bindings also use hand-written or simple generators. |
| Type declarations | `@types/bun` | `bun-types` | `@types/bun` is the newer/preferred package; it re-exports `bun-types` |

## Version Compatibility Matrix

| Component | Minimum | Tested | Notes |
|-----------|---------|--------|-------|
| Bun | 1.3.0 | 1.3.9 | Latest stable as of March 2026 |
| TypeScript | 5.5 | 5.7+ | For `isolatedDeclarations` support |
| `@types/bun` | latest | latest | Tracks Bun releases |
| Biome | 2.0 | 2.0+ | Major version for stability |
| libquiver_c | current | current | Must match C API header definitions |

## Confidence Assessment

| Area | Confidence | Reason |
|------|------------|--------|
| `bun:ffi` usage patterns | HIGH | Official Bun documentation, multiple community examples, SQLite binding example in docs |
| Type mapping C -> FFI | HIGH | Official FFIType documentation, verified against Bun API reference |
| Out-parameter pattern | MEDIUM | Inferred from `read.ptr()` docs and community examples; no official "out parameter" guide exists. Pattern is standard for C FFI but Bun-specific ergonomics need validation during implementation. |
| `bun:test` | HIGH | Official documentation, Jest-compatible API, widely used |
| TypeScript config | HIGH | Official `@tsconfig/bun` package, Bun docs recommend exact settings |
| npm publishing | HIGH | `bun publish` documented, standard npm workflow |
| Library loading chain | HIGH | Verified against existing Python/Dart loaders in this repo |
| Biome | MEDIUM | Recommended by ecosystem but not verified against Bun-specific patterns |

## Sources

- [Bun FFI Documentation](https://bun.com/docs/runtime/ffi) -- Official guide, type mappings, dlopen usage
- [Bun FFI API Reference](https://bun.com/reference/bun/ffi) -- Complete API surface
- [Bun FFI FFIType Enum](https://bun.com/reference/bun/ffi/FFIType) -- All supported types
- [Bun FFI CString Class](https://bun.com/reference/bun/ffi/CString) -- String handling, memory cloning behavior
- [Bun FFI read Object](https://bun.com/reference/bun/ffi/read) -- Pointer reading functions
- [Bun Test Runner](https://bun.com/docs/test) -- Built-in test framework
- [Bun TypeScript Configuration](https://bun.com/docs/typescript) -- Recommended tsconfig
- [@tsconfig/bun (npm)](https://www.npmjs.com/package/@tsconfig/bun) -- Base tsconfig for Bun
- [@types/bun (npm)](https://www.npmjs.com/package/@types/bun) -- Bun type definitions
- [bun publish Documentation](https://bun.com/docs/pm/cli/publish) -- npm publishing
- [Bun Releases](https://github.com/oven-sh/bun/releases) -- Version history, v1.3.9 latest
- [bun-ffi-gen (npm)](https://www.npmjs.com/package/bun-ffi-gen) -- Evaluated and rejected for this project
- [Building a TypeScript Library in 2026 with Bun](https://dev.to/arshadyaseen/building-a-typescript-library-in-2026-with-bunup-3bmg) -- Ecosystem context
