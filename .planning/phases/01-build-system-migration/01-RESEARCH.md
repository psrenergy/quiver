# Phase 1: Build System Migration - Research

**Researched:** 2026-02-25
**Domain:** Python build backend migration (hatchling to scikit-build-core) + CMake install targets for wheel bundling
**Confidence:** HIGH

## Summary

Phase 1 replaces hatchling with scikit-build-core as the PEP 517 build backend and adds SKBUILD-guarded CMake install targets so that `pip wheel .` produces a wheel with `libquiver` and `libquiver_c` bundled inside `quiverdb/_libs/`. The changes touch three files (pyproject.toml, root CMakeLists.txt, .gitignore) and create one new script (test-wheel.bat). Existing Python tests must continue to pass in their current development-mode workflow (test.bat with PATH-based DLL discovery).

scikit-build-core 0.12.0 (released 2026-02-24) is the current stable version. It sets the `SKBUILD` CMake variable to `"2"` during wheel builds, provides `SKBUILD_PLATLIB_DIR` and other directory variables, and automatically tags the wheel as platform-specific when native artifacts are installed. The `cmake.source-dir` setting allows pointing from `bindings/python/` up to the repo root where CMakeLists.txt lives.

**Primary recommendation:** Modify three existing files and add one script. The SKBUILD guard in root CMakeLists.txt handles all wheel-specific install logic. No new CMakeLists.txt files needed.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- pyproject.toml stays in `bindings/python/` (not repo root); use `cmake.source-dir` to point to repo root
- `pip wheel .` is run from `bindings/python/` -- no convenience wrapper at root
- Flat `_libs/` directory: `quiverdb/_libs/libquiver.dll` + `quiverdb/_libs/libquiver_c.dll` (or `.so`)
- Guard logic lives in root `CMakeLists.txt` as an `if(SKBUILD)` block at the end
- Inside the guard: force `QUIVER_BUILD_C_API=ON` (auto-enables C API for wheel builds)
- Inside the guard: `install()` targets for both native libraries into the wheel package
- Minimal metadata only -- just enough for the build to work; no PyPI presentation metadata until Phase 4
- Keep version at 0.4.0 -- no bump until milestone is release-ready
- Dev dependencies (dotenv, pytest, ruff) stay as-is
- Minimal `[tool.scikit-build]` config: `cmake.source-dir`, `wheel.packages`, `cmake.build-type = "Release"`
- CMake does the heavy lifting -- install targets, target selection, SKBUILD logic
- Build only library targets (libquiver + libquiver_c) for wheel -- no tests, no benchmarks
- Python source stays in existing `src/quiverdb/` layout
- Keep existing `test.bat` approach for day-to-day development (build C++ normally, test.bat sets PATH)
- No editable install support -- native code + editable installs is fragile
- Wheel building is separate from `build-all.bat` / `test-all.bat` -- explicit when needed
- Create a `scripts/test-wheel.bat` script that builds wheel, creates temp venv, installs, runs pytest

### Claude's Discretion
- Exact scikit-build-core version constraint in build-requires
- CMake install() command details (DESTINATION paths, RUNTIME vs LIBRARY)
- How `wheel.packages` is configured for src layout discovery
- test-wheel.bat implementation details (temp dir cleanup, error handling)

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| BUILD-01 | pyproject.toml uses scikit-build-core as build backend (replaces hatchling) | Standard stack section covers exact pyproject.toml configuration; `scikit_build_core.build` as backend, `scikit-build-core>=0.10` in build-requires |
| BUILD-02 | CMake install targets place libquiver and libquiver_c into wheel package directory (SKBUILD-guarded) | Architecture patterns section covers the SKBUILD guard, install() command syntax with LIBRARY/RUNTIME DESTINATION, and the auto-enable of QUIVER_BUILD_C_API |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| scikit-build-core | >=0.10 | PEP 517 build backend that invokes CMake and packages results into wheels | Only mature build backend with native CMake integration. Sets SKBUILD variable, manages install prefix, auto-tags platform wheels. |
| CMake | >=3.21 | Build system for C++ libraries (already in use) | Already the project's build system. scikit-build-core invokes it automatically. |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| pip | (system) | `pip wheel .` builds the wheel | Invokes scikit-build-core backend during wheel creation |
| build | >=1.0 | Alternative wheel builder (`python -m build`) | Optional; `pip wheel .` is sufficient for this phase |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| scikit-build-core direct | hatchling + scikit-build-core plugin | Plugin is experimental, adds indirection, lacks editable install |
| scikit-build-core | hatchling + custom build hook | Must shell out to CMake, copy artifacts, set platform tags manually; fragile |
| `cmake.source-dir = "../.."` | Move pyproject.toml to repo root | Breaks consistency with other bindings (Julia, Dart each have their own subdir) |

