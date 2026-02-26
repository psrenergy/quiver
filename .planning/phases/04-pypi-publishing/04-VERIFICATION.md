---
phase: 04-pypi-publishing
verified: 2026-02-25T22:00:00Z
status: human_needed
score: 6/6 must-haves verified
re_verification: false
human_verification:
  - test: "Push a v*.*.* tag and observe CI"
    expected: "publish.yml workflow triggers, builds 2 wheels, validates tag vs pyproject.toml version, creates a GitHub Release with wheel attachments, and publishes to PyPI"
    why_human: "Cannot trigger a real GitHub Actions run or verify PyPI upload from static analysis"
  - test: "pip install quiverdb on a clean Windows machine after first publish"
    expected: "Package installs without errors and 'import quiverdb' works without any system-level DLL setup"
    why_human: "Requires a live PyPI publish and a clean environment — cannot verify statically"
  - test: "pip install quiverdb on a clean Linux machine after first publish"
    expected: "Package installs the manylinux wheel and 'import quiverdb' works without any system-level setup"
    why_human: "Requires a live PyPI publish and a clean environment — cannot verify statically"
  - test: "Confirm PyPI pending trusted publisher is registered"
    expected: "PyPI shows a pending publisher for project quiverdb, owner matching the repo, workflow publish.yml, environment pypi"
    why_human: "PyPI dashboard is not accessible programmatically — requires browser login to verify"
---

# Phase 04: PyPI Publishing Verification Report

**Phase Goal:** Tagged releases automatically publish tested wheels to PyPI with no manual intervention
**Verified:** 2026-02-25
**Status:** human_needed
**Re-verification:** No — initial verification

## Goal Achievement

### Observable Truths

