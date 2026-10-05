# Quiver Lua Runner Refactor

## What This Is

Quiver is PSR's SQLite wrapper library: a C++20 core, a C API for FFI, and bindings for Julia, Dart,
Python and JS, plus an embedded Lua scripting layer (sol2) that exposes the database, binary and
expression APIs to untrusted scripts. The v0.12.9 milestone (shipped 2026-10-03, released as part of
0.13.0) restructured that Lua layer: the 2,539-line `src/lua_runner.cpp` is now a `src/lua_runner/`
folder of small per-domain files, each registering and implementing its own slice. The `LuaRunner` class
and its names in every layer stayed as they were, and the duplication and bugs the mapping found were
fixed along the way, each with its own test and CHANGELOG line.

## Current Milestone: lua-2 Abstract Expressions and Quiver File Layout

**Goal:** A `BinaryFile` *is* an expression. Every expression operation takes one abstract expression type,
which sol2 type-checks in Lua, and `src/lua_runner/` follows the quiver file pattern used by `src/` and `src/c/`.

**Target features:**
- C++: a new `quiver::AbstractExpression` (abstract; one pure virtual `node()`; the expression methods are
  non-virtual members built on it). `Expression` (the concrete lazy-DAG value every operation returns) and
  `BinaryFile` both derive from it. Every operator and free function takes `const AbstractExpression&` (plus
  the `double` overloads); the friend block goes; `explicit Expression(const AbstractExpression&)` replaces
  the implicit `Expression(const BinaryFile&)`. A file's `node()` is today's path-based leaf, so an expression
  never touches the caller's handle.
- One metadata accessor, `get_metadata`, on the abstract type (BREAKING: `Expression::metadata()` and Lua
  `e:metadata()` are renamed).
- Lua: typed `const AbstractExpression&` parameters checked by sol2 through bases (the zero-cost trait form if
  the `f:read` benchmark shows the runtime `bases` tag costs too much); the 13 pinned Pattern 1 operand
  messages are kept through sol2's documented fallback overload, which also reports
  `Cannot <op>: too many arguments (expected N, got M)`; `to_expression`, `is_number` and the `sol::object`
  operator plumbing are deleted; a file gains `aggregate`, `aggregate_agents`, `select_agents`,
  `rename_agents` and `save`.
- Julia: `abstract type AbstractExpression` with `Binary.File` and `Expression` as subtypes; the 97
  forwarding methods collapse onto one conversion through the existing `quiver_expression_from_file`. No C API
  source change.
