# Research Summary: quiverdb PyPI Distribution

**Domain:** PyPI distribution of Python package with bundled native C/C++ shared libraries
**Researched:** 2026-02-25
**Overall confidence:** MEDIUM-HIGH

## Executive Summary

Publishing `quiverdb` to PyPI requires replacing the current hatchling build backend with scikit-build-core, which natively integrates CMake builds with Python wheel packaging. The key architectural insight is that scikit-build-core invokes CMake as part of the wheel build process, and CMake `install()` targets place the native libraries directly into the wheel's package directory. This happens before wheel repair tools run, which is critical because CFFI ABI-mode's `ffi.dlopen()` is invisible to repair tools like auditwheel and delvewheel.

The `_loader.py` module needs a targeted rewrite to first check a `_libs/` subdirectory relative to `__file__` (the installed wheel case), falling back to the current development-mode directory walk and then system PATH. On Windows, `os.add_dll_directory()` must register the bundled library directory so that `libquiver_c.dll` can find its dependency `libquiver.dll`.

The CI pipeline uses cibuildwheel, which handles the full build-test-repair cycle on each platform. On Linux, it runs inside manylinux_2_28 Docker containers (required for C++20 support). On Windows, it runs natively. scikit-build-core auto-installs CMake and ninja inside the build environment, so no manual tool installation is needed. Publishing uses PyPI trusted publishing (OIDC) from GitHub Actions, eliminating API token management.

The total code changes are modest: ~5 lines added to `src/CMakeLists.txt`, ~15 lines changed in `pyproject.toml`, ~40 lines rewritten in `_loader.py`, and a new ~60-line GitHub Actions workflow. The risk is concentrated in two areas: the `cmake.source-dir = "../.."` path resolution (unusual layout, needs testing) and the auditwheel interaction with pre-bundled CFFI libraries (may move files to unexpected locations).

## Key Findings

**Stack:** scikit-build-core as build backend (replaces hatchling), cibuildwheel for CI, auditwheel/delvewheel for wheel repair, trusted publishing for PyPI.

**Architecture:** CMake `install()` targets guarded by `SKBUILD` variable place native libs into `quiverdb/_libs/` inside the wheel. `_loader.py` discovers them via `Path(__file__).parent / "_libs"`. cibuildwheel orchestrates the full pipeline.

**Critical pitfall:** auditwheel and delvewheel cannot detect CFFI dlopen dependencies. Libraries MUST be pre-bundled via CMake install, not rely on repair tools to discover them.

## Implications for Roadmap

Based on research, suggested phase structure:

1. **Build system migration** - Switch pyproject.toml from hatchling to scikit-build-core, add SKBUILD-guarded CMake install targets
   - Addresses: CMake-to-wheel integration, platform-specific wheel tags
   - Avoids: Pitfall 4 (cmake.source-dir), test path resolution early

2. **Loader update** - Rewrite _loader.py for bundled library discovery
   - Addresses: Self-contained pip install, Windows DLL loading
   - Avoids: Pitfall 2 (DLL chain), Pitfall 7 (RPATH)

3. **Local validation** - Build wheel locally, install in clean venv, verify all tests pass
   - Addresses: End-to-end verification before CI investment
   - Avoids: Pitfall 1 (invisible dlopen), Pitfall 8 (auditwheel moves libs)

4. **CI workflow** - GitHub Actions with cibuildwheel for Windows + Linux
   - Addresses: Automated cross-platform wheel building
   - Avoids: Pitfall 3 (C++20/manylinux), Pitfall 9 (too many variants)

5. **PyPI publishing** - Trusted publisher setup, publish job in workflow
   - Addresses: Automated release pipeline
   - Avoids: Pitfall 5 (OIDC mismatch)

**Phase ordering rationale:**
- Build system migration must come first because everything depends on scikit-build-core producing a correct wheel.
- Loader update is next because it is the only Python code change, and local testing requires it.
- Local validation before CI avoids debugging build system issues inside Docker containers.
- CI workflow depends on the build producing correct wheels locally.
- Publishing is last because it depends on everything else working.

**Research flags for phases:**
- Phase 1: Likely needs experimentation with `cmake.source-dir` path resolution. If it fails, fallback is moving `pyproject.toml` to repo root.
- Phase 3: Needs careful testing of auditwheel's behavior with pre-bundled libraries. May need `--exclude` flags.
- Phase 4: Standard cibuildwheel patterns, unlikely to need additional research.

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | scikit-build-core + cibuildwheel is the established pattern for CMake+Python |
| Features | HIGH | Requirements are well-defined in PROJECT.md, standard packaging features |
| Architecture | MEDIUM-HIGH | Core pattern is proven; cmake.source-dir layout and auditwheel interaction need testing |
| Pitfalls | HIGH | All critical pitfalls are documented in official tool documentation |

## Gaps to Address

- **cmake.source-dir="../.." path resolution**: Needs local testing. If it fails, alternative is restructuring the pyproject.toml location or using a thin CMakeLists.txt wrapper.
- **auditwheel behavior with pre-bundled CFFI libs**: Does it move them? Does it patch RPATH correctly? Does `--exclude` work as expected? Needs testing during local validation phase.
- **Windows without delvewheel**: The recommendation is to skip delvewheel initially (rely on CMake install + os.add_dll_directory). If MSVC runtime is missing on clean systems, delvewheel must be added. Testable only on a clean Windows VM.
- **SQLite symbol conflicts**: Hidden visibility + static linking should prevent conflicts, but `nm -D libquiver.so | grep sqlite3` should be checked during local validation.
- **FetchContent download time in manylinux containers**: May slow CI. Not blocking, but GitHub Actions caching could help long-term.
