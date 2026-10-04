# Roadmap: Quiver Lua Runner Refactor

## Milestones

- ✅ **v0.12.9 Quiver Lua Runner Refactor** — Phases 1-5 (shipped 2026-10-03; ships in release 0.13.0)
- 🚧 **lua-2 Abstract Expressions and Quiver File Layout** — Phases 6-9 (in progress; ships in release 0.13.0)

## Overview

lua-2 makes a `BinaryFile` *be* an expression, and lays `src/lua_runner/` out the way `src/` and `src/c/` are laid
out. The order is strict. First the binder files are renamed and re-split to mirror the core's `database_*.cpp`
files. That is a pure move, proven by the existing suites and the golden harness, so the expression work then lands
in its final file (`src/lua_runner/expression.cpp`) and is never moved twice. Next the C++ core gains
`AbstractExpression`, the one parameter type of every expression operation, with `Expression` and `BinaryFile`
derived from it and ownership unchanged. Lua then type-checks operands through sol2 against that base, keeping the
13 pinned Pattern 1 operand messages. Julia mirrors the type last, and the docs are finished once, against the final
code. Each BREAKING change gets its CHANGELOG `[0.13.0] — unreleased` line in the phase that makes it; Phase 9 checks
that the section is complete. No version bump: everything lands in 0.13.0. Every phase keeps the AGENTS.md nearest
its change current (Self-Updating rule) and writes no planning ID into a code or test comment. After Phase 6 the Dart
suite runs only after deleting `bindings/dart/.dart_tool/hooks_runner/` and `.dart_tool/lib/`, because the hook's
cache does not notice source-list changes.

Baseline at `da6f67b`: `quiver_tests` 1454 (`--gtest_filter=Lua*` 477 in 12 suites, `SandboxedPathTest*` 11),
`quiver_c_tests` 543 (`LuaRunnerCApiTest*` 27); Linux GCC 13 / Clang 18: `Lua*` 475 (474 + 1 skip), sandbox 10.

## Phases

<details>
<summary>✅ v0.12.9 Quiver Lua Runner Refactor (Phases 1-5) — SHIPPED 2026-10-03</summary>

- [x] Phase 1: Behaviour Pins (2/2 plans) — completed 2026-10-02
- [x] Phase 2: Mechanical Split (4/4 plans) — completed 2026-10-03
- [x] Phase 3: Dedupe (3/3 plans) — completed 2026-10-03
- [x] Phase 4: Fixes and Release Type Safety (4/4 plans) — completed 2026-10-03
- [x] Phase 5: Path-Policy Test and Docs (4/4 plans) — completed 2026-10-03

Full phase details: [milestones/v0.12.9-ROADMAP.md](milestones/v0.12.9-ROADMAP.md) ·
Requirements: [milestones/v0.12.9-REQUIREMENTS.md](milestones/v0.12.9-REQUIREMENTS.md) ·
Audit: [milestones/v0.12.9-MILESTONE-AUDIT.md](milestones/v0.12.9-MILESTONE-AUDIT.md) ·
Phase artifacts: `milestones/v0.12.9-phases/`

</details>

### 🚧 lua-2 Abstract Expressions and Quiver File Layout (In Progress)

**Milestone Goal:** A `BinaryFile` *is* an expression. Every expression operation takes one abstract expression type,
which sol2 type-checks in Lua, and `src/lua_runner/` follows the quiver file pattern used by `src/` and `src/c/`.

**Phase Numbering:**

- Integer phases (6, 7, 8): Planned milestone work
- Decimal phases (6.1, 6.2): Urgent insertions (marked with INSERTED)

