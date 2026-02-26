---
phase: 02-loader-rewrite
verified: 2026-02-25T22:00:00Z
status: human_needed
score: 5/6 must-haves verified automatically
re_verification: false
human_verification:
  - test: "Run `scripts/test-wheel-install.bat` from the project root in a clean environment"
    expected: "Wheel builds, installs in a temp venv without PATH setup, `_load_source` is 'bundled', `version()` returns a non-empty string, and the full test suite passes"
    why_human: "Plan 02 Task 2 was an explicit human-verify checkpoint. The scripts exist and are correctly wired, but the actual wheel build + bundled load path requires executing the wheel install flow. The SUMMARY records user approval (2026-02-25T21:53:00Z). Recording here for audit trail."
---

# Phase 2: Loader Rewrite Verification Report

**Phase Goal:** Python code discovers and loads bundled native libraries from an installed wheel, with fallback for development
**Verified:** 2026-02-25T22:00:00Z
**Status:** human_needed (all automated checks pass; one truth confirmed by prior human checkpoint)
**Re-verification:** No — initial verification

## Goal Achievement

### Observable Truths

| #  | Truth                                                                                     | Status     | Evidence                                                                                       |
|----|-------------------------------------------------------------------------------------------|------------|-----------------------------------------------------------------------------------------------|
| 1  | Bundled _libs/ discovery checks specific lib file existence (not just directory)          | VERIFIED   | `_loader.py` line 31-32: `c_api_path = _LIBS_DIR / _LIB_C_API; if c_api_path.exists()`      |
| 2  | When bundled libs fail to load, RuntimeError raised immediately with no dev fallback      | VERIFIED   | `_loader.py` lines 43-47: `except OSError as e: raise RuntimeError(...) from None` — no fallthrough |
| 3  | When _libs/ is missing or empty, dev-mode fallback loads from system PATH                 | VERIFIED   | `_loader.py` lines 49-66: bare-name `ffi.dlopen` with `_load_source = "development"`         |
| 4  | os.add_dll_directory() called on Windows for bundled lib dir; handle kept alive           | VERIFIED   | `_loader.py` lines 35-38: `if sys.platform == "win32": _dll_dir_handle = os.add_dll_directory(str(_LIBS_DIR))` |
| 5  | _load_source attribute at module level indicates which strategy succeeded                 | VERIFIED   | `_loader.py` line 15: `_load_source: str = ""`; set to `"bundled"` (line 41) or `"development"` (line 59) |
| 6  | Linux RPATH $ORIGIN set on libquiver_c for SKBUILD builds                                | VERIFIED   | `CMakeLists.txt` lines 118-123: `if(NOT WIN32)` guard with `INSTALL_RPATH "$ORIGIN"` on `quiver_c` |
| 7  | Wheel installs in clean venv and import quiverdb loads from bundled _libs/ without PATH  | HUMAN      | Scripts exist and correctly wired. Human checkpoint approved per 02-02-SUMMARY.md (2026-02-25T21:53Z). End-to-end execution cannot be reproduced programmatically. |

**Automated Score:** 6/6 automated truths verified
**Human checkpoint:** 1 item (prior approval documented in SUMMARY)

### Required Artifacts

| Artifact                                            | Expected                                             | Status     | Details                                                                        |
|-----------------------------------------------------|------------------------------------------------------|------------|--------------------------------------------------------------------------------|
| `bindings/python/src/quiverdb/_loader.py`           | Bundled-first discovery with dev-mode fallback       | VERIFIED   | 80 lines; contains `_load_source`, `_dll_dir_handle`, two-step fallback chain, `__main__` diagnostic block |
| `CMakeLists.txt`                                    | INSTALL_RPATH $ORIGIN for libquiver_c in SKBUILD     | VERIFIED   | Lines 113-124: SKBUILD block with `INSTALL_RPATH "$ORIGIN"` guarded by `NOT WIN32` |
| `scripts/test-wheel-install.bat`                    | End-to-end wheel install validation script           | VERIFIED   | 130 lines; builds wheel, creates temp venv, installs without PATH, runs validation and full test suite |
| `scripts/validate_wheel_install.py`                 | Import + _load_source + version() validation script  | VERIFIED   | 57 lines; checks `_load_source == "bundled"`, `version()` non-empty, prints diagnostics |

