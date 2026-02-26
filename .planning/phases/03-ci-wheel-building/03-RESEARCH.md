# Phase 3: CI Wheel Building - Research

**Researched:** 2026-02-25
**Domain:** GitHub Actions CI / cibuildwheel / scikit-build-core wheel builds
**Confidence:** HIGH

## Summary

cibuildwheel v3.3.1 (latest stable, January 2026) is the standard tool for building platform-specific Python wheels in CI. It integrates directly with GitHub Actions via `pypa/cibuildwheel@v3.3.1` and handles the entire build-test cycle inside isolated environments. For this project, the key complexity is that pyproject.toml lives in a subdirectory (`bindings/python/`) while CMake sources are at the repo root -- this is solved via the `package-dir` input parameter.

scikit-build-core (already in use) automatically provides cmake and ninja as build dependencies inside cibuildwheel containers, so no `before-all` installation step is needed. The manylinux_2_28 image is the default in cibuildwheel v3, matching the project requirement exactly.

The primary risk area is auditwheel's interaction with pre-bundled shared libraries (`libquiver.so` and `libquiver_c.so` installed into `quiverdb/_libs/` by CMake). Since these are loaded via CFFI `dlopen` (not linked as Python extension modules), auditwheel cannot statically detect the dependency chain. However, auditwheel should still process them correctly because the libraries are ELF files with proper `DT_NEEDED` entries and the `$ORIGIN` RPATH is already set in CMakeLists.txt.

**Primary recommendation:** Use `pypa/cibuildwheel@v3.3.1` with `package-dir: bindings/python`, configure `[tool.cibuildwheel]` in `bindings/python/pyproject.toml` to restrict builds to `cp313-win_amd64` and `cp313-manylinux_x86_64`, run `pytest {project}/bindings/python/tests` as test-command, and merge per-platform artifacts into a single "wheels" artifact.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Triggers on push to main/master and on pull requests
- Tag-triggered builds deferred to Phase 4 (PyPI publish workflow)
- Full build matrix (both platforms) on every trigger -- no partial builds
- Single workflow file: `.github/workflows/build-wheels.yml`
- Workflow name in Actions UI: "Build Wheels"
- CPython 3.13 only (matches `requires-python >= 3.13` in pyproject.toml)
- No PyPy builds
- manylinux_2_28_x86_64 for Linux (no musllinux/Alpine)
- win_amd64 for Windows
- Full Python test suite (all 203 pytest tests) run inside cibuildwheel for each platform
- No separate C++ or C API test jobs -- native libs are validated through the Python tests
- If any platform fails tests, the entire workflow fails (no partial artifact upload)
- All platform wheels bundled into a single "wheels" artifact
- 7-day retention period
- Phase 4 publish workflow will consume this artifact

### Claude's Discretion
- Test environment setup (temp directories, schema file access in cibuildwheel containers)
- cibuildwheel configuration details (pyproject.toml vs env vars)
- Exact manylinux container image and build dependencies

