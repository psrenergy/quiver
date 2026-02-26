# Phase 4: PyPI Publishing - Context

**Gathered:** 2026-02-25
**Status:** Ready for planning

<domain>
## Phase Boundary

Automated release pipeline: pushing a version tag triggers a GitHub Actions workflow that builds tested wheels and publishes them to PyPI via trusted publisher (OIDC). Also creates a GitHub Release with attached wheels. No manual intervention required.

</domain>

<decisions>
## Implementation Decisions

### Tag format & versioning
- v-prefixed semver tags: `v0.5.0`, `v0.5.1`, etc.
- Version in `pyproject.toml` is maintained manually — update before tagging
- Workflow validates tag version matches `pyproject.toml` version; hard fail on mismatch
- Stable releases only — no pre-release tag support (no rc, beta, alpha patterns)

### Workflow structure
- Separate workflow file (e.g., `publish.yml`), not merged into existing CI workflow
- Builds wheels fresh via cibuildwheel (self-contained pipeline, no cross-workflow artifact dependency)
- cibuildwheel runs tests as part of its build process — no additional test step needed
- Wheels only, no sdist (native-bundled package requires CMake/compilers for source builds)

### Release artifacts
- Auto-create GitHub Release from the tag
- Attach built `.whl` files to the GitHub Release as downloadable assets
- Release body uses GitHub's auto-generated changelog (PR titles since last tag)
- Automatically marked as 'latest' release (no draft/manual promotion step)

### Safety guardrails
- No manual approval gate — tag push goes straight to build + publish
- Pre-publish validation: tag-vs-pyproject.toml version match AND expected wheel count (2: Windows + Linux)
- No branch restriction on tag triggers — any tag matching `v*.*.*` publishes
- No auto-retry on PyPI upload failure — fail and re-run manually (PyPI uploads not idempotent)

### Claude's Discretion
- Exact workflow job structure and step ordering
- GitHub Actions action versions (e.g., pypa/gh-action-pypi-publish version)
- OIDC permission configuration details
- Wheel count validation implementation approach

</decisions>

<specifics>
## Specific Ideas

No specific requirements — open to standard approaches. The workflow should follow PyPI trusted publisher best practices and use pypa/gh-action-pypi-publish for the actual upload step.

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 04-pypi-publishing*
*Context gathered: 2026-02-25*