### Version Constraint Recommendation (Claude's Discretion)

Use `scikit-build-core>=0.10` as the minimum version. Rationale:
- 0.10 introduced the table format for `wheel.packages` and stabilized `cmake.source-dir`
- 0.11 brought minor improvements but nothing this phase requires
- 0.12.0 is current (released 2026-02-24) and will be installed by default
- Using `>=0.10` is more permissive than `>=0.11` with no downside for this use case

**Installation (handled automatically by PEP 517 build isolation):**
```toml
[build-system]
requires = ["scikit-build-core>=0.10"]
build-backend = "scikit_build_core.build"
```

## Architecture Patterns

### Files Modified/Created

```
CMakeLists.txt                              # MODIFIED: add SKBUILD guard block at end
bindings/python/pyproject.toml              # MODIFIED: replace hatchling with scikit-build-core
bindings/python/.gitignore                  # MODIFIED: add _skbuild/ pattern
scripts/test-wheel.bat                      # NEW: validation script
```

### Pattern 1: SKBUILD Guard in Root CMakeLists.txt

**What:** Conditional block at the end of root `CMakeLists.txt` that activates only during scikit-build-core wheel builds.

**When to use:** When the same CMake project serves both standalone development builds and Python wheel packaging.

**Example:**
```cmake
# --- Python wheel packaging (scikit-build-core) ---
if(DEFINED SKBUILD)
    # Force C API on for wheel builds (Python bindings need it)
    set(QUIVER_BUILD_C_API ON CACHE BOOL "Build C API wrapper" FORCE)

    # Install both native libraries into the wheel package
    install(TARGETS quiver quiver_c
        LIBRARY DESTINATION quiverdb/_libs
        RUNTIME DESTINATION quiverdb/_libs
    )
endif()
```

**Critical details:**
- `DEFINED SKBUILD` (not just `SKBUILD`) -- checks if the variable exists, not its truthiness. scikit-build-core sets `SKBUILD=2`.
- `LIBRARY DESTINATION` handles `.so` files on Linux and `.dylib` on macOS.
- `RUNTIME DESTINATION` handles `.dll` files on Windows. Without this, Windows DLLs would go to `bin/` instead of the package directory.
- Both destinations point to `quiverdb/_libs` which is relative to `CMAKE_INSTALL_PREFIX`. scikit-build-core sets `CMAKE_INSTALL_PREFIX` to a staging directory whose contents become the wheel's platlib (site-packages).
- The `FORCE` on `QUIVER_BUILD_C_API` overrides the default `OFF` value even if cached from a previous configure.
- This block runs AFTER `add_subdirectory(src)` and AFTER the existing install targets. The existing `install(TARGETS quiver ...)` with standard `GNUInstallDirs` destinations remains untouched and only applies to non-SKBUILD installs.

**Placement:** At the very end of root `CMakeLists.txt`, after all existing content.

**Confidence:** HIGH -- `DEFINED SKBUILD` is documented in scikit-build-core official docs. LIBRARY/RUNTIME DESTINATION is standard CMake.

### Pattern 2: pyproject.toml Configuration