### Deferred Ideas (OUT OF SCOPE)
- Tag-triggered publish workflow -- Phase 4 (CI-04)
- macOS wheels -- future milestone (DIST-01)
- musllinux/Alpine wheels -- future if needed
- PyPy support -- future if needed
- C++ / C API test jobs in CI -- consider for a future CI hardening phase
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| CI-01 | GitHub Actions workflow builds platform-specific wheels via cibuildwheel | `pypa/cibuildwheel@v3.3.1` action with `package-dir: bindings/python` handles subdirectory layout; workflow triggers on push to master and PRs |
| CI-02 | Wheels built for Windows x64 and Linux x64 (manylinux_2_28) | `build = ["cp313-win_amd64", "cp313-manylinux_x86_64"]` in `[tool.cibuildwheel]`; manylinux_2_28 is v3 default |
| CI-03 | Wheels tested in clean environment inside CI before publishing | `test-command = "pytest {project}/bindings/python/tests"` with `test-requires = ["pytest"]`; tests run from temp dir against installed wheel |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| cibuildwheel | v3.3.1 | Build and test wheels across platforms | PyPA official tool; only maintained multi-platform wheel builder |
| scikit-build-core | >=0.10 | CMake-based build backend | Already in pyproject.toml; auto-provides cmake/ninja in containers |
| pypa/cibuildwheel action | v3.3.1 | GitHub Actions integration | Official composite action; handles Python setup, venv isolation |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| actions/upload-artifact | v6 | Upload wheel artifacts | Store built wheels as workflow artifacts |
| actions/upload-artifact/merge | v4+ | Merge per-platform artifacts | Combine Windows + Linux wheels into single "wheels" artifact |
| actions/checkout | v6 | Repository checkout | Already used in existing CI workflow |
| actions/download-artifact | v5+ | Download artifacts (Phase 4) | Consumer workflow downloads merged artifact |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| cibuildwheel | Manual matrix + `uv build --wheel` | Would need to manually handle manylinux containers, auditwheel, platform-specific quirks; not worth it |
| pyproject.toml config | CIBW_* env vars in workflow | Env vars work but pyproject.toml is more readable, version-controlled with the project, and the recommended approach |
| upload-artifact/merge | download-artifact with merge-multiple | Both work; merge sub-action is simpler for "build then merge" pattern |

## Architecture Patterns

### Recommended Workflow Structure
```
.github/workflows/build-wheels.yml    # New: cibuildwheel workflow
bindings/python/pyproject.toml        # Modified: add [tool.cibuildwheel] section
```

### Pattern 1: Subdirectory Package Build
**What:** Use `package-dir` input to point cibuildwheel at `bindings/python/` while the repo root provides CMake sources.
**When to use:** When pyproject.toml is not at the repo root (monorepo / multi-language projects).
**Example:**
```yaml
# Source: https://github.com/pypa/cibuildwheel/issues/2398
- uses: pypa/cibuildwheel@v3.3.1
  with:
    package-dir: bindings/python
    output-dir: wheelhouse
```
cibuildwheel mounts the checkout directory as the working directory. `{project}` resolves to the repo root. `{package}` resolves to `bindings/python`. scikit-build-core's `cmake.source-dir = "../.."` (already configured in pyproject.toml) reaches the root CMakeLists.txt.

### Pattern 2: Build Selection via pyproject.toml
**What:** Restrict which platform/Python combinations to build.
**When to use:** Always -- avoids building unnecessary combinations.
**Example:**
```toml
# Source: https://cibuildwheel.pypa.io/en/stable/options/
[tool.cibuildwheel]
build = ["cp313-win_amd64", "cp313-manylinux_x86_64"]
```
This builds exactly two wheels: CPython 3.13 for Windows x64 and CPython 3.13 for manylinux_2_28 x86_64 (the v3 default image).

### Pattern 3: Test Against Installed Wheel
**What:** cibuildwheel installs the built wheel, then runs test-command from a temporary directory outside the source tree.
**When to use:** Always -- ensures tests validate the installed package, not source imports.
**Example:**
```toml
[tool.cibuildwheel]
test-requires = ["pytest"]
test-command = "pytest {project}/bindings/python/tests"
```
`{project}` expands to the absolute path of the repo root. The conftest.py's `schemas_path` fixture resolves test schemas via `Path(__file__).resolve().parent.parent.parent.parent / "tests" / "schemas"` -- this works because `{project}/bindings/python/tests/conftest.py` is the actual file on disk, and the parent chain walks up to `{project}/tests/schemas/`. Tests import `quiverdb` from the installed wheel (not source tree) because the working directory is a temp dir.

### Pattern 4: Per-Platform Artifact Upload + Merge
**What:** Upload wheels per-platform with unique names, then merge into a single artifact.
**When to use:** When building across multiple OS runners in a matrix.
**Example:**
```yaml
# In build job (per-platform):
- uses: actions/upload-artifact@v6
  with:
    name: wheels-${{ matrix.os }}
    path: ./wheelhouse/*.whl

# In merge job (after all builds):
- uses: actions/upload-artifact/merge@v4
  with:
    name: wheels
    pattern: wheels-*
    delete-merged: true
    retention-days: 7
```

