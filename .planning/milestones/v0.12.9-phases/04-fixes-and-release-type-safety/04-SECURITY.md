---
phase: 4
slug: fixes-and-release-type-safety
status: verified
# threats_open = count of OPEN threats at or above workflow.security_block_on severity (the blocking gate)
threats_open: 0
asvs_level: 1
created: 2026-10-03
---

# Phase 4 — Security

> Per-phase security contract: threat register, accepted risks, and audit trail.

---

## Trust Boundaries

| Boundary | Description | Data Crossing |
|----------|-------------|---------------|
| Lua script to LuaRunner bindings | Untrusted scripts. Wrong-typed tables, keys and optional arguments used to be UB or silent no-ops in Release | Script arguments |
| Script source to the Lua VM | `run()`'s script and `load` chunks: bytecode is a memory-safety vector | Chunks |
| LuaRunner to filesystem | Containment must still win over type errors | File paths, options |
| LuaRunner to host process | Transaction state after a failed commit; stderr output | Database state, stderr |

---

## Threat Register

| Threat ID | Category | Component | Severity | Disposition | Mitigation | Status |
|-----------|----------|-----------|----------|-------------|------------|--------|
| T-04-01 | Tampering | Group writers cleared by a userdata payload | high | mitigate | `require_table` in `collect_group_columns`; `GroupWritersRejectNonTableColumns` (update + time_series files) asserts the rows survive; mutation caught (04-01) | closed |
| T-04-02 | Elevation of privilege | `lua_next` on a non-table in Release | high | mitigate | Every table parameter is `const sol::object&` checked by `require_table`; 0 bound `sol::table` parameters across `src/lua_runner/*.cpp`; one test per site | closed |
| T-04-03 | Tampering | Non-string map keys (nullptr `std::string`) | high | mitigate | `lua_string_key` at the 4 sites with a `get_type()` string check; number/boolean key tests | closed |
| T-04-04 | Tampering | Wrong-typed optional arguments silently ignored | medium | mitigate | `optional_from_lua<T>` at the 8 sites; per-site tests | closed |
| T-04-05 | Elevation of privilege | Type check masking containment | medium | mitigate | Metadata decoded after containment; `open_file("../x","w",{})` pin; Phase 1 order pins green; review confirmed containment-first everywhere | closed |
| T-04-06 | Repudiation | Unrecorded error-text changes | low | mitigate | Golden re-baselines limited to the listed keys and quoted in the SUMMARYs; CHANGELOG BREAKING; substring tests unedited | closed |
| T-04-07 | Tampering | COMMIT failure leaves the transaction open | high | mitigate | Finish inside the try with best-effort abort and rethrow; `TransactionBlockCommitFailureRollsBack` (`in_transaction()` false, 0 rows); mutation caught | closed |
| T-04-08 | Denial of service | Non-function argument opens a scope first | medium | mitigate | Function check before `begin`; tests inside and outside an open transaction; mutation caught | closed |
| T-04-09 | Tampering | Empty array clears data, including shared-name fan-out | high | mitigate | BREAKING CHANGELOG (shared names, `{ date_time = {} }`, both round-trip outcomes); tests pin the clear, the fan-out (review WR-02 fix `bc38384`), the misspelled-name throw, the length error and create's skip | closed |
| T-04-10 | Repudiation | Finish errors swallowed by the new catch | medium | mitigate | `throw;` rethrows the original; `ScopedBlockFinishErrorsStillSurface` | closed |
| T-04-11 | Elevation of privilege | Bytecode via `load` | critical | mitigate | Wrapper forces mode `"t"` (string, reader, `"b"` and `"bt"` covered by `LoadRefusesBinaryChunks`); mutation to `"bt"` caught. Extended by review CR-01 (`c666ced`): `run()` loads its script with `sol::load_mode::text`, pinned by `RunRefusesBinaryChunks`, red before | closed |
| T-04-12 | Tampering | Wrapper breaks string-form `load` | medium | mitigate | `LoadStillAcceptsTextChunks` (no env, explicit env, nil mode, explicit nil env); review confirmed none vs nil env semantics | closed |
| T-04-13 | Elevation of privilege | Script reaches the original `load` | medium | mitigate | Original held only as the wrapper's upvalue, installed in the constructor before `quiver`/`db`; review confirmed it is unreachable | closed |
| T-04-14 | Repudiation | Operand errors naming no operation | low | mitigate | `to_expression(o, operation)` with Lua event names; leftmost bad operand deterministic (WR-01 fix `b21f98d`, `OperandErrorsReportTheLeftmostBadOperand`) | closed |
| T-04-15 | Tampering | Dead-branch removal changing outputs | low | mitigate | Golden byte-identical across `87a8266`; lowercase mutation caught | closed |
| T-04-16 | Denial of service | Release dot-call null dereference | high | mitigate | `SOL_ALL_SAFETIES_ON=1` (`SOL_SAFE_USERTYPE`); `DotCallThrowsInsteadOfCrashing`, red in Release before (0xc0000005) | closed |
| T-04-17 | Elevation of privilege | Unchecked typed parameters in Release | high | mitigate | `SOL_SAFE_FUNCTION_CALLS` via the flag; `grep 'SOL_SAFE_(FUNCTION_CALLS\|USERTYPE)=0'` = 0 | closed |
| T-04-18 | Information disclosure | Caught errors printed to host stderr | medium | mitigate | `SOL_PRINT_ERRORS=0`; `CaughtScriptErrorsWriteNothingToStderr` (CaptureStderr, caught and propagated); dropping the define fails it | closed |
| T-04-19 | Elevation of privilege | Unguarded `.as<T>()` | medium | mitigate | The `SOL_SAFE_GETTER=0` fallback means the getter is unchecked in every build, Debug included; the 04-04 SUMMARY audit table and the verifier's independent audit found every `.as<`/`.get<` site guarded. Residual (new code must guard its own) is recorded in STATE PR notes | closed |
| T-04-20 | Denial of service | Flag cost on hot paths | low | mitigate | Measured: +16.4% → fallback → −1.9% / +0.5%, within the 5% budget | closed |
| T-04-21 | Tampering | Stale native caches in binding suites | low | mitigate | Both Dart cache dirs deleted before `test-all.bat` (04-04); six suites PASS | closed |

*Status: open · closed · open — below high threshold (non-blocking)*
*Severity: critical > high > medium > low — only open threats at or above workflow.security_block_on count toward threats_open*
*Disposition: mitigate (implementation required) · accept (documented risk) · transfer (third-party)*

---

## Accepted Risks Log

No accepted risks. The Debug getter-check loss under T-04-19 is mitigated (all sites guarded), not accepted. It is recorded as a review note for future code.

*Accepted risks do not resurface in future audit runs.*

---

## Security Audit Trail

| Audit Date | Threats Total | Closed | Open | Run By |
|------------|---------------|--------|------|--------|
| 2026-10-03 | 21 | 21 | 0 | Claude (secure-phase, L1 grep-depth plus executor/reviewer mutation evidence; key tests re-run: 7/7 pass) |

---

## Sign-Off

- [x] All threats have a disposition (mitigate / accept / transfer)
- [x] Accepted risks documented in Accepted Risks Log
- [x] `threats_open: 0` confirmed
- [x] `status: verified` set in frontmatter

**Approval:** verified 2026-10-03
