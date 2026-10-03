# Milestones

## v0.12.9 Quiver Lua Runner Refactor (Shipped: 2026-10-03)

**Phases completed:** 5 phases, 17 plans
**Closeout:** verified_closeout (all phases verified `passed`; audit `tech_debt`, no blockers; no git tag, by user decision, because the milestone id collides with the existing `v0.12.9` release tag)
**Git range:** `4b4728c` → `8fda38f` (144 commits, 2026-10-02 → 2026-10-03); 56 files outside `.planning/`, +4,489 / −3,028
**Ships in:** 0.13.0 (CHANGELOG `[0.13.0] — unreleased`)

**Delivered:** The 2,539-line `src/lua_runner.cpp` is now `src/lua_runner/`, a folder of small per-domain files. Existing scripts behave as before, apart from the deliberate fixes, each pinned by a test.

**Key accomplishments:**

- Behaviour pins landed before any code moved: move survival (kept-alive and freed source, plus `sizeof(LuaRunner) == sizeof(void*)`), check orders, the key-width cap, closed-writer order, and sync-test guards. Every pin was mutation-tested.
- The monolith was split into 11 per-domain files of 446 lines or fewer, each registering and implementing its own slice of the Lua surface. The sync test now reads the folder recursively.
- Every repeated pattern now exists once: `run_in_scope`, the member-pointer forwarders, `option_entries`, `RunHandles::add_*` with `close_open_handles`, and `binop<Op>`.
- Release type safety: a wrong-type table argument, map key or optional argument raises a Pattern 1 error in every build. `SOL_ALL_SAFETIES_ON` with `SOL_PRINT_ERRORS=0` is the backstop, so a Release dot-call throws instead of crashing. The perf fallback brought the cost back within noise.
- Fixes, each red-then-green with a CHANGELOG line: `load` and `run()` accept text only, `db:transaction`/`db:dry_run` check their argument and roll back a failed COMMIT, the empty-array rule (C7, BREAKING), and expression operand errors.
- `SandboxedPathTest` tests the path gate directly, through a sol2-free header. No planning-ID comments remain. AGENTS.md, `LUA_DB_API_REFERENCE` and the CHANGELOG `[0.13.0]` section match the code.

**Final counts:** `quiver_tests` 1454 (`Lua*` 477, `SandboxedPathTest` 11), `quiver_c_tests` 543 (`LuaRunnerCApiTest` 27). On Linux GCC 13 and Clang 18/libc++: `Lua*` 475 (474 pass + 1 skip), sandbox 10.

**Known tech debt:** see `milestones/v0.12.9-MILESTONE-AUDIT.md`. The main items: positional-argument type errors still use sol2's raw text; `{}` behaves differently in create and update; three 05-REVIEW info items; Apple Clang is covered only by PR CI.

---
