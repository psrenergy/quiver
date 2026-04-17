# Requirements: Quiver Deno FFI Migration

**Defined:** 2026-04-17
**Core Value:** Uniform, type-safe database access across multiple languages through a single C++ implementation with thin FFI bindings.

## v1 Requirements

Requirements for Deno FFI migration. Each maps to roadmap phases.

### FFI Foundation

- [ ] **FFI-01**: Library loads via Deno.dlopen with all C API symbols defined using Deno FFI type descriptors
- [ ] **FFI-02**: Symbol type mapping covers all used koffi types (int, int64_t, uint64_t, double, str, void*, const char**)
- [ ] **FFI-03**: Library search paths work for bundled libs, dev build/bin, and system PATH

### Data Marshaling

- [ ] **MARSH-01**: Pointer out-parameters work using BigInt (Deno.UnsafePointer) instead of koffi Buffer
- [ ] **MARSH-02**: Integer/float array decoding works using Deno.UnsafePointerView instead of koffi.decode
- [ ] **MARSH-03**: String marshaling uses TextEncoder/TextDecoder for C string encoding/decoding
- [ ] **MARSH-04**: Native memory allocation (int64, float64, string, pointer tables) works using Deno FFI primitives
- [ ] **MARSH-05**: Struct field reading at byte offsets works for metadata (ScalarMetadata, GroupMetadata)

### Configuration

- [ ] **CONF-01**: Package config migrated from package.json to deno.json with koffi dependency removed
- [ ] **CONF-02**: All node: protocol imports removed (node:fs, node:path, node:url) and replaced with Deno equivalents
- [ ] **CONF-03**: TypeScript config updated for Deno (no separate tsconfig needed)

### Testing

- [ ] **TEST-01**: Existing test scenarios pass under Deno runtime
- [ ] **TEST-02**: Test runner migrated from vitest to Deno.test (or compatible runner)

### Documentation

- [ ] **DOCS-01**: README updated with Deno run command including --allow-ffi flag
- [ ] **DOCS-02**: MIGRATION.md created explaining what changed and the new permission model

## v2 Requirements

Deferred to future release. Tracked but not in current roadmap.

### Binary Subsystem

- **BIN-01**: BinaryFile operations bound through Deno FFI
- **BIN-02**: CSVConverter bound through Deno FFI
- **BIN-03**: BinaryMetadata bound through Deno FFI

## Out of Scope

| Feature | Reason |
|---------|--------|
| Multi-runtime support (Node.js, Bun) | Dropping in favor of Deno-only; koffi removal is intentional |
| New API methods | Public JS API surface stays identical |
| WASM binding changes | Separate package (js-wasm), not affected |
| UnsafePointer in public API | Security constraint: no unsafe Deno FFI types exposed to consumers |
| Browser support | Deno.dlopen is Deno-only, no browser FFI |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| FFI-01 | Phase 1 | Pending |
| FFI-02 | Phase 1 | Pending |
| FFI-03 | Phase 1 | Pending |
| MARSH-01 | Phase 2 | Pending |
| MARSH-02 | Phase 3 | Pending |
| MARSH-03 | Phase 2 | Pending |
| MARSH-04 | Phase 2 | Pending |
| MARSH-05 | Phase 4 | Pending |
| CONF-01 | Phase 5 | Pending |
| CONF-02 | Phase 1 | Pending |
| CONF-03 | Phase 5 | Pending |
| TEST-01 | Phase 6 | Pending |
| TEST-02 | Phase 6 | Pending |
| DOCS-01 | Phase 7 | Pending |
| DOCS-02 | Phase 7 | Pending |

**Coverage:**
- v1 requirements: 15 total
- Mapped to phases: 15
- Unmapped: 0

---
*Requirements defined: 2026-04-17*
*Last updated: 2026-04-17 after roadmap creation*
