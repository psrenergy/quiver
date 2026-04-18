# Requirements: Quiver JS Binding

**Defined:** 2026-04-18
**Core Value:** Uniform, type-safe database access across multiple languages through a single C++ implementation with thin FFI bindings.

## v1.1 Requirements

Requirements for JSR publishing and CI pipeline. Each maps to roadmap phases.

### JSR Publishing

- [ ] **JSR-01**: deno.json configured with JSR metadata (name: @psrenergy/quiver, version: 0.1.0, license: MIT, exports: ./mod.ts)
- [ ] **JSR-02**: mod.ts entry point re-exports public API from src/index.ts
- [ ] **JSR-03**: Package publishes successfully to jsr.io via `npx jsr publish`

### CI/CD

- [ ] **CI-01**: Deno test job runs JS binding tests in ci.yml on push/PR
- [ ] **CI-02**: publish.yml replaced with JSR publish workflow (OIDC auth, triggered on push to main)
- [ ] **CI-03**: Old npm publishing logic removed (no package.json references, no npm install/build/publish)

## v2 Requirements

Deferred to future release. Tracked but not in current roadmap.

- **CI-04**: CI runs JS binding tests on multiple platforms (Linux, macOS, Windows)
- **JSR-04**: Automated version bump workflow

## Out of Scope

| Feature | Reason |
|---------|--------|
| npm publishing | Binding is Deno-only, no Node.js compatibility |
| Dual npm+JSR publishing | Unnecessary complexity, single registry is sufficient |
| Native library bundling in JSR package | JSR distributes TypeScript source; native libs are built separately |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| JSR-01 | Phase 8 | Pending |
| JSR-02 | Phase 8 | Pending |
| JSR-03 | Phase 8 | Pending |
| CI-01 | Phase 9 | Pending |
| CI-02 | Phase 9 | Pending |
| CI-03 | Phase 9 | Pending |

**Coverage:**
- v1.1 requirements: 6 total
- Mapped to phases: 6
- Unmapped: 0

---
*Requirements defined: 2026-04-18*
*Last updated: 2026-04-18 after roadmap creation*
