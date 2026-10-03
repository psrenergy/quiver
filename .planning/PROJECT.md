# Quiver Lua Runner Refactor

## What This Is

Quiver is PSR's SQLite wrapper library: a C++20 core, a C API for FFI, and bindings for Julia, Dart,
Python and JS, plus an embedded Lua scripting layer (sol2) that exposes the database, binary and
expression APIs to untrusted scripts. This milestone restructures that Lua layer. Today it lives in a
single 2,539-line `src/lua_runner.cpp`, almost all of it one `LuaRunner::Impl` struct. It becomes a
`src/lua_runner/` folder of small per-domain files. The `LuaRunner` class and its names in every layer
stay as they are, and the duplication and bugs the mapping found are fixed along the way.

## Core Value

Every file in the Lua scripting layer is small and single-purpose enough for an agent to change
safely, and every existing script behaves exactly as before, apart from the deliberate, test-pinned
fixes listed below.

## Requirements

### Validated

<!-- Existing capabilities the refactor must preserve (inferred from the codebase + AGENTS.md). -->

- ✓ `LuaRunner::run(script)` runs a script against a borrowed `Database&` exposed as `db`, and returns the first return value as JSON (`""` for no return, `"null"` for nil, 32-level / 64 MiB caps, sorted keys, UTF-8 validation, duplicate-key rejection). Existing.
- ✓ The full `db:` surface mirrors the C++ API 1:1 (CRUD, `_by_label` writes, relations, scalar/vector/set/time-series reads and writes, metadata, query, describe, CSV import/export, transactions, dry runs, migrations validation). Existing.
- ✓ Lua-only CSV I/O: `db:read_csv`, `db:read_csv_stream`, `db:write_csv` → `w:write_row` / `w:close`, with writers closed at `run()` exit through a `weak_ptr` registry. Existing.
- ✓ Binary + expression subsystem in Lua (`db:open_file`, `db:bin_to_csv`, `db:csv_to_bin`, `quiver.metadata*`, `quiver.expression`, operator metamethods, `quiver.gt/lt/...`, `quiver.ifelse`, aggregate/select/rename, `expr:save`). Existing.
- ✓ Filesystem policy: every file operation resolves against the database directory with strict containment; `:memory:` rejects all file ops; `dofile`/`loadfile` removed; stdlib limited to base/string/table/math/coroutine/utf8. Existing.
- ✓ Write-side typing policy: a Lua boolean is INTEGER 1/0 on every write path; converters throw on unsupported types and never skip; NULL is a `nil` hole on reads. Existing.
- ✓ Exposed in every layer: C API `quiver_lua_runner_*`, `LuaRunner` in Julia/Dart/Python/JS, used by `quiver_cli`. Existing.
- ✓ Agent-facing Lua reference `LUA_DB_API_REFERENCE` (`bindings/js/src/lua-api.ts`), kept in sync with the source by `bindings/js/test/lua-api-sync.test.ts`. Existing.
- ✓ Behaviour the refactor could break is pinned before any code moves: runner move survival (source kept alive and source freed, plus `static_assert(sizeof(LuaRunner) == sizeof(void*))`), the check orders of `open_file`/`read_csv`/`read_csv_stream`/`write_csv`/`export_csv`/`import_csv` with their "options must be a table" controls, the 1,000,000 key-width cap, closed-writer order, and sync-test guards (parse-derived usertype set with a four-type floor, single `open_libraries(`). All Debug/Release-defined; every pin mutation-tested. Baseline `Lua*` = 444 / 12 suites, C API 27. — Phase 1
- ✓ `src/lua_runner.cpp` is split into `src/lua_runner/`: 11 files (`lua_runner.cpp` lifecycle shell of 144 lines, `internal.h`, `return_json.cpp`, `path_policy.cpp`, and seven per-domain binders `db_core`/`db_read`/`db_write`/`db_time_series`/`db_metadata`/`csv`/`binary`), each ≤446 lines, each registering and implementing its own slice. Pure moves: the same 71 `db:` + 15 `quiver.*` names, `Lua*` 444 / C API 27 in Debug and Release, and the same Lua* results on Linux GCC 13 and Clang 18/libc++. — Phase 2
- ✓ The sync test reads `src/lua_runner/` recursively, fails on any `.set_function(` it cannot parse, and guards all usertypes. `/bigobj` covers the whole `quiver` target, and each NOLINT pair moved with its code. — Phase 2

