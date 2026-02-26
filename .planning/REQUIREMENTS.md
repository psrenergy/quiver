# Requirements: Quiver — PyPI Distribution

**Defined:** 2026-02-25
**Core Value:** `pip install quiverdb` is self-contained with bundled native libraries

## v0.5 Requirements

Requirements for PyPI distribution milestone. Each maps to roadmap phases.

### Build System

- [x] **BUILD-01**: pyproject.toml uses scikit-build-core as build backend (replaces hatchling)
- [x] **BUILD-02**: CMake install targets place libquiver and libquiver_c into wheel package directory (SKBUILD-guarded)

### Loader

- [x] **LOAD-01**: `_loader.py` discovers bundled native libs from `_libs/` subdirectory relative to package
- [x] **LOAD-02**: `_loader.py` falls back to development PATH discovery when bundled libs not found
- [x] **LOAD-03**: Windows `os.add_dll_directory()` registers bundled lib directory for transitive DLL resolution

### CI/CD

- [x] **CI-01**: GitHub Actions workflow builds platform-specific wheels via cibuildwheel
- [x] **CI-02**: Wheels built for Windows x64 and Linux x64 (manylinux_2_28)
- [x] **CI-03**: Wheels tested in clean environment inside CI before publishing
- [ ] **CI-04**: PyPI trusted publisher (OIDC) publishes wheels on tagged release

## Future Requirements

### Distribution

- **DIST-01**: macOS wheels (arm64 and x64)
- **DIST-02**: Source distribution (sdist) support
- **DIST-03**: Python 3.12 support (lower minimum version)

## Out of Scope

| Feature | Reason |
|---------|--------|
| macOS wheels | Defer to future milestone — ship Windows + Linux first |
| Source distribution (sdist) | Wheels-only for native-bundled package |
| TestPyPI staging | Publish directly to real PyPI per user decision |
| Python < 3.13 | Current binding requires 3.13+ |
| CFFI API-mode migration | ABI-mode works, no compiler needed at install time |
| Dynamic versioning | Keep version in pyproject.toml, sync manually |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| BUILD-01 | Phase 1 | Complete |
| BUILD-02 | Phase 1 | Complete |
| LOAD-01 | Phase 2 | Complete |
| LOAD-02 | Phase 2 | Complete |
| LOAD-03 | Phase 2 | Complete |
| CI-01 | Phase 3 | Complete |
| CI-02 | Phase 3 | Complete |
| CI-03 | Phase 3 | Complete |
| CI-04 | Phase 4 | Pending |

**Coverage:**
- v0.5 requirements: 9 total
- Mapped to phases: 9
- Unmapped: 0

---
*Requirements defined: 2026-02-25*
*Last updated: 2026-02-25 after roadmap creation*
