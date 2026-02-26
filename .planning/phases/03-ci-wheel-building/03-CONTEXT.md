# Phase 3: CI Wheel Building - Context

**Gathered:** 2026-02-25
**Status:** Ready for planning

<domain>
## Phase Boundary

GitHub Actions workflow that automatically builds and tests platform-specific Python wheels (Windows x64 + Linux x64) on every push to main and on pull requests. Built wheels are uploaded as downloadable artifacts. Publishing to PyPI is Phase 4.

</domain>

<decisions>
## Implementation Decisions

### Workflow triggers
- Triggers on push to main/master and on pull requests
- Tag-triggered builds deferred to Phase 4 (PyPI publish workflow)
- Full build matrix (both platforms) on every trigger — no partial builds
- Single workflow file: `.github/workflows/build-wheels.yml`
- Workflow name in Actions UI: "Build Wheels"

### Python versions
- CPython 3.13 only (matches `requires-python >= 3.13` in pyproject.toml)
- No PyPy builds
- manylinux_2_28_x86_64 for Linux (no musllinux/Alpine)
- win_amd64 for Windows

### Test scope in CI
- Full Python test suite (all 203 pytest tests) run inside cibuildwheel for each platform
- No separate C++ or C API test jobs — native libs are validated through the Python tests
- If any platform fails tests, the entire workflow fails (no partial artifact upload)

### Artifact handling
- All platform wheels bundled into a single "wheels" artifact
- 7-day retention period
- Phase 4 publish workflow will consume this artifact

### Claude's Discretion
- Test environment setup (temp directories, schema file access in cibuildwheel containers)
- cibuildwheel configuration details (pyproject.toml vs env vars)
- Exact manylinux container image and build dependencies

</decisions>

<specifics>
## Specific Ideas

- `UV_PUBLISH_TOKEN` GitHub secret is already configured (for Phase 4 use)
- cibuildwheel should handle the scikit-build-core CMake compilation inside containers
- Tests must run against the installed wheel (not source), proving bundled `_libs/` discovery works

</specifics>

<deferred>
## Deferred Ideas

- Tag-triggered publish workflow — Phase 4 (CI-04)
- macOS wheels — future milestone (DIST-01)
- musllinux/Alpine wheels — future if needed
- PyPy support — future if needed
- C++ / C API test jobs in CI — consider for a future CI hardening phase

</deferred>

---

*Phase: 03-ci-wheel-building*
*Context gathered: 2026-02-25*
