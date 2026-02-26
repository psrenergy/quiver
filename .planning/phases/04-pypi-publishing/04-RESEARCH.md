# Phase 4: PyPI Publishing - Research

**Researched:** 2026-02-25
**Domain:** GitHub Actions release automation / PyPI trusted publisher (OIDC) / tag-triggered publishing
**Confidence:** HIGH

## Summary

PyPI Trusted Publishing via OIDC is the standard mechanism for automated package publishing from GitHub Actions. It replaces API tokens with short-lived, automatically-minted credentials based on the GitHub workflow identity. The `pypa/gh-action-pypi-publish@release/v1` action handles the entire OIDC token exchange and upload process with zero configuration when the job has `id-token: write` permission and a matching trusted publisher is registered on PyPI.

The publish workflow will be self-contained: triggered by a `v*.*.*` tag push, it builds wheels fresh via cibuildwheel (same configuration as the existing `build-wheels.yml`), validates that the tag version matches `pyproject.toml`, validates expected wheel count, creates a GitHub Release with auto-generated notes and wheel attachments, then publishes to PyPI. Since `quiverdb` does not yet exist on PyPI, the project must be created via the "pending publisher" mechanism on PyPI before the first publish.

The key architectural decision from CONTEXT.md is that this workflow builds wheels itself (no cross-workflow artifact dependency). This means the workflow is a superset of the existing build-wheels workflow plus validation, release creation, and PyPI upload steps.

**Primary recommendation:** Create `.github/workflows/publish.yml` triggered by `v*.*.*` tag pushes. Build wheels via cibuildwheel, validate tag-vs-pyproject.toml version match and wheel count (2), create GitHub Release with `softprops/action-gh-release@v2` using `generate_release_notes: true`, then publish to PyPI with `pypa/gh-action-pypi-publish@release/v1` using trusted publishing (OIDC). Register `quiverdb` as a pending trusted publisher on PyPI beforehand.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- v-prefixed semver tags: `v0.5.0`, `v0.5.1`, etc.
- Version in `pyproject.toml` is maintained manually -- update before tagging
- Workflow validates tag version matches `pyproject.toml` version; hard fail on mismatch
- Stable releases only -- no pre-release tag support (no rc, beta, alpha patterns)
- Separate workflow file (e.g., `publish.yml`), not merged into existing CI workflow
- Builds wheels fresh via cibuildwheel (self-contained pipeline, no cross-workflow artifact dependency)
- cibuildwheel runs tests as part of its build process -- no additional test step needed
- Wheels only, no sdist (native-bundled package requires CMake/compilers for source builds)
- Auto-create GitHub Release from the tag
- Attach built `.whl` files to the GitHub Release as downloadable assets
- Release body uses GitHub's auto-generated changelog (PR titles since last tag)
- Automatically marked as 'latest' release (no draft/manual promotion step)
- No manual approval gate -- tag push goes straight to build + publish
- Pre-publish validation: tag-vs-pyproject.toml version match AND expected wheel count (2: Windows + Linux)
- No branch restriction on tag triggers -- any tag matching `v*.*.*` publishes
- No auto-retry on PyPI upload failure -- fail and re-run manually (PyPI uploads not idempotent)

