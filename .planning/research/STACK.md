# Technology Stack

**Project:** quiverdb PyPI distribution
**Researched:** 2026-02-25

## Recommended Stack

### Build Backend
| Technology | Version | Purpose | Why |
|------------|---------|---------|-----|
| scikit-build-core | >=0.11 | PEP 517 build backend that invokes CMake | Only mature build backend that natively integrates CMake with Python wheel packaging. Replaces hatchling for this project. Automatically places CMake install() targets into the wheel. Sets platform-specific wheel tags when native artifacts are present. |

### CI Wheel Building
| Technology | Version | Purpose | Why |
|------------|---------|---------|-----|
| cibuildwheel | >=2.22 | Cross-platform wheel building in CI | Industry standard. Handles manylinux containers, repair tools, and platform matrix. Used by numpy, scipy, pandas. |

### Wheel Repair (bundling transitive native deps)
| Technology | Version | Purpose | Why |
|------------|---------|---------|-----|
| auditwheel | (bundled in manylinux) | Linux wheel repair, manylinux compliance | Default in cibuildwheel for Linux. Bundles libstdc++ and other transitive deps, patches RPATH, tags manylinux. |
| delvewheel | >=1.10 | Windows wheel repair, DLL bundling | Only option for Windows. Bundles MSVC runtime and other transitive DLL deps. May be optional -- see PITFALLS.md. |

### CI/CD
| Technology | Version | Purpose | Why |
|------------|---------|---------|-----|
| GitHub Actions | N/A | CI orchestration | Already used by project. Native support for cibuildwheel. |
| gh-action-pypi-publish | release/v1 | PyPI publishing | Official PyPA action. Supports trusted publishing (OIDC, no API tokens needed). |

### Runtime Dependency
| Technology | Version | Purpose | Why |
|------------|---------|---------|-----|
| cffi | >=2.0.0 | Python FFI for loading native libraries | Already used. ABI-mode dlopen of libquiver_c. No change needed. |

## Alternatives Considered

| Category | Recommended | Alternative | Why Not |
|----------|-------------|-------------|---------|
| Build backend | scikit-build-core | hatchling + custom hook | Custom hook must shell out to CMake, copy artifacts, set platform tags manually. Fragile and unmaintained. |
| Build backend | scikit-build-core | hatchling + scikit-build-core plugin | Plugin is marked `experimental` in docs. Lacks editable install support. Unnecessary indirection. |
| Build backend | scikit-build-core | meson-python | Project uses CMake, not Meson. Migration cost with zero benefit. |
| Build backend | scikit-build-core | setuptools + cmake extension | Legacy approach. setuptools CMake support is ad-hoc and poorly maintained. |
| CFFI mode | ABI-mode (keep) | API-mode (compile at install) | ABI-mode means no compiler needed at install time. Do NOT switch. |
| Wheel repair (Win) | delvewheel (optional) | None | Without delvewheel, MSVC runtime may not be bundled. Start without it, add if needed. |
| CI wheel tool | cibuildwheel | Manual CI matrix | cibuildwheel handles containers, repair, testing automatically. Manual setup is error-prone. |
| Publishing | Trusted publishing (OIDC) | API token | Trusted publishing is more secure (no long-lived secrets), officially recommended by PyPI. |

## What Changes from Current Setup

| Component | Current | New |
|-----------|---------|-----|
| `pyproject.toml` build-backend | `hatchling.build` | `scikit_build_core.build` |
| `pyproject.toml` build-system.requires | `["hatchling"]` | `["scikit-build-core>=0.11"]` |
| `src/CMakeLists.txt` | No SKBUILD install rules | SKBUILD-guarded install targets for quiver + quiver_c |
| `_loader.py` | Walks up to find build/ directory | Checks `_libs/` subdirectory first, then dev fallback |
| CI workflow | None for wheels | New `wheels.yml` with cibuildwheel + publish |

## Installation

### Build dependencies (in pyproject.toml, handled automatically)
```toml
[build-system]
requires = ["scikit-build-core>=0.11"]
build-backend = "scikit_build_core.build"
```

### CI dependencies
```bash
# cibuildwheel: installed via GitHub Actions step
pip install cibuildwheel

# auditwheel: pre-installed in manylinux containers (no action needed)

# delvewheel: installed via CIBW_BEFORE_BUILD_WINDOWS (if needed)
pip install delvewheel
```

### Development dependencies (unchanged)
```bash
pip install cffi>=2.0.0 pytest ruff dotenv
```

## Version Constraints

| Dependency | Min Version | Reason |
|------------|-------------|--------|
| scikit-build-core | 0.11 | Stable release with SKBUILD variable, wheel.packages support |
| cibuildwheel | 2.22 | Python 3.13 support, manylinux_2_28 defaults |
| Python | 3.13 | Project requirement (already set) |
| CMake | 3.21 | Project requirement (already set in CMakeLists.txt) |

## What NOT to Change

1. **Do NOT switch to CFFI API-mode.** ABI-mode means zero compiler at install time.
2. **Do NOT use the scikit-build-core hatchling plugin.** It is experimental.
3. **Do NOT add sdist support.** Complex CMake dependencies make source builds fragile.
4. **Do NOT support macOS yet.** Scoped to Windows x64 + Linux x64.
5. **Do NOT create separate native lib and Python binding packages.** Bundle everything in one wheel.

## Sources

- [scikit-build-core PyPI](https://pypi.org/project/scikit-build-core/) -- version history, feature set
- [scikit-build-core docs](https://scikit-build-core.readthedocs.io/en/stable/) -- configuration reference
- [scikit-build-core hatchling plugin](https://scikit-build-core.readthedocs.io/en/stable/plugins/hatchling.html) -- confirmed experimental
- [cibuildwheel docs](https://cibuildwheel.pypa.io/en/stable/) -- setup, options, platform support
- [delvewheel GitHub](https://github.com/adang1345/delvewheel) -- Windows wheel repair
- [auditwheel GitHub](https://github.com/pypa/auditwheel) -- Linux wheel repair
- [gh-action-pypi-publish](https://github.com/pypa/gh-action-pypi-publish) -- trusted publishing setup
- [PyPI trusted publishers](https://docs.pypi.org/trusted-publishers/) -- OIDC configuration
