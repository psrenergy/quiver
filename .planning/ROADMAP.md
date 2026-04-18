# Roadmap: Quiver Deno FFI Migration

## Overview

Migrate the JS binding from koffi (Node.js FFI) to Deno.dlopen while preserving the public API surface. The migration proceeds bottom-up: replace the library loader and symbol definitions first, then rewrite data marshaling helpers, convert domain modules that use koffi directly, update indirect modules, reconfigure packaging for Deno, validate with tests, and document the transition.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [ ] **Phase 1: FFI Foundation & Library Loading** - Replace koffi.load with Deno.dlopen and define all C API symbols using Deno FFI type descriptors
- [ ] **Phase 2: Pointer & String Marshaling** - Rewrite pointer out-parameters, string encoding, and native memory allocation for Deno FFI
- [ ] **Phase 3: Array Decoding & Domain Helpers** - Rewrite array decoders and convert domain modules with direct koffi usage (query, csv, time-series)
- [ ] **Phase 4: Struct Marshaling & Indirect Modules** - Rewrite struct field reading for metadata and update all indirect domain modules
- [ ] **Phase 5: Configuration & Packaging** - Migrate package config to deno.json and update TypeScript configuration
- [ ] **Phase 6: Test Migration & Validation** - Migrate test runner to Deno.test and validate all existing scenarios pass
- [ ] **Phase 7: Documentation** - Update README with Deno instructions and create MIGRATION.md

## Phase Details

### Phase 1: FFI Foundation & Library Loading
**Goal**: The native library loads via Deno.dlopen with all C API symbols available and no Node.js-specific imports
**Depends on**: Nothing (first phase)
**Requirements**: FFI-01, FFI-02, FFI-03, CONF-02
**Success Criteria** (what must be TRUE):
  1. Calling loadLibrary() opens libquiver_c via Deno.dlopen and returns a symbol object with all C API functions
  2. Symbol type descriptors correctly map every used C type (int, int64_t, uint64_t, double, pointer, const char**) to Deno FFI equivalents
  3. Library discovery works from bundled libs directory, dev build/bin walk-up, and system PATH without any node:fs/node:path/node:url imports
  4. No koffi import or node: protocol import remains in loader.ts
**Plans:** 1 plan

Plans:
- [x] 01-01-PLAN.md -- Rewrite loader.ts with Deno.dlopen, 85 symbol definitions, and 3-tier library search

### Phase 2: Pointer & String Marshaling
**Goal**: All pointer out-parameter, string marshaling, and native allocation helpers work using Deno FFI primitives
**Depends on**: Phase 1
**Requirements**: MARSH-01, MARSH-03, MARSH-04
**Success Criteria** (what must be TRUE):
  1. allocPtrOut/readPtrOut work using BigInt (Deno.UnsafePointer) instead of koffi Buffer/decode
  2. String encoding uses TextEncoder to produce null-terminated C strings and TextDecoder to read them back
  3. allocNativeInt64, allocNativeFloat64, allocNativeString, allocNativeStringArray, and allocNativePtrTable produce valid native memory using Deno FFI primitives (no koffi.alloc/encode)
  4. No koffi import remains in ffi-helpers.ts
**Plans:** 1 plan

Plans:
- [x] 02-01-PLAN.md -- Rewrite ffi-helpers.ts with Deno FFI primitives, add Allocation type, fix errors.ts pointer decoding

### Phase 3: Array Decoding & Domain Helpers
**Goal**: Array decoding works via UnsafePointerView and all domain modules with direct koffi usage are converted
**Depends on**: Phase 2
**Requirements**: MARSH-02
**Success Criteria** (what must be TRUE):
  1. decodeInt64Array, decodeFloat64Array, decodeUint64Array, decodeStringArray, and decodePtrArray read native memory using Deno.UnsafePointerView instead of koffi.decode
  2. query.ts parameterized query marshaling (marshalParams) works without koffi using the new allocation helpers
  3. csv.ts CSV options struct building (buildCsvOptionsBuffer) works without koffi.alloc/encode
  4. time-series.ts columnar read/write works without koffi.decode for type arrays and data pointers
  5. No koffi import remains in query.ts, csv.ts, or time-series.ts