- [ ] **Phase 6: Quiver File Layout** - `src/lua_runner/` binders mirror the core's `database_*.cpp` files plus `csv`, `binary` and `expression`, with zero behaviour change
- [ ] **Phase 7: AbstractExpression in C++** - `AbstractExpression` is the one parameter type of every C++ expression operation; `Expression` and `BinaryFile` derive from it
- [ ] **Phase 8: Typed Expression Parameters in Lua** - sol2 type-checks every expression operand as `AbstractExpression`, the Pattern 1 texts hold, and files take every expression method
- [ ] **Phase 9: Julia AbstractExpression and Docs** - Julia's `Binary.File` and `Expression` share one abstract type without the 97 forwarders; AGENTS.md and the CHANGELOG are finished

## Phase Details

### Phase 6: Quiver File Layout

**Goal**: Each Lua name is registered in the `src/lua_runner/` file named after the core file that implements its C++ method, so a change to a core file has an obvious Lua counterpart, and no script can tell that anything moved.
**Depends on**: Nothing in this milestone (builds on v0.12.9 Phase 5)
**Requirements**: LAYOUT-01, LAYOUT-02, LAYOUT-03, LAYOUT-04, LAYOUT-05
**Success Criteria** (what must be TRUE):

  1. `src/lua_runner/` holds exactly 19 files: `lua_runner.cpp`, `internal.h`, `return_json.cpp`, `path_policy.h`, `path_policy.cpp`, `database.cpp`, the ten `database_{create,read,update,delete,describe,metadata,query,time_series,csv_export,csv_import}.cpp`, `csv.cpp`, `binary.cpp` and `expression.cpp`. No `db_*.cpp` remains, each file is listed explicitly in `QUIVER_SOURCES`, and `wc -l` shows none over about 450 lines. Placement follows the core: `number_of_elements` is registered in `database_read.cpp`, `describe`/`describe_collection`/`summarize_collection` in `database_describe.cpp`, `get_time_series_metadata`/`list_time_series_groups` in `database_time_series.cpp`, and the lifecycle, transaction, dry-run and `validate_migrations` names in `database.cpp`. `return_json.cpp` and `path_policy.{h,cpp}` have an empty diff; `lua_runner.cpp` and `internal.h` change only in the binder declarations and calls and in the shared-helper declarations the moves need.
  2. `lua_runner.cpp` calls the 14 binders in the order LAYOUT-02 lists them (`bind_database` through `bind_expression`), and `bind_binary` returns the `sol::usertype<BinaryFile>` that is passed to `bind_expression`. The constructor still runs `open_libraries` → nil `dofile`/`loadfile` → text-only `load` wrapper → `quiver` table → the one `new_usertype<Database>` → binders → `lua["db"] = &db`. Across the folder, `new_usertype<Database>` and `open_libraries(` each appear exactly once.
  3. Nothing a script can observe changes. The folder still has 86 `set_function(` registrations (71 on `bind`, 15 on `ns`), and the sorted list of registered names is identical to `da6f67b`. No test expectation changes: `Lua*` 477 in 12 suites, `SandboxedPathTest` 11, `LuaRunnerCApiTest` 27, full `quiver_tests` 1454 and `quiver_c_tests` 543 pass in Debug and Release. The Julia, Dart, Python and JS suites and the lua-api sync test pass. The golden harness (Lua probe scripts through `quiver_cli`) is byte-identical to `da6f67b` in Debug and Release.
  4. Every new file whose functions take sol2 arguments by value has its own `NOLINTBEGIN/END(performance-unnecessary-value-param)` pair. clang-format 22.1.8 is clean on `src/lua_runner/`, and `scripts/tidy.bat` reports nothing beyond the 14-warning baseline. The files that keep most of their content (`db_read` → `database_read`, `db_metadata` → `database_metadata`, `db_time_series` → `database_time_series`) are moved with `git mv`, so `git log --follow` reaches their history.
  5. `git grep -nE 'db_core|db_read|db_write|db_metadata|db_time_series|bind_core|bind_write' -- ':!.planning'` goes from 24 lines at `da6f67b` to 0. The root and `src/` AGENTS.md list the new files, and the `lua-api.ts` maintainer header names `bind_database` through `bind_expression`.

**Plans**: 1/3 plans executed (sequential, waves 1-3)

Plans:
**Wave 1**