### Anti-Patterns to Avoid
- **Putting cibuildwheel config in workflow env vars:** Harder to maintain and review; put in `[tool.cibuildwheel]` in pyproject.toml instead.
- **Using `before-all` to install cmake:** scikit-build-core handles this automatically via `build-system.requires`; adding a manual install creates version conflicts.
- **Running tests with `{package}/tests` instead of `{project}/bindings/python/tests`:** While `{package}` points to `bindings/python`, using `{project}` makes the full path explicit and avoids confusion about what `{package}` resolves to inside containers.
- **Uploading artifacts with the same name from different runners:** v4+ of upload-artifact forbids this; must use unique names per runner then merge.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Cross-platform wheel building | Manual matrix + docker + `uv build` | cibuildwheel | Handles manylinux containers, auditwheel, platform quirks automatically |
| manylinux compliance | Manual auditwheel invocation | cibuildwheel's built-in repair | Default `auditwheel repair` is automatic on Linux builds |
| CMake in containers | `before-all: yum install cmake` | scikit-build-core auto-provides cmake | scikit-build-core installs cmake via pip in the build environment |
| Artifact merging | Custom download + reupload script | actions/upload-artifact/merge | Official sub-action, handles deduplication and cleanup |

**Key insight:** cibuildwheel + scikit-build-core together handle the full complexity of "CMake project to manylinux-compliant wheel." The only custom configuration needed is build selection and test setup.

## Common Pitfalls

### Pitfall 1: auditwheel and CFFI dlopen Libraries
**What goes wrong:** auditwheel uses `DT_NEEDED` from Python extension modules to discover shared library dependencies. CFFI `dlopen`-loaded libraries are invisible to this static analysis, so auditwheel may not detect that `libquiver_c.so` depends on `libquiver.so`.
**Why it happens:** CFFI ABI-mode loads libraries at runtime via `ffi.dlopen()`, not via compiled extension modules with ELF linkage.
**How to avoid:** The CMake install already places both `libquiver.so` and `libquiver_c.so` into `quiverdb/_libs/` with `INSTALL_RPATH "$ORIGIN"` on `libquiver_c`. auditwheel will still process these `.so` files because they are proper ELF shared objects with `DT_NEEDED` entries. The key is that `libquiver_c.so` has a `DT_NEEDED` entry for `libquiver.so`, and both are in the same directory. auditwheel should detect this chain and handle it correctly. If it does not, the fallback is to customize `repair-wheel-command` to pass `--exclude` flags or skip repair entirely (since the libraries are already correctly co-located).
**Warning signs:** Wheel installs but `import quiverdb` fails with `OSError: libquiver.so: cannot open shared object file` on Linux.

### Pitfall 2: Test Schema File Access in cibuildwheel
**What goes wrong:** Tests fail with "Schema file not found" because the conftest.py `schemas_path` fixture cannot resolve the shared test schemas.
**Why it happens:** cibuildwheel runs tests from a temporary working directory outside the source tree. If the path resolution depended on `os.getcwd()`, it would break.
**How to avoid:** The conftest.py uses `Path(__file__).resolve().parent` chain (file-relative, not cwd-relative), so it works regardless of working directory. The `test-command` uses `{project}/bindings/python/tests` which points to the actual test files on disk. No changes needed -- the existing conftest design handles this correctly.
**Warning signs:** `FileNotFoundError` for schema files in CI logs.

### Pitfall 3: Windows CMake Compiler Not Found
**What goes wrong:** cibuildwheel on Windows cannot find the MSVC compiler when building with scikit-build-core.
**Why it happens:** The `windows-latest` GitHub runner has MSVC pre-installed, but the cibuildwheel environment may not have the correct Visual Studio environment activated.
**How to avoid:** cibuildwheel v3 handles Visual Studio detection automatically on Windows runners. scikit-build-core also auto-detects MSVC. No `before-all` step is needed. If issues arise, `CIBW_ENVIRONMENT_WINDOWS` can set VS paths, but this is rarely needed on standard runners.
**Warning signs:** `CMake Error: CMAKE_CXX_COMPILER not set` in CI logs.