- Layout: `src/lua_runner/` mirrors the core. `database.cpp` (lifecycle, transactions, dry runs) plus
  `database_{create,read,update,delete,describe,metadata,query,time_series,csv_export,csv_import}.cpp`, with
  `csv.cpp` (mirrors `src/csv/`) and `binary.cpp` + `expression.cpp` (mirror `src/binary/` and
  `src/expression/`). A pure move: no Lua name or error text changes.

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
- ✓ Behaviour the refactor could break is pinned before any code moves: runner move survival (source kept alive and source freed, plus `static_assert(sizeof(LuaRunner) == sizeof(void*))`), the check orders of `open_file`/`read_csv`/`read_csv_stream`/`write_csv`/`export_csv`/`import_csv` with their "options must be a table" controls, the 1,000,000 key-width cap, closed-writer order, and sync-test guards (parse-derived usertype set with a four-type floor, single `open_libraries(`). All Debug/Release-defined; every pin mutation-tested. Baseline `Lua*` = 444 / 12 suites, C API 27. — Phase 1 (v0.12.9)
- ✓ `src/lua_runner.cpp` is split into `src/lua_runner/`: 11 files (`lua_runner.cpp` lifecycle shell of 144 lines, `internal.h`, `return_json.cpp`, `path_policy.cpp`, and seven per-domain binders `db_core`/`db_read`/`db_write`/`db_time_series`/`db_metadata`/`csv`/`binary`), each ≤446 lines, each registering and implementing its own slice. Pure moves: the same 71 `db:` + 15 `quiver.*` names, `Lua*` 444 / C API 27 in Debug and Release, and the same Lua* results on Linux GCC 13 and Clang 18/libc++. — Phase 2 (v0.12.9)
- ✓ The sync test reads `src/lua_runner/` recursively, fails on any `.set_function(` it cannot parse, and guards all usertypes. `/bigobj` covers the whole `quiver` target, and each NOLINT pair moved with its code. — Phase 2 (v0.12.9)
- ✓ Every repeated pattern in `src/lua_runner/` exists once (M1, M3–M5, M7–M16): `run_in_scope`, 36 member-pointer registrations (17 forwarders, `bulk_read_lua`/`collection_read_lua`, `metadata_to_lua`), `std::optional` returns, `read_groups_by_id`, `option_entries`/`collect_entries`/`option_table` with named slots, CSV cells through `lua_to_value`, `CsvWriter` members, `header_object`, prune-on-insert `RunHandles::add_*` plus `close_open_handles`, and `binop<Op>` functors. `src/lua_runner/` went from 2,711 to 2,465 lines. Release golden output is byte-identical; `Lua*` 444 / C API 27; tidy 15 → 14. — Phase 3 (v0.12.9)
- ✓ Release type safety: wrong-type table arguments, map keys and optional arguments raise Pattern 1 `Cannot <op>: <what> must be <expected>, got <lua type>` (`require_table`, `lua_string_key`, `optional_from_lua`) in every build. `SOL_ALL_SAFETIES_ON=1` + `SOL_PRINT_ERRORS=0` are the backstop (a Release dot-call throws instead of crashing; no stderr), with the perf fallback `SOL_SAFE_GETTER=0` / `SOL_SAFE_STACK_CHECK=0` (+16.4% → −1.9% on a bulk read). — Phase 4 (v0.12.9)
- ✓ `db:transaction`/`db:dry_run` reject a non-function before opening anything; a failed COMMIT rolls back and rethrows. — Phase 4 (v0.12.9)
- ✓ Text-only Lua: `load` and the script given to `run()` refuse bytecode (the second was found by code review). — Phase 4 (v0.12.9)
- ✓ An empty array in `update_element` reaches the core and clears the group (BREAKING, CHANGELOG documents the shared-column and round-trip consequences); `create_element` still skips it. — Phase 4 (v0.12.9)
- ✓ Expression operand errors name the public operation, the leftmost bad operand is reported deterministically, and the dead branches are gone. `Lua*` 444 → 477 (Linux 475), C API 27. — Phase 4 (v0.12.9)
- ✓ `resolve_sandboxed_path` is unit-tested directly: `SandboxedPathTest` (11 on Windows, 10 on Linux) through the sol2-free `src/lua_runner/path_policy.h`, with `path_policy.cpp` compiled into `quiver_tests` (nothing newly exported). Outside the `Lua*` filter, which stays at 477. — Phase 5 (v0.12.9)
- ✓ No planning-ID comment remains outside `.planning/` (the repo-wide gate went 256 → 0 lines). Each removed ID was replaced by its reason or the name of the test that pins it. — Phase 5 (v0.12.9)
- ✓ Every AGENTS.md, `LUA_DB_API_REFERENCE` (the empty-array rule, plus what the sandbox does not limit: instructions, memory, wall time, globals across `run()`) and CHANGELOG `[0.13.0] — unreleased` match the finished milestone. All six suites, the sync test, Debug/Release and Linux GCC/Clang are green; the version is 0.13.0. — Phase 5 (v0.12.9)
- ✓ `src/lua_runner/` file names and their split mirror the core: 19 files (`database.cpp`, the ten `database_*.cpp`, `csv.cpp`, `binary.cpp`, `expression.cpp`, plus the shell/helpers), 14 binders `bind_database` … `bind_expression` called in LAYOUT-02 order, every Lua name registered in the file named after the core file that implements it. A pure move: the same 86 names, golden output byte-identical in Debug and Release, `Lua*` 477 / C API 27, all six suites green, tidy at the 14-warning baseline, `git log --follow` intact for the renamed files. — Phase 6 (lua-2)
- ✓ `AbstractExpression` (one pure virtual `node()`, non-virtual `save`/`aggregate*`/`select_agents`/`rename_agents`) is the one parameter type of every C++ expression operator and free function; `Expression final` and `BinaryFile` derive from it, a file's `node()` is a fresh path-based leaf so an expression never touches the caller's handle, `Expression(const AbstractExpression&)` is explicit and `metadata()` is `get_metadata()` in C++ (both BREAKING, CHANGELOG). No C API change. `quiver_tests` 1463, `ExpressionFixture` 125, `Lua*` 477 unchanged, C API 543; Windows Debug/Release and Linux GCC 13 / Clang 18 green. Phase 8 baselines: Release `src/lua_runner` warnings 0, 1M `f:write`/`f:read` medians 2174/2082 ms. — Phase 7 (lua-2)
- ✓ In Lua a binary file is an expression: `BinaryFile` and `Expression` register `AbstractExpression` as their sol2 base through three compile-time traits in `src/lua_runner/internal.h`, every `quiver.*` function, operator metamethod and expression method takes `const AbstractExpression&`, and `to_expression`/`is_number` are gone. A trailing `sol::variadic_args` candidate keeps the 13 pinned Pattern 1 texts byte-identical and adds `Cannot <op>: too many arguments (expected N, got M)` (BREAKING). A file takes `aggregate`, `aggregate_agents`, `select_agents`, `rename_agents`, `save` and `get_metadata`; Lua `e:metadata()` is `get_metadata()` (BREAKING). Traits kept over the runtime `bases` tag by the pre-set benchmark rule (tag +2.3% on `f:read`, within run-to-run noise). `Lua*` 490, `LuaExpressionTest` 41, `quiver_tests` 1476, C API 543; Release C4702 0; Linux GCC 13 / Clang 18 and all six suites green. — Phase 8 (lua-2)

