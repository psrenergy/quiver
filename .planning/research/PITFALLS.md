# Domain Pitfalls

**Domain:** PyPI distribution of CFFI ABI-mode Python package with bundled native C/C++ shared libraries
**Researched:** 2026-02-25

## Critical Pitfalls

Mistakes that cause broken wheels, failed installs, or rejected uploads.

### Pitfall 1: auditwheel and delvewheel Cannot Detect CFFI dlopen Dependencies

**What goes wrong:** The wheel repair tools (auditwheel on Linux, delvewheel on Windows) work by reading DLL/ELF headers to find statically-linked dependencies (`DT_NEEDED` entries). CFFI ABI-mode loads libraries at runtime via `ffi.dlopen()`, which means the repair tools cannot detect `libquiver.dll`/`libquiver.so` or `libquiver_c.dll`/`libquiver_c.so` as dependencies.

**Why it happens:** This is a fundamental limitation of static binary analysis. `dlopen`/`LoadLibrary` calls are runtime operations invisible to header inspection. Both auditwheel and delvewheel document this explicitly.

**Consequences:** If relying solely on repair tools to bundle libraries, wheels ship without native libraries. `pip install` succeeds but `import quiverdb` crashes with "library not found." This is the most dangerous pitfall because it produces a "silently broken" artifact.

**Prevention:**
- The architecture uses scikit-build-core with CMake `install()` to pre-bundle `libquiver` and `libquiver_c` directly into the wheel's `quiverdb/_libs/` directory during the build step. This happens BEFORE wheel repair. The libraries are already in the wheel; repair tools then handle their transitive dependencies.
- On Linux: auditwheel will see `libquiver_c.so` has `DT_NEEDED` for `libquiver.so`, find it in the wheel, and patch RPATH accordingly. It will also bundle transitive system deps (libstdc++, etc.).
- On Windows: if using delvewheel, pass `--add-path` pointing to the build directory and `--include libquiver.dll --include libquiver_c.dll` to force inclusion of CFFI-loaded DLLs.
- **Always test with a clean venv on a different machine** after building the wheel. Never rely only on the build machine where libs are already on PATH.

**Detection:** `pip install quiverdb && python -c "import quiverdb; print(quiverdb.version())"` fails with `OSError: cannot load library`.

**Confidence:** HIGH -- documented limitation of both auditwheel and delvewheel.

