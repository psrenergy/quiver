# Requirements: Quiver JS Binding

**Defined:** 2026-04-18
**Core Value:** Uniform, type-safe database access across multiple languages through a single C++ implementation with thin FFI bindings.

## v1.2 Requirements

Requirements for native library bundling in JSR package. Each maps to roadmap phases.

### Native Library Packaging

- [ ] **LIB-01**: Pre-built native libraries (libquiver + libquiver_c) for Linux x64 and Windows x64 are included in the published JSR package under `libs/{os}-{arch}/`
- [ ] **LIB-02**: `deno.json` `publish.include` includes `libs/**/*` so binaries ship with the package
- [ ] **LIB-03**: The loader's Tier 1 search (`libs/{os}-{arch}/`) resolves correctly when the package is imported via `jsr:@psrenergy/quiver`

### CI Pipeline

- [ ] **CI-05**: `publish.yml` builds native libraries for Linux x64 and Windows x64 before running `npx jsr publish`
- [ ] **CI-06**: Native library build uses Release mode CMake with `QUIVER_BUILD_C_API=ON`

## v1.1 Requirements (Complete)

### JSR Publishing

- [x] **JSR-01**: deno.json configured with JSR metadata (name: @psrenergy/quiver, version: 0.1.0, license: MIT, exports: ./mod.ts)
- [x] **JSR-02**: mod.ts entry point re-exports public API from src/index.ts
- [x] **JSR-03**: `npx jsr publish --dry-run` validates the package structure for JSR

### CI/CD

- [x] **CI-01**: Deno test job runs JS binding tests in ci.yml on push/PR
- [x] **CI-02**: publish.yml replaced with JSR publish workflow (OIDC auth, triggered on push to main)
- [x] **CI-03**: Old npm publishing logic removed (no package.json references, no npm install/build/publish)

## Future Requirements

Deferred to future release. Tracked but not in current roadmap.

- **CI-04**: CI runs JS binding tests on multiple platforms (Linux, macOS, Windows)
- **JSR-04**: Automated version bump workflow
- **LIB-04**: macOS ARM64 and x64 native library support
- **LIB-05**: Multi-arch native library support (ARM64)

## Out of Scope

| Feature | Reason |
|---------|--------|
| npm publishing | Binding is Deno-only, no Node.js compatibility |
| macOS native libs | Deferred to future milestone |
| ARM64 native libs | Deferred to future milestone |
| Separate native library package | Single JSR package with bundled binaries is simpler |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| LIB-01 | TBD | Pending |
| LIB-02 | TBD | Pending |
| LIB-03 | TBD | Pending |
| CI-05 | TBD | Pending |
| CI-06 | TBD | Pending |
| JSR-01 | Phase 8 | Complete |
| JSR-02 | Phase 8 | Complete |
| JSR-03 | Phase 8 | Complete |
| CI-01 | Phase 9 | Complete |
| CI-02 | Phase 9 | Complete |
| CI-03 | Phase 9 | Complete |

**Coverage:**
- v1.2 requirements: 5 total
- Mapped to phases: 0 (pending roadmap)
- Unmapped: 5

---
*Requirements defined: 2026-04-18*
*Last updated: 2026-04-18 after milestone v1.2 start*