### Active

- [ ] Repeated boilerplate is collapsed into shared helpers: the transaction/dry-run body, bulk-read adapters, member-pointer forwarders, one registration style, the metadata wrappers, option decoders, and `CsvWriter` behaviour as members. No behaviour change.
- [ ] Release builds type-check table and `self` arguments: an explicit `require_table` gives a Pattern 1 error at every table parameter, and `SOL_ALL_SAFETIES_ON` is turned on as a backstop. The performance cost is measured.
- [ ] Map keys are type-checked before they become column, dimension or attribute names (Pattern 1 instead of a wrong name or a raw panic).
- [ ] An optional argument with the wrong type raises an error instead of being treated as absent. `db:transaction`/`db:dry_run` reject a non-function with Pattern 1. A failed COMMIT in `db:transaction` rolls back.
- [ ] Lua `load` accepts text chunks only; a bytecode chunk raises an error, and string-form `load` keeps working.
- [ ] An empty array in Lua `create_element`/`update_element` is passed to the core the way C++/Python/JS pass it, instead of being dropped by the Lua converter. `update_element` with `{col = {}}` then clears that group. `create_element` still skips it, because the core does, and the reference text is updated to match.
- [ ] Error messages from the expression helpers name the public operation; dead branches are removed.
- [ ] The path-containment gate `resolve_sandboxed_path` is unit-tested directly, in addition to the Lua-level tests.
- [ ] Planning IDs in comments (`D-xx`, `LUA-xx`, `WRITE-xx`, `FMT-xx`, `TEST-xx`, references to deleted `.planning` files) are replaced repo-wide with their one-line reason or the test that pins them.
- [ ] Every AGENTS.md, the shipped Lua reference text (including what the sandbox does not limit) and the CHANGELOG (`[0.13.0]`, BREAKING entries saying what a script author must change) match the new layout and behaviour.

### Out of Scope

- Sparse-extent cap for `update_vector_group`/`update_set_group`: the user decided against it. The host is responsible for limiting scripts, and the documented "sparse columns write NULL up to the largest index" rule stands.
- Compile-time optimisation as a goal: splitting across TUs may change build times. We only keep `/bigobj` working and note the measurement; PCH is not planned.
- Moving the sol2-free half of the JSON encoder to `src/json/`: it waits for a second JSON consumer.
- Lua whole-group readers, Lua boolean readers, and the binary/expression subsystems in Dart/Python/JS: these are documented design decisions.
- Relocating `LUA_DB_API_REFERENCE` out of `lua-api.ts`: on the Do-Not-Fix list. Only its text changes.
- New Lua features or new `db:` methods: this milestone restructures and fixes, it does not extend.
- Renaming `LuaRunner` to `quiver::Sandbox` in any layer: the class and header, the C API `quiver_lua_runner_*`, every binding's `LuaRunner` and file names, Dart's `LuaException`, `LuaSandboxTest` and the gtest suite names, `resolve_sandboxed_path` and the "sandboxed" wording, `tests/sandbox` / `quiver_sandbox`, and the closed/disposed/not-closed messages. The rename was dropped from this milestone by user decision (keep it simple) on 2026-10-02; see D-01 in `.planning/phases/01-behaviour-pins/01-CONTEXT.md`.

## Context

