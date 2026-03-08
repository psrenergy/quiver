# Project Research Summary

**Project:** Quiver JS/TS Binding (v0.5)
**Domain:** Bun-native FFI binding for a C API database wrapper library
**Researched:** 2026-03-08
**Confidence:** MEDIUM-HIGH

## Executive Summary

The Quiver JS/TS binding is a thin wrapper over the existing `libquiver_c` shared library, built using Bun's native FFI mechanism (`bun:ffi`). The approach mirrors the existing Julia, Dart, and Python bindings: no logic lives in the binding layer, all intelligence remains in the C++ core, and the JS layer is a mechanical translation of C API calls. Bun is a hard requirement (the project is Bun-only for v0.5), and its built-in `bun:ffi`, `bun:test`, and TypeScript runtime eliminate the need for any external dependencies beyond devtools (TypeScript, Biome).

The recommended approach is to ship TypeScript source directly (no build step), use `bun:ffi`'s `dlopen` for C function binding with hand-written symbol declarations, and follow the naming convention from CLAUDE.md (snake_case C++ to camelCase JS). The most technically risky element is `bun:ffi` itself: it has known bugs, no native struct support, no automatic memory management, and pointer-to-pointer out-parameters require careful manual handling. All of these are solvable with established patterns, but they must be validated early.

The critical risk is memory leaks from the ~15 alloc/free pairs in the C API. Every read operation allocates C memory that must be explicitly freed via matching `quiver_database_free_*` calls. Bun provides no automatic cleanup. The mitigation is a strict `try/finally` discipline in every wrapper method, a centralized `memory.ts` helper module for out-parameter allocation, and a `check()` error handler that reads error messages immediately. The second risk is `bun:ffi`'s documented instability -- avoided by keeping the FFI surface minimal (v0.5 excludes structs, time series, metadata, blob, and CSV).

## Key Findings

### Recommended Stack

The stack is entirely Bun-native with no external runtime dependencies. Bun >= 1.3.0 provides everything: FFI via `bun:ffi`, testing via `bun:test`, TypeScript execution natively, and npm publishing via `bun publish`. TypeScript >= 5.5 is required for `isolatedDeclarations` support. Biome (>= 2.0) handles both linting and formatting in a single Rust-based tool. No bundler, build step, or compilation is required -- Bun runs `.ts` source files directly, and the npm package ships source.

See `.planning/research/STACK.md` for full type mapping tables, tsconfig, package.json, and library loading patterns.

**Core technologies:**
- `bun:ffi` (built-in): C shared library binding via `dlopen` -- the only viable approach for Bun-native FFI without compilation
- TypeScript >= 5.5: Type safety and `.d.ts` generation for npm consumers -- Bun transpiles at runtime, tsc is typecheck-only
- `bun:test` (built-in): Jest-compatible test runner -- zero external dependency, 10-30x faster than Jest
- Biome >= 2.0: Combined linter + formatter -- replaces ESLint + Prettier with one tool
- `bun publish`: npm distribution -- built-in, handles workspace protocol and registry auth

### Expected Features

The v0.5 scope is tightly constrained to what `bun:ffi` handles cleanly: pointer-in/out patterns and allocated arrays. Struct unmarshaling, multi-column typed data (time series), and metadata operations are explicitly deferred due to the complexity of manual struct layout in `bun:ffi`.

See `.planning/research/FEATURES.md` for the full feature list, complexity assessments, TypeScript type definitions, and MVP recommendation.

**Must have (table stakes):**
- Database lifecycle (`fromSchema`, `fromMigrations`, `close`) -- cannot use the library without this
- Error handling via thrown exceptions (`QuiverError` class) -- JS convention; return codes are an anti-pattern
- Full CRUD operations (`createElement`, `updateElement`, `deleteElement`) -- core write operations
- Read scalar, vector, set values (all types: integer, float, string; bulk and by-ID) -- core read operations
- Query operations (plain + parameterized: string, integer, float) -- SQL queries are fundamental
- Transaction control (`beginTransaction`, `commit`, `rollback`, `inTransaction`) -- multi-operation atomicity
- TypeScript types for all public API -- untyped FFI wrappers are a non-starter
- Platform-specific DLL loading with dependency pre-loading -- foundation for everything
- String marshaling (null-terminated UTF-8 `Buffer.from` + `CString`) -- required for every C API call
- Read element IDs (`readElementIds`) -- list what elements exist

