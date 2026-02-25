# Feature Landscape

**Domain:** PyPI distribution of native-library Python package
**Researched:** 2026-02-25

## Table Stakes

Features users expect from `pip install quiverdb`. Missing = product feels broken.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| `pip install quiverdb` works on Windows x64 | Users on Windows expect one-command install | High | Requires wheel with bundled DLLs, Windows CI |
| `pip install quiverdb` works on Linux x64 | Users on Linux expect one-command install | High | Requires manylinux wheel with bundled .so files |
| No system dependencies needed | CFFI ABI-mode promise: no compiler, no dev headers | Med | CMake install + wheel repair must bundle everything |
| All existing Python API works after install | Users expect full functionality | Low | API is unchanged; only loader changes |
| Correct platform wheel tags | pip must select right wheel for platform | Low | scikit-build-core handles automatically (detects native artifacts) |
| Version number accessible | `quiverdb.version()` must work | Low | Already implemented, depends on libquiver_c loading |

## Differentiators

Features that set the distribution apart. Not expected by default, but add value.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Trusted publishing (OIDC) | No API token secrets to manage | Low | PyPI configuration, not code |
| Automated release on tag push | Push `v0.5.0` tag, wheels auto-published | Med | GitHub Actions workflow |
| CI tests wheels after build | Wheels are tested on target platform before publish | Low | cibuildwheel `test-command` |
| Wheel size optimization | Strip debug symbols, minimize bundled deps | Low | `install.strip = true` in scikit-build-core |
| Sigstore attestations | Supply-chain security, automatic with gh-action-pypi-publish | Low | Free with trusted publishing, no extra config |
| Clear error on lib not found | Instead of cryptic OSError, tell user what went wrong | Low | Wrap dlopen in try/except with informative QuiverError |

## Anti-Features

Features to explicitly NOT build in this milestone.

| Anti-Feature | Why Avoid | What to Do Instead |
|--------------|-----------|-------------------|
| macOS wheels | Two platforms first, validate the pattern, then extend | Add in future milestone after Windows+Linux proven |
| Source distribution (sdist) | C++20 + FetchContent deps makes source builds fragile for end users | Only publish wheels. Users needing source can clone the repo. |
| Python <3.13 support | Binding requires 3.13+, no backport planned | Keep `requires-python = ">=3.13"` |
| TestPyPI staging | Extra complexity, extra trusted publisher setup | Validate wheels locally with `pip install ./wheelhouse/*.whl` |
| Multiple Python versions per platform | 3.13 only for now | Add 3.14 when released, just update CIBW_BUILD |
| ARM/aarch64 wheels | x86_64 only for initial release | Add later with cibuildwheel QEMU cross-compilation |
| Editable install via scikit-build-core | Development workflow uses existing PATH-based approach | scikit-build-core editable installs are complex; keep dev workflow separate |
| Dynamic version from git tags | Adds build-system dependency, complicates local dev | Static version in pyproject.toml, bump manually before tagging |
| CFFI API mode (compile-time) | Requires C compiler at install time, contradicts zero-deps goal | Stay with ABI mode. Already working and tested. |

## Feature Dependencies

```
CMake install targets (SKBUILD guard) --> scikit-build-core packages libs into wheel
scikit-build-core wheel build --> platform-specific wheel tags (automatic)
_loader.py update --> find bundled libs in _libs/ subdirectory
_loader.py update --> os.add_dll_directory() on Windows
All above --> pip install works end-to-end

cibuildwheel config --> GitHub Actions workflow
cibuildwheel --> scikit-build-core wheel build (invoked internally)
cibuildwheel --> auditwheel/delvewheel repair
GitHub Actions workflow --> PyPI publish job
Trusted publishing --> PyPI project config (manual, one-time on pypi.org)
```

## MVP Recommendation

Prioritize (in order):
1. **CMake install targets** -- add SKBUILD-guarded install in src/CMakeLists.txt (~5 lines)
2. **pyproject.toml migration** -- switch build backend from hatchling to scikit-build-core
3. **_loader.py update** -- add _libs/ discovery as first search location, keep dev fallback
4. **Local wheel build test** -- verify `pip wheel bindings/python/` produces working wheel
5. **GitHub Actions workflow** -- cibuildwheel for Windows + Linux
6. **PyPI trusted publisher setup** -- register on pypi.org, add publish job

Defer:
- macOS support: different milestone
- delvewheel: start without it, add only if Windows wheels fail on clean systems
- Multiple Python versions: add when 3.14 releases

## Sources

- [PyPI trusted publishing](https://docs.pypi.org/trusted-publishers/) -- OIDC setup
- [cibuildwheel test-command](https://cibuildwheel.pypa.io/en/stable/options/) -- post-build testing
- [scikit-build-core install.strip](https://scikit-build-core.readthedocs.io/en/stable/) -- binary stripping
- [gh-action-pypi-publish](https://github.com/pypa/gh-action-pypi-publish) -- automatic Sigstore attestations
