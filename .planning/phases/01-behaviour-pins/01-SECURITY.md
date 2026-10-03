---
phase: 01
slug: behaviour-pins
status: verified
# threats_open = count of OPEN threats at or above workflow.security_block_on severity (the blocking gate)
threats_open: 0
asvs_level: 1
created: 2026-10-02
---

# Phase 01 — Security

> Per-phase security contract: threat register, accepted risks, and audit trail.

Phase 1 is tests-only: no file under `src/`, `include/`, `cmake/` or `bindings/*/src` changed (`git diff --quiet
5b57e7c HEAD -- src` and tree hashes for the other shipping paths are identical). The register below therefore
guards the pins that protect the Phase 2 split, and the integrity of the working tree during execution.

---

## Trust Boundaries

| Boundary | Description | Data Crossing |
|----------|-------------|---------------|
| Lua script to LuaRunner bindings | Scripts are untrusted; the bindings validate argument types and the path before touching the filesystem | Script-supplied paths, option tables, row tables |
| LuaRunner to filesystem | `resolve_sandboxed_path` confines every file operation to the database directory | File paths, CSV/binary contents |
| Working tree to commit history | Hand mutations of `src/lua_runner.cpp` and pre-existing `.planning` edits share the tree with plan commits | Source and planning files |
| Sync test to shipped Lua reference | `LUA_DB_API_REFERENCE` is prompt payload for a downstream LLM; the sync test guards that it matches the bound surface | Binding names, reference text |

---

## Threat Register

| Threat ID | Category | Component | Severity | Disposition | Mitigation | Status |
|-----------|----------|-----------|----------|-------------|------------|--------|
| T-01-01 | Tampering / Information disclosure | `resolve_sandboxed_path` callers (`read_csv`, `read_csv_stream`, `write_csv`, `export_csv`, `import_csv`) | high | mitigate | Escape-before-options order pins in all five entry points (`test_lua_runner_read_csv.cpp:1153,1166,1179`, `test_lua_runner_write_csv.cpp:1250`, `test_lua_runner_csv_export.cpp:197`, `test_lua_runner_csv_import.cpp:278`), each with its own "options must be a table" control (import/write_csv controls added in 46f9e1e) so neither side can pass vacuously; every reversed order was mutation-killed | closed |
| T-01-02 | Denial of service | `csv_max_integer_key` (row and header) | medium | mitigate | `RowKeyPastMaximumWidthThrows` / `HeaderKeyPastMaximumWidthThrows` (`test_lua_runner_write_csv.cpp:403,422`) pin `kMaxWidth = 1'000'000` (`src/lua_runner.cpp:364`); raising, lowering or dropping the cap was mutation-killed | closed |
| T-01-03 | Tampering | Test values that are Release UB | medium | mitigate | Every bad value reaches a `sol::object` parameter with an explicit `get_type()` check (`src/lua_runner.cpp:790,823,876` and the decoders); the full `Lua*` suite (444) passes in `build/release` | closed |
| T-01-04 | Tampering | Pre-existing `.planning/PROJECT.md`, `REQUIREMENTS.md`, `ROADMAP.md` edits | high | mitigate | Edits were committed in 4b4728c before execution; every line it added is present at HEAD; later hunks are tracking-only (01-UAT test 2) | closed |
| T-01-05 | Tampering | Escape-path tests writing outside the sandbox | low | accept | Every escape case throws in the path resolver before any file is opened | closed |
| T-01-06 | Tampering | `src/lua_runner.cpp` hand mutation | high | mitigate | `git diff --exit-code -- src/` exits 0; `src/` is identical to 5b57e7c and HEAD (blob e968172f on disk) | closed |
| T-01-07 | Tampering | Pre-existing `.planning` edits (plan 01-02) | high | mitigate | Explicit-path staging only; `git diff -w --ignore-blank-lines 4b4728c..HEAD` shows only checkbox, status and plan-list lines (01-UAT test 2) | closed |
| T-01-08 | Repudiation | Recorded baseline counts | medium | mitigate | Re-observed in build/dev, build/release and build/: `Lua*` 444/12, quiver_tests 1410, quiver_c_tests 543, `LuaRunnerCApiTest` 27; 441 appears only as a prediction (01-UAT test 3) | closed |
| T-01-09 | Tampering | `lua-api-sync.test.ts` passing vacuously | medium | mitigate | Parse-derived usertype set with the four known types as a floor (`lua-api-sync.test.ts:63`) and a single `open_libraries(` count (`:72`); empty-usertype, undocumented-usertype and duplicate-call mutations all fail the test | closed |
| T-01-SC | Tampering | Package installs | low | accept | No manifest or lockfile changed in 5b57e7c..HEAD; only the repo's pinned `uvx clang-format==22.1.8` and `bunx biome` were run | closed |

*Status: open · closed · open — below high threshold (non-blocking)*
*Severity: critical > high > medium > low — only open threats at or above workflow.security_block_on count toward threats_open*
*Disposition: mitigate (implementation required) · accept (documented risk) · transfer (third-party)*

---

## Accepted Risks Log

| Risk ID | Threat Ref | Rationale | Accepted By | Date |
|---------|------------|-----------|-------------|------|
| AR-01-01 | T-01-05 | Escape cases are rejected by the resolver before any file is opened; tests write only relative names under `LuaSandboxTest`'s per-test temp dir | 01-01-PLAN threat model | 2026-10-02 |
| AR-01-02 | T-01-SC | No dependency was added; the only tools run are the repo's existing pinned formatters | 01-01-PLAN / 01-02-PLAN threat models | 2026-10-02 |

*Accepted risks do not resurface in future audit runs.*

---

## Security Audit Trail

| Audit Date | Threats Total | Closed | Open | Run By |
|------------|---------------|--------|------|--------|
| 2026-10-02 | 10 | 10 | 0 | gsd-secure-phase (L1 grep-depth; evidence cross-checked against the phase mutation runs and 01-UAT) |

---

## Sign-Off

- [x] All threats have a disposition (mitigate / accept / transfer)
- [x] Accepted risks documented in Accepted Risks Log
- [x] `threats_open: 0` confirmed
- [x] `status: verified` set in frontmatter

**Approval:** verified 2026-10-02
