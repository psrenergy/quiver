# Requirements: Quiver Lua Runner Refactor — milestone lua-2

**Defined:** 2026-10-04
**Milestone:** lua-2 Abstract Expressions and Quiver File Layout
**Core Value:** Every file in the Lua scripting layer is small and single-purpose enough for an agent to change safely, and every existing script behaves exactly as before, apart from the deliberate, test-pinned fixes listed below.

Research: `.planning/research/SUMMARY.md` (recommendation) and `.planning/research/DESIGN-STUDY.md` (file:line
evidence, the layout mapping table, the judged designs and the compiled spike). Baseline at `da6f67b`:
`quiver_tests` 1454 (`--gtest_filter=Lua*` 477 in 12 suites, `SandboxedPathTest*` 11), `quiver_c_tests` 543
(`LuaRunnerCApiTest*` 27); Linux GCC 13 / Clang 18: `Lua*` 475 (474 + 1 skip), sandbox 10.

## v1 Requirements

### Quiver file layout (pure move)

- [ ] **LAYOUT-01**: The binder files in `src/lua_runner/` are `database.cpp` + `database_{create,read,update,delete,describe,metadata,query,time_series,csv_export,csv_import}.cpp`, `csv.cpp`, `binary.cpp` and `expression.cpp`. Each Lua name lives in the file named after the core file that implements its C++ method (e.g. `number_of_elements` in `database_read.cpp`, `describe`/`describe_collection`/`summarize_collection` in `database_describe.cpp`, `get_time_series_metadata`/`list_time_series_groups` in `database_time_series.cpp`; lifecycle, transactions, dry runs and `validate_migrations` in `database.cpp`). `lua_runner.cpp`, `internal.h`, `return_json.cpp` and `path_policy.{h,cpp}` are unchanged. No file exceeds ~450 lines.
- [ ] **LAYOUT-02**: Binder entry points are named after their files (`bind_database`, `bind_create`, `bind_read`, `bind_update`, `bind_delete`, `bind_describe`, `bind_metadata`, `bind_query`, `bind_time_series`, `bind_csv_export`, `bind_csv_import`, `bind_csv`, `bind_binary`, `bind_expression`) and are called in core order. `bind_binary` returns the `BinaryFile` usertype, which is passed to `bind_expression`. The constructor-order invariants hold (`open_libraries` → nil `dofile`/`loadfile` → text-only `load` wrapper → `quiver` table → one `new_usertype<Database>` → binders → `lua["db"] = &db`), and there is exactly one `new_usertype<Database>` in the folder.
- [ ] **LAYOUT-03**: The move is behaviour-neutral: no Lua name, usertype name or error text changes. `Lua*` 477, `SandboxedPathTest` 11, `LuaRunnerCApiTest` 27, full `quiver_tests` 1454 and `quiver_c_tests` 543 pass, as do the Julia, Dart, Python and JS suites and the lua-api sync test. Debug and Release golden output is byte-identical to the base.
- [ ] **LAYOUT-04**: New files are listed explicitly in `QUIVER_SOURCES` (same PRIVATE sol2 defines and target-wide `/bigobj`). Every file whose functions take sol2 arguments by value has its own `NOLINTBEGIN/END(performance-unnecessary-value-param)` pair. clang-format 22.1.8 is clean, and `scripts/tidy.bat` reports nothing beyond the 14-warning baseline. Rename-only files use `git mv`.
- [ ] **LAYOUT-05**: Every citation of the old file and binder names is updated (root and `src/` AGENTS.md, the `lua-api.ts` maintainer header, test comments, cmake, the Dart hook comment if any), re-derived with `git grep`. `git grep -nE 'db_core|db_read|db_write|db_metadata|db_time_series|bind_core|bind_write'` outside `.planning/` returns nothing.

### AbstractExpression in C++

