---
phase: 08
slug: typed-expression-parameters-in-lua
status: verified
# threats_open = count of OPEN threats at or above workflow.security_block_on severity (the blocking gate)
threats_open: 0
asvs_level: 1
created: 2026-10-04
---

# Phase 08 — Security

> Per-phase security contract: threat register, accepted risks, and audit trail.

---

## Trust Boundaries

| Boundary | Description | Data Crossing |
|----------|-------------|---------------|
| Lua script → host (LuaRunner sandbox) | Untrusted script text reaches the binder; `f:save` is a new file-writing entry point. | Script text, output paths |
| Lua value → C++ object (sol2 userdata cast) | A script-supplied value becomes a `const AbstractExpression&`; a wrong cast would read unrelated memory. | Userdata pointers (memory safety) |
| Docker image + apt → build container | Ubuntu archive packages enter an ephemeral container; nothing built there ships. | Build-only packages |

---

## Threat Register

| Threat ID | Category | Component | Severity | Disposition | Mitigation | Status |
|-----------|----------|-----------|----------|-------------|------------|--------|
| T-08-01 | Tampering / Elevation | `f:save` path (new entry point) | high | mitigate | the shared `save` lambda calls `resolve_sandboxed_path(db, "save", path)` for files and expressions alike (`src/lua_runner/expression.cpp:136`); `LuaExpressionTest.FileSaveGuards` (`tests/test_lua_expression.cpp:768`) pins "escapes the database directory" | closed |
| T-08-02 | Tampering | overwriting the input through `f:save` | high | mitigate | core output-collision check (`src/expression/expression.cpp:51`); `FileSaveGuards` pins "collides with input file" twice (`expr_a` and `./expr_a`) and that nothing is written | closed |
| T-08-03 | Tampering | saving from a file open for writing | high | mitigate | process-wide write registry (`src/binary/binary_file.cpp:68`); `FileSaveGuards` pins "already open for writing" and that the writer stays open and writable | closed |
| T-08-04 | Tampering / DoS | unchecked userdata cast to `AbstractExpression` | high | mitigate | `SOL_ALL_SAFETIES_ON=1` (`src/CMakeLists.txt:86`, keeps `SOL_SAFE_USERTYPE` / `SOL_SAFE_FUNCTION_CALLS`; only the getter and stack check are off), compile-time traits with no unsafe base lookup; `OperandErrorsForMissingAndMixedOperands` (`test_lua_expression.cpp:639`) pins `quiver.abs(db)` and `quiver.ifelse(e, e, db)` → `... got userdata` | closed |
| T-08-05 | Tampering (undefined behaviour) | traits not visible in a TU that instantiates sol2 for these types | medium | mitigate | traits in `src/lua_runner/internal.h:38-40`; every `src/lua_runner/*.cpp` that binds `BinaryFile`/`Expression` includes it, `path_policy.cpp` has no sol2 (re-checked by the milestone integration check); Release and Linux GCC 13 / Clang 18 builds green | closed |
| T-08-06 | DoS | the caller's file closed or altered by `f:save` | medium | mitigate | `BinaryFile::node()` builds a path-based leaf; `FileSaveKeepsFileOpen` (`test_lua_expression.cpp:740`) and `FileSaveGuards` assert `is_open()` and readable values after successful and refused saves | closed |
| T-08-07 | Information disclosure | the writer-refusal text carries the canonical absolute path | low | accept | see Accepted Risks Log | closed |
| T-08-08 | Tampering (gate integrity) | editing the pinned operand tests to fit new texts | medium | mitigate | the gate compared both pinned bodies with BASE and rejected any hunk inside them; 08-VERIFICATION confirms the pinned bodies byte-identical to base | closed |
| T-08-09 | Tampering | the measurement copy left in the tree, or the runtime tag shipped unmeasured | high | mitigate | `git grep 'sol::bases<' -- src` is empty on HEAD; `build/typed-check/FORM` = `traits`; `FileAndExpressionKeepTableIndex` (`test_lua_expression.cpp:852`) fails on any tag build | closed |
| T-08-10 | Tampering | C4702 suppression hiding other warnings | medium | mitigate | one `#pragma warning(disable : 4702)` in one TU (`src/lua_runner/expression.cpp:5`); `release-warnings.txt` empty across 17 TUs; the no-pragma mutation (`release-warnings-no-pragma.txt`) shows exactly what it hides | closed |
| T-08-11 | Tampering (supply chain) | ubuntu:24.04 image and apt packages in the Linux check | low | accept | see Accepted Risks Log | closed |
| T-08-12 | Repudiation | benchmark numbers that cannot be reproduced | low | mitigate | `build/perf-phase7/`, `build/perf-phase8-tag/`, `build/perf-phase8-traits/` each keep `COMMIT` + `SHA256SUMS` (`sha256sum -c` passes on re-check); 30 raw runs in `build/typed-check/perf-runs.txt`; rerun command in STATE.md | closed |

*Status: open · closed · open — below high threshold (non-blocking)*
*Severity: critical > high > medium > low — only open threats at or above workflow.security_block_on count toward threats_open*
*Disposition: mitigate (implementation required) · accept (documented risk) · transfer (third-party)*

---

## Accepted Risks Log

| Risk ID | Threat Ref | Rationale | Accepted By | Date |
|---------|------------|-----------|-------------|------|
| AR-08-01 | T-08-07 | The writer-refusal text ("Cannot open_file: file is already open for writing: <canonical path>") is pre-existing core text, pinned as is by plan decision; the script already knows the path it opened, so the canonical form discloses nothing it could not resolve itself. | Phase 08 plan (08-01 threat model) | 2026-10-04 |
| AR-08-02 | T-08-11 | The ubuntu:24.04 Docker container takes `git archive` input, is ephemeral and build-only, and nothing built there ships; PR CI is the authoritative Linux build. | Phase 08 plan (08-02 threat model) | 2026-10-04 |

*Accepted risks do not resurface in future audit runs.*

---

## Security Audit Trail

| Audit Date | Threats Total | Closed | Open | Run By |
|------------|---------------|--------|------|--------|
| 2026-10-04 | 12 | 12 | 0 | /gsd-secure-phase orchestrator (ASVS L1 grep-depth against HEAD `58578aa`; register authored at plan time, no auditor spawn required) |

---

## Sign-Off

- [x] All threats have a disposition (mitigate / accept / transfer)
- [x] Accepted risks documented in Accepted Risks Log
- [x] `threats_open: 0` confirmed
- [x] `status: verified` set in frontmatter

**Approval:** verified 2026-10-04