### Claude's Discretion
- Exact workflow job structure and step ordering
- GitHub Actions action versions (e.g., pypa/gh-action-pypi-publish version)
- OIDC permission configuration details
- Wheel count validation implementation approach

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| CI-04 | PyPI trusted publisher (OIDC) publishes wheels on tagged release | `pypa/gh-action-pypi-publish@release/v1` with `id-token: write` permission handles OIDC authentication automatically; register pending trusted publisher on PyPI with repo owner/name, workflow filename `publish.yml`, and optional environment `pypi` |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| pypa/gh-action-pypi-publish | release/v1 | Upload wheels to PyPI via OIDC | PyPA official action; the "blessed" way to publish; handles OIDC token exchange, attestation generation, and upload |
| softprops/action-gh-release | v2 (v2.5.0) | Create GitHub Release with assets | Most widely used release action; supports `generate_release_notes`, file glob upload, auto-latest marking |
| pypa/cibuildwheel | v3.3.1 | Build and test wheels | Already used in build-wheels.yml; same configuration reused in publish workflow |
| actions/download-artifact | v4 | Download per-platform wheel artifacts | Downloads merged artifacts within same workflow for publish/release steps |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| actions/checkout | v6 | Repository checkout | Already used in all workflows |
| actions/upload-artifact | v6 | Upload per-platform wheels | Same pattern as build-wheels.yml for artifact staging |
| actions/upload-artifact/merge | v4 | Merge per-platform artifacts | Same merge pattern as build-wheels.yml |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| softprops/action-gh-release | `gh release create` CLI | CLI works but requires manual token setup and more bash scripting; action is declarative and well-tested |
| pypa/gh-action-pypi-publish | `twine upload` + API token | Twine requires long-lived token stored as secret; OIDC is more secure and recommended by PyPA |
| Separate publish.yml | Combined build-wheels.yml with tag condition | User decision: separate file for clarity and different trigger semantics |

## Architecture Patterns

### Recommended Workflow Structure
```
.github/workflows/publish.yml    # New: tag-triggered publish workflow
bindings/python/pyproject.toml   # Existing: version field + cibuildwheel config (no changes)
```

### Pattern 1: Self-Contained Tag-Triggered Publish Workflow
**What:** A single workflow file that builds, validates, releases, and publishes on tag push. No dependency on artifacts from other workflows.
**When to use:** When the publish pipeline must be deterministic and self-contained (per user decision).
**Example:**
```yaml
# Source: CONTEXT.md decision + cibuildwheel docs + PyPI trusted publisher docs
name: Publish

on:
  push:
    tags:
      - "v*.*.*"

jobs:
  build:
    # Build wheels via cibuildwheel (same as build-wheels.yml)
    ...

  merge:
    # Merge per-platform artifacts into single "wheels" artifact
    ...

  validate:
    # Check tag matches pyproject.toml, count wheels
    needs: merge
    ...

  release:
    # Create GitHub Release + attach wheels
    needs: validate
    ...

  publish:
    # Upload to PyPI via OIDC
    needs: validate
    environment: pypi
    permissions:
      id-token: write
    ...
```

### Pattern 2: Tag-vs-Version Validation
**What:** Extract version from `pyproject.toml`, strip `v` prefix from tag, compare. Hard fail on mismatch.
**When to use:** Always -- prevents publishing a misversioned package.
**Example:**
```yaml
# Source: Common CI pattern, verified against community examples
- name: Validate version
  run: |
    TAG_VERSION="${GITHUB_REF_NAME#v}"
    PKG_VERSION=$(python -c "import tomllib; print(tomllib.load(open('bindings/python/pyproject.toml','rb'))['project']['version'])")
    if [ "$TAG_VERSION" != "$PKG_VERSION" ]; then
      echo "::error::Tag version ($TAG_VERSION) does not match pyproject.toml version ($PKG_VERSION)"
      exit 1
    fi
```

### Pattern 3: Wheel Count Validation
**What:** After build and merge, count the number of `.whl` files. Fail if not exactly 2 (Windows + Linux).
**When to use:** Always -- catches build failures that silently produce fewer wheels.
**Example:**
```yaml
- name: Validate wheel count
  run: |
    WHEEL_COUNT=$(ls dist/*.whl 2>/dev/null | wc -l)
    if [ "$WHEEL_COUNT" -ne 2 ]; then
      echo "::error::Expected 2 wheels, found $WHEEL_COUNT"
      exit 1
    fi
```