- [ ] **EXPR-01**: `include/quiver/expression/abstract_expression.h` declares `class QUIVER_API AbstractExpression` with a public virtual destructor, protected copy and move operations (no slicing or assignment through a base reference), exactly one pure virtual `std::shared_ptr<ExpressionNode> node() const`, and non-virtual `save`, `aggregate`, `aggregate_agents`, `select_agents` and `rename_agents` built on `node()`. It can never be constructed on its own.
- [ ] **EXPR-02**: `class Expression final : public AbstractExpression` keeps its `node_`; `class BinaryFile : public AbstractExpression` returns a fresh path-based `ExpressionFile` leaf from `node()` (today's `Expression(const BinaryFile&)` semantics), so an expression never opens, closes or references the caller's handle. `AbstractExpression::save` holds the root node in a local while it runs. Tests pin that a file stays open and readable after `save` from it, and that an expression survives closing and destroying its source file.
- [ ] **EXPR-03**: Every operator (`+ - * /`, `> < >= <= == !=`, `&& || !`, unary `-`) and free function (`abs`, `sqrt`, `log`, `exp`, `ifelse`) takes `const AbstractExpression&` for every expression operand; the `double` overloads stay. The friend block in `expression.h` is deleted, and each operator calls `node()` once per operand.
- [ ] **EXPR-04**: `explicit Expression(const AbstractExpression&)` replaces the implicit `Expression(const BinaryFile&)`. Copy-initialization `Expression e = file;` no longer compiles (BREAKING, CHANGELOG); direct-initialization call sites (C API, tests) compile unchanged.
- [ ] **EXPR-05**: `get_metadata()` is the one metadata accessor on `AbstractExpression`. `Expression::metadata()` is renamed (BREAKING, CHANGELOG). A `BinaryFile`'s `get_metadata()` keeps returning the handle's in-memory metadata; an `Expression`'s returns its node's metadata.
- [ ] **EXPR-06**: `ExpressionAggregate::Operation` moves to namespace scope as `quiver::AggregateOperation`, with `using Operation = AggregateOperation;` kept, so there is no header cycle. The C API compiles with no source change. The C++ and C API suites pass, and their new counts are recorded.
- [ ] **EXPR-07**: Linux GCC 13 and Clang 18/libc++ (Docker) build and pass the C++, C API and Lua suites, including C++20 rewritten `==`/`!=` candidates with base-class parameters and the exported polymorphic base.

### Typed AbstractExpression parameters in Lua

- [ ] **LUA-01**: The `BinaryFile` and `Expression` usertypes register `AbstractExpression` as their base; `AbstractExpression` is not itself a registered usertype. Every `quiver.*` expression function (`expression`, `abs`, `sqrt`, `log`, `exp`, `ifelse`, `gt`, `lt`, `gte`, `lte`, `eq`, `neq`), every operator metamethod and every expression method takes `const AbstractExpression&`, so sol2 does the type check. `to_expression`, `is_number` and the `sol::object` `binop` are deleted.
- [ ] **LUA-02**: A wrong-type operand raises byte-identical Pattern 1 text (`Cannot <op>: operand must be an expression or a binary file, got <lua type>`), produced by sol2's documented fallback overload, which runs only after every typed candidate fails. The 13 pinned expectations (`LuaExpressionTest.OperandErrorsNameTheOperation`, `OperandErrorsReportTheLeftmostBadOperand`) pass unmodified.
- [ ] **LUA-03**: Extra arguments to a `quiver.*` expression function or binary operator raise `Cannot <op>: too many arguments (expected N, got M)`. BREAKING (they were silently ignored), tested, and in the CHANGELOG.
- [ ] **LUA-04**: A `BinaryFile` accepts `f:aggregate`, `f:aggregate_agents`, `f:select_agents`, `f:rename_agents`, `f:save` and `f:get_metadata` without `quiver.expression`. `f:save` keeps the sandbox, output-collision and write-registry guards, and the file stays open afterwards. Each method is tested on a raw file.
- [ ] **LUA-05**: `e:metadata()` becomes `e:get_metadata()` (BREAKING, CHANGELOG). `quiver.expression(x)` stays and returns a concrete `Expression`.
- [ ] **LUA-06**: The base registration does not slow the binary hot path. A Release benchmark of 1M `f:read` and `f:write` calls (median of interleaved runs) is taken before and after; the compile-time traits form (`SOL_BASE_CLASSES`/`SOL_DERIVED_CLASSES`, in one shared `src/lua_runner/` header included by every TU that binds these types) is used if the runtime `sol::base_classes` tag costs measurably. The result is recorded.
- [ ] **LUA-07**: Release builds report no new compiler warnings from `src/lua_runner/` (the fallback's C4702 "unreachable code" is suppressed or avoided).
- [ ] **LUA-08**: `LUA_DB_API_REFERENCE` (`bindings/js/src/lua-api.ts`) documents files as expressions, the expression methods on files, `get_metadata`, and the arity rule. The lua-api sync test passes.

### Julia

- [ ] **JUL-01**: `abstract type AbstractExpression end` is declared in Quiver.jl before the Binary module is included; `Binary.File <: AbstractExpression` and `Expression <: AbstractExpression`.
- [ ] **JUL-02**: Expression operations are defined once on `AbstractExpression`, converting a `Binary.File` through the existing `quiver_expression_from_file`. The 97 `Binary.File` forwarding methods in `expression.jl` are deleted. No C API change, and the Julia suite passes.
- [ ] **JUL-03**: Every expression operation works on a `Binary.File` in Julia, including `save` and `get_metadata`, with tests (parity with Lua).

### Docs

- [ ] **DOC-01**: The root, `src/`, `src/c/` and `bindings/julia/` AGENTS.md files describe the abstract expression type and the new `src/lua_runner/` layout: a root Design Decision for `AbstractExpression`, updated cross-layer table rows, and a fix for the inaccurate "ExpressionFile caches an open BinaryFile" line in `src/AGENTS.md`.
- [ ] **DOC-02**: Each BREAKING change gets its CHANGELOG `[0.13.0] — unreleased` line in the phase that makes it, saying what a caller must change; the last phase checks the section is complete. All six suites are green, and the version stays 0.13.0.

## v2 Requirements

Deferred. Tracked, not in the current roadmap.

- **EQ-01**: Lua `==`/`<` between expressions stop silently returning true (sol2's automatic `__eq`/`__lt` on the Expression-returning C++ operators; `sol::is_automagical` = false).
- **META-01**: `save` detects metadata that went stale between building an expression and saving it.

## Out of Scope

| Feature | Reason |
|---------|--------|
| A C API "file as expression" handle, or file-taking variants of every `quiver_expression_*` | C has no inheritance; a borrowed view would make `quiver_expression_t*` owned-or-borrowed by type (against the ownership rule); Julia does not need it; `quiver_expression_from_file` stays the bridge |
| Making `Expression` the abstract type and renaming the concrete one | `AbstractExpression` + concrete `Expression` follows Julia's `AbstractArray`/`Array`; operations must return a concrete value; keeps every `Expression e = a + b;` call site |
| A node that reuses the caller's open file handle | `save()` opens and closes its inputs, so it would close the user's reader/writer and dangle after close; the path-based leaf keeps every guarantee |
| New `db:` methods | lua-2 adds none; its only additions are the expression methods a file gains by becoming an expression |
| Binary/expression in Dart, Python or JS | Documented design decision: Julia and Lua only |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|

**Coverage:**
- v1 requirements: 25 total
- Mapped to phases: 0
- Unmapped: 25 ⚠️

---
*Requirements defined: 2026-10-04*
*Last updated: 2026-10-04 after initial definition*
