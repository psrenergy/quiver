---
phase: 07
slug: abstractexpression-in-c
status: verified
# threats_open = count of OPEN threats at or above workflow.security_block_on severity (the blocking gate)
threats_open: 0
asvs_level: 1
created: 2026-10-04
---

# Phase 07 — Security

> Per-phase security contract: threat register, accepted risks, and audit trail.

---

## Trust Boundaries

| Boundary | Description | Data Crossing |
|----------|-------------|---------------|
| Lua script → host (LuaRunner sandbox) | Behaviour untouched this phase; the binder changed by one accessor call and one comment. | Script text, file paths |
| C API consumers (Julia) → libquiver | Opaque handles; the struct layout gains a vptr inside the library only. | Opaque pointers |
| Docker image + apt → build container | Ubuntu archive packages enter an ephemeral build container; nothing built there ships. | Build-only packages |

---

## Threat Register

| Threat ID | Category | Component | Severity | Disposition | Mitigation | Status |
|-----------|----------|-----------|----------|-------------|------------|--------|
| T-07-01 | Tampering / DoS (use-after-free) | `AbstractExpression::save` on a BinaryFile, whose `node()` is a temporary leaf | high | mitigate | `const auto root = node();` is save's first statement (`src/expression/expression.cpp:43`); `ExpressionFixture.FileStaysOpenAndReadableAfterSave` (`tests/test_expression.cpp:2963`) value-checks a save from a raw file | closed |
| T-07-02 | Tampering | expression closing or writing through the caller's handle | high | mitigate | `BinaryFile::node()` builds a leaf from the path only (`src/expression/expression_file.cpp:30-31`); `FileStaysOpenAndReadableAfterSave`, `FailedSaveLeavesFileOpen`, `ExpressionOutlivesItsFile` (`test_expression.cpp:2963,2996,3011`) | closed |
| T-07-03 | Information disclosure (dangling reference) | `get_metadata()` through `const AbstractExpression&` on a file | medium | mitigate | `virtual get_metadata()` (`abstract_expression.h:32`) with a `BinaryFile` override (`binary_file.h:46`) returning the handle's own metadata; `GetMetadataReturnsHandleOrNodeMetadata` (`test_expression.cpp:3046`) checks address identity | closed |
| T-07-04 | Elevation of privilege | Lua sandbox (`expr:save` path policy) | high | mitigate | `save` calls `resolve_sandboxed_path(db, "save", path)` (`src/lua_runner/expression.cpp:136`); Lua surface frozen at the phase gate (golden byte-identical, `path_policy`/`return_json` diffs empty, `Lua*` 477) | closed |
| T-07-05 | Tampering (ABI) | prebuilt natives built against the old class layout | low | mitigate | `git diff c20c0d0 HEAD -- include/quiver/c bindings/julia/src/c_api.jl` is empty on HEAD (no C header or generated-binding change through phases 7-9); opaque handles; Debug/Release rebuilt and Dart hook cache cleared before the binding suites | closed |
| T-07-06 | DoS | `node()` on a moved-from BinaryFile (null Pimpl) | low | accept | see Accepted Risks Log | closed |
| T-07-07 | Repudiation / Tampering (evidence integrity) | gates, golden baseline, Linux results | medium | mitigate | `build/abstract-check/{BASE,gate.sh,phase_gate.sh,phase-gate-out.txt,linux_gcc.txt,linux_clang.txt}` present; golden baseline never recaptured; each Linux result names its commit | closed |
| T-07-08 | Tampering (stale binaries) | perf binaries, Dart hook cache | low | mitigate | `build/perf-phase7/` holds `COMMIT` (`f2769c3`) and `SHA256SUMS`; `sha256sum -c` passes on re-check | closed |
| T-07-09 | Tampering (supply chain) | ubuntu:24.04 image and apt packages in the Linux harness | low | accept | see Accepted Risks Log | closed |
| T-07-10 | Denial of service (measurement noise) | benchmark | low | mitigate | one warm-up, median of five interleaved runs (`build/abstract-check/perf/runs.txt`); rerun command recorded in STATE.md | closed |

*Status: open · closed · open — below high threshold (non-blocking)*
*Severity: critical > high > medium > low — only open threats at or above workflow.security_block_on count toward threats_open*
*Disposition: mitigate (implementation required) · accept (documented risk) · transfer (third-party)*

---

## Accepted Risks Log

| Risk ID | Threat Ref | Rationale | Accepted By | Date |
|---------|------------|-----------|-------------|------|
| AR-07-01 | T-07-06 | Calling `node()` on a moved-from `BinaryFile` is the same precondition the old `Expression(moved_from)` had; it is documented in the header comment (`abstract_expression.h`: "the caller must not call it on a moved-from file") and left unguarded per the project's clean-code-over-defensive-code principle. | Phase 07 plan (07-01 threat model) | 2026-10-04 |
| AR-07-02 | T-07-09 | The ubuntu:24.04 Docker container is ephemeral and build-only (already used in Phases 2-6); its outputs are logs, never shipped artifacts. PR CI is the authoritative Linux build. | Phase 07 plan (07-02 threat model) | 2026-10-04 |

*Accepted risks do not resurface in future audit runs.*

---

## Security Audit Trail

| Audit Date | Threats Total | Closed | Open | Run By |
|------------|---------------|--------|------|--------|
| 2026-10-04 | 10 | 10 | 0 | /gsd-secure-phase orchestrator (ASVS L1 grep-depth against HEAD `58578aa`; register authored at plan time, no auditor spawn required) |

---

## Sign-Off

- [x] All threats have a disposition (mitigate / accept / transfer)
- [x] Accepted risks documented in Accepted Risks Log
- [x] `threats_open: 0` confirmed
- [x] `status: verified` set in frontmatter

**Approval:** verified 2026-10-04
