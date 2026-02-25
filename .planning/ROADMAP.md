# Roadmap: Quiver v0.5 PyPI Distribution

## Overview

Transform the existing Python binding (`quiverdb`) from a development-only package into a self-contained PyPI distribution. The journey starts with switching the build backend to scikit-build-core so CMake compiles and bundles native libraries into the wheel, then rewrites the loader to discover bundled libraries, then automates cross-platform wheel building in CI, and finally wires up automated PyPI publishing on tagged releases.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [ ] **Phase 1: Build System Migration** - Replace hatchling with scikit-build-core and add CMake install targets for native library bundling
- [ ] **Phase 2: Loader Rewrite** - Rewrite _loader.py to discover bundled native libraries and fall back to development mode
- [ ] **Phase 3: CI Wheel Building** - GitHub Actions workflow builds and tests platform-specific wheels via cibuildwheel
- [ ] **Phase 4: PyPI Publishing** - Automated trusted publisher release to PyPI on tagged commits

## Phase Details

### Phase 1: Build System Migration
**Goal**: Building a wheel produces a self-contained package with native libraries bundled inside
**Depends on**: Nothing (first phase)
**Requirements**: BUILD-01, BUILD-02
**Success Criteria** (what must be TRUE):
  1. `pyproject.toml` declares scikit-build-core as the build backend and the project builds without hatchling
  2. Running `pip wheel .` in the bindings/python directory produces a `.whl` file containing `quiverdb/_libs/libquiver` and `quiverdb/_libs/libquiver_c` (platform-appropriate extensions)
  3. The CMake install targets only activate when the `SKBUILD` variable is set (normal CMake builds are unaffected)
  4. Existing Python tests still pass after the build backend switch (no functional regression)
**Plans**: TBD

Plans:
- [ ] 01-01: TBD

### Phase 2: Loader Rewrite
**Goal**: Python code discovers and loads bundled native libraries from an installed wheel, with fallback for development
**Depends on**: Phase 1
**Requirements**: LOAD-01, LOAD-02, LOAD-03
**Success Criteria** (what must be TRUE):
  1. When installed from a wheel, `import quiverdb` loads native libraries from the `_libs/` subdirectory without any PATH configuration
  2. When running in development mode (no bundled libs), `_loader.py` falls back to finding libraries via PATH (existing behavior preserved)
  3. On Windows, `os.add_dll_directory()` is called for the bundled lib directory so `libquiver_c.dll` resolves its `libquiver.dll` dependency
  4. Installing the wheel in a clean virtual environment and running the full test suite passes (end-to-end local validation)
**Plans**: TBD

Plans:
- [ ] 02-01: TBD

### Phase 3: CI Wheel Building
**Goal**: GitHub Actions automatically builds and tests correct wheels for both target platforms on every push
**Depends on**: Phase 2
**Requirements**: CI-01, CI-02, CI-03
**Success Criteria** (what must be TRUE):
  1. A GitHub Actions workflow triggers on push and builds wheels using cibuildwheel
  2. Wheels are produced for Windows x64 (win_amd64) and Linux x64 (manylinux_2_28_x86_64)
  3. cibuildwheel runs the test suite inside a clean environment for each platform before the wheel is considered successful
  4. Built wheels are uploaded as workflow artifacts and downloadable from the Actions UI
**Plans**: TBD

Plans:
- [ ] 03-01: TBD

### Phase 4: PyPI Publishing
**Goal**: Tagged releases automatically publish tested wheels to PyPI with no manual intervention
**Depends on**: Phase 3
**Requirements**: CI-04
**Success Criteria** (what must be TRUE):
  1. Pushing a version tag (e.g., `v0.5.0`) triggers the publish workflow
  2. Publishing uses PyPI trusted publisher (OIDC) -- no API tokens stored in repository secrets
  3. `pip install quiverdb` on a clean machine (Windows or Linux) installs the package and `import quiverdb` works without any system-level setup
**Plans**: TBD

Plans:
- [ ] 04-01: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1 -> 2 -> 3 -> 4

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Build System Migration | 0/0 | Not started | - |
| 2. Loader Rewrite | 0/0 | Not started | - |
| 3. CI Wheel Building | 0/0 | Not started | - |
| 4. PyPI Publishing | 0/0 | Not started | - |