### Pattern 4: PyPI Trusted Publishing with OIDC
**What:** Job with `id-token: write` permission and `environment: pypi` uses OIDC to authenticate with PyPI. No API tokens needed.
**When to use:** Always for PyPI publishing from GitHub Actions.
**Example:**
```yaml
# Source: https://docs.pypi.org/trusted-publishers/using-a-publisher/
publish:
  runs-on: ubuntu-latest
  needs: validate
  environment: pypi
  permissions:
    id-token: write
  steps:
    - uses: actions/download-artifact@v4
      with:
        name: wheels
        path: dist/

    - uses: pypa/gh-action-pypi-publish@release/v1
```
Key details:
- `id-token: write` is **mandatory** for OIDC -- set at job level (not workflow level) for least privilege
- `environment: pypi` is optional but strongly recommended by PyPI docs for additional security controls
- `packages-dir` defaults to `dist/` -- download wheels there to match
- Attestation generation is automatic since v1.11.0 (no opt-in needed)
- No `with:` block needed for username/password -- OIDC handles it

### Pattern 5: GitHub Release with Auto-Generated Notes
**What:** Create a GitHub Release from the tag with auto-generated changelog body and attached wheel files.
**When to use:** Always -- per user decision, release is auto-created and marked latest.
**Example:**
```yaml
# Source: https://github.com/softprops/action-gh-release
release:
  runs-on: ubuntu-latest
  needs: validate
  permissions:
    contents: write
  steps:
    - uses: actions/download-artifact@v4
      with:
        name: wheels
        path: dist/

    - uses: softprops/action-gh-release@v2
      with:
        generate_release_notes: true
        files: dist/*.whl
```
Key details:
- `contents: write` permission required for creating releases
- `generate_release_notes: true` uses GitHub's built-in changelog (PR titles since last tag)
- By default, `softprops/action-gh-release` marks the release as "latest" (no extra config needed)
- Tag name is automatically derived from `github.ref_name`
- `files` accepts glob patterns -- `dist/*.whl` uploads all wheel files as release assets

### Anti-Patterns to Avoid
- **Storing PyPI API tokens as repository secrets:** Use OIDC trusted publishing instead. API tokens are long-lived and a security risk.
- **Setting `id-token: write` at workflow level:** Set it only on the `publish` job to minimize credential exposure.
- **Re-running a failed publish without checking PyPI:** PyPI uploads are not idempotent for the same version. If the upload partially succeeded, the version may be claimed and re-running will fail with a conflict error.
- **Publishing from a reusable workflow:** Trusted publishing cannot be used from within a reusable workflow (known limitation). Must be in a non-reusable workflow.
- **Downloading artifacts from the build-wheels workflow:** Per user decision, publish builds fresh. Cross-workflow artifact download requires `dawidd6/action-download-artifact` or `workflow_run` triggers, adding complexity.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| OIDC token exchange with PyPI | Manual `requests` to PyPI OIDC endpoint | `pypa/gh-action-pypi-publish@release/v1` | Handles token exchange, retry, attestation, and error reporting |
| GitHub Release creation | `gh release create` bash scripting | `softprops/action-gh-release@v2` | Declarative, handles file upload, auto-notes, error handling |
| Wheel upload to PyPI | `twine upload` with API token | `pypa/gh-action-pypi-publish@release/v1` | Trusted publishing is more secure; twine needs stored credentials |
| Version extraction from pyproject.toml | `grep` or `sed` parsing | `python -c "import tomllib; ..."` | TOML parsing is reliable; regex can break on formatting changes |
| Attestation generation | Manual Sigstore signing | `pypa/gh-action-pypi-publish@release/v1` (built-in since v1.11.0) | Automatic, no extra steps needed |

**Key insight:** The entire publish pipeline is well-served by three established actions (`cibuildwheel`, `action-gh-release`, `gh-action-pypi-publish`) plus ~10 lines of bash validation. No custom tooling needed.

## Common Pitfalls