**Plans:** 1 plan

Plans:
- [x] 03-01-PLAN.md -- Implement 5 array decoders in ffi-helpers.ts and remove koffi from query.ts, csv.ts, time-series.ts

### Phase 4: Struct Marshaling & Indirect Modules
**Goal**: Struct field reading at byte offsets works for metadata types and all remaining modules compile and function under Deno
**Depends on**: Phase 3
**Requirements**: MARSH-05
**Success Criteria** (what must be TRUE):
  1. ScalarMetadata struct fields (name, dataType, notNull, primaryKey, defaultValue, isForeignKey, references) are read at correct byte offsets using Deno.UnsafePointerView instead of koffi.decode
  2. GroupMetadata struct fields (groupName, dimensionColumn, valueColumns pointer, valueColumnCount) are read correctly including nested ScalarMetadata arrays
  3. database.ts, create.ts, read.ts, transaction.ts, introspection.ts, composites.ts, lua-runner.ts, errors.ts, types.ts, and index.ts have no Buffer references and use Deno-compatible types throughout
  4. No koffi import remains anywhere in bindings/js/src/
**Plans:** 2 plans

Plans:
- [x] 04-01-PLAN.md -- Rewrite metadata.ts struct readers from koffi to UnsafePointerView at verified byte offsets
- [x] 04-02-PLAN.md -- Fix Deno FFI compatibility in indirect modules and .js to .ts import extensions across all files

### Phase 5: Configuration & Packaging
**Goal**: The binding is packaged as a Deno module with no Node.js tooling remnants
**Depends on**: Phase 4
**Requirements**: CONF-01, CONF-03
**Success Criteria** (what must be TRUE):
  1. deno.json exists with correct module entry point, tasks, and no koffi dependency
  2. package.json is removed or replaced by deno.json
  3. tsconfig.json is removed (Deno handles TypeScript natively) or replaced with minimal Deno-compatible config if needed
**Plans:** 1 plan

Plans:
- [x] 05-01-PLAN.md -- Create deno.json, delete Node.js config files, update test.bat to deno test, regenerate deno.lock

### Phase 6: Test Migration & Validation
**Goal**: All existing test scenarios pass under Deno runtime with Deno.test
**Depends on**: Phase 5
**Requirements**: TEST-01, TEST-02
**Success Criteria** (what must be TRUE):
  1. Test runner uses Deno.test (or deno test CLI) instead of vitest
  2. All existing test scenarios (database lifecycle, CRUD, reads, queries, metadata, time series, CSV, transactions, composites, introspection, Lua runner) pass
  3. Tests run with deno test --allow-ffi --allow-read --allow-write and produce clear pass/fail output
**Plans:** 2 plans

Plans:
- [x] 06-01-PLAN.md -- Migrate 6 pure-database test files (create, read, update, query, transaction, introspection) from vitest to Deno.test
- [x] 06-02-PLAN.md -- Migrate 6 remaining test files (database, csv, metadata, time-series, composites, lua-runner) and validate full suite

### Phase 7: Documentation
**Goal**: Users know how to run the Deno binding and understand what changed from the koffi version
**Depends on**: Phase 6
**Requirements**: DOCS-01, DOCS-02
**Success Criteria** (what must be TRUE):
  1. README includes deno run/test commands with --allow-ffi flag and explains the permission model
  2. MIGRATION.md documents what changed (koffi to Deno.dlopen), why (path-scoped permissions, native Deno support), and any breaking changes to developer workflow (not public API)
**Plans**: TBD

Plans:
- [ ] 07-01: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1 -> 2 -> 3 -> 4 -> 5 -> 6 -> 7

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. FFI Foundation & Library Loading | 0/1 | Planning complete | - |
| 2. Pointer & String Marshaling | 0/1 | Planning complete | - |
| 3. Array Decoding & Domain Helpers | 0/1 | Planning complete | - |
| 4. Struct Marshaling & Indirect Modules | 0/2 | Planning complete | - |
| 5. Configuration & Packaging | 0/1 | Planning complete | - |
| 6. Test Migration & Validation | 0/2 | Planning complete | - |
| 7. Documentation | 0/0 | Not started | - |
