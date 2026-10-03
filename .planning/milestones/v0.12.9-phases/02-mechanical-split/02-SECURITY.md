---
phase: 2
slug: mechanical-split
status: verified
# threats_open = count of OPEN threats at or above workflow.security_block_on severity (the blocking gate)
threats_open: 0
asvs_level: 1
created: 2026-10-03
---

# Phase 2 — Security

> Per-phase security contract: threat register, accepted risks, and audit trail.

---

## Trust Boundaries

| Boundary | Description | Data Crossing |
|----------|-------------|---------------|
| Lua script to LuaRunner bindings | Scripts are untrusted. Every binding moved byte-for-byte; argument checks, check order and messages unchanged | Script arguments (tables, strings, paths) |
| LuaRunner to filesystem | `resolve_sandboxed_path` confines every file operation to the database directory | File paths |
| Host to LuaRunner lifetime | A runner can be moved; closures must not reach the moved-from object | Run handles (writer and binary-file registries) |
| Build configuration to every sol2 TU | Every new TU must see the same sol2 defines and options, or a silent ODR violation results | Compile definitions |
| Repo docs and `lua-api.ts` to agents | AGENTS.md and the shipped `LUA_DB_API_REFERENCE` steer later changes and LLM prompts | Documentation text |

---

## Threat Register

| Threat ID | Category | Component | Severity | Disposition | Mitigation | Status |
|-----------|----------|-----------|----------|-------------|------------|--------|
| T-02-01 | Tampering | `src/CMakeLists.txt` sol2 flags | medium | mitigate | `/bigobj` target-wide; `git diff bab557e -- src/CMakeLists.txt` changes 0 `SOL_` lines | closed |
| T-02-02 | Tampering | Sync test passing vacuously | medium | mitigate | Phase 1 floors kept. Hardened by review fix e5b00b7: recursive read, and every `.set_function(` must be parsed (86) | closed |
| T-02-03 | Elevation of privilege | Sandbox code during comment strip | high | mitigate | Comment-only strip commit cfb37d4; `DofileAndLoadfileRemoved`/`StandardLibrariesEnabled`/`UnsafeLibrariesNotLoaded` in `Lua*` 444/444 | closed |
| T-02-04 | Tampering | Unrelated `.planning/config.json`, `.gsd/` | medium | mitigate | `git log 47bec06..HEAD -- .planning/config.json .gsd` = 0 commits | closed |
| T-02-05 | Elevation of privilege | `resolve_sandboxed_path` and callers | high | mitigate | `weakly_canonical` appears only in `src/lua_runner/path_policy.cpp`; escape tests pass on Windows, GCC 13 and Clang 18/libc++ | closed |
| T-02-06 | Elevation of privilege | Script environment set up in `Impl` ctor | high | mitigate | `lua_runner.cpp:73-94` order: `open_libraries` → `dofile`/`loadfile` nil → `quiver` table → 7 binders → `lua["db"]` | closed |
| T-02-07 | Denial of service | Closures after a runner move | high | mitigate | `RunHandles` held in heap `Impl`; 4 `LuaRunner_Lifecycle` pins pass in Debug and Release; `sizeof(LuaRunner)==sizeof(void*)` static_assert compiles | closed |
| T-02-08 | Tampering | ODR across new TUs | medium | mitigate | `internal.h` has 0 `static` functions and 0 anonymous namespaces; sol2 defines target-wide | closed |
| T-02-09 | Tampering | Check order picking the reported error | high | mitigate | Bodies moved verbatim (verifier compared old and new code lines); Phase 1 order pins pass | closed |
| T-02-10 | Information disclosure | `CsvWriter` sol2 registry key text | low | accept | See Accepted Risks | closed |
| T-02-11 | Elevation of privilege | File ops in `csv.cpp`/`binary.cpp`/`db_core.cpp` | high | mitigate | Pure cuts; every `resolve_sandboxed_path` call is inside an unchanged lambda; escape and in-memory tests pass | closed |
| T-02-12 | Tampering | Check order (03 extractions) | high | mitigate | Pure moves; `OpenFileReportsInvalidModeBeforeEscapingPath` and the escape-before-options pins pass | closed |
| T-02-13 | Tampering | Same-named helpers / usertype key collisions | medium | mitigate | `CsvWriter` defined once (`csv.cpp:24`, in named `quiver::lua_internal`); file-local helpers are in anonymous namespaces | closed |
| T-02-14 | Denial of service | csv-parser headers in a sol2 TU (SIMD SIGILL) | medium | mitigate | `grep -rn 'csv\.hpp' src/lua_runner` = 0 | closed |
| T-02-15 | Tampering | `LUA_DB_API_REFERENCE` text | medium | mitigate | `git diff bab557e -- bindings/js/src/lua-api.ts` has 0 non-comment changed lines | closed |
| T-02-16 | Tampering | Stale docs leading to a second sandbox gate | low | mitigate | `src/AGENTS.md` names `path_policy.cpp` (2 hits) as the single gate; no stale `src/lua_runner.cpp` citation | closed |
| T-02-17 | Tampering | Dart suite against cached pre-split native | medium | mitigate | 02-04 SUMMARY records the Dart hook cache cleared before `test-all.bat` | closed |
| T-02-18 | Tampering | Sync-test mutation left in tree | low | mitigate | `git status --porcelain src/lua_runner` is empty | closed |
| T-02-19 | Elevation of privilege | Sandbox behaviour in Release builds | high | mitigate | Release `Lua*` 444 and C API 27 pass, including escape, in-memory and `dofile`/`loadfile` tests | closed |
| T-02-SC | Tampering | Package installs | low | accept | See Accepted Risks | closed |

*Status: open · closed · open — below high threshold (non-blocking)*
*Severity: critical > high > medium > low — only open threats at or above workflow.security_block_on count toward threats_open*
*Disposition: mitigate (implementation required) · accept (documented risk) · transfer (third-party)*

---

## Accepted Risks Log

| Risk ID | Threat Ref | Rationale | Accepted By | Date |
|---------|------------|-----------|-------------|------|
| AR-02-01 | T-02-10 | The key changes to `quiver::lua_internal::CsvWriter`. It shows only in sol2's Debug type text; scripts cannot see it (no `debug` library) and no test pins it | Plan 02-02 (disposition accept) | 2026-10-03 |
| AR-02-02 | T-02-SC | No new packages. Tools used: `uvx clang-format==22.1.8` (repo's CI pin), `bunx biome` (installed devDependency), `dart format`; Linux check used the stock `ubuntu:24.04` image with distro toolchains, outside the repo | Plans 02-01..04 (disposition accept) | 2026-10-03 |

*Accepted risks do not resurface in future audit runs.*

---

## Security Audit Trail

| Audit Date | Threats Total | Closed | Open | Run By |
|------------|---------------|--------|------|--------|
| 2026-10-03 | 20 | 20 | 0 | Claude (secure-phase, L1 grep-depth; register authored at plan time) |

---

## Sign-Off

- [x] All threats have a disposition (mitigate / accept / transfer)
- [x] Accepted risks documented in Accepted Risks Log
- [x] `threats_open: 0` confirmed
- [x] `status: verified` set in frontmatter

**Approval:** verified 2026-10-03