### Pitfall 1: Missing `id-token: write` Permission
**What goes wrong:** PyPI publish step fails with "OIDC token exchange failed" or similar authentication error.
**Why it happens:** GitHub Actions jobs default to read-only permissions. The `id-token: write` permission must be explicitly granted for the OIDC token exchange.
**How to avoid:** Set `permissions: id-token: write` on the publish job specifically.
**Warning signs:** Error message mentioning "id-token" or "OIDC" in the publish step logs.

### Pitfall 2: Trusted Publisher Not Registered on PyPI
**What goes wrong:** OIDC token exchange succeeds but PyPI rejects the upload because no matching trusted publisher exists.
**Why it happens:** The PyPI project must have a trusted publisher configured that matches the repository owner, repository name, workflow filename, and optional environment name.
**How to avoid:** Register a "pending trusted publisher" on PyPI at https://pypi.org/manage/account/publishing/ BEFORE the first tag push. Required fields: repository owner, repository name, workflow filename (`publish.yml`), and environment name (`pypi` if using environments).
**Warning signs:** Error message "publisher not found" or "not authorized" during upload.

### Pitfall 3: Environment Name Mismatch
**What goes wrong:** Trusted publisher configured with environment name `pypi` but workflow job omits `environment: pypi` (or vice versa).
**Why it happens:** The environment name in the workflow must EXACTLY match what was configured in the trusted publisher on PyPI.
**How to avoid:** Use `environment: pypi` in the publish job AND specify `pypi` as the environment name when registering the trusted publisher on PyPI. Or use no environment on both sides.
**Warning signs:** Same "publisher not found" error as pitfall 2 despite correct repo/workflow configuration.

### Pitfall 4: Tag Version Mismatch with pyproject.toml
**What goes wrong:** Wheels are published with a version that does not match the git tag, causing confusion.
**Why it happens:** Developer tags `v0.6.0` but pyproject.toml still says `version = "0.5.0"`.
**How to avoid:** Validate tag version against pyproject.toml version in a dedicated step before build. Fail hard on mismatch.
**Warning signs:** N/A -- this is a logic error that only manifests downstream. The validation step is the prevention.

### Pitfall 5: PyPI Upload Not Idempotent
**What goes wrong:** Re-running a failed workflow after partial upload succeeds in PyPI results in "File already exists" error for the already-uploaded wheel.
**Why it happens:** PyPI does not allow re-uploading the same filename to the same version. Once a file is uploaded, it is permanent.
**How to avoid:** Per user decision, no auto-retry. If upload fails partially, the version may need to be bumped (or the partially uploaded version deleted from PyPI, if it has no downloads). Investigate the failure before re-running.
**Warning signs:** HTTP 400 "File already exists" during upload step.

### Pitfall 6: Workflow Filename Must Match Trusted Publisher
**What goes wrong:** Trusted publisher registered with workflow name `release.yml` but actual file is `publish.yml` (or the file is nested in a subdirectory).
**Why it happens:** PyPI validates the exact workflow filename (just the filename, not the full path).
**How to avoid:** Register the trusted publisher with the exact filename: `publish.yml`. The file must be at `.github/workflows/publish.yml`.
**Warning signs:** Same "publisher not found" error despite correct repo configuration.

## Code Examples

Verified patterns from official sources:

