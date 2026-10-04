# Project Research Summary

**Project:** Quiver Lua Runner Refactor, milestone lua-2 (Abstract Expressions and Quiver File Layout)
**Domain:** A C++20 library's expression type hierarchy (lazy DAG over `.qvr` binary files), its sol2 v3.5.0 Lua
binding, and its Julia binding; plus renaming and re-splitting the Lua binding files
**Researched:** 2026-10-04 (workflow `wf_21c91395-8d2`: 4 readers, 3 designs, 2 judges, 1 compiled MSVC spike)
**Confidence:** HIGH for C++ and Lua (verified by compiling against the build's own sol2/Lua with the project's
flags); MEDIUM for GCC/Clang/MinGW and the exported polymorphic base (not compiled off Windows yet)

Evidence for every claim below, with file:line citations, is in `DESIGN-STUDY.md`.

## Executive Summary

In C++ a `BinaryFile` already behaves as an `Expression` for operators and free functions, through the implicit
`Expression(const BinaryFile&)` (`include/quiver/expression/expression.h:19`). That conversion copies only the
file's **path**: `ExpressionFile` re-reads the TOML and owns a private, unopened `BinaryFile` that `save()` opens
and closes (`src/expression/expression_file.cpp:10`, `src/expression/expression.cpp:49-97`). Lua cannot see that
conversion, because sol2 never applies C++ implicit conversions between usertypes, and `sol::bases` works only
through real inheritance (`static_cast<Base*>`). So Lua uses `sol::object` + `to_expression`
(`src/lua_runner/binary.cpp:99-139`) and Julia uses 97 forwarding methods (`bindings/julia/src/expression.jl`).

The user directed that the only parameter of every expression operation be an abstract expression type. The
recommended design, which both judges picked independently (32/40 each) and the spike verified, is:
- a new `quiver::AbstractExpression` with one pure virtual `node()`;
- `Expression` (the concrete lazy-DAG value that operations return) and `BinaryFile` both derive from it;
- a file's `node()` is exactly today's path-based leaf, so ownership and lifetime do not change;
- every operator and free function takes `const AbstractExpression&`;
- Lua declares `bases<AbstractExpression>` and uses typed parameters, so sol2 does the type check;
- sol2's documented fallback overload keeps the 13 pinned Pattern 1 operand messages byte-identical.

The C API needs no source change. Julia mirrors the abstract type locally.

The second direction, mirroring the core's `database_*.cpp` split in `src/lua_runner/`, is a pure move. No binder
overrides a name another sets (86 names, each registered once). Splitting `binary.cpp` into binary + expression
removes the build's long pole (42 s), so wall-clock time falls even though 6 sol2 TUs are added.

## Key Findings

### Recommended Stack

No new dependency. Everything uses what is already fetched: sol2 v3.5.0 (`build/_deps/sol2-src`), Lua 5.4.8, and
the project's sol2 defines (`SOL_ALL_SAFETIES_ON=1`, `SOL_PRINT_ERRORS=0`, `SOL_SAFE_NUMERICS=1`, `SOL_NO_NIL=1`,
`SOL_SAFE_GETTER=0`, `SOL_SAFE_STACK_CHECK=0`).

**sol2 mechanisms used:**
- `bases<AbstractExpression>` registration: sets `weak_derive<AbstractExpression>` and stores class_check/class_cast,
  so a typed `const AbstractExpression&` parameter accepts either userdata, including a `shared_ptr<BinaryFile>`
  (what `db:open_file` returns) and a by-value file. Two forms:
  - the runtime tag `sol::base_classes, sol::bases<...>()` swaps each derived metatable's `__index` table for a C
    closure, measured at +80-110 ns per method call (about 2% on a `f:read` loop);
  - the compile-time traits `SOL_BASE_CLASSES` / `SOL_DERIVED_CLASSES` keep `__index` a plain table at no extra
    cost, but must be visible in every TU that instantiates sol2 for these types (put them in one shared header).
- `sol::overload` of typed candidates `(A,A)`, `(A,double)`, `(double,A)` for operators and comparisons, with a final
  `sol::variadic_args` "overloading with fallback" candidate that runs only after every typed candidate failed and
  throws the Pattern 1 text through `lua_type_error`.
- Not used: `sol_lua_check`/`sol_lua_get` customization (works, but loses the operation name and adds an ODR
  hazard), `argument_handler` specialization (keyed by signature, cannot name the operation), and base-member
  lookup (documented by sol2 as unsupported, `usertype.rst:288`).

