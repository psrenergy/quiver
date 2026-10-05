---
phase: 09
slug: julia-abstractexpression-and-docs
status: verified
# threats_open = count of OPEN threats at or above workflow.security_block_on severity (the blocking gate)
threats_open: 0
asvs_level: 1
created: 2026-10-04
---

# Phase 09 — Security

> Per-phase security contract: threat register, accepted risks, and audit trail.

---

## Trust Boundaries

| Boundary | Description | Data Crossing |
|----------|-------------|---------------|
| Julia object lifetime → C handle | A Julia finalizer frees the C handle of a `Binary.File` or `Expression`; a `.ptr` loaded for a ccall is valid only while its owner is rooted. | Raw C pointers (memory safety) |
| Julia caller → filesystem (`Quiver.save`) | A raw file can now be saved; Julia is a trusted host, so no sandbox applies (the sandbox is LuaRunner policy only). | Output `.qvr`/`.toml` paths and contents |
| Caller's file handle → expression engine | Converting or saving from a file must never close or reuse the caller's handle. | File handle ownership |
| AGENTS.md → agents and maintainers | Agents act on these files; a wrong sentence about handle ownership invites an unsafe change. | Maintainer guidance (integrity) |
| CHANGELOG → downstream callers | A missing BREAKING line or caller action lets an upgrade break silently. | Release notes (integrity) |
| Test gate → shipped natives | A stale Dart hook cache or native library can make the gate test something other than HEAD. | Build artifacts (integrity) |

---

## Threat Register

| Threat ID | Category | Component | Severity | Disposition | Mitigation | Status |
|-----------|----------|-----------|----------|-------------|------------|--------|
| T-09-01 | Tampering / DoS (use-after-free) | `_binop`, `_unop`, `ifelse`, `save`, `get_metadata`, `aggregate*`, `*_agents`, `Expression(file)` | medium | mitigate | 14 `GC.@preserve` sites in `bindings/julia/src/expression.jl` around every ccall that passes a `.ptr`; testset "Expression from a raw Binary.File outlives it" (`test_expression.jl:2400`) forces `GC.gc()` between operations and saves | closed |
| T-09-02 | Tampering | `Quiver.save(file, own_path)` overwriting its input | medium | mitigate | C++ collision check `src/expression/expression.cpp:51` ("Cannot save: output path collides with input file"); pinned by "Save guards on a raw Binary.File" (`test_expression.jl:2278`) | closed |
| T-09-03 | Tampering | saving from a file open for writing (half-written input) | medium | mitigate | C++ write registry `src/binary/binary_file.cpp:68` ("file is already open for writing"); pinned by message, no output `.qvr`/`.toml`, and writer still writable (`test_expression.jl:2283-2291`) | closed |
| T-09-04 | DoS | a closed or freed file used as an operand | medium | mitigate | `Binary.close!` nulls `ptr`; `QUIVER_REQUIRE(file, out)` in `src/c/expression/expression.cpp:100` returns `Null argument` instead of dereferencing; pinned by "Closed Binary.File is not an operand" (`test_expression.jl:2323`, both the operator and save paths, nothing written) | closed |
| T-09-05 | Elevation of privilege | `Quiver.save(file, path)` writes anywhere the process can | low | accept | Julia is a trusted host and `save(::Expression)` always wrote anywhere; the database-directory sandbox is LuaRunner policy by root design decision — see Accepted Risks Log | closed |
| T-09-06 | DoS | the caller's file closed or altered by an operation | low | mitigate | `BinaryFile::node()` builds a fresh path-based leaf (`src/expression/expression_file.cpp:30-31`), so the handle is never shared; "Saving a raw Binary.File twice" (`test_expression.jl:2378`) reads the file after each save | closed |
| T-09-07 | Tampering (integrity of guidance) | `src/AGENTS.md` ExpressionFile line, root Design Decision | medium | mitigate | the stale "caches an open BinaryFile" claim is gone (`grep -ci 'caches an open' src/AGENTS.md` = 0); the root decision states why a node must never hold the caller's handle; doc claims cross-checked against code by the verifier and the milestone integration check | closed |
| T-09-08 | Repudiation | CHANGELOG `[0.13.0]` completeness | medium | mitigate | `build/julia-check/changelog_check.sh` prints `CHANGELOG OK` (four BREAKING lines with caller actions, both Added entries, order, compare link, no planning IDs; mutation-tested three ways); `scripts/assert_version.py`: all five manifests at 0.13.0 | closed |
| T-09-09 | Tampering (gate integrity) | six-suite run | low | mitigate | Debug rebuilt and Dart hook cache deleted before `scripts/test-all.bat`; log kept at `build/julia-check/test-all.txt`; an independent orchestrator re-run on HEAD also passed all six suites | closed |

*Status: open · closed · open — below high threshold (non-blocking)*
*Severity: critical > high > medium > low — only open threats at or above workflow.security_block_on count toward threats_open*
*Disposition: mitigate (implementation required) · accept (documented risk) · transfer (third-party)*

---

## Accepted Risks Log

| Risk ID | Threat Ref | Rationale | Accepted By | Date |
|---------|------------|-----------|-------------|------|
| AR-09-01 | T-09-05 | Julia is a trusted host: `Quiver.save(::Expression)` has always written to any path the process can, and the database-directory containment is LuaRunner policy (root AGENTS.md design decision "Lua file operations are db-scoped and sandboxed"), not binary-subsystem policy. Saving from a raw file adds no capability a Julia caller lacked. | Phase 09 plan (09-01 threat model) | 2026-10-04 |

*Accepted risks do not resurface in future audit runs.*

---

## Security Audit Trail

| Audit Date | Threats Total | Closed | Open | Run By |
|------------|---------------|--------|------|--------|
| 2026-10-04 | 9 | 9 | 0 | /gsd-secure-phase orchestrator (ASVS L1 grep-depth; register authored at plan time, no auditor spawn required) |

---

## Sign-Off

- [x] All threats have a disposition (mitigate / accept / transfer)
- [x] Accepted risks documented in Accepted Risks Log
- [x] `threats_open: 0` confirmed
- [x] `status: verified` set in frontmatter

**Approval:** verified 2026-10-04