### Complete Publish Workflow Structure
```yaml
# Sources: cibuildwheel docs, PyPI trusted publisher docs, softprops/action-gh-release README
name: Publish

on:
  push:
    tags:
      - "v*.*.*"

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

  validate:
    name: Validate release
    runs-on: ubuntu-latest
    needs: merge
    steps:
      - uses: actions/checkout@v6

      - uses: actions/download-artifact@v4
        with:
          name: wheels
          path: dist/

      - name: Validate tag matches pyproject.toml version
        run: |
          TAG_VERSION="${GITHUB_REF_NAME#v}"
          PKG_VERSION=$(python3 -c "import tomllib; print(tomllib.load(open('bindings/python/pyproject.toml','rb'))['project']['version'])")
          if [ "$TAG_VERSION" != "$PKG_VERSION" ]; then
            echo "::error::Tag version ($TAG_VERSION) != pyproject.toml version ($PKG_VERSION)"
            exit 1
          fi
          echo "Version validated: $TAG_VERSION"

      - name: Validate wheel count
        run: |
          WHEEL_COUNT=$(ls dist/*.whl | wc -l)
          if [ "$WHEEL_COUNT" -ne 2 ]; then
            echo "::error::Expected 2 wheels, found $WHEEL_COUNT"
            exit 1
          fi
          echo "Wheel count validated: $WHEEL_COUNT"

  release:
    name: GitHub Release
    runs-on: ubuntu-latest
    needs: validate
    permissions:
      contents: write
    steps:
      - uses: actions/download-artifact@v4
        with:
          name: wheels
          path: dist/

      - uses: softprops/action-gh-release@v2
        with:
          generate_release_notes: true
          files: dist/*.whl

  publish:
    name: Publish to PyPI
    runs-on: ubuntu-latest
    needs: validate
    environment: pypi
    permissions:
      id-token: write
    steps:
      - uses: actions/download-artifact@v4
        with:
          name: wheels
          path: dist/

      - uses: pypa/gh-action-pypi-publish@release/v1
```

### PyPI Pending Publisher Registration
Before the first publish, the repository owner must:
1. Log in to https://pypi.org
2. Navigate to Account Settings > Publishing
3. Add a new pending publisher with:
   - PyPI project name: `quiverdb`
   - Owner: (GitHub username/org)
   - Repository: `quiver2` (or whatever the repo name is)
   - Workflow name: `publish.yml`
   - Environment name: `pypi` (must match the `environment:` in the workflow)
4. The pending publisher converts to a real publisher on first successful upload

### Version Validation Script
```bash
# Extract version from tag (strip v prefix)
TAG_VERSION="${GITHUB_REF_NAME#v}"

# Extract version from pyproject.toml using Python stdlib (no pip install needed)
PKG_VERSION=$(python3 -c "
import tomllib
with open('bindings/python/pyproject.toml', 'rb') as f:
    data = tomllib.load(f)
print(data['project']['version'])
")

# Compare
if [ "$TAG_VERSION" != "$PKG_VERSION" ]; then
  echo "::error::Tag version ($TAG_VERSION) does not match pyproject.toml version ($PKG_VERSION)"
  exit 1
fi
```
Note: `tomllib` is available in Python 3.11+ (stdlib). GitHub-hosted runners have Python 3.11+ by default.

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| PyPI API tokens stored as GitHub Secrets | Trusted Publishing via OIDC | April 2023 (GA) | No long-lived credentials; tokens auto-expire |
| Manual Sigstore signing | Automatic attestations in gh-action-pypi-publish | v1.11.0 (2024) | PEP 740 attestations generated by default, no config |
| `twine upload` with username/password | `pypa/gh-action-pypi-publish@release/v1` | 2023+ | Handles all auth, retry, attestation automatically |
| `softprops/action-gh-release@v1` | `softprops/action-gh-release@v2` (v2.5.0, Dec 2025) | 2024 | Improved Node.js runtime, better error handling |
| actions/upload-artifact@v3 (same-name uploads) | actions/upload-artifact@v4+ (unique names + merge) | Jan 2025 | Must use unique artifact names per runner |

**Deprecated/outdated:**
- PyPI API tokens: Still functional but OIDC is strongly recommended by PyPI
- `pypa/gh-action-pypi-publish@master`: Sunset; use `@release/v1` instead
- `softprops/action-gh-release@v1`: Still functional but v2 is current

## Open Questions

