# Phase 1: FFI Foundation & Library Loading - Context

**Gathered:** 2026-04-17
**Status:** Ready for planning

<domain>
## Phase Boundary

Replace koffi-based library loading in `bindings/js/src/loader.ts` with Deno.dlopen and define all ~75 C API symbols using Deno FFI type descriptors. Remove all `node:` protocol imports (node:fs, node:path, node:url) from loader.ts. No changes to ffi-helpers.ts marshaling logic (Phase 2) or domain modules (Phases 3-4).

</domain>

<decisions>
## Implementation Decisions

### Library Search Paths
- **D-01:** Keep all 3 tiers of library discovery: bundled `libs/{os}-{arch}/`, dev `build/bin/` walk-up (5 levels), system PATH fallback
- **D-02:** Replace `node:fs` (existsSync) with `Deno.statSync`, `node:url` (fileURLToPath) with `import.meta.dirname`, `node:path` (join/dirname) with `jsr:@std/path`
- **D-03:** Replace `process.platform`/`process.arch` with `Deno.build.os`/`Deno.build.arch`

### Windows DLL Chain
- **D-04:** Explicitly pre-load `libquiver.dll` (core) before loading `libquiver_c.dll` using `Deno.dlopen(corePath, {})` with empty symbols. Same strategy as current koffi code -- do not rely on OS DLL search order.

### Symbol Organization
- **D-05:** Group ~75 symbol definitions by domain (lifecycle, element, read, query, metadata, time-series, csv, lua, free) as separate const objects, then spread into one combined symbols object for `Deno.dlopen`. Mirrors the C API file structure (`src/c/database_*.cpp`).

### Type Mapping
- **D-06:** Define named constants for Deno FFI types at top of loader.ts: `P = 'pointer'`, `I32 = 'i32'`, `I64 = 'i64'`, `U64 = 'u64'`, `F64 = 'f64'`, `V = 'void'`. Mirrors existing koffi shorthand pattern (`P = "void *"`, `S = "str"`, `I64 = "int64_t"`).
- **D-07:** koffi `"str"` maps to `"pointer"` in Deno (strings are passed as pointers, encoded/decoded in ffi-helpers.ts Phase 2). koffi `"const char **"` maps to `"pointer"`. koffi `"int"` maps to `"i32"`.

### Claude's Discretion
- Error message format for library-not-found (QuiverError) -- keep consistent with current style
- Exact domain grouping boundaries for symbols (lifecycle vs element vs CRUD splits)
- Whether to keep the `_symbols` / `_coreLib` caching pattern or simplify

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Current Implementation (to be replaced)
- `bindings/js/src/loader.ts` -- Current koffi-based loader with all ~75 symbol definitions
- `bindings/js/src/ffi-helpers.ts` -- Marshaling helpers (Phase 2 scope, but read for type understanding)
- `bindings/js/src/types.ts` -- Pure type definitions (no koffi dependency)
- `bindings/js/package.json` -- Current koffi dependency and package config

### C API Headers (symbol source of truth)
- `include/quiver/c/database.h` -- Database lifecycle, CRUD, read, query, metadata, time series, CSV symbols
- `include/quiver/c/element.h` -- Element create/destroy/set symbols
- `include/quiver/c/options.h` -- Option types and defaults
- `include/quiver/c/lua_runner.h` -- Lua runner symbols

### Other Binding Loaders (reference patterns)
- `bindings/python/quiver/_loader.py` -- Python's DLL pre-loading approach on Windows
- `bindings/dart/lib/src/quiver_bindings.dart` -- Dart's FFI symbol definitions

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `bindings/js/src/types.ts` -- Pure TypeScript types, no koffi dependency. Can be kept as-is.
- `bindings/js/src/errors.ts` -- QuiverError class used by loader. Likely no koffi dependency.

### Established Patterns
- **Symbol binding:** Current `bindSymbols()` uses shorthand variables (`P`, `S`, `I64`) for type names -- D-06 continues this pattern with Deno equivalents
- **Singleton caching:** `_symbols` variable caches loaded library -- pattern worth keeping for Deno
- **3-tier search:** bundled -> dev walk-up -> system PATH with try/catch fallthrough at each tier

### Integration Points
- `loadLibrary()` and `getSymbols()` are the public API from loader.ts -- all other modules import from here
- `NativePointer` type export will change from koffi's opaque type to `Deno.PointerObject` or `Deno.PointerValue`
- `Symbols` type will change from `Record<string, FFIFunction>` to the Deno.dlopen return type
- `DatabaseOptionsType` koffi struct definition is used only in loader.ts -- will be removed (struct handling moves to Phase 2/4)

</code_context>

<specifics>
## Specific Ideas

No specific requirements -- open to standard approaches. User confirmed recommended options for all 4 discussion areas.

</specifics>

<deferred>
## Deferred Ideas

None -- discussion stayed within phase scope.

</deferred>

---

*Phase: 01-ffi-foundation-library-loading*
*Context gathered: 2026-04-17*