**Should have (competitive/idiomatic):**
- `transaction(fn)` callback wrapper -- auto-commit/rollback; idiomatic JS matching Dart/Julia/Lua
- Object-based `createElement`/`updateElement` (accept `{ label: "x", value: 42 }`) -- matches Python kwargs pattern
- `Symbol.dispose` support -- `using db = Database.fromSchema(...)` auto-close; TS 5.2+ pattern
- `version()` -- diagnostic utility
- `isHealthy()`, `path()`, `describe()` -- inspection methods

**Defer to v0.6+:**
- Metadata operations (struct unmarshaling complexity -- bun:ffi has no struct support)
- Composite reads (`readScalarsById`, `readVectorsById` -- depend on metadata)
- Time series operations (multi-column typed data; combines struct + nested array complexity)
- CSV import/export (out of scope per project decision)
- Blob subsystem (out of scope per project decision)
- Lua scripting (out of scope per project decision)

### Architecture Approach

The binding is a 6-file flat structure under `bindings/js/src/`. Each file has a single clear responsibility: `ffi.ts` declares all C symbols and loads libraries, `memory.ts` provides typed allocation helpers, `errors.ts` implements the `check()` pattern, `element.ts` wraps the Element builder, `database.ts` wraps the Database class, and `index.ts` is the barrel export. This mirrors the Dart and Python binding structure exactly.

See `.planning/research/ARCHITECTURE.md` for the full symbol declaration map, data flow diagrams, anti-patterns, and the suggested build order.

**Major components:**
1. `src/ffi.ts` -- `dlopen` with all symbol declarations; platform-specific library loading with dependency pre-loading
2. `src/memory.ts` -- Typed out-parameter helpers: `allocPtr()`, `allocI64()`, `allocF64()`, `allocI32()`, `readPtrFromBuf()`, `encodeString()`, `createDefaultOptions()`
3. `src/errors.ts` -- `QuiverError extends Error`; `check(err)` reads `quiver_get_last_error` immediately and throws
4. `src/element.ts` -- Element builder wrapping opaque `quiver_element_t*`; internal only (users pass plain JS objects)
5. `src/database.ts` -- Database class: factory methods, all CRUD/query/transaction operations, `Symbol.dispose`
6. `src/index.ts` -- Public barrel: exports `Database`, `QuiverError`, `version()`

### Critical Pitfalls

1. **Memory leaks from missing free calls** -- Every C API read allocates memory; `bun:ffi` has no GC integration. Use strict `try/finally` for every alloc/free pair. All 11 alloc/free pairs must be mapped before writing binding code.

2. **Pointer-to-pointer out-parameters** -- Factory functions (`from_schema`, `from_migrations`) use `quiver_database_t** out_db`. Must allocate an 8-byte buffer, pass its pointer, then use `read.ptr()` to extract the handle. Getting buffer size wrong causes silent memory corruption. Build typed `memory.ts` helpers first and test in isolation.

3. **`int64_t` / BigInt type mismatch** -- `i64` FFI type returns `BigInt`, not `number`. Pick one strategy and apply uniformly: use `i64` at the FFI boundary and immediately convert to `number` with `Number()` in every wrapper method. Never expose `BigInt` to the public API.

4. **Struct return-by-value (`quiver_database_options_default`)** -- `bun:ffi` cannot handle struct return by value. Do NOT call `quiver_database_options_default()` via FFI. Hardcode defaults in TypeScript: `read_only = 0`, `console_level = 4 (LOG_OFF)`.

5. **Windows DLL dependency chain** -- `libquiver_c.dll` depends on `libquiver.dll`. Must pre-load `libquiver.dll` with an empty `dlopen` call before loading `libquiver_c.dll`. Failure produces a cryptic "cannot load library" error.

## Implications for Roadmap

Based on research, suggested phase structure:

### Phase 1: FFI Foundation and Library Loading

**Rationale:** Every other feature depends on successfully loading the shared library and validating the pointer-to-pointer out-parameter pattern. This is the highest-risk pattern in `bun:ffi` and must be proven before any feature work. Pitfall #5 (DLL loading) and Pitfall #2 (pointer-to-pointer) are both critical-severity and foundational.

**Delivers:** Package skeleton, `ffi.ts` with minimal declarations (`version`, `get_last_error`, `from_schema`, `close`), `memory.ts` helpers, `errors.ts` with `check()`, `database.ts` with `fromSchema()` and `close()` only, first passing test (open/close cycle).

**Addresses features:** Database lifecycle, platform-specific DLL loading, error handling infrastructure.

**Avoids pitfalls:** #5 (Windows DLL chain), #2 (ptr-to-ptr), #6 (struct defaults), #14 (return-by-value options), #9 (error message race).

### Phase 2: Element Builder and CRUD

