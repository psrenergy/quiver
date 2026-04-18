# Roadmap: Quiver JS Binding

## Milestones

- **v1.0 Deno FFI Migration** - Phases 1-7 (shipped)
- **v1.1 JSR Publishing & CI** - Phases 8-9 (shipped)
- **v1.2 Native Library Bundling** - Phase 10 (in progress)

## Phases

<details>
<summary>v1.0 Deno FFI Migration (Phases 1-7) - SHIPPED</summary>

- [x] **Phase 1: FFI Foundation & Library Loading** - Replace koffi.load with Deno.dlopen and define all C API symbols using Deno FFI type descriptors
- [x] **Phase 2: Pointer & String Marshaling** - Rewrite pointer out-parameters, string encoding, and native memory allocation for Deno FFI
- [x] **Phase 3: Array Decoding & Domain Helpers** - Rewrite array decoders and convert domain modules with direct koffi usage (query, csv, time-series)
- [x] **Phase 4: Struct Marshaling & Indirect Modules** - Rewrite struct field reading for metadata and update all indirect domain modules
- [x] **Phase 5: Configuration & Packaging** - Migrate package config to deno.json and update TypeScript configuration
- [x] **Phase 6: Test Migration & Validation** - Migrate test runner to Deno.test and validate all existing scenarios pass
- [x] **Phase 7: Documentation** - Update README with Deno instructions and create MIGRATION.md

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
**Plans:** 1 plan

Plans:
- [x] 07-01-PLAN.md -- Rewrite README.md for Deno usage and create MIGRATION.md documenting koffi-to-Deno.dlopen transition

</details>

<details>
<summary>v1.1 JSR Publishing & CI (Phases 8-9) - SHIPPED</summary>

- [x] **Phase 8: JSR Package Configuration** - Configure deno.json for JSR and create mod.ts entry point
- [x] **Phase 9: CI Pipeline** - Add Deno test job and JSR publish workflow to GitHub Actions

### Phase 8: JSR Package Configuration
**Goal**: The JS binding is published as @psrenergy/quiver on jsr.io
**Depends on**: Nothing (builds on completed v1.0)
**Requirements**: JSR-01, JSR-02, JSR-03
**Success Criteria** (what must be TRUE):
  1. deno.json contains name: @psrenergy/quiver, version: 0.1.0, exports: ./mod.ts
  2. mod.ts exists and re-exports the public API from src/index.ts
  3. `npx jsr publish --dry-run` succeeds locally (package structure is valid for JSR)
**Plans**: 1 plan

Plans:
- [x] 08-01-PLAN.md -- Update deno.json with JSR metadata, create mod.ts entry point, validate with dry-run publish

### Phase 9: CI Pipeline
**Goal**: CI tests and publishes the Deno binding automatically
**Depends on**: Phase 8
**Requirements**: CI-01, CI-02, CI-03
**Success Criteria** (what must be TRUE):
  1. Push/PR to the repo triggers a Deno test job that runs JS binding tests
  2. publish.yml uses npx jsr publish with OIDC authentication (no manual tokens)
  3. No package.json, npm install, npm build, or npm publish references remain in CI workflow files
  4. The ci.yml Deno test job passes in GitHub Actions
**Plans**: 1 plan

Plans:
- [x] 09-01-PLAN.md -- Add Deno test job to ci.yml and replace npm publish with JSR publish in publish.yml

</details>

### v1.2 Native Library Bundling (In Progress)

**Milestone Goal:** Bundle pre-built native libraries in the JSR package so consumers get working binaries without building C++

- [ ] **Phase 10: Native Library Build & Bundle** - Build native libraries in CI and bundle them in the JSR package for Linux x64 and Windows x64

## Phase Details

### Phase 10: Native Library Build & Bundle
**Goal**: Consumers importing @psrenergy/quiver from JSR get pre-built native libraries that load automatically
**Depends on**: Phase 9 (publish workflow exists)
**Requirements**: LIB-01, LIB-02, LIB-03, CI-05, CI-06
**Success Criteria** (what must be TRUE):
  1. publish.yml builds libquiver and libquiver_c for Linux x64 and Windows x64 before running `npx jsr publish`
  2. Native library builds use Release mode CMake with QUIVER_BUILD_C_API=ON
  3. The published JSR package contains native binaries under `libs/linux-x86_64/` and `libs/windows-x86_64/`
  4. The loader's Tier 1 search (`libs/{os}-{arch}/`) resolves the bundled libraries when the package is imported via `jsr:@psrenergy/quiver`
**Plans**: 1 plan

Plans:
- [ ] 10-01-PLAN.md -- Add build-native CI job and wire publish-jsr to bundle native libraries for Linux x86_64 and Windows x86_64

## Progress

**Execution Order:**
Phase 10

| Phase | Milestone | Plans Complete | Status | Completed |
|-------|-----------|----------------|--------|-----------|
| 1. FFI Foundation & Library Loading | v1.0 | 1/1 | Complete | - |
| 2. Pointer & String Marshaling | v1.0 | 1/1 | Complete | - |
| 3. Array Decoding & Domain Helpers | v1.0 | 1/1 | Complete | - |
| 4. Struct Marshaling & Indirect Modules | v1.0 | 2/2 | Complete | - |
| 5. Configuration & Packaging | v1.0 | 1/1 | Complete | - |
| 6. Test Migration & Validation | v1.0 | 2/2 | Complete | - |
| 7. Documentation | v1.0 | 1/1 | Complete | - |
| 8. JSR Package Configuration | v1.1 | 1/1 | Complete | - |
| 9. CI Pipeline | v1.1 | 1/1 | Complete | - |
| 10. Native Library Build & Bundle | v1.2 | 0/1 | Not started | - |
