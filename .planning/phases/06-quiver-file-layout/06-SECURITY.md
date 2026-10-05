---
phase: 06
slug: quiver-file-layout
status: verified
# threats_open = count of OPEN threats at or above workflow.security_block_on severity (the blocking gate)
threats_open: 0
asvs_level: 1
created: 2026-10-04
---

# Phase 06 — Security

> Per-phase security contract: threat register, accepted risks, and audit trail.

---

## Trust Boundaries

| Boundary | Description | Data Crossing |
|----------|-------------|---------------|
| Lua script → host (LuaRunner sandbox) | Script text and every argument are untrusted. The controls are the constructor's stdlib set, nil `dofile`/`loadfile`, the text-only `load` wrapper and `resolve_sandboxed_path` on every file-touching name. The phase moved registrations next to them, never through them. | Script text, file paths, CSV/option tables |
| Repository docs → future maintainers/agents | AGENTS.md is what the next agent trusts; a stale citation sends a change to the wrong file. | Maintainer guidance (integrity) |

---

## Threat Register

| Threat ID | Category | Component | Severity | Disposition | Mitigation | Status |
|-----------|----------|-----------|----------|-------------|------------|--------|
| T-06-01 | Elevation of privilege / Tampering | file-op lambdas (`export_csv`, `import_csv`, `validate_migrations`, `open_file`, `bin_to_csv`, `csv_to_bin`, `read_csv`, `read_csv_stream`, `write_csv`, `save`) | high | mitigate | all 10 sandboxed operations still call `resolve_sandboxed_path` first in `src/lua_runner/*.cpp` (one call each); escape tests ("escapes the database directory") in `test_lua_binary.cpp`, `test_lua_expression.cpp`, `test_lua_runner_{csv_export,csv_import,migrations,read_csv,write_csv}.cpp`; all six suites pass on HEAD | closed |
| T-06-02 | Elevation of privilege | `LuaRunner::Impl` constructor (`lua_runner.cpp`) | high | mitigate | `open_libraries(` = 1 and `new_usertype<Database>` = 1 in `src/lua_runner/`; `dofile`/`loadfile` set to nil and `load` forced to mode `"t"` (`lua_runner.cpp:102-108`); the load/dofile tests run in `Lua*` | closed |
| T-06-03 | Tampering (script-observable semantics) | `binary.cpp` include set | medium | mitigate | `#include "quiver/expression/expression.h"` kept in `src/lua_runner/binary.cpp:10` with its reason comment; `src/AGENTS.md` records why | closed |
| T-06-04 | Tampering (input validation, ASVS V5) | moved adapters, decoders and Pattern 1 texts | medium | mitigate | verbatim moves; golden Debug/Release byte-compare at the phase gate; operand pins `OperandErrorsNameTheOperation` / `OperandErrorsReportTheLeftmostBadOperand` (`test_lua_expression.cpp:556,575`) still present and passing | closed |
| T-06-05 | Denial of service | a second Database usertype would clear every method | low | mitigate | `new_usertype<Database>` appears exactly once in `src/lua_runner/` | closed |
| T-06-06 | Elevation of privilege | closure captures after the move | low | mitigate | `bind_expression` captures `[&db]` (`expression.cpp:136`), `open_file` captures `[&handles]` (`binary.cpp:112`), never the Impl pointer; move pins `MoveConstructor`, `MoveAssignment`, `MoveConstructorOutlivesSource`, `MoveAssignmentOutlivesSource` (`test_lua_runner_lifecycle.cpp:54-98`) | closed |
| T-06-07 | Tampering | templates moved out of the anonymous namespace | low | accept | see Accepted Risks Log | closed |
| T-06-08 | Repudiation / Information | stale docs citations | low | mitigate | `git grep -nE 'db_core\|db_read\|db_write\|db_metadata\|db_time_series\|bind_core\|bind_write'` outside `.planning/` returns 0 lines (re-checked on HEAD and by the milestone integration check) | closed |

*Status: open · closed · open — below high threshold (non-blocking)*
*Severity: critical > high > medium > low — only open threats at or above workflow.security_block_on count toward threats_open*
*Disposition: mitigate (implementation required) · accept (documented risk) · transfer (third-party)*

---

## Accepted Risks Log

| Risk ID | Threat Ref | Rationale | Accepted By | Date |
|---------|------------|-----------|-------------|------|
| AR-06-01 | T-06-07 | Moving templates out of the anonymous namespace leaves their function types unchanged, so sol2's Debug text cannot change; the golden debug_text probe confirmed it at the phase gate. Linkage widening only, no runtime surface. | Phase 06 plan (06-01 threat model) | 2026-10-04 |

*Accepted risks do not resurface in future audit runs.*

---

## Security Audit Trail

| Audit Date | Threats Total | Closed | Open | Run By |
|------------|---------------|--------|------|--------|
| 2026-10-04 | 8 | 8 | 0 | /gsd-secure-phase orchestrator (ASVS L1 grep-depth against HEAD `58578aa`; register authored at plan time, no auditor spawn required) |

---

## Sign-Off

- [x] All threats have a disposition (mitigate / accept / transfer)
- [x] Accepted risks documented in Accepted Risks Log
- [x] `threats_open: 0` confirmed
- [x] `status: verified` set in frontmatter

**Approval:** verified 2026-10-04