### Pitfall 4: Workflow Fails-Fast Defeats Matrix
**What goes wrong:** One platform fails and cancels the other platform's build, making it hard to diagnose platform-specific issues.
**Why it happens:** GitHub Actions matrix `fail-fast` defaults to `true`.
**How to avoid:** Set `fail-fast: false` in the matrix strategy. However, per user decision, the entire workflow should fail if any platform fails -- but this means "don't upload partial artifacts," not "cancel in-progress builds." Using `fail-fast: false` lets both platforms finish and report, while the merge job's `needs: [build]` ensures artifacts are only merged if all builds succeed.
**Warning signs:** One platform's build is cancelled mid-way with "The job was cancelled because another job failed."

### Pitfall 5: Artifact Name Collision
**What goes wrong:** `actions/upload-artifact@v4+` rejects uploads with duplicate artifact names.
**Why it happens:** v4+ removed the ability to upload to the same artifact name from multiple jobs.
**How to avoid:** Use unique per-runner artifact names (e.g., `wheels-${{ matrix.os }}`) and merge them afterward with `actions/upload-artifact/merge`.
**Warning signs:** Upload step fails with "Artifact with name 'wheels' already exists."

## Code Examples

Verified patterns from official sources:

### Complete Workflow File
```yaml
# Source: https://github.com/pypa/cibuildwheel/blob/main/examples/github-deploy.yml
# Adapted for Quiver project structure
name: Build Wheels

on:
  push:
    branches: [master]
  pull_request:
    branches: [master]

jobs:
  build:
    name: Build wheels (${{ matrix.os }})
    runs-on: ${{ matrix.os }}
    strategy:
      fail-fast: false
      matrix:
        os: [ubuntu-latest, windows-latest]

    steps:
      - uses: actions/checkout@v6

      - uses: pypa/cibuildwheel@v3.3.1
        with:
          package-dir: bindings/python
          output-dir: wheelhouse

      - uses: actions/upload-artifact@v6
        with:
          name: wheels-${{ matrix.os }}
          path: ./wheelhouse/*.whl

  merge:
    name: Merge wheel artifacts
    runs-on: ubuntu-latest
    needs: build
    steps:
      - uses: actions/upload-artifact/merge@v4
        with:
          name: wheels
          pattern: wheels-*
          delete-merged: true
          retention-days: 7
```

### cibuildwheel Configuration in pyproject.toml
```toml
# Source: https://cibuildwheel.pypa.io/en/stable/options/
[tool.cibuildwheel]
build = ["cp313-win_amd64", "cp313-manylinux_x86_64"]
test-requires = ["pytest"]
test-command = "pytest {project}/bindings/python/tests"
```