1. **PyPI project name availability**
   - What we know: Search results show no `quiverdb` package on PyPI currently. The name `quiver` exists but is a different package.
   - What's unclear: Whether the name is truly available or reserved by another user. PyPI name reservation through pending publishers does NOT reserve the name until first actual publish.
   - Recommendation: Register the pending publisher promptly. If the name is taken by first publish attempt, fall back to an alternative name. LOW risk -- the name appears available.

2. **GitHub Environment `pypi` auto-creation**
   - What we know: The `environment: pypi` in the workflow references a GitHub Environment that provides additional protection. PyPI docs recommend it.
   - What's unclear: Whether the environment auto-creates on first workflow run or must be manually created in repository settings.
   - Recommendation: The environment is auto-created on first reference in a workflow run. No manual setup needed. However, if additional protection rules (required reviewers, wait timers) are desired, they must be configured manually in Settings > Environments. Per user decision, no manual approval gate is needed, so the auto-created environment is sufficient.

3. **release and publish job ordering**
   - What we know: Both `release` and `publish` jobs depend on `validate`. They could run in parallel.
   - What's unclear: Whether publishing to PyPI should strictly happen before or after GitHub Release creation, or if parallel is acceptable.
   - Recommendation: Run in parallel (`both needs: validate`). If PyPI upload fails, the GitHub Release still gets created (tag already exists; wheels are attached). The user can investigate the PyPI failure independently. If GitHub Release creation fails, PyPI still gets the package. Neither depends on the other's output.

## Sources

### Primary (HIGH confidence)
- [PyPI Trusted Publishers docs](https://docs.pypi.org/trusted-publishers/) - OIDC setup, pending publishers, adding publishers, using publishers
- [PyPI Creating a Project through OIDC](https://docs.pypi.org/trusted-publishers/creating-a-project-through-oidc/) - Pending publisher registration fields and behavior
- [pypa/gh-action-pypi-publish](https://github.com/pypa/gh-action-pypi-publish) - Official action, release/v1 branch, attestations feature, packages-dir input
- [cibuildwheel Delivering to PyPI](https://cibuildwheel.pypa.io/en/stable/deliver-to-pypi/) - Complete workflow example with OIDC publishing
- [cibuildwheel GitHub deploy example](https://github.com/pypa/cibuildwheel/blob/main/examples/github-deploy.yml) - Reference YAML for build + publish
- [softprops/action-gh-release](https://github.com/softprops/action-gh-release) - v2.5.0, generate_release_notes, files input, permissions
- [Python Packaging User Guide - Publishing with GitHub Actions](https://packaging.python.org/en/latest/guides/publishing-package-distribution-releases-using-github-actions-ci-cd-workflows/) - Canonical workflow structure

### Secondary (MEDIUM confidence)
- [GitHub Docs - Configuring OIDC in PyPI](https://docs.github.com/actions/deployment/security-hardening-your-deployments/configuring-openid-connect-in-pypi) - GitHub-side OIDC configuration
- [actions/download-artifact](https://github.com/actions/download-artifact) - v4 merge-multiple parameter, pattern matching
- [GitHub Docs - Automatically generated release notes](https://docs.github.com/en/repositories/releasing-projects-on-github/automatically-generated-release-notes) - How generate_release_notes works

### Tertiary (LOW confidence)
- PyPI name availability for `quiverdb`: Based on web search only, not verified via PyPI API. Needs validation at registration time.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - All actions verified via official repos and documentation with version numbers confirmed
- Architecture: HIGH - Workflow pattern follows canonical cibuildwheel + PyPI publishing examples from official docs; self-contained build is straightforward
- Pitfalls: HIGH - OIDC pitfalls (permissions, environment mismatch, publisher registration) are well-documented by PyPI; non-idempotent upload is stated in PyPI docs

**Research date:** 2026-02-25
**Valid until:** 2026-03-25 (all tools are stable; no expected breaking changes)