- ✓ In Julia a binary file is an expression: `abstract type AbstractExpression` is declared in `Quiver.jl` before the Binary include, `Binary.File` and `Expression` subtype it, and every operation is defined once on it, converting through a private `_expression` (the existing `quiver_expression_from_file`); the 97 forwarders are gone (`Binary.File` lines in `expression.jl` 54 → 1), every ccall holds its converted handle in `GC.@preserve` (14 sites), and a file gains `Quiver.save` and `Quiver.get_metadata` (now one generic owned by `Binary`). No C API, `c_api.jl` or other-binding change; no ambiguities. Expression tests 186 → 287 (byte-identical parity of every operation on raw files vs expressions), Julia 1675. Root/`src`/`src/c`/Julia AGENTS.md and CHANGELOG `[0.13.0]` describe the finished milestone; all six suites and the lua-api sync test green. — Phase 9 (lua-2)

### Active

(none — milestone lua-2 complete)

### Out of Scope

- Sparse-extent cap for `update_vector_group`/`update_set_group`: the user decided against it. The host is responsible for limiting scripts, and the documented "sparse columns write NULL up to the largest index" rule stands.
- Compile-time optimisation as a goal: splitting across TUs may change build times. We only keep `/bigobj` working and note the measurement; PCH is not planned.
- Moving the sol2-free half of the JSON encoder to `src/json/`: it waits for a second JSON consumer.
- Lua whole-group readers, Lua boolean readers, and the binary/expression subsystems in Dart/Python/JS: these are documented design decisions.
- Relocating `LUA_DB_API_REFERENCE` out of `lua-api.ts`: on the Do-Not-Fix list. Only its text changes.
- New `db:` methods: lua-2 adds none. (The v0.12.9 milestone added no Lua features at all; lua-2's only additions are the expression methods a file gains by becoming an expression.)
- A C API change for the abstract type (a borrowed "file as expression" handle, or file-taking variants of each `quiver_expression_*`): C has no inheritance, a borrowed view would make `quiver_expression_t*` owned-or-borrowed by type, and Julia does not need it. `quiver_expression_from_file` stays the bridge.
- Fixing Lua `==`/`<` between expressions always returning true (sol2's automatic `__eq`/`__lt` on the Expression-returning C++ operators): pre-existing and unchanged by lua-2; a separate decision (`sol::is_automagical` = false).
- Renaming `Expression` to make it the abstract type: `AbstractExpression` + concrete `Expression` follows the Julia `AbstractArray`/`Array` convention and keeps every `Expression e = a + b;` call site.
- Renaming `LuaRunner` to `quiver::Sandbox` in any layer: the class and header, the C API `quiver_lua_runner_*`, every binding's `LuaRunner` and file names, Dart's `LuaException`, `LuaSandboxTest` and the gtest suite names, `resolve_sandboxed_path` and the "sandboxed" wording, `tests/sandbox` / `quiver_sandbox`, and the closed/disposed/not-closed messages. The rename was dropped from this milestone by user decision (keep it simple) on 2026-10-02; see D-01 in `.planning/phases/01-behaviour-pins/01-CONTEXT.md`.

## Context

- Codebase map: `.planning/codebase/*.md`. Detailed map of the file (32 clusters with line ranges, seams, 8 bugs C1–C8, 16 dedup items M1–M16, rename blast radius of 53 files [historical: the rename is out of scope], sync-test contract), plus a critic's corrections: `.planning/milestones/v0.12.9-research/LUA-RUNNER-MAP.md`.
- `Impl` is a set of free functions in disguise. Instance state is only `db`, `lua`, the writer/binary-file registries (`open_writers`, `open_binary_files`, `path_has_open_writer`, `close_open_writers`) and three `[this]` captures (`open_file`, `write_csv`, `expr:save`). The split therefore needs a small `Context`/`RunHandles` held inside the heap-allocated `Impl`, so captured references survive a move. Phase 1 enforces this: `tests/test_lua_runner_lifecycle.cpp` static-asserts `sizeof(LuaRunner) == sizeof(void*)` (fails in every build if run state moves onto `LuaRunner`), and its freed-source pins crash in Debug if bindings reach state through the moved-from runner.
- The sync test (`bindings/js/test/lua-api-sync.test.ts`) reads `src/lua_runner/` recursively (since Phase 2). Pass 1 matches `(bind|ns).set_function("name"`, so the local/parameter names `bind` and `ns` are load-bearing. Pass 2 relies on unqualified `new_usertype<X>`, one method name per line, and the 120-column limit. `current` must reset at file boundaries.
- CI builds Release, where sol2 does not check `sol::table` parameters at all and Lua's API checks are off. That makes C1 undefined behaviour, not just a wrong error.
- Version: CMake and all manifests are at 0.13.0, and the latest tag is `v0.12.9`, so the milestone's BREAKING changes land in 0.13.0 with no further bump. CHANGELOG now has a complete `[0.13.0] — unreleased` section with its compare link; a `[0.12.9]` section is still missing (backfilling it is the maintainer's call).
- **Current state (after v0.12.9):** `src/lua_runner/` is 13 files, 2,575 lines (largest `csv.cpp`, 446). Final counts:
  `quiver_tests` 1454 (`Lua*` 477, `SandboxedPathTest` 11; Linux `Lua*` 475 with 1 skip, sandbox 10), `quiver_c_tests`
  543 (`LuaRunnerCApiTest` 27). sol2 defines: `SOL_ALL_SAFETIES_ON=1`, `SOL_PRINT_ERRORS=0`, `SOL_SAFE_NUMERICS=1`,
  `SOL_NO_NIL=1`, `SOL_SAFE_GETTER=0`, `SOL_SAFE_STACK_CHECK=0`.
- **Known tech debt (from the v0.12.9 audit):** wrong-type *positional* string/number arguments still produce sol2's
  raw text, not Pattern 1; `{}` skips on `create_element` but clears on `update_element`; three info items from
  05-REVIEW (IN-01 static-link rationale, IN-03 partial pin note, IN-04 symlink-free sibling-prefix test); Apple
  Clang and the off-Windows binding suites are proven only by PR CI; the CHANGELOG `[0.12.9]` section is missing.
  Full list: `.planning/milestones/v0.12.9-MILESTONE-AUDIT.md`.
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
| Release type safety: explicit `require_table` (Pattern 1) plus `SOL_ALL_SAFETIES_ON` backstop | Release currently has UB on wrong-type arguments; explicit checks give good messages, the flag covers `self` and anything missed | ✓ Phase 4. The 5% budget was exceeded (+16.4%), so the roadmap's fallback `SOL_SAFE_GETTER=0` + `SOL_SAFE_STACK_CHECK=0` landed (−1.9% / +0.5%). Side effect: the explicit getter define also applies in Debug, so unguarded `.as<T>()` is no longer caught there; all current sites are guarded |
| No sparse-extent cap on the vector/set group writers | User choice: the host limits scripts | ✓ Held through v0.12.9 (documented rule stands; listed in Out of Scope) |
| An empty array in Lua `create_element`/`update_element` is passed through to the core (clears on update) | Consistency with C++/Python/JS. On update, a typo'd empty column now throws ("does not match any vector, set, or time series table") instead of being ignored | ✓ Phase 4 (BREAKING). Also clears every group sharing the column name and, for `{ date_time = {} }`, every time-series group; tests pin each edge |
| Rename tail: closed/disposed messages, `SandboxException`, "directory containment" wording (`resolve_contained_path`), one `Sandbox*` test prefix | So "sandbox" means only the class | Reverted 2026-10-02: user decision, dropped with the rename. The scope statement (what the sandbox does not limit) stays, in DOC-04 |
| Debug-only sol2 diagnostic text may change in the dedupe (dot-call text, and the bound C++ signature in "bad argument" for the M3 member pointers and the M5 `std::optional` returns) | Roadmap allowed it for member pointers; research showed the same mechanism also covers M5. No test pins it, and Release prints none of it | ✓ Phase 3: 12 probes changed, all within the allowance; 13 control probes are unchanged. Orchestrator decision, flagged to the user |
| Planning-ID comments replaced repo-wide | They point at deleted `.planning` files; Human-Centric principle | ✓ Phase 5: gate at 0. Code review caught one left-behind numbering (the Reader error "catalogue" ordinals); it now names the pinning tests |
| One PR per phase into master, each green on its own | Reviewable and bisectable; split and dedupe stay provably behaviour-neutral | ⚠️ Revisit: all five phases were built on the single branch `rs/runner`; per-phase PRs are still possible by splitting at the phase boundaries, or it ships as one PR |
| Tests that pin behaviour first, then split, dedupe, fixes, and the path-policy test and docs last | The split is only safe once the existing behaviour is pinned | ✓ Phase 1 done: 16 pins + 2 controls, mutation-tested; a live-source-only move pin was shown too weak (run state reached through the moved-from runner passed), fixed with freed-source pins and a `sizeof` static_assert |
| Delete the `SOL_SAFE_FUNCTION=1` define and its AGENTS.md claim (SAFE-06) | sol2 v3.5.0 never reads it (only `SOL_SAFE_FUNCTIONS`, `SOL_SAFE_FUNCTION_OBJECTS`, `SOL_SAFE_FUNCTION_CALLS`), so it is dead; `SOL_ALL_SAFETIES_ON` covers what it claimed. Chosen in REQUIREMENTS over the research default of keeping it with a corrected comment | ✓ Phase 4 |
| Lua `load` accepts text chunks only (SAFE-07) | A bytecode chunk is a crash vector for an untrusted script; string-form `load` stays, so the root sandbox decision only gains "text chunks only". Adopted in REQUIREMENTS although research listed it as v2 | ✓ Phase 4. Extended to `run()`'s own script after code review found it still accepted bytecode |
| Binding source, test and header files are renamed with the class (`quiverdb.sandbox` module path) | No-alias policy, and a `lua_runner` file name would keep the old meaning alive | Reverted 2026-10-02: user decision, dropped with the rename |
| `AbstractExpression` (abstract parameter type) + `Expression` (concrete result) + `BinaryFile` (file leaf), not a Lua-only conversion | User directive (lua-2): "the only parameter of a binary op, unary op, etc should be an abstract type of an expression". An operation must return a value, so the input type is wider than the output type (Julia's AbstractArray/Array) | ✓ Phase 7 (C++), Phase 8 (Lua), Phase 9 (Julia; C API unchanged, `quiver_expression_from_file` is the bridge) |
| Pattern 1 operand errors kept under typed sol2 parameters via sol2's fallback overload | User choice (lua-2): sol2 does the type check; the fallback runs only after every typed candidate fails, so the 13 pinned messages and the root error-message rule hold | ✓ Phase 8: pinned test bodies byte-identical to base; the fallback also owns the too-many-arguments text. Base registered by compile-time traits (runtime tag measured +2.3% on `f:read`) |
| One metadata accessor `get_metadata` on files and expressions | User choice (lua-2): matches Julia and the C API; BREAKING rename of `Expression::metadata()` / `e:metadata()` | ✓ Phase 7 (C++) + Phase 8 (Lua; no alias kept) |
| `src/lua_runner/` mirrors the core's `database_*.cpp` split (+ `csv`, `binary`, `expression`) | User request (lua-2): follow the quiver name pattern; the v0.12.9 `db_*` domain names cut across the core's create/read/update/delete/describe split | ✓ Phase 6 |

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
*Last updated: 2026-10-04 after Phase 9 (Julia AbstractExpression and Docs) of milestone lua-2 — milestone complete*