**Rationale:** CRUD operations are the core write path. The Element builder is internal (not public API) -- users pass plain JS objects. This phase introduces string array passing to C (Pitfall #4) and element lifecycle management (Pitfall #8). Building this as a self-contained internal implementation first keeps the user-facing API clean.

**Delivers:** `element.ts` (internal Element class wrapping opaque handle), `database.ts` extended with `createElement`, `updateElement`, `deleteElement`. Full CRUD round-trip tests.

**Addresses features:** Create element, update element, delete element, object-based API.

**Avoids pitfalls:** #8 (element lifecycle leak via try/finally), #4 (string arrays to C), #7 (use-after-close via `_isClosed` flag).

### Phase 3: Read Operations

**Rationale:** Read operations cover three distinct `bun:ffi` complexity levels: scalar by-ID (Pattern 2: simple out-params), bulk scalar reads (Pattern 3: allocated arrays), and vector/set by-ID and bulk (Pattern 4: nested pointer arrays). These must be tackled in order of increasing complexity. Memory cleanup discipline (Pitfall #1) is validated comprehensively in this phase.

**Delivers:** All scalar, vector, set read methods (by-ID and bulk, all types: integer, float, string), `readElementIds`, comprehensive read tests including memory leak checks.

**Addresses features:** Read scalar/vector/set by ID, read all scalars/vectors/sets, read element IDs.

**Avoids pitfalls:** #1 (memory leaks), #3 (BigInt conversion), #15 (null pointer guards on optional strings).

### Phase 4: Query and Transaction Operations

**Rationale:** Queries and transactions are independent of each other and of the read implementation, but logically come after CRUD is established. Parameterized queries introduce the typed-parameter encoding problem (parallel type/value arrays). Transactions compose directly from `begin`/`commit`/`rollback` primitives.

**Delivers:** All query methods (plain + parameterized: string, integer, float), transaction control (`beginTransaction`, `commit`, `rollback`, `inTransaction`), `transaction(fn)` callback wrapper.

**Addresses features:** Query operations, transaction control, transaction callback wrapper.

**Avoids pitfalls:** #4 (string parameter encoding for parameterized queries), #3 (BigInt for integer query results).

### Phase 5: Package Configuration and Polish

**Rationale:** Once all features are implemented and tested, the package infrastructure (npm metadata, TypeScript config, test scripts, public exports) is assembled. This is the last phase because distributing a broken package is worse than not distributing at all. `Symbol.dispose` and `version()` are stretch features that slot naturally here.

**Delivers:** `package.json` (name: `quiverdb`, engines: bun >= 1.3.0), `tsconfig.json`, `biome.json`, `src/index.ts` barrel export, `test/test.bat` and `test/test.sh`, `Symbol.dispose` on Database, `version()` function, full test suite pass, `bun pack` validation.

**Addresses features:** TypeScript type definitions for all public API, `Symbol.dispose`, `version()`, npm distribution.

**Avoids pitfalls:** #10 (npm package structure), #11 (Bun version pinning), #12 (bool/int type precision).

### Phase Ordering Rationale

- Phases are ordered by dependency: the FFI foundation must exist before any C function can be called; CRUD must exist before reads can be round-trip tested; reads must exist before queries make sense in context.
- Memory management patterns (#1) are established in Phase 1 (single pointer) and expanded in Phase 3 (allocated arrays). Every subsequent phase builds on proven patterns.
- Struct complexity (metadata operations, time series) is deliberately excluded from all 5 phases -- deferred to v0.6+ based on research findings that these require manual struct layout work that is fragile and error-prone in `bun:ffi`.
- The `element.ts` internal design (users never see Element; they pass plain objects) eliminates a whole category of lifecycle bugs and keeps the public API idiomatic JS.

### Research Flags

Phases likely needing deeper research during planning:
- **Phase 1:** The pointer-to-pointer out-parameter pattern for `bun:ffi` lacks an official guide. The `read.ptr()` usage on a TypedArray buffer needs empirical validation (Pitfall #2 is rated MEDIUM confidence). Recommend a spike/proof-of-concept for `quiver_database_from_schema` before committing to the broader architecture.
- **Phase 3 (nested pointer arrays):** `read_vector_integers` returns pointer-to-pointer (array of arrays). The pattern for reading nested C arrays via `bun:ffi` is inferred from docs, not demonstrated in official examples. Needs validation early in Phase 3 before committing to all vector/set reads.

Phases with well-documented, standard patterns (skip research-phase):
- **Phase 2 (Element/CRUD):** The Python binding uses an identical internal-only Element pattern. Patterns are proven and directly applicable.
- **Phase 4 (Queries/Transactions):** Transaction control is Pattern 1 (simple pointer-in, error-code-out). Query parameterization follows the same typed-array pattern established in Phase 1. No new `bun:ffi` patterns introduced.
- **Phase 5 (Package):** Standard Bun/npm package configuration. No implementation risk.

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | Official Bun documentation verified. `bun:ffi` type mappings confirmed against FFIType enum reference. tsconfig settings from `@tsconfig/bun` official package. |
| Features | MEDIUM-HIGH | Core features (CRUD, query, transaction) are straightforward C API wrappers. Complexity assessments for bun:ffi patterns are well-grounded. Struct/metadata deferral is a correct risk-reduction call. |
| Architecture | HIGH | 6-file structure is a direct projection of existing binding patterns (Dart, Python). Component boundaries are clear. The pointer-to-pointer pattern is the only MEDIUM-confidence item -- needs empirical validation in Phase 1. |
| Pitfalls | HIGH | All 6 critical pitfalls are independently confirmed by: (a) official bun:ffi documentation, (b) GitHub issue tracker reports, (c) existing binding code in this repo. The alloc/free pairing table is derived directly from the C API headers. |

**Overall confidence:** MEDIUM-HIGH

### Gaps to Address

- **Pointer-to-pointer out-parameter behavior:** `read.ptr()` on a TypedArray buffer for factory function output is not demonstrated in official examples. Build a minimal proof-of-concept for `quiver_database_from_schema` as the first action of Phase 1 before writing any other code. If this pattern does not work as documented, the architecture needs adjustment (e.g., using a `Uint8Array(8)` + `DataView` instead of `BigInt64Array`).

- **Nested array reading pattern:** `quiver_database_read_vector_integers` returns `int64_t**` (pointer to array of pointers). Reading this via `toArrayBuffer` + offset-based `read.ptr()` calls is inferred but not verified. Validate with a single concrete function early in Phase 3.

- **`usize` FFI type availability:** PITFALLS.md rates `usize` as MEDIUM confidence -- mentioned in search results but not verified in the current FFIType enum. If `usize` is not available, use `u64` for 64-bit-only targets or add a platform-width check. Confirm during Phase 1 when declaring size_t parameters.

- **npm binary distribution strategy:** For v0.5 development use, PATH-based library discovery is sufficient (matches Python/Dart test scripts). Production npm distribution of native binaries is explicitly deferred -- the `optionalDependencies` per-platform pattern is the recommended direction but has MEDIUM confidence (less established for Bun-specific packages).

## Sources

### Primary (HIGH confidence)

- [Bun FFI Documentation](https://bun.com/docs/runtime/ffi) -- dlopen, type mappings, CString, memory management
- [Bun FFI API Reference](https://bun.com/reference/bun/ffi) -- FFIType enum, read functions, module exports
- [Bun FFI CString Reference](https://bun.com/reference/bun/ffi/CString) -- null-terminated string handling, UTF-8/UTF-16 transcoding
- [Bun FFI read Reference](https://bun.com/reference/bun/ffi/read) -- pointer reading functions
- [Bun Test Runner](https://bun.com/docs/test) -- built-in test framework
- [Bun TypeScript Configuration](https://bun.com/docs/typescript) -- recommended tsconfig settings
- [@tsconfig/bun](https://www.npmjs.com/package/@tsconfig/bun) -- official base tsconfig
- [@types/bun](https://www.npmjs.com/package/@types/bun) -- Bun type definitions
- Project CLAUDE.md -- cross-layer naming conventions, architecture principles, C API contracts
- `bindings/dart/` -- proven FFI patterns (DLL loading, memory management, error handling)
- `bindings/python/` -- proven loading patterns (`_loader.py`), internal Element pattern
- `include/quiver/c/` -- C API headers (struct layouts, function signatures, alloc/free pairings)

### Secondary (MEDIUM confidence)

- [bun-ffi-gen (npm)](https://www.npmjs.com/package/bun-ffi-gen) -- evaluated and rejected; hand-written bindings preferred
- [TypeScript 5.2 Disposable pattern](https://www.typescriptlang.org/docs/handbook/release-notes/typescript-5-2.html) -- `Symbol.dispose` / `using` keyword
- [Sentry: Publishing binaries on npm](https://sentry.engineering/blog/publishing-binaries-on-npm) -- `optionalDependencies` pattern for native binaries
- [better-sqlite3 API design](https://github.com/WiseLibs/better-sqlite3) -- synchronous API precedent

### Tertiary (MEDIUM-LOW confidence)

- GitHub bun:ffi issues (#6709, #7582, #11598, #16131, #27664) -- stability concerns, documented crashes, pointer reading edge cases; informed the risk assessment for Pitfall #11

---
*Research completed: 2026-03-08*
*Ready for roadmap: yes*