- Codebase map: `.planning/codebase/*.md`. Detailed map of the file (32 clusters with line ranges, seams, 8 bugs C1–C8, 16 dedup items M1–M16, rename blast radius of 53 files [historical: the rename is out of scope], sync-test contract), plus a critic's corrections: `.planning/research/LUA-RUNNER-MAP.md`.
- `Impl` is a set of free functions in disguise. Instance state is only `db`, `lua`, the writer/binary-file registries (`open_writers`, `open_binary_files`, `path_has_open_writer`, `close_open_writers`) and three `[this]` captures (`open_file`, `write_csv`, `expr:save`). The split therefore needs a small `Context`/`RunHandles` held inside the heap-allocated `Impl`, so captured references survive a move. Phase 1 enforces this: `tests/test_lua_runner_lifecycle.cpp` static-asserts `sizeof(LuaRunner) == sizeof(void*)` (fails in every build if run state moves onto `LuaRunner`), and its freed-source pins crash in Debug if bindings reach state through the moved-from runner.
- The sync test (`bindings/js/test/lua-api-sync.test.ts`) reads `src/lua_runner/` recursively (since Phase 2). Pass 1 matches `(bind|ns).set_function("name"`, so the local/parameter names `bind` and `ns` are load-bearing. Pass 2 relies on unqualified `new_usertype<X>`, one method name per line, and the 120-column limit. `current` must reset at file boundaries.
- CI builds Release, where sol2 does not check `sol::table` parameters at all and Lua's API checks are off. That makes C1 undefined behaviour, not just a wrong error.
- Version: CMake and all manifests are at 0.13.0, and the latest tag is `v0.12.9`, so the milestone's BREAKING changes land in 0.13.0 with no further bump. CHANGELOG has no `[0.12.9]` or `[0.13.0]` section yet; the newest is `[0.12.8]`.
- Development is on Windows (Git Bash/PowerShell). Python runs via `uv run`. `scripts/test-all.bat` runs the six suites.

## Constraints