| #   | Truth                                                                  | Status     | Evidence                                                                                          |
| --- | ---------------------------------------------------------------------- | ---------- | ------------------------------------------------------------------------------------------------- |
| 1   | Pushing a v*.*.* tag triggers the publish workflow                     | VERIFIED   | `on.push.tags: ["v*.*.*"]` at line 6 of publish.yml                                              |
| 2   | Workflow builds fresh wheels via cibuildwheel for Windows and Linux    | VERIFIED   | build job matrix: [ubuntu-latest, windows-latest], cibuildwheel@v3.3.1, package-dir: bindings/python |
| 3   | Tag version validated against pyproject.toml version before publish    | VERIFIED   | validate job: tomllib extracts version from bindings/python/pyproject.toml, exits 1 on mismatch  |
| 4   | Exactly 2 wheels validated before publish                              | VERIFIED   | validate job: `WHEEL_COUNT=$(ls dist/*.whl | wc -l)` checked against `-ne 2`, exits 1 on failure |
| 5   | GitHub Release auto-created with notes and wheel attachments           | VERIFIED   | release job: softprops/action-gh-release@v2 with generate_release_notes: true and files: dist/*.whl |
| 6   | Wheels published to PyPI via OIDC trusted publishing (no API tokens)   | VERIFIED   | publish job: id-token: write permission, environment: pypi, pypa/gh-action-pypi-publish@release/v1 with no token arguments |

**Score:** 6/6 truths verified

### Required Artifacts

| Artifact                              | Expected                                                    | Status     | Details                                                                   |
| ------------------------------------- | ----------------------------------------------------------- | ---------- | ------------------------------------------------------------------------- |
| `.github/workflows/publish.yml`       | Tag-triggered publish pipeline: build, merge, validate, release, publish | VERIFIED | 102-line file, all 5 jobs present, commit 481433e |
| `bindings/python/pyproject.toml`      | [tool.cibuildwheel] configuration reused by publish.yml     | VERIFIED   | build = ["cp313-win_amd64", "cp313-manylinux_x86_64"], test-requires and test-command present |

### Key Link Verification

| From                                     | To                                  | Via                                   | Status   | Details                                                                          |
| ---------------------------------------- | ----------------------------------- | ------------------------------------- | -------- | -------------------------------------------------------------------------------- |
| publish.yml validate job                 | bindings/python/pyproject.toml      | tomllib version extraction            | VERIFIED | Line 56: `tomllib.load(open('bindings/python/pyproject.toml','rb'))['project']['version']` |
| publish.yml publish job                  | PyPI                                | OIDC trusted publishing               | VERIFIED | Line 95: `id-token: write` at job level only; line 93: `environment: pypi`; line 102: pypa/gh-action-pypi-publish@release/v1 with no token |
| publish.yml release job                  | GitHub Releases                     | softprops/action-gh-release           | VERIFIED | Line 84-87: softprops/action-gh-release@v2 with generate_release_notes: true    |

### Requirements Coverage

| Requirement | Source Plan | Description                                          | Status   | Evidence                                                                 |
| ----------- | ----------- | ---------------------------------------------------- | -------- | ------------------------------------------------------------------------ |
| CI-04       | 04-01-PLAN  | PyPI trusted publisher (OIDC) publishes wheels on tagged release | VERIFIED | publish.yml: id-token:write, environment:pypi, pypa/gh-action-pypi-publish@release/v1 |

No orphaned requirements: REQUIREMENTS.md maps only CI-04 to Phase 4, and the plan claims exactly CI-04.

### Anti-Patterns Found

| File                                  | Line | Pattern                  | Severity | Impact                                                                           |
| ------------------------------------- | ---- | ------------------------ | -------- | -------------------------------------------------------------------------------- |
| `.github/workflows/publish.yml`       | 35   | upload-artifact/merge@v4 while build uses upload-artifact@v6 | INFO | Mixed versions for upload-artifact and its merge action — v4 merge is the correct companion action for the merge pattern; v6 is used for regular uploads. No functional issue. |

No TODO/FIXME/placeholder comments. No stub implementations. No API tokens or secrets referenced anywhere in the workflow. No workflow-level `permissions:` block (least-privilege correctly applied at job level only).

### Human Verification Required

#### 1. End-to-End Pipeline Test

**Test:** Push a version tag (`git tag v0.5.0 && git push --tags`) to the repository.
**Expected:** GitHub Actions triggers the Publish workflow. All 5 jobs run in order (build -> merge -> validate -> release // publish). The release job creates a GitHub Release named v0.5.0 with auto-generated notes and two .whl files attached. The publish job uploads both wheels to PyPI under the quiverdb project.
**Why human:** Cannot trigger a live GitHub Actions run or verify PyPI upload programmatically from static analysis. The OIDC token exchange, PyPI trusted publisher lookup, and artifact upload all require a real CI execution.

#### 2. Clean Windows Install Test

**Test:** On a clean Windows machine with Python 3.13+ and no Quiver development setup, run `pip install quiverdb` (after first publish), then open a Python prompt and run `import quiverdb`.
**Expected:** Package installs successfully. Import succeeds without any system-level DLL setup, PATH changes, or missing dependency errors. The bundled libquiver.dll and libquiver_c.dll are discovered from the `_libs/` subdirectory.
**Why human:** Requires a live PyPI publish and a genuinely clean machine — cannot verify DLL bundling behavior or import success from static file analysis.

#### 3. Clean Linux Install Test

**Test:** On a clean Linux machine with Python 3.13+, run `pip install quiverdb`, then run `import quiverdb`.
**Expected:** The manylinux_2_28 wheel installs and the import works without any system library installation or LD_LIBRARY_PATH manipulation.
**Why human:** Same reasons as Windows test — requires live publish and clean environment.

#### 4. PyPI Trusted Publisher Registration

**Test:** Log in to PyPI at https://pypi.org/manage/account/publishing/ and confirm the pending publisher entry exists.
**Expected:** Entry shows: PyPI project name = quiverdb, Repository = quiver2, Workflow name = publish.yml, Environment name = pypi.
**Why human:** PyPI dashboard requires browser login — cannot be verified programmatically. The SUMMARY claims user confirmed this, but verification cannot be done from static analysis.

### Gaps Summary

No automated gaps. All 6 observable truths are verified from the static workflow definition:

- The trigger, job structure, job dependencies, permissions scoping, OIDC configuration, version validation logic, wheel count validation, GitHub Release configuration, and PyPI publish action are all present and correctly wired.
- The workflow contains no API tokens, no stored secrets, and no workflow-level permission grants (least-privilege correctly applied).
- The key link from validate job to pyproject.toml is present and uses the correct path (bindings/python/pyproject.toml).

Status is `human_needed` — not `gaps_found` — because all automated checks pass. The outstanding items are live-execution behaviors that cannot be verified without a real tag push and a clean install environment.

---

_Verified: 2026-02-25_
_Verifier: Claude (gsd-verifier)_