### Expected Features

**Must have (the milestone's scope, confirmed by the user):**
- `AbstractExpression` as the only expression parameter type of every C++ operator, unary function, comparison,
  logical op, `ifelse` and expression method.
- Lua typed parameters checked by sol2, with the Pattern 1 operand messages unchanged and a clear
  `Cannot <op>: too many arguments (expected N, got M)` for extra arguments.
- Files accept every expression method in Lua (`f:aggregate`, `aggregate_agents`, `select_agents`,
  `rename_agents`, `save`), matching what Julia already allows.
- One metadata accessor, `get_metadata`, on files and expressions (BREAKING rename of `Expression::metadata()` and
  Lua `e:metadata()`).
- Julia `abstract type AbstractExpression`, with `Binary.File` and `Expression` as subtypes, replacing the 97
  forwarders.
- `src/lua_runner/` mirroring the core file names.

**Deferred / out of scope:**
- A C API "file as expression" borrowed handle (would make `quiver_expression_t*` owned-or-borrowed by type).
- Fixing Lua `==`/`<` between expressions always being true (pre-existing sol2 automagic; separate decision).
- Reusing an open file's in-memory metadata, or detecting stale metadata at save (pre-existing).

### Architecture Approach

`AbstractExpression` is a stateless polymorphic base: public virtual destructor, protected copy/move (no slicing or
assignment through a base reference), one pure virtual `std::shared_ptr<ExpressionNode> node() const`, and
non-virtual `get_metadata`/`save`/`aggregate`/`aggregate_agents`/`select_agents`/`rename_agents` built on
`node()`. `Expression final` keeps its `node_`. `BinaryFile` returns a fresh `ExpressionFile` leaf from its path
on each `node()` call (the leaf never references the caller's handle). Every operator calls `node()` once per
operand into locals (one TOML parse per operand, leftmost error first). `ExpressionAggregate::Operation` moves
to namespace scope as `AggregateOperation`, with an alias kept, which breaks the would-be header cycle
(`expression_node.h` includes `binary_file.h`).

**Major components:**
1. `include/quiver/expression/abstract_expression.h` (new): the base.
2. `expression.h` / `expression.cpp`: `Expression final`; operators on the base; friend block (expression.h:43-89)
   deleted; `explicit Expression(const AbstractExpression&)`.
3. `binary_file.h` / `expression_file.cpp`: `BinaryFile : AbstractExpression`, `node()`.
4. `src/lua_runner/expression.cpp` (after the split): typed bindings, fallback overloads, shared expression
   methods listed on both usertypes, and the shared sol2 traits header.
5. `bindings/julia/src/expression.jl`: abstract type, one conversion, forwarders deleted.

### Critical Pitfalls

1. **Never reference the caller's handle from a node.** `save()` opens every collected input with 'r' and closes
   it on exit, so a node holding the user's `BinaryFile` would close the user's reader or writer, dangle after the
   C API's `quiver_binary_file_close` (which deletes), or after Lua's run-handle cleanup. Keep the path-based leaf.
2. **`AbstractExpression::save` must hold `const auto root = node();`** A file's leaf is a temporary; without the
   local, the `BinaryFile*` collected by `collect_input_files` dangles (UB).
3. **Release C4702 noise.** The always-throwing fallback adds about 12 "unreachable code" warnings inside sol2
   headers at /O2 (0 today). There is no `/WX`, so it is noise; suppress locally or accept.
4. **Method-call cost under `bases`.** The runtime tag adds about 80-110 ns per method call on files and
   expressions, which touches the Do-Not-Fix binary hot path. Benchmark `f:read` before and after; prefer the
   traits form in a shared header (which also prevents an ODR violation once BinaryFile and Expression bind in
   different TUs).
5. **lua-api-sync parse traps.** Bind only through receivers named `bind`/`ns`; list the shared expression methods
   by quoted name inside both `new_usertype` calls; define helper lambdas where a clang-format-wrapped `"name",`
   line cannot be attributed to a usertype (`binary.cpp:53-55`).
6. **Arity becomes strict under `sol::overload`.** `quiver.abs(e, 99)` used to ignore the 99; it now raises the
   too-many-arguments Pattern 1 message. BREAKING, CHANGELOG it.
7. **Off-Windows unknowns.** C++20 reversed `==`/`!=` candidates with base-class parameters, `QUIVER_API`
   export of a polymorphic base (vtable/typeinfo), and GCC/Clang demangled names are unverified; prove them in CI.

## Implications for Roadmap

Phase numbering continues from v0.12.9 (which ended at Phase 5).

### Phase 6: Quiver file layout for src/lua_runner
**Rationale:** a pure move first, so the expression work lands in its final file (`expression.cpp`) and the shared
sol2 traits header has a home before any typing change.
**Delivers:** `database.cpp` + `database_{create,read,update,delete,describe,metadata,query,time_series,csv_export,csv_import}.cpp`,
`csv.cpp`, `binary.cpp`, `expression.cpp`; new binder names and call order (bind_binary hands the BinaryFile
usertype to bind_expression); every citation updated. Same Lua names, same error text, `Lua*` count unchanged.
**Avoids:** pitfall 5 (sync test), the basename-ambiguity doc drift.

### Phase 7: AbstractExpression in C++
**Rationale:** the core type change everything else depends on.
**Delivers:** the base, the two derived kinds, operators on the base, `explicit` ctor, `get_metadata` rename,
`AggregateOperation` hoist; C++ and C API tests green with no C API source change.
**Avoids:** pitfalls 1, 2, 7.

### Phase 8: Typed AbstractExpression parameters in Lua
**Rationale:** needs Phase 7's types and Phase 6's expression.cpp.
**Delivers:** bases registration (traits form after the benchmark), typed operators/functions/methods with the
fallback, file expression methods, `e:get_metadata()`, the deleted `sol::object` plumbing; Lua tests; lua-api.ts
reference updated.
**Avoids:** pitfalls 3, 4, 5, 6.

### Phase 9: Julia AbstractExpression and docs
**Rationale:** Julia mirrors the finished C++/Lua surface; docs and CHANGELOG are written once against final code.
**Delivers:** Julia abstract type, forwarders collapsed, `save`/`get_metadata` on files in Julia; AGENTS.md (root,
src, src/c, julia), CHANGELOG `[0.13.0]` BREAKING entries; all six suites green.

### Phase Ordering Rationale
- Layout before typing: the move is behaviour-neutral and proven by the existing suites, and it avoids moving
  freshly rewritten code a second time.
- C++ before bindings: Lua and Julia both consume the new types.
- Docs last, as in v0.12.9, so every AGENTS.md and the CHANGELOG are finished once.

### Research Flags
- **Phase 8:** benchmark protocol for `f:read` (Release, median of interleaved runs, as in v0.12.9 SAFE-06), and
  whether the traits form compiles cleanly with `QUIVER_API` classes in the real tree.
- **Phase 7:** off-Windows compilation of the polymorphic exported base (Linux GCC 13/Clang 18 Docker, as in
  v0.12.9).
- **Phase 6:** standard (pure move; the mapping table in `DESIGN-STUDY.md` is ready).

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | sol2 v3.5.0 mechanisms read in source and exercised in a compiled spike with the project's flags |
| Features | HIGH | scope confirmed by the user (abstract type, Pattern 1 kept, get_metadata, Julia in scope, layout) |
| Architecture | HIGH (Windows) / MEDIUM (others) | spike + a real-tree scratch build passed quiver_tests and lua-api-sync; GCC/Clang unverified |
| Pitfalls | HIGH | each pitfall reproduced or cited (ownership, dangling leaf, C4702, `__index` cost, arity) |

**Overall confidence:** HIGH

### Gaps to Address
- Off-Windows build of the polymorphic exported base and C++20 reversed comparison candidates: verify in Phase 7
  with the Linux Docker toolchains.
- Traits form in the real tree with dllexport classes: verify in Phase 8; fall back to the runtime tag if it fails,
  with the measured cost recorded.
- Julia's `Binary.get_metadata(::File)` vs a shared `Quiver.get_metadata`: settle the generic-function placement in
  Phase 9 planning.

## Sources

- `DESIGN-STUDY.md` (this folder): reader facts with file:line evidence, the layout mapping table, the three
  designs, the two judgments and the spike's verified claims and observed error texts.
- sol2 v3.5.0 source: `build/_deps/sol2-src/include/sol/` (usertype_storage.hpp, stack_check_unqualified.hpp,
  stack_get_unqualified.hpp, call.hpp, trampoline.hpp, forward.hpp) and `documentation/source/api/usertype.rst`,
  `examples/source/overloading_with_fallback.cpp`.
- Lua 5.4.8 source: `lvm.c:1555/1566` (unary metamethods get the operand twice), `lstrlib.c:277` (string `__add`
  hands off to the other operand's metamethod).
- Quiver source and tests cited throughout `DESIGN-STUDY.md`.
