# Quiver Sandbox Refactor

## What This Is

Quiver is PSR's SQLite wrapper library: a C++20 core, a C API for FFI, and bindings for Julia, Dart,
Python and JS, plus an embedded Lua scripting layer (sol2) that exposes the database, binary and
expression APIs to untrusted scripts. This milestone restructures that Lua layer. Today it lives in a
single 2,539-line `src/lua_runner.cpp`, almost all of it one `LuaRunner::Impl` struct. It becomes a
`src/sandbox/` folder of small per-domain files. The class is renamed `quiver::Sandbox` in every
layer, and the duplication and bugs the mapping found are fixed along the way.

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

### Active

- [ ] Untested behaviour that the refactor could break is pinned by tests before any code moves (the 1,000,000 key-width cap, the check orders that pick which error a call reports, the sync test's usertype guards, a runner move). Only behaviour defined in both Debug and Release is pinned; non-string keys and non-table payloads are Release UB today, so their tests are written red-then-green with the fixes.
- [ ] `src/lua_runner.cpp` is split into `src/sandbox/` per-domain translation units, each of which both registers and implements its slice of the surface. No file is over ~450 lines and behaviour does not change.
- [ ] The sync test reads every source file in the new folder and guards all usertypes; `/bigobj` applies to every sol2 TU; the NOLINT blocks move with their code.
- [ ] Repeated boilerplate is collapsed into shared helpers: the transaction/dry-run body, bulk-read adapters, member-pointer forwarders, one registration style, the metadata wrappers, option decoders, and `CsvWriter` behaviour as members. No behaviour change.
- [ ] Release builds type-check table and `self` arguments: an explicit `require_table` gives a Pattern 1 error at every table parameter, and `SOL_ALL_SAFETIES_ON` is turned on as a backstop. The performance cost is measured.
- [ ] Map keys are type-checked before they become column, dimension or attribute names (Pattern 1 instead of a wrong name or a raw panic).
- [ ] An optional argument with the wrong type raises an error instead of being treated as absent. `db:transaction`/`db:dry_run` reject a non-function with Pattern 1. A failed COMMIT in `db:transaction` rolls back.
- [ ] Lua `load` accepts text chunks only; a bytecode chunk raises an error, and string-form `load` keeps working.
- [ ] An empty array in Lua `create_element`/`update_element` is passed to the core the way C++/Python/JS pass it, instead of being dropped by the Lua converter. `update_element` with `{col = {}}` then clears that group. `create_element` still skips it, because the core does, and the reference text is updated to match.
- [ ] Error messages from the expression helpers name the public operation; dead branches are removed.
- [ ] `LuaRunner` becomes `quiver::Sandbox` in every layer: C++ (`include/quiver/sandbox.h`), C API `quiver_sandbox_*`, and `Sandbox` in Julia/Dart/Python/JS and the CLI. The closed/disposed messages and Dart's `SandboxException` follow it, the file rule is renamed "directory containment" (`resolve_contained_path`), and all Lua suites share one `Sandbox*` gtest prefix (`LuaSandboxTest` becomes `SandboxFileTest`). Source, test and header files are renamed to match (Python's module path becomes `quiverdb.sandbox`).
- [ ] The scratch target `tests/sandbox` → `tests/scratch` / `quiver_scratch` (still kept on purpose).
- [ ] Planning IDs in comments (`D-xx`, `LUA-xx`, `WRITE-xx`, `FMT-xx`, `TEST-xx`, references to deleted `.planning` files) are replaced repo-wide with their one-line reason or the test that pins them.
- [ ] Every AGENTS.md, the shipped Lua reference text and the CHANGELOG (`[0.13.0]`, BREAKING entry with caller migration) match the new layout and names.

### Out of Scope

- Sparse-extent cap for `update_vector_group`/`update_set_group`: the user decided against it. The host is responsible for limiting scripts, and the documented "sparse columns write NULL up to the largest index" rule stands.
- Compile-time optimisation as a goal: splitting across TUs may change build times. We only keep `/bigobj` working and note the measurement; PCH is not planned.
- Moving the sol2-free half of the JSON encoder to `src/json/`: it waits for a second JSON consumer.
- Lua whole-group readers, Lua boolean readers, and the binary/expression subsystems in Dart/Python/JS: these are documented design decisions.
- Relocating `LUA_DB_API_REFERENCE` out of `lua-api.ts`: on the Do-Not-Fix list. Only its text changes.
- New Lua features or new `db:` methods: this milestone restructures and fixes, it does not extend.
- Compatibility aliases for `LuaRunner` / `quiver_lua_runner_*`: the project is WIP and the policy is to delete, not deprecate.

## Context

- Codebase map: `.planning/codebase/*.md`. Detailed map of the file (32 clusters with line ranges, seams, 8 bugs C1–C8, 16 dedup items M1–M16, rename blast radius of 53 files, sync-test contract), plus a critic's corrections: `.planning/research/LUA-RUNNER-MAP.md`.
- `Impl` is a set of free functions in disguise. Instance state is only `db`, `lua`, the writer/binary-file registries (`open_writers`, `open_binary_files`, `path_has_open_writer`, `close_open_writers`) and three `[this]` captures (`open_file`, `write_csv`, `expr:save`). The split therefore needs a small `Context`/`RunHandles` held inside the heap-allocated `Impl`, so captured references survive a move.
- The sync test (`bindings/js/test/lua-api-sync.test.ts`) hardcodes `src/lua_runner.cpp`. Pass 1 matches `(bind|ns).set_function("name"`, so the local/parameter names `bind` and `ns` are load-bearing. Pass 2 relies on unqualified `new_usertype<X>`, one method name per line, and the 120-column limit. `current` must reset at file boundaries.
- CI builds Release, where sol2 does not check `sol::table` parameters at all and Lua's API checks are off. That makes C1 undefined behaviour, not just a wrong error.
- Version: CMake and all manifests are at 0.13.0, and the latest tag is `v0.12.9`, so the BREAKING rename lands in 0.13.0 with no further bump. CHANGELOG has no `[0.12.9]` or `[0.13.0]` section yet; the newest is `[0.12.8]`.
- Development is on Windows (Git Bash/PowerShell). Python runs via `uv run`. `scripts/test-all.bat` runs the six suites.