- **Behaviour**: zero behaviour change in the split and dedupe phases. Each behaviour fix lands with its own test and CHANGELOG line, because the shipped Lua reference and four binding suites depend on exact semantics.
- **Design decisions**: the AGENTS.md "Design Decisions" and "Do Not Fix" items stay as they are, except the empty-array rule (C7), which the user explicitly changed. They are settled, not up for re-litigation.
- **sol2 build**: `SOL_SAFE_NUMERICS`/`SOL_NO_NIL` stay PRIVATE on the `quiver` target, and every new TU stays in it. Phase 4 adds `SOL_ALL_SAFETIES_ON=1`/`SOL_PRINT_ERRORS=0` there and deletes the no-op `SOL_SAFE_FUNCTION=1` (see Key Decisions). Write `sol::lua_nil`, never `sol::nil`. csv-parser headers must never be included from `src/lua_runner/`.
- **Order-sensitive code**: in the ctor, `open_libraries` → nil `dofile`/`loadfile` → create the `quiver` table → binders → `lua["db"]`. `GcGuard` is declared before `result`, with `close_open_writers` then exactly one `collect_garbage()`. Check orders that pick which error a call reports (e.g. `open_file` validates `mode` before the path) stay byte-for-byte.
- **Cross-layer rules**: names map mechanically across layers, tests exist at every layer, error messages are defined only in C++/C API, and every AGENTS.md nearest a change is updated (Self-Updating).
- **FFI declarations**: only Julia's `bindings/julia/src/c_api.jl` is regenerated (its own `generator.bat`; review every hunk). Dart's `bindings.dart` is **hand-edited** in its existing style. Regenerating it with the pinned ffigen rewrites the whole file into breaking enums (`bindings/dart/AGENTS.md`), so never run `scripts/generator.bat`. Python's `_c_api.py` cdefs (plus `generator.py`'s header list) and the JS `loader.ts` symbol table are hand-maintained.
- **Tooling**: clang-format 22.1.8 (120 columns), and `scripts/tidy.bat` lints the new `src/lua_runner/` files.

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Rename `LuaRunner` → `quiver::Sandbox` in every layer (C API `quiver_sandbox_*`) | User request; the mechanical cross-layer naming rule | Reverted 2026-10-02: user decision, dropped from this milestone to keep it simple |
| Scratch target `tests/sandbox` → `tests/scratch` / `quiver_scratch` | Frees the name; the scratch target is still kept on purpose | Reverted 2026-10-02: user decision, dropped with the rename |
| Per-domain layout: each `src/lua_runner/*.cpp` registers and implements its own slice, ~450-line ceiling | User's pain is file size for agent editing; locality means adding a method touches one file plus the reference | ✓ Phase 2: largest file `csv.cpp` at 446 lines. Clean build of `quiver` went from 70 s to 46 s wall time, but total CPU time across the Lua files went from 61 s to 223 s, because each file now parses sol2 separately. Accepted (compile time is out of scope); Phase 3 should not add more sol2 TUs without reason |
| Split folder is `src/lua_runner/` (`lua_runner.cpp` plus the per-domain files), not `src/sandbox/` | Follows from dropping the rename: the folder keeps the class's name. The C API TU `src/c/lua_runner.cpp` is not renamed | ✓ Phase 2 |
| Release type safety: explicit `require_table` (Pattern 1) plus `SOL_ALL_SAFETIES_ON` backstop | Release currently has UB on wrong-type arguments; explicit checks give good messages, the flag covers `self` and anything missed | — Pending |
| No sparse-extent cap on the vector/set group writers | User choice: the host limits scripts | — Pending |
| An empty array in Lua `create_element`/`update_element` is passed through to the core (clears on update) | Consistency with C++/Python/JS. On update, a typo'd empty column now throws ("does not match any vector, set, or time series table") instead of being ignored | — Pending |
| Rename tail: closed/disposed messages, `SandboxException`, "directory containment" wording (`resolve_contained_path`), one `Sandbox*` test prefix | So "sandbox" means only the class | Reverted 2026-10-02: user decision, dropped with the rename. The scope statement (what the sandbox does not limit) stays, in DOC-04 |
| Planning-ID comments replaced repo-wide | They point at deleted `.planning` files; Human-Centric principle | — Pending |
| One PR per phase into master, each green on its own | Reviewable and bisectable; split and dedupe stay provably behaviour-neutral | — Pending |
| Tests that pin behaviour first, then split, dedupe, fixes, and the path-policy test and docs last | The split is only safe once the existing behaviour is pinned | ✓ Phase 1 done: 16 pins + 2 controls, mutation-tested; a live-source-only move pin was shown too weak (run state reached through the moved-from runner passed), fixed with freed-source pins and a `sizeof` static_assert |
| Delete the `SOL_SAFE_FUNCTION=1` define and its AGENTS.md claim (SAFE-06) | sol2 v3.5.0 never reads it (only `SOL_SAFE_FUNCTIONS`, `SOL_SAFE_FUNCTION_OBJECTS`, `SOL_SAFE_FUNCTION_CALLS`), so it is dead; `SOL_ALL_SAFETIES_ON` covers what it claimed. Chosen in REQUIREMENTS over the research default of keeping it with a corrected comment | — Pending |
| Lua `load` accepts text chunks only (SAFE-07) | A bytecode chunk is a crash vector for an untrusted script; string-form `load` stays, so the root sandbox decision only gains "text chunks only". Adopted in REQUIREMENTS although research listed it as v2 | — Pending |
| Binding source, test and header files are renamed with the class (`quiverdb.sandbox` module path) | No-alias policy, and a `lua_runner` file name would keep the old meaning alive | Reverted 2026-10-02: user decision, dropped with the rename |

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `/gsd-transition`):
1. Requirements invalidated? → Move to Out of Scope with reason
2. Requirements validated? → Move to Validated with phase reference
3. New requirements emerged? → Add to Active
4. Decisions to log? → Add to Key Decisions
5. "What This Is" still accurate? → Update if drifted

**After each milestone** (via `/gsd-complete-milestone`):
1. Full review of all sections
2. Core Value check — still the right priority?
3. Audit Out of Scope — reasons still valid?
4. Update Context with current state

---
*Last updated: 2026-10-03 after Phase 2 (mechanical split)*
