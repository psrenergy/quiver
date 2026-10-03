---
phase: 3
slug: dedupe
status: verified
# threats_open = count of OPEN threats at or above workflow.security_block_on severity (the blocking gate)
threats_open: 0
asvs_level: 1
created: 2026-10-03
---

# Phase 3 — Security

> Per-phase security contract: threat register, accepted risks, and audit trail.

---

## Trust Boundaries

| Boundary | Description | Data Crossing |
|----------|-------------|---------------|
| Lua script to LuaRunner bindings | Untrusted scripts reach every deduplicated binding; registrations, check order and messages must not change | Script arguments (tables, strings, paths, numbers) |
| LuaRunner to filesystem | `resolve_sandboxed_path` confines file operations; options decoding must stay after the containment check | File paths, options tables |
| Run lifetime | The writer and binary-file registries must close every live handle at `run()` exit | Run handles |
| Build to shipped binaries | Release behaviour is what CI ships | Compiled library |

---

## Threat Register

| Threat ID | Category | Component | Severity | Disposition | Mitigation | Status |
|-----------|----------|-----------|----------|-------------|------------|--------|
| T-03-01 | Tampering | 17 forwarder registrations | high | mitigate | Name==member check (17 pairs, 0 mismatch) in every gate, plus the commit/rollback mutation (caught); golden `forwarders` probes Debug+Release | closed |
| T-03-02 | Tampering | `run_in_scope` | high | mitigate | The finish/abort swap mutation failed `TransactionBlockAutoCommit` (caught); golden `scoped_blocks` | closed |
| T-03-03 | Tampering | Adapter templates bound to the wrong member | high | mitigate | Name==member check (36 pairs) plus two mapping mutations (caught); golden `bulk_reads`/`metadata` | closed |
| T-03-04 | Information disclosure | `std::optional` returns | medium | mitigate | Golden `optional_returns` (`select('#')`, `math.type`, key list) identical in Debug and Release | closed |
| T-03-05 | Denial of service | No-rows guard moved into `columns_to_cpp_rows` | high | mitigate | Guard is the first statement; the drop mutation failed all four no-rows tests | closed |
| T-03-06 | Elevation of privilege | Release dot calls (`db.method()`) | low | accept | See Accepted Risks | closed |
| T-03-07 | Tampering | `option_entries` table check | high | mitigate | `internal.h:201` throws `options must be a table` before iteration; the drop mutation failed 5 options tests | closed |
| T-03-08 | Elevation of privilege | Sandbox-before-options order | high | mitigate | `resolve_sandboxed_path(` count per file equals the base (csv 3, binary 4, db_core 3); `path_policy.cpp` diff empty; escape-before-options pins pass | closed |
| T-03-09 | Tampering | Non-finite CSV cell | medium | mitigate | `isfinite` check retained (`csv.cpp`); the drop mutation failed 3 NonFinite tests | closed |
| T-03-10 | Denial of service | 1,000,000 key-width cap | medium | mitigate | `csv_max_integer_key` body is byte-identical to the base (diff 0); cap pins pass | closed |
| T-03-11 | Tampering | `CsvWriter::write_row` check order | medium | mitigate | Moved verbatim; the order-swap mutation failed the check-order test | closed |
| T-03-12 | Tampering | `quiver.metadata` slot mapping | medium | mitigate | Named structured bindings; the swap mutation failed 45 Lua tests plus golden `metadata_slots` | closed |
| T-03-13 | Tampering | `add_writer` prune (dropping a live writer) | high | mitigate | Both predicates are `expired()` only (2 greps); the erase-everything mutation failed golden `csv_lifecycle`; `SecondWriterOnAnAlreadyOpenPathIsRefused` passes. Binary-file half: a scratch over-prune mutation failed (03-VERIFICATION resolution) | closed |
| T-03-14 | Denial of service | Close-at-exit rename / skipped append | high | mitigate | `close_open_handles` used 3× in `lua_runner.cpp`; `close_open_writers` absent outside `.planning/`; the skip-append mutations failed the run-exit tests | closed |
| T-03-15 | Tampering | `binop<Op>` mapping | medium | mitigate | 12 `&binop<std::` registrations; the gte→greater mutation was caught by golden `operators` | closed |
| T-03-16 | Elevation of privilege | Release behaviour of the whole phase | high | mitigate | Release `Lua*` 444 / C API 27; golden Release byte-identical; six suites PASS; Linux GCC 13 + Clang 18/libc++ pass. Apple Clang by PR CI | closed |
| T-03-17 | Tampering | Stale helper names in `src/AGENTS.md` | low | mitigate | Every helper is named in the Shared helpers bullet; the wording was corrected after review in 88dcd1f | closed |
| T-03-SC | Tampering | Tool / package installs | low | accept | See Accepted Risks | closed |

*Status: open · closed · open — below high threshold (non-blocking)*
*Severity: critical > high > medium > low — only open threats at or above workflow.security_block_on count toward threats_open*
*Disposition: mitigate (implementation required) · accept (documented risk) · transfer (third-party)*

---

## Accepted Risks Log

| Risk ID | Threat Ref | Rationale | Accepted By | Date |
|---------|------------|-----------|-------------|------|
| AR-03-01 | T-03-06 | A Release dot-call (`db.commit()`) was undefined behaviour before this phase and still is: the process exits 139 (03-REVIEW IN-06). Phase 3 must not change behaviour. Phase 4 (SAFE-0x, `SOL_ALL_SAFETIES_ON`) owns the fix | Plan 03-01 (disposition accept); carried in STATE.md for Phase 4 | 2026-10-03 |
| AR-03-02 | T-03-SC | No repo dependency was added. `uvx clang-format==22.1.8` is CI's pin; the apt packages live only in throwaway `ubuntu:24.04` containers | Plans 03-01..03 (disposition accept) | 2026-10-03 |

*Accepted risks do not resurface in future audit runs.*

---

## Security Audit Trail

| Audit Date | Threats Total | Closed | Open | Run By |
|------------|---------------|--------|------|--------|
| 2026-10-03 | 18 | 18 | 0 | Claude (secure-phase, L1 grep-depth plus the executors' mutation evidence; register authored at plan time) |

---

## Sign-Off

- [x] All threats have a disposition (mitigate / accept / transfer)
- [x] Accepted risks documented in Accepted Risks Log
- [x] `threats_open: 0` confirmed
- [x] `status: verified` set in frontmatter

**Approval:** verified 2026-10-03