### Key Link Verification

| From                                                     | To                                            | Via                                        | Status   | Details                                                               |
|----------------------------------------------------------|-----------------------------------------------|--------------------------------------------|----------|-----------------------------------------------------------------------|
| `bindings/python/src/quiverdb/_loader.py`                | `bindings/python/src/quiverdb/_c_api.py`      | `load_library(ffi)` called from `get_lib()` | WIRED    | `_c_api.py` lines 357-359: `from quiverdb._loader import load_library; _lib = load_library(ffi)` |
| `CMakeLists.txt` SKBUILD block                           | `bindings/python/src/quiverdb/_loader.py`     | RPATH ensures libquiver_c.so finds libquiver.so at runtime | VERIFIED | `INSTALL_RPATH "$ORIGIN"` in `CMakeLists.txt` line 121, `NOT WIN32` guard at line 119 |
| `scripts/test-wheel-install.bat`                         | `bindings/python/src/quiverdb/_loader.py`     | Installs wheel then imports quiverdb to exercise bundled discovery | WIRED    | Script invokes `validate_wheel_install.py` which does `import quiverdb` and checks `_load_source` |

### Requirements Coverage

| Requirement | Source Plans    | Description                                                                          | Status    | Evidence                                                                                              |
|-------------|-----------------|--------------------------------------------------------------------------------------|-----------|-------------------------------------------------------------------------------------------------------|
| LOAD-01     | 02-01, 02-02    | `_loader.py` discovers bundled native libs from `_libs/` subdirectory relative to package | SATISFIED | `_LIBS_DIR = _PACKAGE_DIR / "_libs"`, `c_api_path.exists()` check before `ffi.dlopen(absolute_path)` |
| LOAD-02     | 02-01, 02-02    | `_loader.py` falls back to development PATH discovery when bundled libs not found    | SATISFIED | Dev-mode block lines 49-66: bare-name `ffi.dlopen`, `_load_source = "development"`                   |
| LOAD-03     | 02-01, 02-02    | Windows `os.add_dll_directory()` registers bundled lib directory for transitive DLL resolution | SATISFIED | `_dll_dir_handle = os.add_dll_directory(str(_LIBS_DIR))` on `sys.platform == "win32"`, handle at module scope |

No orphaned requirements — all three LOAD IDs appear in both plans' `requirements` fields and all are mapped to Phase 2 in `REQUIREMENTS.md`.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| — | — | — | — | No anti-patterns detected |

No TODO/FIXME/HACK/PLACEHOLDER comments. No empty implementations. No return-null stubs. No macOS/darwin code paths in `_loader.py`.

### Human Verification Required

#### 1. End-to-End Wheel Install (Prior Checkpoint — Already Approved)

**Test:** Run `scripts/test-wheel-install.bat` from the project root with no PATH manipulation for `build/bin/`.
**Expected:**
- Wheel builds successfully via `uv build --wheel`
- Clean venv created in `%TEMP%`
- Wheel installs via `uv pip install`
- `validate_wheel_install.py` reports `PASS: _load_source = 'bundled'` and a valid version string
- Full test suite passes using the venv's pytest (no `build/bin/` in PATH)

**Why human:** Requires building a wheel (scikit-build-core invokes CMake), creating a temporary venv, and verifying that the bundled `_libs/` directory inside the installed wheel is discovered at import time. This cannot be reproduced by static grep analysis. The 02-02-SUMMARY.md records that the human approved this checkpoint on 2026-02-25 at 21:53Z.

### Gaps Summary

None. All automated must-haves pass. The single human checkpoint (wheel install end-to-end) was already approved by the user per 02-02-SUMMARY.md. The phase goal is achieved.

---

## Commit Verification

All commits documented in SUMMARYs exist in `git log`:

| Commit  | Description                                               | Plan  |
|---------|-----------------------------------------------------------|-------|
| `9d3e5bf` | feat(02-01): rewrite _loader.py with bundled-first library discovery | 02-01 |
| `79a407e` | feat(02-01): add Linux RPATH $ORIGIN for libquiver_c in SKBUILD builds | 02-01 |
| `6d0ef36` | feat(02-02): add end-to-end wheel install validation scripts | 02-02 |

---

_Verified: 2026-02-25T22:00:00Z_
_Verifier: Claude (gsd-verifier)_