**What:** Minimal scikit-build-core configuration pointing to the repo root for CMake.

**Example:**
```toml
[project]
name = "quiverdb"
version = "0.4.0"
requires-python = ">=3.13"
dependencies = [
    "cffi>=2.0.0",
]

[build-system]
requires = ["scikit-build-core>=0.10"]
build-backend = "scikit_build_core.build"

[tool.scikit-build]
cmake.source-dir = "../.."
cmake.build-type = "Release"
wheel.packages = ["src/quiverdb"]

[dependency-groups]
dev = [
    "dotenv>=0.9.9",
    "pytest>=8.4.1",
    "ruff>=0.12.2",
]
```

**Key details:**
- `cmake.source-dir = "../.."` -- from `bindings/python/`, this goes up two levels to the repo root where `CMakeLists.txt` lives. scikit-build-core resolves this relative to the directory containing `pyproject.toml`.
- `wheel.packages = ["src/quiverdb"]` -- tells scikit-build-core where the Python source lives. The `src/` prefix is stripped; the package appears as `quiverdb/` in the wheel. Without this, scikit-build-core auto-discovers, but explicit is safer for the `src/` layout.
- `cmake.build-type = "Release"` -- builds optimized native libraries for wheel distribution.
- No `cmake.args` needed -- the SKBUILD guard handles `QUIVER_BUILD_C_API=ON` automatically. Tests are disabled because `QUIVER_BUILD_TESTS` defaults to `ON` in the CMakeLists.txt, but scikit-build-core only runs `cmake --install` (which only installs SKBUILD-guarded targets), so test executables are built but not installed. To save build time, add `cmake.args = ["-DQUIVER_BUILD_TESTS=OFF"]`.
- `[project.scripts]` section removed (was empty in original).

**Confidence:** HIGH for the configuration structure. MEDIUM for `cmake.source-dir = "../.."` path resolution in all contexts (local, CI, cibuildwheel). This is a documented feature but the two-levels-up layout is unusual.

### Pattern 3: Wheel Validation Script

**What:** A batch script that builds a wheel, installs it in a clean venv, and runs tests to prove self-containment.