**Sources:**
- [auditwheel GitHub](https://github.com/pypa/auditwheel) -- "dependencies that are loaded via [dlopen] will be missed"
- [delvewheel GitHub](https://github.com/adang1345/delvewheel) -- "DLLs loaded at runtime using ctypes/cffi will be missed"

---

### Pitfall 2: Windows DLL Dependency Chain Loading Order

**What goes wrong:** `libquiver_c.dll` depends on `libquiver.dll`. When both are bundled in the wheel, `ffi.dlopen("libquiver_c.dll")` fails because Windows cannot find `libquiver.dll` in its search path. The error is typically `OSError: [WinError 126] The specified module could not be found` -- misleadingly pointing to `libquiver_c.dll` when the real missing file is `libquiver.dll`.

**Why it happens:** Since Python 3.8, Windows DLL search order no longer includes the CWD or PATH for DLL dependencies. Even if both DLLs are in the same directory, Windows may not look there for transitive dependencies of a loaded DLL.

**Consequences:** Import fails on Windows with a confusing error that suggests the main library is missing when actually it is the dependency that cannot be found.

**Prevention:**
- Use `os.add_dll_directory(str(libs_dir))` before any `ffi.dlopen()` call. This registers the `_libs/` directory in the Windows DLL search path.
- Pre-load `libquiver.dll` via `ffi.dlopen()` before `libquiver_c.dll`. The existing loader already does this; the updated `_loader.py` preserves this pattern.
- Use absolute paths with path separators in `ffi.dlopen()`. CFFI >= 1.17 detects path separators and passes `LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR` to `LoadLibraryEx()`.

**Detection:** Windows-only failure. `import quiverdb` raises `OSError` on Windows but works on Linux.

**Confidence:** HIGH -- well-documented Windows DLL loading behavior.

**Sources:**
- [CFFI whatsnew](https://cffi.readthedocs.io/en/stable/whatsnew.html) -- LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR behavior
- [Python 3.8 changelog](https://docs.python.org/3/whatsnew/3.8.html) -- DLL search path changes

---

### Pitfall 3: manylinux2014 Does Not Support C++20

**What goes wrong:** The project uses C++20 (`CMAKE_CXX_STANDARD 20`). If cibuildwheel uses the `manylinux2014` image (CentOS 7 base, old GCC), the CMake build fails or produces binaries linked against an old `libstdc++` that crashes at runtime.

**Why it happens:** manylinux2014's toolchain does not fully support C++20. As of cibuildwheel >= 2.22, the default switched to `manylinux_2_28`, but explicit configuration is safer than relying on defaults.

**Consequences:** Build failure in CI, or silent ABI incompatibility.

**Prevention:** Explicitly set in the cibuildwheel configuration:
```
CIBW_MANYLINUX_X86_64_IMAGE=manylinux_2_28
```

The manylinux_2_28 image is based on AlmaLinux 8 with GCC 12+, which fully supports C++20.

**Detection:** CMake errors about unsupported C++ features, or runtime errors referencing `GLIBCXX_3.4.29` or similar missing symbol versions.

**Confidence:** HIGH -- documented on cibuildwheel's C++ standards page.

**Sources:**
- [cibuildwheel C++ standards](https://cibuildwheel.pypa.io/en/stable/cpp_standards/)

---

### Pitfall 4: cmake.source-dir Pointing Outside Package Directory

**What goes wrong:** scikit-build-core is configured with `cmake.source-dir = "../.."` to point from `bindings/python/` up to the repository root where `CMakeLists.txt` lives. This is an unusual layout. If scikit-build-core resolves paths incorrectly or if cibuildwheel changes the working directory, the CMake build fails to find source files.

**Why it happens:** Most scikit-build-core projects have `CMakeLists.txt` in the same directory as `pyproject.toml`. The quiver project has the CMake build system at the repo root with the Python package in a subdirectory.

**Consequences:** Build fails during `pip wheel` with "CMakeLists.txt not found" or similar path resolution error.

**Prevention:**
- Test locally with `pip wheel bindings/python/ --no-build-isolation` first to verify path resolution.
- If `cmake.source-dir` does not work reliably, alternative: create a thin `bindings/python/CMakeLists.txt` that uses `add_subdirectory(../../src ...)` or `ExternalProject_Add` to build the root project.
- Another alternative: move `pyproject.toml` to the repo root and adjust `wheel.packages` to point to `bindings/python/src/quiverdb`.

**Detection:** Build fails in the CMake configure step.

**Confidence:** MEDIUM -- `cmake.source-dir` is documented, but this specific layout is unusual and needs testing.

---

### Pitfall 5: GitHub Actions Trusted Publisher Configuration Mismatch

**What goes wrong:** The publish workflow runs successfully but PyPI rejects the upload with `invalid-publisher`. The OIDC token does not match the registered publisher.

**Why it happens:** Multiple configuration values must match exactly between the PyPI trusted publisher registration and the GitHub Actions workflow:
1. Repository owner (case-sensitive)
2. Repository name (must match current name, not pre-rename)
3. Workflow filename (exact match, including `.github/workflows/wheels.yml`)
4. Environment name (if configured, must match the `environment:` in the workflow)
5. The workflow must have `id-token: write` permission

**Consequences:** Wheels build successfully but cannot be uploaded. Release is blocked.

**Prevention:**
- Register the trusted publisher on PyPI BEFORE the first publish attempt.
- Use a dedicated `environment: pypi` in the workflow and match it on PyPI.
- Set permissions explicitly: `permissions: { id-token: write }`.
- Keep build and publish in separate jobs.
- Use the official `pypa/gh-action-pypi-publish` action.
- Verify project name matches between `pyproject.toml` and PyPI (watch for `-` vs `_` normalization: `quiverdb` is the same either way here).

**Detection:** Workflow fails at the upload step with OIDC token errors.

**Confidence:** HIGH -- documented in PyPI trusted publisher troubleshooting.

**Sources:**
- [PyPI Trusted Publishers docs](https://docs.pypi.org/trusted-publishers/)
- [PyPI Trusted Publishers troubleshooting](https://docs.pypi.org/trusted-publishers/troubleshooting/)

## Moderate Pitfalls

### Pitfall 6: Version Drift Between CMakeLists.txt and pyproject.toml

**What goes wrong:** The C++ library version in `CMakeLists.txt` (`project(quiver VERSION 0.4.0)`) and the Python package version in `pyproject.toml` (`version = "0.4.0"`) are maintained separately. When one is bumped and the other is not, the Python package reports a different version than the native library it bundles.

**Prevention:**
- Simplest: have a CI step that checks both versions match before building wheels.
- Alternative: read version from a single `VERSION` file in both CMake (`file(READ ...)`) and pyproject.toml (scikit-build-core supports `version.provider = "scikit_build_core.metadata.regex"` for dynamic version extraction).
- The current approach (static in both files) is fine as long as the CI check catches drift.

**Detection:** `quiverdb.version()` (C library) differs from `importlib.metadata.version("quiverdb")` (Python package).

---

### Pitfall 7: Linux RPATH Not Set for Bundled Library Inter-Dependencies

**What goes wrong:** On Linux, `libquiver_c.so` links against `libquiver.so`. When both are bundled in the wheel's `_libs/` directory, the dynamic linker cannot find `libquiver.so` because its RPATH points to the original build directory, not `$ORIGIN`.

**Prevention:**
- auditwheel's repair step patches RPATH for libraries it detects via `DT_NEEDED`. Since `libquiver_c.so` has a `DT_NEEDED` for `libquiver.so` and both are in the wheel, auditwheel should handle this correctly.
- As a belt-and-suspenders approach, the `_loader.py` pre-loads `libquiver.so` before `libquiver_c.so`, which keeps it in process memory. This makes RPATH irrelevant for this specific dependency.
- If auditwheel moves libraries to a different directory within the wheel, verify RPATH is updated accordingly.

**Detection:** Linux-only `OSError: libquiver.so: cannot open shared object file` when importing.

---

### Pitfall 8: auditwheel Moves Libraries to Unexpected Location

**What goes wrong:** auditwheel repair may move bundled libraries from `quiverdb/_libs/` to `quiverdb.libs/` (at the wheel root level with name-mangling hashes). The `_loader.py` checks `_libs/` but not `quiverdb.libs/`, so loading fails after repair.

**Prevention:**
- Test the full flow: build wheel, repair wheel, install in clean venv, verify import works.
- If auditwheel moves libraries, either: (a) add `quiverdb.libs/` as another search path in `_loader.py`, or (b) use `auditwheel repair --exclude libquiver.so --exclude libquiver_c.so` to tell auditwheel to leave these libraries where they are and only handle transitive deps.
- The `--exclude` approach is simpler since it keeps the wheel layout predictable.

**Detection:** Wheel works before repair but fails after repair.

---

### Pitfall 9: cibuildwheel Builds Too Many Wheel Variants

**What goes wrong:** By default, cibuildwheel builds wheels for every supported Python version and architecture. The project only needs Python >= 3.13, Windows x64, and Linux x64. Without filtering, CI builds dozens of unnecessary wheels, wasting 30+ minutes of CI time.

**Prevention:** Restrict builds explicitly:
```
CIBW_BUILD = "cp313-*"
CIBW_ARCHS_LINUX = "x86_64"
CIBW_ARCHS_WINDOWS = "AMD64"
```

---

### Pitfall 10: SQLite Symbol Conflicts with Python's Bundled SQLite

**What goes wrong:** Python itself ships with a built-in `sqlite3` module linked against SQLite. Quiver's `libquiver.so`/`libquiver.dll` also links against SQLite (via FetchContent static linking). If both are loaded in the same process, symbol conflicts can cause crashes or data corruption.

**Prevention:**
- On Linux: the CMake build uses FetchContent for SQLite, which means it is statically linked into `libquiver.so`. Combined with hidden visibility (`CMAKE_CXX_VISIBILITY_PRESET hidden`), SQLite symbols should not leak. Verify with `nm -D libquiver.so | grep sqlite3` -- symbols should not appear.
- On Windows: DLL symbol isolation is the default (each DLL gets its own copy), so this is less of a concern.
- Test by running `import sqlite3; import quiverdb` in the same process and verifying both work.

**Detection:** Rare crashes or "database disk image is malformed" when using `quiverdb` alongside Python's `sqlite3`.

**Confidence:** MEDIUM -- the hidden visibility + static linking combination should prevent this, but needs explicit verification.

---

### Pitfall 11: scikit-build-core CMake FetchContent Slow in CI

**What goes wrong:** The CMake build uses FetchContent to download SQLite, spdlog, lua, sol2, tomlplusplus, and rapidcsv. Each cibuildwheel invocation starts a fresh build, downloading all dependencies. On Linux, this happens inside a Docker container with no persistent cache.

**Prevention:**
- scikit-build-core supports `build-dir = "build/{wheel_tag}"` for persistent build directories between invocations, but this only helps for multiple Python version builds (not relevant here with cp313 only).
- For Linux manylinux containers, caching is harder. Accept the download time (~1-2 minutes) for the initial release.
- Long-term: use GitHub Actions cache to cache the `_deps/` directory between CI runs.

**Detection:** CI builds take 10+ minutes when they should take 3-4 minutes.

## Minor Pitfalls

### Pitfall 12: cffi Package Version Compatibility

**What goes wrong:** The project requires `cffi>=2.0.0` but CFFI 2.0 was a major release. If a user has an older CFFI installed and pip does not upgrade it, the import may fail.

**Prevention:** The `>=2.0.0` pin in `pyproject.toml` dependencies tells pip to install/upgrade cffi. This should work correctly in all normal scenarios. No action needed unless users report issues.

---

### Pitfall 13: Windows MSVC Runtime Dependency

**What goes wrong:** Libraries built with MSVC depend on the Visual C++ runtime (`vcruntime140.dll`). If users do not have the Visual C++ Redistributable installed, the DLL fails to load.

**Prevention:**
- Most Windows systems have the VC++ runtime installed (Python itself bundles it).
- If this becomes an issue, add delvewheel to the Windows wheel repair step -- it detects and bundles MSVC runtime DLLs.
- Alternative: build with `/MT` (static CRT) in CMake to eliminate the dependency: `set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")`

**Detection:** `OSError` mentioning `vcruntime140.dll` on a clean Windows machine.

---

### Pitfall 14: CMake Not Available in manylinux Container

**What goes wrong:** The manylinux Docker container used by cibuildwheel does not include CMake. scikit-build-core auto-installs it, but if the version is too old for the project's `cmake_minimum_required(VERSION 3.21)`, the build fails.

**Prevention:** scikit-build-core >= 0.11 auto-installs a recent CMake (via pip) during the build process. This is a solved problem -- scikit-build-core lists `cmake>=3.15` and `ninja` as build dependencies and installs them automatically. No manual `CIBW_BEFORE_BUILD` needed for CMake.

**Detection:** CMake version errors during configure step.

---

### Pitfall 15: Wheel Artifact Not Preserved Between GitHub Actions Jobs

**What goes wrong:** Build and publish are separate jobs. Wheel artifacts from the build job are not available in the publish job unless explicitly passed.

**Prevention:** Use `actions/upload-artifact` in the build job and `actions/download-artifact` with `merge-multiple: true` in the publish job. Use matching artifact name patterns.

## Phase-Specific Warnings

| Phase Topic | Likely Pitfall | Mitigation |
|-------------|---------------|------------|
| pyproject.toml migration | Pitfall 4 (cmake.source-dir) | Test locally first, have fallback plan for path resolution |
| _loader.py rewrite | Pitfall 2 (DLL chain), Pitfall 7 (RPATH) | Pre-load core lib first, use os.add_dll_directory on Windows |
| CMake install targets | Pitfall 1 (dlopen invisible to repair) | SKBUILD-guarded install puts libs in wheel before repair step |
| cibuildwheel setup | Pitfall 3 (C++20/manylinux), Pitfall 9 (too many variants) | Force manylinux_2_28, restrict CIBW_BUILD to cp313 |
| Linux wheel repair | Pitfall 8 (auditwheel moves libs), Pitfall 10 (SQLite conflicts) | Test post-repair wheel, verify hidden symbol visibility |
| Windows wheel | Pitfall 2 (DLL loading), Pitfall 13 (MSVC runtime) | os.add_dll_directory, test on clean Windows |
| PyPI publish | Pitfall 5 (trusted publisher mismatch), Pitfall 15 (artifact passing) | Pre-register publisher, use upload/download-artifact |
| Version management | Pitfall 6 (version drift) | CI check that CMake and pyproject.toml versions match |

## Sources

- [auditwheel - GitHub](https://github.com/pypa/auditwheel) -- dlopen limitation documented
- [delvewheel - GitHub](https://github.com/adang1345/delvewheel) -- --add-path, --include flags, CFFI limitation
- [cibuildwheel tips and tricks](https://cibuildwheel.pypa.io/en/stable/faq/) -- native library bundling
- [cibuildwheel C++ standards](https://cibuildwheel.pypa.io/en/stable/cpp_standards/) -- manylinux_2_28 for C++20
- [cibuildwheel options](https://cibuildwheel.pypa.io/en/stable/options/) -- CIBW_BUILD, CIBW_REPAIR_WHEEL_COMMAND
- [scikit-build-core docs](https://scikit-build-core.readthedocs.io/en/stable/) -- cmake.source-dir, build procedure
- [scikit-build-core dynamic linking](https://scikit-build-core.readthedocs.io/en/stable/guide/dynamic_link.html) -- auditwheel/delvewheel integration
- [PyPI Trusted Publishers](https://docs.pypi.org/trusted-publishers/) -- OIDC setup and troubleshooting
- [CFFI whatsnew](https://cffi.readthedocs.io/en/stable/whatsnew.html) -- DLL loading improvements
- [Python 3.8 changelog](https://docs.python.org/3/whatsnew/3.8.html) -- DLL search path changes