### Build Selection Identifiers
```
# Source: https://cibuildwheel.pypa.io/en/stable/options/#build-skip
# Format: {python_tag}-{platform_tag}
cp313-win_amd64              # CPython 3.13, Windows 64-bit
cp313-manylinux_x86_64       # CPython 3.13, manylinux (2_28 default in v3)
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| cibuildwheel v2 + manylinux2014 default | cibuildwheel v3 + manylinux_2_28 default | June 2025 (v3.0.0) | No need to set manylinux image explicitly |
| `pypa/cibuildwheel@v2.x` | `pypa/cibuildwheel@v3.3.1` | June 2025 | New `test-sources` option; `build` as default frontend; PyPy opt-in |
| `actions/upload-artifact@v3` (same-name uploads) | `actions/upload-artifact@v4+` (unique names + merge) | January 2025 (v3 deprecated) | Must use unique artifact names per runner |
| Manual cmake install in containers | scikit-build-core auto-provides cmake | Ongoing | No `before-all` needed for cmake |

**Deprecated/outdated:**
- cibuildwheel v2: Still works but v3 is current. v2's manylinux2014 default would need explicit override.
- `actions/upload-artifact@v3`: Deprecated since January 2025. v4+ required.

## Open Questions

1. **auditwheel behavior with pre-bundled CFFI libraries**
   - What we know: auditwheel processes ELF files found in the wheel and handles `DT_NEEDED` chains. `libquiver_c.so` has `DT_NEEDED` for `libquiver.so`. Both are co-located in `quiverdb/_libs/` with `$ORIGIN` RPATH.
   - What's unclear: Whether auditwheel will rename/relocate the pre-bundled libraries, potentially breaking the `_loader.py` discovery by filename. auditwheel typically renames libraries with hashes (e.g., `libquiver-abc123.so`).
   - Recommendation: Test the default `auditwheel repair` first. If it renames libraries and breaks loading, set `repair-wheel-command-linux = ""` to skip auditwheel entirely (since the libraries are already correctly bundled with proper RPATH). This is safe because the libraries are self-contained and don't depend on system libraries beyond what manylinux_2_28 guarantees (libc, libm, libpthread, etc.).

2. **`{project}` path inside Linux containers**
   - What we know: cibuildwheel mounts the source directory into the container. `{project}` should resolve to the mount point.
   - What's unclear: Whether the absolute path to `{project}` is preserved or remapped inside the container.
   - Recommendation: cibuildwheel documentation states `{project}` is "an absolute path to the project root." This is used in test-command extensively by the community. LOW risk. If path issues arise, `test-sources` can copy test files into the working directory as a fallback.

3. **Windows `delvewheel` behavior**
   - What we know: cibuildwheel does not run a repair tool on Windows by default (`repair-wheel-command` is empty for Windows).
   - What's unclear: N/A -- this is fine for our use case since Windows DLLs are already bundled in `_libs/` with `os.add_dll_directory()` in `_loader.py`.
   - Recommendation: No action needed. Windows repair is already empty by default.

## Sources

### Primary (HIGH confidence)
- [cibuildwheel options documentation](https://cibuildwheel.pypa.io/en/stable/options/) - build selection, test-command, test-requires, repair-wheel-command, before-all, placeholders
- [cibuildwheel GitHub deploy example](https://github.com/pypa/cibuildwheel/blob/main/examples/github-deploy.yml) - complete workflow YAML with v3.3.1
- [cibuildwheel action.yml](https://github.com/pypa/cibuildwheel/blob/main/action.yml) - action inputs: package-dir, output-dir, config-file
- [cibuildwheel changelog](https://cibuildwheel.pypa.io/en/stable/changelog/) - v3.3.1 (Jan 2026), v3.0.0 manylinux_2_28 default
- [cibuildwheel issue #2398](https://github.com/pypa/cibuildwheel/issues/2398) - subdirectory pyproject.toml support via package-dir

### Secondary (MEDIUM confidence)
- [scikit-build-core dynamic linking guide](https://scikit-build-core.readthedocs.io/en/latest/guide/dynamic_link.html) - RPATH, auditwheel interaction
- [actions/upload-artifact merge README](https://github.com/actions/upload-artifact/blob/main/merge/README.md) - merge sub-action usage
- [auditwheel GitHub](https://github.com/pypa/auditwheel) - DT_NEEDED analysis, RPATH patching

### Tertiary (LOW confidence)
- auditwheel behavior with pre-bundled CFFI libraries: Based on general understanding of ELF analysis and community reports (issues #212, #416). Needs validation during implementation.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - cibuildwheel v3.3.1 confirmed current via changelog, action.yml verified
- Architecture: HIGH - subdirectory layout confirmed via issue #2398, workflow pattern from official examples
- Pitfalls: MEDIUM - auditwheel interaction with pre-bundled libraries needs runtime validation; all other pitfalls are well-documented

**Research date:** 2026-02-25
**Valid until:** 2026-03-25 (cibuildwheel is stable; no expected breaking changes)
