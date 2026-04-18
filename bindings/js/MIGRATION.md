# Migration Guide: koffi to Deno FFI

This document describes the migration of the quiverdb JS binding from koffi (Node.js FFI) to Deno's native FFI (`Deno.dlopen`).

## What Changed

### 1. FFI Layer

`koffi.load()` replaced with `Deno.dlopen()`. All 85 C API symbols now use Deno FFI type descriptors (`"pointer"`, `"i32"`, `"i64"`, `"f64"`, `"buffer"`, `"usize"`, `"void"`) instead of koffi type strings. Symbol definitions are grouped by domain (lifecycle, element, CRUD, read, query, transaction, metadata, time series, CSV, free, Lua) in `src/loader.ts`.

### 2. Memory Marshaling

- **Pointer out-parameters**: `Uint8Array` + `DataView` with `Deno.UnsafePointer.of()` instead of koffi `Buffer`
- **String encoding**: `TextEncoder`/`TextDecoder` instead of koffi string coercion
- **Array decoding**: `Deno.UnsafePointerView` with `getArrayBuffer()` and typed arrays (`BigInt64Array`, `Float64Array`) instead of `koffi.decode`
- **Struct field reading**: `Deno.UnsafePointerView` at byte offsets instead of koffi struct definitions
- **Native memory allocation**: `Uint8Array`/`DataView` instead of `koffi.alloc`/`koffi.encode`
- **Pointer arithmetic**: `Deno.UnsafePointer.create()`/`Deno.UnsafePointer.value()` with `BigInt` addresses

All marshaling helpers are in `src/ffi-helpers.ts`.

### 3. Package Configuration

- `package.json` replaced by `deno.json`
- `tsconfig.json` removed (Deno handles TypeScript natively)
- `vitest.config.ts` removed
- No `node_modules` directory needed

### 4. Test Runner

- `vitest` replaced by `Deno.test` with `@std/assert`
- Test command: `deno test --allow-ffi --allow-read --allow-write --allow-env test/`

### 5. Imports

- All `node:fs`, `node:path`, `node:url`, `node:os` imports removed
- Uses `jsr:@std/path` for path operations
- Uses `import.meta.dirname` instead of `dirname(fileURLToPath(import.meta.url))`
- All internal imports use `.ts` extension (Deno requirement)

## Why

### Path-scoped FFI permissions

Deno's `--allow-ffi` flag makes native library loading explicit. Users must opt in to FFI access, unlike Node.js where any package can silently load native code via koffi or N-API. This is a security improvement.

### Native Deno support

`Deno.dlopen` is a built-in API -- no third-party FFI library dependency. This eliminates the koffi npm package (and its native compilation step) from the dependency chain.

### Single runtime target

The binding previously aimed for Node.js/Deno/Bun compatibility via koffi. Targeting Deno exclusively simplifies the codebase and removes compatibility shims.

## Breaking Changes (Developer Workflow)

The public API (`Database` class, method signatures, types) did **not** change. These are developer workflow changes only:

| Before (koffi) | After (Deno FFI) |
|-----------------|-------------------|
| `npm install` | No install step (Deno imports from source) |
| `bun test` or `npx vitest` | `deno task test` |
| `node script.js` | `deno run --allow-ffi --allow-read --allow-write script.ts` |
| `package.json` scripts | `deno.json` tasks |
| `tsconfig.json` required | Not needed (Deno built-in TypeScript) |
| koffi npm dependency | No FFI dependency (Deno built-in) |
| Works with Node.js, Deno, Bun | Deno only |

## Public API Unchanged

All public methods, types, and error handling remain identical. Code using `Database.fromSchema`, `createElement`, `readScalarIntegers`, etc. requires no changes beyond updating the import path to use `.ts` extension:

```typescript
// Before
import { Database } from "quiverdb";

// After
import { Database } from "./src/index.ts";
```