- [x] 06-01-PLAN.md — Tracer: `build/layout-check` harness + base baselines, then `database_describe.cpp` end to end; rename-only `git mv` commit (read, metadata, time series); `number_of_elements` and the time-series metadata pair moved

**Wave 2** *(blocked on Wave 1 completion)*

- [ ] 06-02-PLAN.md — Carve the core binder into `database.cpp`, `database_query.cpp`, `database_csv_export.cpp`, `database_csv_import.cpp`, and the write binder into `database_create.cpp`, `database_update.cpp` (git mv), `database_delete.cpp`

**Wave 3** *(blocked on Wave 2 completion)*

- [ ] 06-03-PLAN.md — Split `binary.cpp` into `binary.cpp` + `expression.cpp` (`bind_binary` returns the BinaryFile usertype), update every citation (24 → 0), phase gate and six suites

### Phase 7: AbstractExpression in C++

**Goal**: In C++ a `BinaryFile` is an expression: `AbstractExpression` is the one parameter type of every expression operator, free function and method, `Expression` and `BinaryFile` derive from it, and an expression still never touches the caller's open file handle.
**Depends on**: Phase 6 (the Lua binder that must keep compiling now lives in `src/lua_runner/expression.cpp`)
**Requirements**: EXPR-01, EXPR-02, EXPR-03, EXPR-04, EXPR-05, EXPR-06, EXPR-07
**Success Criteria** (what must be TRUE):

  1. `include/quiver/expression/abstract_expression.h` declares `class QUIVER_API AbstractExpression` with exactly one pure virtual `node()` and non-virtual `save`, `aggregate`, `aggregate_agents`, `select_agents` and `rename_agents`. A new C++ test static-asserts the shape: `std::is_abstract_v<AbstractExpression>`, `std::has_virtual_destructor_v<AbstractExpression>`, no copy or move assignment through the base, `std::is_base_of_v` for both `BinaryFile` and `Expression`, `std::is_final_v<Expression>`, `std::is_constructible_v<Expression, const BinaryFile&>`, and `!std::is_convertible_v<const BinaryFile&, Expression>` (so `Expression e = file;` no longer compiles, while the direct-initialization call sites in the C API and tests compile unchanged).
  2. New tests pin ownership: a `BinaryFile` open for reading is still `is_open()` and readable after `file.save(out)` and after `(file * 2.0).save(out)`, and an `Expression` built from a file still saves the right values after that file is closed and destroyed. `AbstractExpression::save` holds the root node in a local while it runs.
  3. `grep -n friend include/quiver/expression/expression.h` returns nothing. Every operator (`+ - * /`, `> < >= <= == !=`, `&& || !`, unary `-`) and free function (`abs`, `sqrt`, `log`, `exp`, `ifelse`) takes `const AbstractExpression&` for each expression operand, with the `double` overloads kept, and calls `node()` once per operand. A test builds `file_a + file_b`, `2.0 + file` and `ifelse(file > 1.0, file, 0.0)` with no `Expression(...)` wrapper.
  4. `git grep -n 'Expression::metadata' -- include src` returns nothing. Tests show `get_metadata()` on a `BinaryFile` returns the handle's in-memory metadata (the same value through a `const AbstractExpression&`), and on an `Expression` returns its node's metadata. `quiver::AggregateOperation` is at namespace scope, and `ExpressionAggregate` keeps `using Operation = AggregateOperation;`. No C API header or signature changes and `bindings/julia/src/c_api.jl` is untouched; the only `src/c/` edit is the renamed accessor call at `src/c/expression/expression.cpp:248`. CHANGELOG `[0.13.0] — unreleased` gains BREAKING lines for copy-initialization from a file (write `Expression e(file);`) and the rename (call `get_metadata()`).
  5. `ExpressionFixture` (116 plus the new tests), `ExpressionCApiFixture` 73 and full `quiver_c_tests` 543 pass, and the new `quiver_tests` count is recorded. `Lua*` stays 477 with no Lua expectation changed (Lua keeps `e:metadata()` until Phase 8). Windows Debug and Release pass, and the Linux GCC 13 and Clang 18/libc++ Docker builds compile and pass the C++, C API and Lua suites (`Lua*` 475 = 474 + 1 skip), including the C++20 rewritten `==`/`!=` candidates with base-class parameters and the exported polymorphic base (no missing vtable or typeinfo at link time).

