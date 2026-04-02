# Requirements: Quiver JS Bindings — Deno Compatibility

**Defined:** 2026-04-01
**Core Value:** The JS binding must load and call the Quiver C API correctly in both Bun and Deno runtimes

## v1.0 Requirements

Requirements for Deno compatibility. Each maps to roadmap phases.

### FFI Abstraction

- [ ] **ABST-01**: Opaque handle type replaces Pointer across all source files
- [ ] **ABST-02**: Abstract library loader (dlopen + symbol definitions)
- [ ] **ABST-03**: Abstract pointer operations (alloc out-param, read pointer, read at offset)
- [ ] **ABST-04**: Abstract string encoding (toCString) and decoding (CString/getCString)
- [ ] **ABST-05**: Abstract buffer operations (ptr(), toArrayBuffer equivalents)

### Runtime Adapters

- [ ] **RTAD-01**: Bun adapter wrapping bun:ffi preserving current behavior
- [ ] **RTAD-02**: Deno adapter wrapping Deno.dlopen/UnsafePointer/UnsafePointerView
- [ ] **RTAD-03**: All 14 source files migrated to use abstraction instead of bun:ffi

### Packaging & Testing

- [ ] **PACK-01**: package.json exports map with bun/deno conditions
- [ ] **PACK-02**: Deno test suite covering core operations (lifecycle, CRUD, read, query, CSV)
- [ ] **PACK-03**: Existing Bun tests pass unchanged

## Future Requirements

(None identified)

## Out of Scope

| Feature | Reason |
|---------|--------|
| Node.js support | Node.js N-API is a fundamentally different FFI model |
| Binary subsystem JS bindings | Not yet bound in JS, separate milestone |
| Runtime auto-detection | Exports map conditions are cleaner than typeof checks |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| ABST-01 | Phase 1 | Pending |
| ABST-02 | Phase 1 | Pending |
| ABST-03 | Phase 2 | Pending |
| ABST-04 | Phase 2 | Pending |
| ABST-05 | Phase 2 | Pending |
| RTAD-01 | Phase 3 | Pending |
| RTAD-02 | Phase 3 | Pending |
| RTAD-03 | Phase 3 | Pending |
| PACK-01 | Phase 4 | Pending |
| PACK-02 | Phase 4 | Pending |
| PACK-03 | Phase 4 | Pending |

**Coverage:**
- v1.0 requirements: 11 total
- Mapped to phases: 11
- Unmapped: 0

---
*Requirements defined: 2026-04-01*
*Last updated: 2026-04-01 after roadmap creation*