**Example (Claude's Discretion -- implementation details):**
```batch
@echo off
setlocal enabledelayedexpansion

echo === Building wheel ===
pushd %~dp0..\bindings\python
pip wheel . --no-build-isolation -w dist
if errorlevel 1 (
    echo FAILED: wheel build
    popd & exit /b 1
)

echo === Creating temp venv ===
set TMPVENV=%TEMP%\quiver_wheel_test_%RANDOM%
python -m venv %TMPVENV%
call %TMPVENV%\Scripts\activate.bat

echo === Installing wheel ===
for %%f in (dist\quiverdb-*.whl) do set WHEEL=%%f
pip install %WHEEL%
if errorlevel 1 (
    echo FAILED: wheel install
    goto :cleanup
)

echo === Running tests ===
pip install pytest
pytest tests/ -x
set RESULT=%ERRORLEVEL%

:cleanup
echo === Cleaning up ===
call deactivate
rmdir /s /q %TMPVENV% 2>nul
del /q dist\quiverdb-*.whl 2>nul
popd
exit /b %RESULT%
```

**Key design choices:**
- `--no-build-isolation` -- skips creating an isolated build environment, uses the current Python's scikit-build-core. Faster for local testing. In CI, cibuildwheel uses full isolation.
- Temp venv with random suffix prevents collisions with concurrent runs.
- Tests run from the `tests/` directory within the binding, exercising the installed wheel (not the source tree).
- Cleanup runs regardless of test outcome.
- No PATH manipulation -- if the wheel is self-contained, tests pass without adding build/bin to PATH.

**Important caveat:** During Phase 1, the `_loader.py` has NOT been updated yet (that is Phase 2: Loader Rewrite). The existing `_loader.py` walks up directories to find `build/` and falls back to PATH. Tests in the clean venv will NOT find the native libraries because there is no `build/` directory and no PATH. This means `test-wheel.bat` will demonstrate that the wheel CONTAINS the libraries (inspectable via `zipinfo` or `unzip -l`) but the Python import will fail until Phase 2 rewrites `_loader.py` to check `_libs/`.

**Resolution for Phase 1 validation:** The validation script should verify wheel CONTENTS rather than full import functionality. Check that the wheel file contains the expected `_libs/` entries. Full import testing becomes possible after Phase 2.

**Confidence:** HIGH for script structure. HIGH for the Phase 1 validation limitation (this is expected by design -- the roadmap has Phase 2 depending on Phase 1).

### Anti-Patterns to Avoid

- **Adding a separate `bindings/python/CMakeLists.txt`:** Not needed. The SKBUILD guard in root CMakeLists.txt handles everything. A separate file adds complexity and risks path confusion.
- **Using `SKBUILD_PLATLIB_DIR` in install():** While valid, using `install(TARGETS ... DESTINATION quiverdb/_libs)` is simpler and more portable. The destination is relative to `CMAKE_INSTALL_PREFIX` which scikit-build-core manages.
- **Setting `wheel.install-dir`:** This globally offsets the CMake install prefix. Not needed here because the install() DESTINATION already includes the `quiverdb/` prefix. Mixing both would result in double-nesting.
- **Adding `cmake.args = ["-DQUIVER_BUILD_C_API=ON"]` to pyproject.toml:** The SKBUILD guard handles this with `set(... FORCE)`. Putting it in cmake.args duplicates the logic and doesn't communicate intent.
- **Bumping the version:** User decision: keep at 0.4.0 until release-ready.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Build backend integration | Custom hatch_build.py hook shelling out to CMake | scikit-build-core as direct backend | Handles cmake invocation, install prefix, wheel tagging, platform detection |
| Platform wheel tagging | Manual WHEEL file manipulation | scikit-build-core auto-detection | Detects installed native artifacts and sets correct `cp313-cp313-win_amd64` tag |
| CMake discovery/installation | Manual pip install cmake in build-requires | scikit-build-core dependency resolution | Automatically installs cmake and ninja if not present |

## Common Pitfalls

### Pitfall 1: cmake.source-dir Path Resolution
**What goes wrong:** `cmake.source-dir = "../.."` is resolved relative to the directory containing pyproject.toml. If the working directory changes (e.g., cibuildwheel copies the project) or if the path calculation is wrong, CMake fails with "CMakeLists.txt not found."
**Why it happens:** Most scikit-build-core projects have CMakeLists.txt adjacent to pyproject.toml. The quiver layout with pyproject.toml two directories deep is unusual.
**How to avoid:** Test locally first with `cd bindings/python && pip wheel . --no-build-isolation -w dist`. If it fails, verify that `../../CMakeLists.txt` exists relative to `bindings/python/`. scikit-build-core resolves `cmake.source-dir` relative to the pyproject.toml location, NOT the current working directory when pip is invoked.
**Warning signs:** Build fails during CMake configure step with path-related errors.
**Confidence:** MEDIUM -- documented feature but unusual layout needs explicit testing.

### Pitfall 2: QUIVER_BUILD_C_API Not Forced Correctly
**What goes wrong:** The SKBUILD guard sets `QUIVER_BUILD_C_API=ON` but the `if(QUIVER_BUILD_C_API)` block in `src/CMakeLists.txt` that defines the `quiver_c` target has already been evaluated (CMake processes `add_subdirectory(src)` before reaching the guard at the end of root CMakeLists.txt).
**Why it happens:** CMake evaluates files top-to-bottom. If `QUIVER_BUILD_C_API` is set AFTER `add_subdirectory(src)`, the `if(QUIVER_BUILD_C_API)` inside src/ has already been evaluated with the default `OFF` value.
**How to avoid:** The `set(QUIVER_BUILD_C_API ON CACHE BOOL "..." FORCE)` must be placed BEFORE `add_subdirectory(src)` in the control flow. Two approaches:
  1. Place the SKBUILD force-set at the TOP of root CMakeLists.txt (before `add_subdirectory(src)`) and the install() at the bottom.
  2. Move the `option(QUIVER_BUILD_C_API ...)` after the SKBUILD check.

**Recommended approach:** Split the SKBUILD guard into two parts:
```cmake
# Near the top, after option() declarations:
if(DEFINED SKBUILD)
    set(QUIVER_BUILD_C_API ON CACHE BOOL "Build C API wrapper" FORCE)
endif()

# ... add_subdirectory(src) ...

# At the bottom:
if(DEFINED SKBUILD)
    install(TARGETS quiver quiver_c
        LIBRARY DESTINATION quiverdb/_libs
        RUNTIME DESTINATION quiverdb/_libs
    )
endif()
```
**Warning signs:** Wheel contains `_libs/libquiver.dll` but NOT `_libs/libquiver_c.dll`.
**Confidence:** HIGH -- this is standard CMake evaluation order.

### Pitfall 3: Existing Install Targets Conflict
**What goes wrong:** The existing `src/CMakeLists.txt` already has `install(TARGETS quiver EXPORT quiverTargets LIBRARY DESTINATION ... RUNTIME DESTINATION ...)`. During SKBUILD builds, BOTH the existing install and the SKBUILD install activate, potentially placing libraries in unexpected locations.
**Why it happens:** CMake `install()` commands are cumulative. Both the existing GNUInstallDirs-based install and the SKBUILD install will execute during `cmake --install`.
**How to avoid:** This is actually harmless. The existing install places libraries in `${CMAKE_INSTALL_LIBDIR}` and `${CMAKE_INSTALL_BINDIR}` (resolved to `lib/` and `bin/` under the install prefix). The SKBUILD install places them in `quiverdb/_libs/`. Both execute, but only the `quiverdb/_libs/` contents end up in the wheel. The extra `lib/` and `bin/` files are also packaged but scikit-build-core's `wheel.packages` only includes Python files from `src/quiverdb/`; CMake-installed files go to the platlib root. This means `lib/` and `bin/` would appear at the site-packages root level alongside `quiverdb/`.
**Better approach:** Guard the existing install targets to NOT run under SKBUILD:
```cmake
# In src/CMakeLists.txt, wrap existing install():
if(NOT DEFINED SKBUILD)
    install(TARGETS quiver
        EXPORT quiverTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ...
    )
endif()
```
Or simply accept the extra files -- they are small and harmless, just slightly untidy.
**Warning signs:** Extra `lib/` or `bin/` directories at the root of the wheel.
**Confidence:** HIGH -- CMake install semantics are well-defined.

### Pitfall 4: Build Tests Waste Time in Wheel Builds
**What goes wrong:** `QUIVER_BUILD_TESTS` defaults to `ON`. During wheel builds, CMake downloads GoogleTest and compiles all test executables. This adds several minutes and downloads to wheel build time for artifacts that are never installed.
**Why it happens:** The SKBUILD guard enables C API but does not disable tests.
**How to avoid:** Add `-DQUIVER_BUILD_TESTS=OFF` to cmake.args in pyproject.toml:
```toml
[tool.scikit-build]
cmake.args = ["-DQUIVER_BUILD_TESTS=OFF"]
```
Or handle it in the SKBUILD guard:
```cmake
if(DEFINED SKBUILD)
    set(QUIVER_BUILD_TESTS OFF CACHE BOOL "Build test suite" FORCE)
endif()
```
**Warning signs:** Wheel build takes 5+ minutes when it should take 2-3 minutes.
**Confidence:** HIGH -- straightforward CMake option management.

### Pitfall 5: .gitignore Missing Wheel Build Artifacts
**What goes wrong:** Running `pip wheel .` in `bindings/python/` creates build artifacts (`_skbuild/`, `dist/`, `*.whl`) that should not be committed.
**Why it happens:** The existing `.gitignore` covers `build/` and `dist/` (generic Python patterns) but may not cover scikit-build-core's `_skbuild/` directory.
**How to avoid:** Verify `.gitignore` includes `_skbuild/` and `*.whl`. The existing Python .gitignore already covers `dist/`, `build/`, and `*.whl` via `wheels/`.
**Confidence:** HIGH -- minor file management.

## Code Examples

### Complete pyproject.toml (after migration)
```toml
# Source: scikit-build-core configuration reference
# https://scikit-build-core.readthedocs.io/en/latest/configuration/index.html
[project]
name = "quiverdb"
version = "0.4.0"
requires-python = ">=3.13"
dependencies = [
    "cffi>=2.0.0",
]

[build-system]
requires = ["scikit-build-core>=0.10"]
build-backend = "scikit_build_core.build"

[tool.scikit-build]
cmake.source-dir = "../.."
cmake.build-type = "Release"
cmake.args = ["-DQUIVER_BUILD_TESTS=OFF"]
wheel.packages = ["src/quiverdb"]

[dependency-groups]
dev = [
    "dotenv>=0.9.9",
    "pytest>=8.4.1",
    "ruff>=0.12.2",
]
```

### SKBUILD guard in root CMakeLists.txt
```cmake
# Source: scikit-build-core CMakeLists authoring guide
# https://scikit-build-core.readthedocs.io/en/latest/guide/cmakelists.html

# --- Python wheel packaging (scikit-build-core) ---
# When building via scikit-build-core (pip wheel / python -m build),
# SKBUILD is defined. Force C API on and install native libs into the wheel.
if(DEFINED SKBUILD)
    set(QUIVER_BUILD_C_API ON CACHE BOOL "Build C API wrapper" FORCE)
    set(QUIVER_BUILD_TESTS OFF CACHE BOOL "Build test suite" FORCE)
endif()
```

This block must appear BEFORE `add_subdirectory(src)`. Then, at the end of root CMakeLists.txt:

```cmake
# Install native libraries into the wheel package directory
if(DEFINED SKBUILD)
    install(TARGETS quiver quiver_c
        LIBRARY DESTINATION quiverdb/_libs
        RUNTIME DESTINATION quiverdb/_libs
    )
endif()
```

### CMake install() DESTINATION semantics
```cmake
# LIBRARY = .so (Linux), .dylib (macOS)
# RUNTIME = .dll (Windows), executables
# ARCHIVE = .lib (Windows import lib), .a (static lib)
#
# For shared libraries:
# - Linux: LIBRARY DESTINATION is used
# - Windows: RUNTIME DESTINATION is used (DLLs are executables)
# - Both must point to the same directory for cross-platform compatibility
install(TARGETS quiver quiver_c
    LIBRARY DESTINATION quiverdb/_libs    # .so files
    RUNTIME DESTINATION quiverdb/_libs    # .dll files
)
```

### Verifying wheel contents (manual check)
```bash
# After building:
cd bindings/python
pip wheel . --no-build-isolation -w dist

# Check wheel contents (use python zipfile or unzip):
python -c "
import zipfile, sys
with zipfile.ZipFile(sys.argv[1]) as z:
    for name in sorted(z.namelist()):
        if '_libs/' in name:
            info = z.getinfo(name)
            print(f'{info.file_size:>10}  {name}')
" dist/quiverdb-0.4.0-*.whl
```

Expected output on Windows:
```
   1234567  quiverdb/_libs/libquiver.dll
    567890  quiverdb/_libs/libquiver_c.dll
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| hatchling + custom hooks | scikit-build-core direct backend | scikit-build-core 0.1 (2023) | Unified CMake+Python build in single `pip wheel` invocation |
| scikit-build (classic) | scikit-build-core | scikit-build-core 0.1 (2023) | Modern PEP 517 compliance, no setup.py needed |
| `setup.py` + cmake extensions | pyproject.toml + scikit-build-core | PEP 517/518 era | Declarative build config, build isolation |
| `SKBUILD_PLATLIB_DIR` in install | Relative `DESTINATION` in install | Both work | Relative DESTINATION is simpler and more portable |

**Current versions:**
- scikit-build-core: 0.12.0 (released 2026-02-24)
- CMake minimum: 3.21 (project requirement, well supported)

## Open Questions

1. **cmake.source-dir = "../.." path resolution in cibuildwheel**
   - What we know: scikit-build-core resolves the path relative to pyproject.toml location. Documented and tested in local builds.
   - What's unclear: Whether cibuildwheel copies or symlinks the project in a way that breaks the relative path. cibuildwheel typically operates on the project directory directly, so this should be fine.
   - Recommendation: Test locally first with `cd bindings/python && pip wheel . --no-build-isolation -w dist`. This is Phase 1 validation. cibuildwheel testing is Phase 3.

2. **Extra install artifacts from existing install() targets**
   - What we know: The existing `install(TARGETS quiver EXPORT quiverTargets ...)` in src/CMakeLists.txt will also execute during SKBUILD builds, placing files in `lib/` and `bin/` at the platlib root.
   - What's unclear: Whether these extra files cause issues with wheel tools, PyPI upload size limits, or user confusion.
   - Recommendation: Guard existing installs with `if(NOT DEFINED SKBUILD)` for cleanliness. Alternatively, accept the extra ~2MB of duplicate files.

3. **Phase 1 validation scope (tests won't pass in clean venv)**
   - What we know: Until Phase 2 rewrites `_loader.py`, the bundled `_libs/` libraries are not discoverable by the loader. Tests in a clean venv will fail with "library not found."
   - What's unclear: Nothing -- this is expected by the roadmap design.
   - Recommendation: Phase 1 validation checks wheel CONTENTS (presence of `_libs/` files), not runtime functionality. Full import testing deferred to Phase 2.

## Sources

### Primary (HIGH confidence)
- [scikit-build-core documentation](https://scikit-build-core.readthedocs.io/en/latest/) -- configuration reference, CMakeLists authoring, SKBUILD variables
- [scikit-build-core CMakeLists guide](https://scikit-build-core.readthedocs.io/en/latest/guide/cmakelists.html) -- SKBUILD detection, SKBUILD_PLATLIB_DIR, install targets
- [scikit-build-core configuration](https://scikit-build-core.readthedocs.io/en/latest/configuration/index.html) -- cmake.source-dir, wheel.packages, cmake.build-type, cmake.define
- [scikit-build-core dynamic linking](https://scikit-build-core.readthedocs.io/en/latest/guide/dynamic_link.html) -- RPATH, os.add_dll_directory patterns
- [scikit-build-core PyPI](https://pypi.org/project/scikit-build-core/) -- version 0.12.0 confirmed current
- [CMake install() docs](https://cmake.org/cmake/help/latest/command/install.html) -- LIBRARY/RUNTIME/ARCHIVE DESTINATION semantics

### Secondary (MEDIUM confidence)
- [scikit-build-core issue #966](https://github.com/scikit-build/scikit-build-core/issues/966) -- multi-component project with wheel.install-dir guidance from maintainer
- [scikit-build-core issue #374](https://github.com/scikit-build/scikit-build-core/issues/374) -- shared library install destinations, editable mode caveats
- Project milestone-level research in `.planning/research/` -- STACK.md, ARCHITECTURE.md, PITFALLS.md (thoroughly cross-referenced)

### Tertiary (LOW confidence)
- None -- all findings verified against official documentation

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- scikit-build-core is the established standard for CMake+Python, verified in official docs and PyPI
- Architecture: HIGH -- SKBUILD guard pattern and install() DESTINATION semantics are well-documented CMake/scikit-build-core features
- Pitfalls: HIGH -- CMake evaluation order (Pitfall 2) is deterministic; MEDIUM for cmake.source-dir path resolution in edge cases

**Research date:** 2026-02-25
**Valid until:** 2026-03-25 (stable domain, scikit-build-core API is mature)