**Plans**: TBD
**Settled (no decision needed)**: Lua `==`/`<` between two files is already `true` today, because the implicit `Expression(const BinaryFile&)` lets sol2's automatic `__eq`/`__lt` apply (checked with quiver_cli at `da6f67b`, Debug and Release: `f == g` true, `e == e2` true, `e == f` false). The abstract base keeps that unchanged; fixing it for files and expressions together is deferred as EQ-01.

### Phase 8: Typed Expression Parameters in Lua

**Goal**: In Lua a binary file is an expression: sol2 type-checks every expression operand as `AbstractExpression`, a wrong operand still gets the pinned Pattern 1 text, and a file accepts every expression method without `quiver.expression`.
**Depends on**: Phase 6 (`src/lua_runner/expression.cpp`), Phase 7 (the C++ types)
**Requirements**: LUA-01, LUA-02, LUA-03, LUA-04, LUA-05, LUA-06, LUA-07, LUA-08
**Success Criteria** (what must be TRUE):

  1. The `BinaryFile` and `Expression` usertypes register `AbstractExpression` as their base, and `git grep -n 'new_usertype<AbstractExpression>' -- src` returns nothing. All 12 `quiver.*` expression functions, every operator metamethod and every expression method take `const AbstractExpression&`. `git grep -nwE 'to_expression|is_number' -- src/lua_runner` returns nothing, and no `sol::object` operand parameter is left in `expression.cpp`.
  2. `LuaExpressionTest.OperandErrorsNameTheOperation` and `OperandErrorsReportTheLeftmostBadOperand` (the 13 pinned expectations) pass with their test bodies unmodified; `git diff` against the phase base shows no hunk inside them. New tests pin `quiver.abs(e, 99)` → `Cannot abs: too many arguments (expected 1, got 2)` and `quiver.gt(e, f, 3)` → `Cannot gt: too many arguments (expected 2, got 3)`, and CHANGELOG gains a BREAKING line saying extra arguments now throw instead of being ignored.
  3. On a raw file from `db:open_file`, with no `quiver.expression`, `f:aggregate`, `f:aggregate_agents`, `f:select_agents`, `f:rename_agents`, `f:save` and `f:get_metadata` each have a test. `f:save` keeps its three guards (a path outside the database directory, saving onto the file's own path, and saving a file that is open for writing are still rejected), and after a successful `f:save` the file is still open (`f:is_open()` is true). `e:get_metadata()` works, `e:metadata()` raises (the method no longer exists), and `quiver.expression(f)` returns an `Expression`. CHANGELOG gains a BREAKING line for `e:metadata()` → `e:get_metadata()` and an entry for the expression methods files gained.
  4. A Release benchmark of 1M `f:read` and 1M `f:write` calls (median of interleaved runs) is taken at the end of Phase 7 and again after the change. The numbers and the form that landed (the runtime `sol::base_classes` tag, or the compile-time `SOL_BASE_CLASSES`/`SOL_DERIVED_CLASSES` traits in one shared `src/lua_runner/` header included by every TU that binds these types, used if the tag costs measurably) are recorded in the phase summary and the STATE.md PR notes. The Release build log for `src/lua_runner/` shows no `C4702` and no warning that the Phase 7 Release log did not have.
  5. `LUA_DB_API_REFERENCE` documents files as expressions, the expression methods on files, `get_metadata` and the arity rule, and `grep -n ':metadata()' bindings/js/src/lua-api.ts` returns nothing. The lua-api sync test passes. The new `Lua*` count (477 plus this phase's tests, equal in Debug and Release) and `LuaRunnerCApiTest` 27 are recorded, and all six suites are green.

**Plans**: TBD

### Phase 9: Julia AbstractExpression and Docs

**Goal**: Julia mirrors the finished surface: `Binary.File` and `Expression` are subtypes of one `AbstractExpression` and a file accepts every expression operation, without the 97 forwarders. The AGENTS.md files and the CHANGELOG describe the finished milestone.
**Depends on**: Phase 7 (the Julia half needs only the C++ types), Phase 8 (the docs and the CHANGELOG check describe the final Lua surface)
**Requirements**: JUL-01, JUL-02, JUL-03, DOC-01, DOC-02
**Success Criteria** (what must be TRUE):

  1. `abstract type AbstractExpression end` is declared in `bindings/julia/src/Quiver.jl` before `include("binary/Binary.jl")`, and a Julia test asserts `Binary.File <: AbstractExpression` and `Expression <: AbstractExpression`.
  2. Expression operations are defined once on `AbstractExpression`, and a `Binary.File` operand is converted through the existing `quiver_expression_from_file`. The 97 forwarding methods are gone: `grep -n 'Binary.File' bindings/julia/src/expression.jl` drops from 54 lines to the conversion alone. `git diff` shows no change to `bindings/julia/src/c_api.jl` or to any C API header.
  3. Julia tests run every expression operation on a raw `Binary.File` (arithmetic with files and numbers on either side, comparisons, logical operators, the unary functions, `ifelse`, `aggregate`, `aggregate_agents`, `select_agents`, `rename_agents`, `save` and `get_metadata`), matching what Lua allows, and the Julia suite passes.
  4. The root AGENTS.md has an `AbstractExpression` Design Decision and updated binary cross-layer rows (`get_metadata` on files and expressions, the expression methods on files in Lua and Julia). `src/AGENTS.md`, `src/c/AGENTS.md` (`quiver_expression_from_file` stays the bridge) and `bindings/julia/AGENTS.md` describe the type and the new `src/lua_runner/` layout, and `grep -n 'caches an open BinaryFile' src/AGENTS.md` returns nothing.
  5. CHANGELOG `[0.13.0] — unreleased` holds a line for every BREAKING change of the milestone, each saying what a caller must change: copy-initialization from a file and `Expression::metadata()` → `get_metadata()` (Phase 7), Lua `e:metadata()` → `e:get_metadata()` and the strict arity of `quiver.*` expression functions (Phase 8). It also has entries for the expression methods files gained in Lua and Julia, and no entry carries a planning ID. `uv run python scripts/assert_version.py` reports 0.13.0 in all five manifests, and all six suites (`scripts/test-all.bat`) and the lua-api sync test are green.

**Plans**: TBD
**Planning flag**: settle where the Julia generic lives, `Binary.get_metadata(::File)` or a shared `Quiver.get_metadata` (SUMMARY.md, Gaps to Address).

## Progress

**Execution Order:**
Phases execute in numeric order: 6 → 7 → 8 → 9

| Phase | Milestone | Plans Complete | Status | Completed |
|-------|-----------|----------------|--------|-----------|
| 1. Behaviour Pins | v0.12.9 | 2/2 | Complete | 2026-10-02 |
| 2. Mechanical Split | v0.12.9 | 4/4 | Complete | 2026-10-03 |
| 3. Dedupe | v0.12.9 | 3/3 | Complete | 2026-10-03 |
| 4. Fixes and Release Type Safety | v0.12.9 | 4/4 | Complete | 2026-10-03 |
| 5. Path-Policy Test and Docs | v0.12.9 | 4/4 | Complete | 2026-10-03 |
| 6. Quiver File Layout | lua-2 | 1/3 | In Progress|  |
| 7. AbstractExpression in C++ | lua-2 | 0/? | Not started | - |
| 8. Typed Expression Parameters in Lua | lua-2 | 0/? | Not started | - |
| 9. Julia AbstractExpression and Docs | lua-2 | 0/? | Not started | - |
