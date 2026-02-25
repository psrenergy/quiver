# Phase 1: Build System Migration - Context

**Gathered:** 2026-02-25
**Status:** Ready for planning

<domain>
## Phase Boundary

Replace hatchling with scikit-build-core as the Python build backend and add CMake install targets so that `pip wheel .` produces a self-contained `.whl` with `libquiver` and `libquiver_c` bundled inside. Normal CMake builds remain unaffected (SKBUILD guard). Existing Python tests pass after migration.

</domain>

<decisions>
## Implementation Decisions

### pyproject.toml Location
- Keep pyproject.toml in `bindings/python/` (not repo root)
- Use scikit-build-core's `cmake.source-dir` to point to repo root for CMake access
- `pip wheel .` is run from `bindings/python/` — no convenience wrapper at root
- Consistent with other bindings (Julia, Dart) each having their own subdirectory

### Native Library Layout
- Flat `_libs/` directory: `quiverdb/_libs/libquiver.dll` + `quiverdb/_libs/libquiver_c.dll` (or `.so`)
- Both libraries side by side, no subdirectory structure

### SKBUILD Guard
- Guard logic lives in root `CMakeLists.txt` as an `if(SKBUILD)` block at the end
- Inside the guard: force `QUIVER_BUILD_C_API=ON` (auto-enables C API for wheel builds)
- Inside the guard: `install()` targets for both native libraries into the wheel package

### Package Metadata
- Minimal metadata only — just enough for the build to work
- No PyPI presentation metadata (description, classifiers, readme, URLs) until Phase 4
- Keep version at 0.4.0 — no bump until milestone is release-ready
- Dev dependencies (dotenv, pytest, ruff) stay as-is

### Build Configuration Style
- Minimal `[tool.scikit-build]` in pyproject.toml: `cmake.source-dir`, `wheel.packages`, `cmake.build-type = "Release"`
- CMake does the heavy lifting — install targets, target selection, SKBUILD logic
- Build only library targets (libquiver + libquiver_c) for wheel — no tests, no benchmarks
- Python source stays in existing `src/quiverdb/` layout

### Development Workflow
- Keep existing `test.bat` approach for day-to-day development (build C++ normally, test.bat sets PATH)
- No editable install support — native code + editable installs is fragile
- Wheel building is separate from `build-all.bat` / `test-all.bat` — explicit when needed
- Create a `scripts/test-wheel.bat` script that builds wheel, creates temp venv, installs, runs pytest (proves self-containment)

### Claude's Discretion
- Exact scikit-build-core version constraint in build-requires
- CMake install() command details (DESTINATION paths, RUNTIME vs LIBRARY)
- How `wheel.packages` is configured for src layout discovery
- test-wheel.bat implementation details (temp dir cleanup, error handling)

</decisions>

<specifics>
## Specific Ideas

- SKBUILD guard should auto-enable QUIVER_BUILD_C_API — self-documenting that "when building for Python, C API is required"
- Validation script (test-wheel.bat) should prove the wheel is truly self-contained by installing in a clean venv with no PATH manipulation

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 01-build-system-migration*
*Context gathered: 2026-02-25*