## Constraints

- **Behaviour**: zero behaviour change in the split and dedupe phases. Each behaviour fix lands with its own test and CHANGELOG line, because the shipped Lua reference and four binding suites depend on exact semantics.
- **Design decisions**: the AGENTS.md "Design Decisions" and "Do Not Fix" items stay as they are, except the empty-array rule (C7), which the user explicitly changed. They are settled, not up for re-litigation.
- **sol2 build**: `SOL_SAFE_NUMERICS`/`SOL_NO_NIL` stay PRIVATE on the `quiver` target, and every new TU stays in it. Phase 4 adds `SOL_ALL_SAFETIES_ON=1`/`SOL_PRINT_ERRORS=0` there and deletes the no-op `SOL_SAFE_FUNCTION=1` (see Key Decisions). Write `sol::lua_nil`, never `sol::nil`. csv-parser headers must never be included from `src/sandbox/`.
- **Order-sensitive code**: in the ctor, `open_libraries` → nil `dofile`/`loadfile` → create the `quiver` table → binders → `lua["db"]`. `GcGuard` is declared before `result`, with `close_open_writers` then exactly one `collect_garbage()`. Check orders that pick which error a call reports (e.g. `open_file` validates `mode` before the path) stay byte-for-byte.
- **Cross-layer rules**: names map mechanically across layers, tests exist at every layer, error messages are defined only in C++/C API, and every AGENTS.md nearest a change is updated (Self-Updating).
- **FFI declarations**: only Julia's `bindings/julia/src/c_api.jl` is regenerated (its own `generator.bat`; review every hunk). Dart's `bindings.dart` is **hand-edited** in its existing style. Regenerating it with the pinned ffigen rewrites the whole file into breaking enums (`bindings/dart/AGENTS.md`), so never run `scripts/generator.bat`. Python's `_c_api.py` cdefs (plus `generator.py`'s header list) and the JS `loader.ts` symbol table are hand-maintained.
- **Tooling**: clang-format 22.1.8 (120 columns), and `scripts/tidy.bat` lints the new `src/sandbox/` files.

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Rename `LuaRunner` → `quiver::Sandbox` in every layer (C API `quiver_sandbox_*`) | User request; the mechanical cross-layer naming rule | — Pending |
| Scratch target `tests/sandbox` → `tests/scratch` / `quiver_scratch` | Frees the name; the scratch target is still kept on purpose | — Pending |
| Per-domain layout: each `src/sandbox/*.cpp` registers and implements its own slice, ~450-line ceiling | User's pain is file size for agent editing; locality means adding a method touches one file plus the reference | — Pending |
| Release type safety: explicit `require_table` (Pattern 1) plus `SOL_ALL_SAFETIES_ON` backstop | Release currently has UB on wrong-type arguments; explicit checks give good messages, the flag covers `self` and anything missed | — Pending |
| No sparse-extent cap on the vector/set group writers | User choice: the host limits scripts | — Pending |
| An empty array in Lua `create_element`/`update_element` is passed through to the core (clears on update) | Consistency with C++/Python/JS. On update, a typo'd empty column now throws ("does not match any vector, set, or time series table") instead of being ignored | — Pending |
| Rename tail: closed/disposed messages, `SandboxException`, "directory containment" wording (`resolve_contained_path`), one `Sandbox*` test prefix | So "sandbox" means only the class | — Pending |
| Planning-ID comments replaced repo-wide | They point at deleted `.planning` files; Human-Centric principle | — Pending |
| One PR per phase into master, each green on its own | Reviewable and bisectable; split and dedupe stay provably behaviour-neutral | — Pending |
| Tests that pin behaviour first, then split, dedupe, fixes, rename | The split is only safe once the existing behaviour is pinned | — Pending |
| Delete the `SOL_SAFE_FUNCTION=1` define and its AGENTS.md claim (SAFE-06) | sol2 v3.5.0 never reads it (only `SOL_SAFE_FUNCTIONS`, `SOL_SAFE_FUNCTION_OBJECTS`, `SOL_SAFE_FUNCTION_CALLS`), so it is dead; `SOL_ALL_SAFETIES_ON` covers what it claimed. Chosen in REQUIREMENTS over the research default of keeping it with a corrected comment | — Pending |
| Lua `load` accepts text chunks only (SAFE-07) | A bytecode chunk is a crash vector for an untrusted script; string-form `load` stays, so the root sandbox decision only gains "text chunks only". Adopted in REQUIREMENTS although research listed it as v2 | — Pending |
| Binding source, test and header files are renamed with the class (`quiverdb.sandbox` module path) | No-alias policy, and a `lua_runner` file name would keep the old meaning alive | — Pending |

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
*Last updated: 2026-10-02 after roadmap revision (SOL_SAFE_FUNCTION, text-only `load` and file-rename decisions recorded)*
