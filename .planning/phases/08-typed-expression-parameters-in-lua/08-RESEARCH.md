# Phase 8: Typed Expression Parameters in Lua - Research

**Researched:** 2026-10-04
**Domain:** sol2 v3.5.0 usertype inheritance, overload resolution and error paths in the Quiver Lua binding (`src/lua_runner/`)
**Confidence:** HIGH (every mechanism read in the vendored sol2/Lua source; a compile-only spike of the new
`expression.cpp` built against the real tree with the exact Release flags, with 0 errors)

<user_constraints>
## User Constraints (no CONTEXT.md)

There is no CONTEXT.md: the user skipped discuss-phase. The orchestrator says to treat the ROADMAP success
criteria and REQUIREMENTS.md LUA-01..LUA-08 as the locked contract. They are copied here verbatim.

### Locked Decisions (ROADMAP Phase 8 success criteria, verbatim)

1. The `BinaryFile` and `Expression` usertypes register `AbstractExpression` as their base, and `git grep -n 'new_usertype<AbstractExpression>' -- src` returns nothing. All 12 `quiver.*` expression functions, every operator metamethod and every expression method take `const AbstractExpression&`. `git grep -nwE 'to_expression|is_number' -- src/lua_runner` returns nothing, and no `sol::object` operand parameter is left in `expression.cpp`.
2. `LuaExpressionTest.OperandErrorsNameTheOperation` and `OperandErrorsReportTheLeftmostBadOperand` (the 13 pinned expectations) pass with their test bodies unmodified; `git diff` against the phase base shows no hunk inside them. New tests pin `quiver.abs(e, 99)` → `Cannot abs: too many arguments (expected 1, got 2)` and `quiver.gt(e, f, 3)` → `Cannot gt: too many arguments (expected 2, got 3)`, and CHANGELOG gains a BREAKING line saying extra arguments now throw instead of being ignored.
3. On a raw file from `db:open_file`, with no `quiver.expression`, `f:aggregate`, `f:aggregate_agents`, `f:select_agents`, `f:rename_agents`, `f:save` and `f:get_metadata` each have a test. `f:save` keeps its three guards (a path outside the database directory, saving onto the file's own path, and saving a file that is open for writing are still rejected), and after a successful `f:save` the file is still open (`f:is_open()` is true). `e:get_metadata()` works, `e:metadata()` raises (the method no longer exists), and `quiver.expression(f)` returns an `Expression`. CHANGELOG gains a BREAKING line for `e:metadata()` → `e:get_metadata()` and an entry for the expression methods files gained.
4. A Release benchmark of 1M `f:read` and 1M `f:write` calls (median of interleaved runs) is taken at the end of Phase 7 and again after the change. The numbers and the form that landed (the runtime `sol::base_classes` tag, or the compile-time `SOL_BASE_CLASSES`/`SOL_DERIVED_CLASSES` traits in one shared `src/lua_runner/` header included by every TU that binds these types, used if the tag costs measurably) are recorded in the phase summary and the STATE.md PR notes. The Release build log for `src/lua_runner/` shows no `C4702` and no warning that the Phase 7 Release log did not have.
5. `LUA_DB_API_REFERENCE` documents files as expressions, the expression methods on files, `get_metadata` and the arity rule, and `grep -n ':metadata()' bindings/js/src/lua-api.ts` returns nothing. The lua-api sync test passes. The new `Lua*` count (477 plus this phase's tests, equal in Debug and Release) and `LuaRunnerCApiTest` 27 are recorded, and all six suites are green.

### Claude's Discretion

Everything the criteria leave open: the shape of the fallback helper, where the shared method lambdas live, how
the file gains its methods, the test names and how many there are, how `C4702` is avoided or suppressed, and
the benchmark decision threshold.

### Deferred Ideas (OUT OF SCOPE, from REQUIREMENTS.md v2 / Out of Scope)

- EQ-01: Lua `==`/`<` between expressions and files always being true. It stays unchanged. `build/abstract-check/eq.sh` must still print `[true,true,false]`.
- META-01: stale-metadata detection at `save`.
- New `db:` methods; binary/expression in Dart/Python/JS; a C API "file as expression" handle.
- Julia (JUL-01..03) and the AGENTS.md documentation pass (DOC-01) are Phase 9.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| LUA-01 | Base registration; typed `const AbstractExpression&` everywhere; delete `to_expression`, `is_number`, `sol::object` binop | §Architecture Pattern 1-2, the verified traits form, the code skeleton |
| LUA-02 | Byte-identical Pattern 1 operand text via sol2's documented fallback | §Pattern 3 (`operand_error` rule, reproducing today's leftmost-bad rule exactly) |
| LUA-03 | `Cannot <op>: too many arguments (expected N, got M)`, BREAKING | §Pattern 3; sol2 arity filter `call.hpp:178-190` |
| LUA-04 | Files accept the six expression methods; `f:save` guards; file stays open | §Pattern 4; guards located at `src/expression/expression.cpp:42-69`, `src/binary/binary_file.cpp:67-69` |
| LUA-05 | `e:metadata()` → `e:get_metadata()`; `quiver.expression` returns `Expression` | §Pattern 4; `expression.cpp:116-117` today |
| LUA-06 | Before/after Release benchmark; traits form if the tag costs | §Benchmark Protocol; the Phase 7 baseline is 2174 / 2082 ms |
| LUA-07 | No new Release warnings (C4702) | §Pitfall 2: the spike measured 34 C4702 unsuppressed and 0 with the pragma |
| LUA-08 | `LUA_DB_API_REFERENCE` updated; sync test passes | §Docs; sync-test mechanics in `bindings/js/test/lua-api-sync.test.ts:25-57` |
</phase_requirements>

## Project Constraints (from AGENTS.md)

- **Error messages live in C++.** Lua operand errors are Pattern 1 (`Cannot {operation}: {reason}`). sol2's raw text is
  accepted only as the backstop for dot-calls and direct metatable calls, as the CHANGELOG already documents.
- **Thin bindings, clean over defensive, delete rather than deprecate**: `to_expression`/`is_number`/`binop` are deleted, not kept as aliases.
- **Every lua_runner TU with by-value sol2 params keeps its `NOLINTBEGIN/END(performance-unnecessary-value-param)` pair**; files stay at about 450 lines or fewer (`src/AGENTS.md`, LuaRunner layout bullet).
- **Sync-test receivers**: every `.set_function(` must be `bind.set_function("x"` or `ns.set_function("x"`. The non-Database usertypes list one bound name per line (`src/AGENTS.md:661-666`).
- **Do Not Fix list**: "the binary hot-path decisions (`src/AGENTS.md`) — load-bearing". `f:read`/`f:write` keep `BinaryFile& self`.
- **Self-updating docs**: fix the AGENTS.md lines this phase makes false (`src/AGENTS.md:697-701` names `binop`/`to_expression`; root `AGENTS.md:831` says `expr:metadata()`).
- **CHANGELOG**: BREAKING entries go under `## [0.13.0] — unreleased`, each saying what a caller must do, with no planning IDs. There is no version bump (0.13.0 stays).
- **clang-format 22.1.8**, `scripts/tidy.bat` with no new (check, line) pair against the Phase 7 baseline.
- **Python via `uv run`** only; never `--no-verify`.

## Summary

The C++ side is finished (Phase 7). `BinaryFile` and `Expression` both derive from `quiver::AbstractExpression`
(`include/quiver/binary/binary_file.h:18`, `include/quiver/expression/expression.h:18`), and every operator and free
function takes `const AbstractExpression&` (`expression.h:30-88`). The Lua layer still converts by hand.
`to_expression` (`src/lua_runner/expression.cpp:33-41`), `is_number` (`:25-27`) and a `sol::object`-taking `binop`
(`:49-65`) decode every operand, and the `Expression` usertype exposes `metadata` (`:116-117`) while `BinaryFile`
exposes `get_metadata` (`src/lua_runner/binary.cpp:186-187`).

Phase 8 has three parts:
1. **Register the base.** Use the compile-time traits form (`SOL_BASE_CLASSES`/`SOL_DERIVED_CLASSES`) in `internal.h`.
   The tag form must still be built and benchmarked first.
2. **Type the operands.** Every operand becomes a typed sol2 candidate, followed by one fallback overload that rebuilds
   the pinned Pattern 1 text, plus the new too-many-arguments text.
3. **Share the methods.** Register the six expression methods once, as `const AbstractExpression&` lambdas, on both usertypes.

All three mechanisms were exercised in this session against the real tree. The spike compiled a prototype
`expression.cpp` with the exact Release command line from `build/release/compile_commands.json`.

The milestone design study already measured that the runtime `sol::base_classes` tag swaps each derived metatable's
`__index` table for a C closure. That adds +80-110 ns per method call: about 4-5% of a 2.08 µs `f:read` call, and it
falls on the protected hot path. The source confirms the cause: `update_bases` calls `change_indexing`
(`usertype_storage.hpp:376-401`), and each lookup then pushes a fresh `c_closure` (`:111-125`). The traits form never
calls `change_indexing`. `register_usertype` always stores `class_check`/`class_cast` from `inheritance<T>`, which reads
`base<T>::type` (`usertype_storage.hpp:1099-1100`, `inheritance.hpp:67-100`). Expect the benchmark to choose the traits form.

The always-throwing fallback makes MSVC `/O2` report `C4702` inside sol2 headers. Measured this session: 34 warnings
for the prototype (17 overload sets × `stack.hpp(285)` + `function_types_overloaded.hpp(48)`), against 0 today. A
`#pragma warning(push) / disable: 4702 / pop` around the TU's includes brings that to 0 (verified). So does defining
the throwing helper in another TU (verified), but `[[noreturn]]` or `__declspec(noinline)` in the same TU do not help
(noinline verified: still 34).

**Primary recommendation:** Do the work in `src/lua_runner/expression.cpp` (plus the traits block in `internal.h` and one
line deleted from `binary.cpp`). The rest:
- One `operand_error(operation, arity, numbers, args)` fallback reproduces today's leftmost-bad-operand rule, then reports too many arguments.
- Wrap expression.cpp's includes in an MSVC-only `C4702` push/disable/pop.
- Benchmark three binaries interleaved: phase7, tag and traits.
- Land the traits form unless the tag's medians are within 2% of Phase 7's.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Expression semantics (operators, `save` guards, metadata) | C++ core (`src/expression/`, `src/binary/`) | — | Done in Phase 7. Phase 8 must not change core code. |
| Operand type check | sol2 (typed `const AbstractExpression&` candidates) | Lua binding fallback | LUA-01: sol2 does the check. The fallback only words the error. |
| Operand / arity error text (Pattern 1) | Lua binding (`expression.cpp` fallback through `lua_type_error`, `internal.h:209-218`) | — | Pre-FFI-style local message, the same as today. The core never sees the bad value. |
| Sandbox of `f:save` / `e:save` paths | Lua binding (`resolve_sandboxed_path`) | — | LuaRunner policy, not binary-subsystem policy (root AGENTS.md). |
| Output collision and open-for-write guards | C++ core (`AbstractExpression::save`, `BinaryFile::open`) | — | Already there. The binding inherits them. |
| Agent-facing docs | JS package (`bindings/js/src/lua-api.ts`) | sync test | A build-time constant (Do Not Fix: do not relocate it). |

## Standard Stack

No new dependency. Everything is already fetched:

| Library | Version | Purpose | Evidence |
|---------|---------|---------|----------|
| sol2 | v3.5.0 | usertypes, overloads, inheritance traits | `git describe --tags` in `build/_deps/sol2-src` → `v3.5.0` [VERIFIED: build/_deps/sol2-src] |
| Lua | 5.4.8 | unary metamethods receive the operand twice | `lvm.c:1555` `luaT_trybinTM(L, rb, rb, ra, TM_UNM)`, `:1566` same for `TM_BNOT` [VERIFIED: build/_deps/lua-src/src/lvm.c] |

sol2 defines in force (verbatim, `src/CMakeLists.txt:85-92`) [VERIFIED: src/CMakeLists.txt:85-92]:
```
target_compile_definitions(quiver PRIVATE
    SOL_ALL_SAFETIES_ON=1
    SOL_PRINT_ERRORS=0
    SOL_SAFE_NUMERICS=1
    SOL_NO_NIL=1
    SOL_SAFE_GETTER=0
    SOL_SAFE_STACK_CHECK=0
)
```
`SOL_STRINGS_ARE_NUMBERS` is off (sol2 default). So the `double` candidate's check is `type_of == number`
(`stack_check_unqualified.hpp:202-208`), and `e + '1'` still reaches the fallback ("got string"), as it does today.

### The two inheritance forms (sol2 v3.5.0 source)

| | Runtime tag `sol::base_classes, sol::bases<AbstractExpression>()` | Compile-time `SOL_BASE_CLASSES` / `SOL_DERIVED_CLASSES` |
|---|---|---|
| How a `const AbstractExpression&` check passes | `weak_derive<AbstractExpression>::value = true` at registration (`usertype_storage.hpp:386`) | `derive<AbstractExpression>` specialized `true_type` (`forward.hpp:257-264`) |
| Checker | `derive<T>::value \|\| weak_derive<T>::value` → rawget `class_check` (`stack_check_unqualified.hpp:541-552`) | same line |
| Getter cast | `class_cast` (`stack_get_unqualified.hpp:905-916`) | same |
| `class_check` stored by | `update_bases` (`type_check_with<Bases...>`) | `register_usertype` always (`inheritance<T>::type_check` over `base<T>::type`, `usertype_storage.hpp:1099-1100`) |
| `__index` | **replaced by a C closure** (`change_indexing`, `usertype_storage.hpp:559-584`, forced by `update_bases_func` `:262-271`). Each method lookup calls `index_call_with_bases` → `string_keys.find` → pushes a new `c_closure` (`:111-125`) | stays the plain fast table (`usertype_storage.hpp:1137`) |
| Cost on `f:read` | +80-110 ns/call measured by the design-study spike (`.planning/research/DESIGN-STUDY.md`, Compiled spike row "sol::base_classes replaces the table __index") | none expected: `BinaryFile& self` matches its own metatable first, and `derive<BinaryFile>`/`weak_derive<BinaryFile>` stay false |
| ODR | none | the specializations must be visible in every TU that instantiates sol2 for these types (ill-formed NDR otherwise). `usertype_storage<T>::index_call_` reads `base<T>::type` (`usertype_storage.hpp:629-633`) |

Macro text (verbatim, `build/_deps/sol2-src/include/sol/forward.hpp:249-265`) [VERIFIED: forward.hpp:249-265]:
```cpp
#define SOL_BASE_CLASSES(T, ...)                       \
	namespace sol {                                   \
		template <>                                  \
		struct base<T> : std::true_type {            \
			typedef ::sol::types<__VA_ARGS__> type; \
		};                                           \
	}                                                 \
	static_assert(true, "")
#define SOL_DERIVED_CLASSES(T, ...)                    \
	namespace sol {                                   \
		template <>                                  \
		struct derive<T> : std::true_type {          \
			typedef ::sol::types<__VA_ARGS__> type; \
		};                                           \
	}                                                 \
	static_assert(true, "")
```
Both macros open `namespace sol`, so they must sit at **global** scope. They need only forward declarations of the
types: this session's spike compiled them after a bare `namespace quiver { class AbstractExpression; class BinaryFile; class Expression; }`.

**Installation:** none.

## Package Legitimacy Audit

No external packages are installed in this phase. **Removed [SLOP]:** none. **Flagged [SUS]:** none.

## Current State (what the plan edits)

| Item | Location | Today |
|------|----------|-------|
| `is_number` | `src/lua_runner/expression.cpp:25-27` | `o.get_type() == sol::type::number` |
| `to_expression` | `expression.cpp:33-41` | `is<Expression>` / `is<BinaryFile>` / `throw lua_type_error(operation, "operand", "an expression or a binary file", o);` (`:40`) |
| `binop<Op>(name)` | `expression.cpp:49-65` | `sol::object` lhs/rhs; number on one side → the other side decoded; else lhs then rhs into locals |
| Operators on both usertypes | `expression.cpp:91-102` (`bind_expression_operators<T>`), applied at `:108` (file) and `:150` (Expression) | `__unm`/`__bnot` take `(sol::object a, sol::object)` |
| `Expression` usertype | `expression.cpp:111-149` | `save`, `metadata`, `aggregate`, `aggregate_agents`, `select_agents`, `rename_agents`, all with `Expression& self` |
| `quiver.*` functions | `expression.cpp:152-170` | `expression`, `abs`, `sqrt`, `log`, `exp`, `ifelse`, then `gt` `lt` `gte` `lte` `eq` `neq` via `binop` (12 total) |
| `BinaryFile` usertype | `src/lua_runner/binary.cpp:164-190` | `read`, `write`, `close`, `is_open`, `get_metadata` (`[](BinaryFile& self) -> BinaryMetadata`), `get_file_path` |
| `bind_expression` signature | `src/lua_runner/internal.h:335` | `void bind_expression(sol::state& state, sol::table& ns, sol::usertype<BinaryFile>& binary_file_type, Database& db);` (keep it) |
| `class BinaryFile;` forward decl | `internal.h:28` | inside `namespace quiver {` |
| Pinned operand tests | `tests/test_lua_expression.cpp:556-582` | 11 + 2 = 13 `expect_lua_error` (substring match, `tests/test_lua_runner.h:48-55`) |
| `:metadata()` call sites to rename | `tests/test_lua_expression.cpp:238,240,302`; `bindings/js/src/lua-api.ts:918`; root `AGENTS.md:831` | — |

The pinned tail, verbatim (`tests/test_lua_expression.cpp:561`) [VERIFIED: tests/test_lua_expression.cpp:556-582]:
`const std::string tail = ": operand must be an expression or a binary file, got ";` The cases are `e + 'x'`→string,
`e - 'x'`→string, `e * {}`→table, `e / 'x'`→string, `e & 'x'`→string, `e | 'x'`→string, `quiver.gt(1, 2)`→number,
`quiver.eq(e, 'x')`→string, `quiver.abs('x')`→string, `quiver.ifelse(e, e, 'x')`→string,
`quiver.expression(5)`→number, `quiver.gt('a', {})`→string, `quiver.ifelse(5, {}, 'x')`→number.

The error-shape helper, verbatim (`src/lua_runner/internal.h:208-218`) [VERIFIED: internal.h:201-218]:
```cpp
// The one shape of an argument type error: "Cannot <op>: <what> must be <expected>, got <type>".
inline std::runtime_error lua_type_error(
    const std::string& operation,
    const std::string& what,
    const char* expected,
    const sol::object& got
) {
```
`lua_type_name` (`internal.h:203-206`) maps `sol::type::none` to `"nil"`, as Lua does for a missing argument.

### `save` guards (unchanged, inherited by `f:save`)

1. **Sandbox**: the binding resolves the path first, `self.save(resolve_sandboxed_path(db, "save", path))` (`expression.cpp:115`). Pinned text: `"Cannot save: path '../out' escapes the database directory"` (`tests/test_lua_expression.cpp:372`).
2. **Output collision**: `throw std::runtime_error("Cannot save: output path collides with input file '" + in_path + "'");` (`src/expression/expression.cpp:51`) [VERIFIED: src/expression/expression.cpp:42-69].
3. **Open for writing**: `save` opens each collected input with `f->open('r')` (`src/expression/expression.cpp:66-68`). `BinaryFile::open` throws `"Cannot open_file: file is already open for writing: " + canonical` (`src/binary/binary_file.cpp:67-69`) [VERIFIED: src/binary/binary_file.cpp:61-69] when the path is in the process-wide `write_registry`. A Lua writer `f` stays registered until `f:close()` (`binary_file.cpp:113-124`).
4. **File stays open**: `BinaryFile::node()` returns `std::make_shared<ExpressionFile>(get_file_path())` (`src/expression/expression_file.cpp:30-32`), and the leaf owns its *own* `BinaryFile file_`. `save` opens and closes only that one (`CloseOnExit`, `expression.cpp:56-64`). The caller's handle is never touched. The C++ side already pins this (Phase 7, EXPR-02).

`save` on a file needs no new C++ guard. Phase 7 review IN-02 notes that guard 3's text names `open_file`, not `save`.
Pin the current text (see Open Questions).

## Architecture Patterns

### System Architecture Diagram

```
Lua script ──► operator (e + x)        ──► metatable __add  ─┐
           ──► quiver.gt(a, b, ...)    ──► ns function      ─┼─► sol::overload
           ──► f:aggregate(...)/e:save ──► usertype method  ─┘     │
                                                                    ├─ candidate (A,A) ──┐ arity == 2 && both
                                                                    ├─ candidate (A,double)│ pass check<const A&>
                                                                    ├─ candidate (double,A)│ (class_check via
                                                                    │                      │  derive<A> trait)
                                                                    │                      ▼
                                                                    │            C++ operator / AbstractExpression
                                                                    │            method → Expression (pushed)
                                                                    └─ fallback (sol::variadic_args)  ← only when
                                                                         operand_error(op, N, numbers, args)  every typed
                                                                         ├─ leftmost bad operand → Pattern 1  candidate
                                                                         └─ else → too many arguments         failed
                                                                         throw std::runtime_error → sol2 trampoline
                                                                         → lua_error(what()) → run() rethrows
```
Methods (`save`/`get_metadata`/`aggregate`/...) are single functions, not overload sets. sol2 type-checks `self`
(SOL_SAFE_USERTYPE), and extra arguments stay ignored, as Lua does elsewhere. LUA-03 covers only the `quiver.*`
functions and the operators.

### Recommended file changes

```
src/lua_runner/
├── internal.h      # + forward decls of AbstractExpression/Expression + the 3 traits macros (global scope)
├── binary.cpp      # - the "get_metadata" entry (now shared, registered by bind_expression)
└── expression.cpp  # rewritten operand plumbing; shared methods; C4702 pragma; still well under 450 lines
tests/test_lua_expression.cpp   # 3 lines :metadata() -> :get_metadata(); new tests appended
bindings/js/src/lua-api.ts      # expression section, sandbox bullet
CHANGELOG.md                    # 2 BREAKING (Changed) + 1 Added
src/AGENTS.md (697-701, 720, 661-666), AGENTS.md:831   # lines made false
```

### Pattern 1: Base registration (traits form, in `internal.h`)

`internal.h` is included by every `src/lua_runner/` TU, so "visible in every TU that binds these types" holds by
construction. A second header would only add a file. The forward declarations go *before* the existing
`namespace quiver {` block (the macros cannot sit inside it):

```cpp
// internal.h, after #include <sol/sol.hpp> and the std includes
namespace quiver {
class AbstractExpression;
class BinaryFile;
class Expression;
}  // namespace quiver

// sol2 inheritance as compile-time traits rather than the sol::base_classes tag: the tag swaps each derived
// metatable's __index table for a C closure, which every f:read / f:write pays. Every TU that instantiates
// sol2 for these types must see these specializations, so they live here, in the header all of
// src/lua_runner/ includes.
SOL_BASE_CLASSES(quiver::BinaryFile, quiver::AbstractExpression);
SOL_BASE_CLASSES(quiver::Expression, quiver::AbstractExpression);
SOL_DERIVED_CLASSES(quiver::AbstractExpression, quiver::BinaryFile, quiver::Expression);
```
[VERIFIED: compiled this session, `expr_proto_fwd.cpp`, exact Release flags, exit 0, 0 warnings]

The **tag form** (needed only for the benchmark build) adds `sol::base_classes, sol::bases<AbstractExpression>(),`
after `sol::no_constructor,` in **both** `new_usertype` calls (`binary.cpp:164`, `expression.cpp:111`), with no traits block.

### Pattern 2: Typed operators via `sol::overload` + fallback

sol2 tries candidates in declaration order. A non-variadic candidate is skipped when `free_arity != fxarity`
(`call.hpp:178-190`) or when `check_types(..., &no_panic, ...)` fails (`call.hpp:191-201`). A `sol::variadic_args`
candidate is runtime-variadic: it skips the arity filter and always type-checks (`stack_check_unqualified.hpp`
`variadic_args` branch), so as the **last** candidate it runs only after every typed one failed. That is sol2's
documented pattern (`examples/source/overloading_with_fallback.cpp`). With no match and no fallback, sol2 raises the raw
`"sol: no matching function call takes this number of arguments and the specified types"` (`call.hpp:157`).
A C++ exception thrown in a candidate is caught by sol2's trampoline and raised verbatim with `lua_error`, with no
`[string ...]:1:` prefix (design-study spike, `trampoline.hpp:116-127`). That is the same path today's `to_expression` throw takes.

**Unary metamethods**: Lua calls `__unm`/`__bnot` with the operand twice (`lvm.c:1555`, `:1566`). Type them
`(const A& a, const A&)`. Inside an overload set the arity must match, so `(const A&)` alone would always fall through.

**Do not wrap single functions without the fallback.** A non-overloaded typed function gets sol2's raw
`stack index N, expected userdata ...` text, with a compiler-dependent signature, and ignores extra arguments.

### Pattern 3: The fallback rule (reproduces today exactly)

Today's rule, from `binop` (`expression.cpp:51-63`): a number on exactly one side means the other side is decoded.
Otherwise lhs is decoded, then rhs. Unary and `ifelse` decode left to right. Restated over an arbitrary argument list:

- Inspect operands `0..arity-1`. A missing one is `nil`.
- An operand is acceptable if it `is<AbstractExpression>()`, **or**, for the arithmetic/comparison/logical binops, if
  it is a number and *not every* operand within the arity is a number.
- Report the first unacceptable operand: `lua_type_error(operation, "operand", "an expression or a binary file", o)`.
- If all are acceptable and `args.size() > arity`, throw
  `"Cannot " + op + ": too many arguments (expected " + N + ", got " + M + ")"`.

Check against every pinned case and the spike texts:
- `gt(1,2)`: both are numbers → lhs → "got number" ✓.
- `gt('a',{})` → "got string" ✓.
- `gt(1,'x')` → `'x'` "got string" (today's behaviour; design-1's rule wrongly reported the number).
- `ifelse(5,{},'x')` → number ✓.
- `abs()` → "got nil".
- `abs(e,99)` → too many (1, 2) ✓.
- `gt(e,f,3)` and `gt(e,1,'x')` → too many (2, 3) ✓.
- `'x' + f`: the string metatable's `trymt` (`lstrlib.c:277-285`) calls f's `__add('x', f)` → "got string" ✓.

`ifelse` has no `double` overload (Phase 7 decision 07-01), so `numbers = false` there, as for the unary functions.

### Pattern 4: Shared expression methods, registered once

`AbstractExpression` provides `get_metadata` (virtual: a file returns its handle's copy, an expression its node's),
`save`, `aggregate`, `aggregate_agents`, `select_agents` and `rename_agents` (`include/quiver/expression/abstract_expression.h:32-46`).
Write each Lua method once as a lambda taking `const AbstractExpression& self`. List them by quoted name in
`new_usertype<Expression>` (that is what the sync test's pass 2 parses). Then attach the same lambdas to the file
with the indexer, next to the existing `bind_expression_operators(binary_file_type)`:

```cpp
binary_file_type["save"] = save;            // not .set_function(: the sync test's pass 1 counts every
binary_file_type["get_metadata"] = get_metadata;   // .set_function( and accepts only bind./ns. receivers
// ... aggregate, aggregate_agents, select_agents, rename_agents
```
Delete `binary.cpp:186-187` (the file's own `get_metadata`), so the name is registered once. sol2's `set` would silently
replace it anyway (`usertype_storage.hpp:694-705`), but double registration misleads readers. Adding a method to an
existing usertype keeps `__index` a table: `poison_indexing` is false for function bindings (`usertype_storage.hpp:717`).

The sync test's `:<name>(` check is receiver-agnostic (`lua-api-sync.test.ts:100-111`). Listing the names once
(on Expression) gives the same coverage as listing them on both. Update the `src/AGENTS.md` layout bullet: file
expression methods are attached by `bind_expression` through the indexer, like the operators.

### Anti-Patterns to Avoid
- **Changing `f:read`/`f:write` to `const AbstractExpression& self`**: every call would then pay four failed `check_metatable` registry lookups plus `class_check`. They must keep `BinaryFile& self`.
- **Registering `AbstractExpression` as a usertype**: forbidden by criterion 1, and unnecessary. sol2 documents base-member lookup as unsupported (`documentation/source/api/usertype.rst:288`).
- **A `sol_lua_check`/`sol_lua_get` customization or an `argument_handler` specialization**: both rejected by the design study (they lose the operation name and add an ODR hazard).
- **Leaving a plain typed lambda without the fallback**: it surfaces raw sol2 text.
- **`[[noreturn]]` on the throwing helper**: it guarantees C4702.
- **Mentioning `to_expression`/`is_number` in a comment**: criterion 1's `git grep -nw` would fail.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Operand type dispatch | `is<Expression>`/`is<BinaryFile>`/`is_number` chains | typed sol2 candidates + `sol::overload` | LUA-01, and sol2 already does the pointer cast (`class_cast`) |
| Error shape | new message strings | `lua_type_error` (`internal.h:209`) | one shape; byte-identical to the pins |
| Path containment | a new check in `f:save` | `resolve_sandboxed_path(db, "save", path)` | the single gate (root AGENTS.md) |
| Output collision / writer guard | a Lua-side registry check | `AbstractExpression::save` + `BinaryFile::open` | already in core |
| Benchmark harness | a new script | `build/abstract-check/perf/bench.sh` (takes any number of `label=cli`) | the same protocol as the Phase 7 baseline |
| Release warning diff | ad-hoc grep | `build/abstract-check/release_warnings.sh` | Phase 7 baseline = 0 warnings, `c4702=0` |
| Lua equality regression check | a new test | `build/abstract-check/eq.sh debug|release` → `[true,true,false]` | EQ-01 is deferred; behaviour must not move |

## Common Pitfalls

### Pitfall 1: The runtime tag slows the binary hot path
**What goes wrong:** `sol::base_classes` makes every `f:read`/`f:write` method lookup a C call, a hash lookup and a closure allocation.
**How to avoid:** use the traits form. Prove the mechanism: `type(getmetatable(f).__index)` must be `"table"`. The golden `surface.lua` (`build/layout-check/scripts/surface.lua`) prints `__index:table` vs `__index:function`.
**Warning signs:** a golden surface diff showing `__index:function`, or `f:read` median > Phase 7 + 2%.

### Pitfall 2: C4702 in Release
**What goes wrong:** MSVC `/O2 /Ob2` sees that the fallback always throws and flags sol2's post-call push code. Measured: 34 × C4702 (`stack.hpp(285)`, `function_types_overloaded.hpp(48)`), 0 today, 0 in Debug.
**How to avoid:** at the very top of `expression.cpp`, before its first include, put
`#ifdef _MSC_VER` / `#pragma warning(push)` / `#pragma warning(disable : 4702)` / `#endif`, and the matching pop after the
last include. C4702 is a code-generation warning, so MSVC uses the state in force at each template definition's opening
brace, which sits in the sol2 header text parsed inside the pushed region. Verified: 0 warnings. The alternative,
defining `operand_error` in another TU without `[[noreturn]]`, also gave 0 but is fragile: anyone who later adds
`[[noreturn]]` brings the warnings back.
**Warning signs:** `release_warnings.sh` reports `c4702>0`. Note that it writes `build/abstract-check/release-warnings.txt`. The Phase 7 file is empty, so the baseline is "0 lines".

### Pitfall 3: ODR with the traits
**What goes wrong:** a TU that instantiates `usertype_storage<BinaryFile>` or `check<const AbstractExpression&>` without seeing the specializations gets different template instances (ill-formed, no diagnostic required).
**How to avoid:** put the traits in `internal.h`, which every `src/lua_runner/*.cpp` includes. `binary.cpp:1` and `expression.cpp:3` include it before any sol2 use. The macros must precede any use in the TU, or GCC/Clang report "specialization after instantiation".

### Pitfall 4: Unary metamethod arity
Lua passes the operand twice, so unary candidates are `(const A&, const A&)`. Their fallback uses arity 2 and
`numbers = false`. `getmetatable(e).__unm(5)` then still says `Cannot unm: ... got number`, as today.

### Pitfall 5: Missing-argument object in the fallback
For `i >= args.size()`, build `sol::make_object(args.lua_state(), sol::lua_nil)`. Do not rely on reading past the stack
top, and never call `get_type()` on a default `sol::object` (null `lua_State`).

### Pitfall 6: Sync-test parse traps
Only `bind.set_function("x"` / `ns.set_function("x"` are allowed (`lua-api-sync.test.ts:25-27,72`). A line holding only
a quoted lowercase name and a comma, after a `new_usertype<T>` line in the same file, is read as a method of `T`
(`:39-56`). Define the shared lambdas **above** `new_usertype<Expression>`. Inside their bodies, no line may be just
`"name",`, so watch clang-format wraps.

### Pitfall 7: Golden harness expectations
`build/layout-check/golden.sh` compares against the Debug baseline. Phase 8 changes `surface.txt` deliberately:
- BinaryFile gains `aggregate`, `aggregate_agents`, `rename_agents`, `save` and `select_agents`.
- Expression loses `metadata` and gains `get_metadata`.

Review the Debug diff with `GOLDEN_CHANGE=1`, recapture the baseline (`--capture`), then require Release to match it byte for byte.

### Pitfall 8: Tests
Only the two pinned bodies are frozen. The three `:metadata()` lines (`test_lua_expression.cpp:238,240,302`) must change
to `get_metadata()`. Prove criterion 2 with `git diff <phase-base> -- tests/test_lua_expression.cpp`: no hunk may touch lines 556-582.

## Code Examples

Skeleton of the new `expression.cpp` core, adapted from the compiled prototype (`scratchpad/expr_proto*.cpp`; the
real file keeps today's `parse_aggregate_op`, the `select_agents`/`rename_agents` bodies and the NOLINT pair):

```cpp
// MSVC /O2 reports C4702 inside sol2 for the always-throwing fallback candidates below; sol2's code, not ours.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
#include "quiver/expression/expression.h"
#include "lua_runner/internal.h"
// ... existing includes ...
#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace quiver::lua_internal {
namespace {

using A = AbstractExpression;

// The last candidate of every expression overload set. sol2 reaches it only after rejecting every typed
// candidate (a wrong type or a wrong count), so it only words the error: the leftmost bad operand, where a
// number beside an expression is a valid operand (`numbers`), else too many arguments.
Expression operand_error(const char* operation, std::size_t arity, bool numbers, const sol::variadic_args& args) {
    const std::size_t count = args.size();
    auto operand = [&](std::size_t i) {
        return i < count ? sol::object(args[i]) : sol::make_object(args.lua_state(), sol::lua_nil);
    };
    std::size_t number_operands = 0;
    for (std::size_t i = 0; i < arity; ++i) {
        number_operands += operand(i).get_type() == sol::type::number ? 1 : 0;
    }
    for (std::size_t i = 0; i < arity; ++i) {
        const auto o = operand(i);
        const bool valid = o.is<A>() || (numbers && number_operands < arity && o.get_type() == sol::type::number);
        if (!valid) {
            throw lua_type_error(operation, "operand", "an expression or a binary file", o);
        }
    }
    throw std::runtime_error(
        "Cannot " + std::string(operation) + ": too many arguments (expected " + std::to_string(arity) + ", got " +
        std::to_string(count) + ")"
    );
}

template <typename Op>
auto binop(const char* operation) {
    return sol::overload(
        [](const A& lhs, const A& rhs) { return Op{}(lhs, rhs); },
        [](const A& lhs, double rhs) { return Op{}(lhs, rhs); },
        [](double lhs, const A& rhs) { return Op{}(lhs, rhs); },
        [operation](sol::variadic_args args) { return operand_error(operation, 2, true, args); }
    );
}

// Lua passes a unary metamethod its operand twice (lvm.c OP_UNM / OP_BNOT), hence arity 2.
template <typename F>
auto unary_metamethod(const char* operation, F f) {
    return sol::overload(
        [f](const A& operand, const A&) { return f(operand); },
        [operation](sol::variadic_args args) { return operand_error(operation, 2, false, args); }
    );
}

template <typename F>
auto unary_function(const char* operation, F f) {
    return sol::overload(f, [operation](sol::variadic_args args) { return operand_error(operation, 1, false, args); });
}
}  // namespace

// in bind_expression:
//   type[sol::meta_function::unary_minus] = unary_metamethod("unm", [](const A& a) { return -a; });
//   type[sol::meta_function::bitwise_not] = unary_metamethod("bnot", [](const A& a) { return !a; });
//   ns.set_function("expression", unary_function("expression", [](const A& o) { return Expression(o); }));
//   ns.set_function("abs", unary_function("abs", [](const A& o) { return quiver::abs(o); }));
//   ns.set_function("ifelse", sol::overload(
//       [](const A& c, const A& t, const A& e) { return quiver::ifelse(c, t, e); },
//       [](sol::variadic_args args) { return operand_error("ifelse", 3, false, args); }));
//   ns.set_function("gt", binop<std::greater<>>("gt"));   // ... lt, gte, lte, eq, neq unchanged in shape
```
The metamethod names stay `add sub mul div unm band bor bnot`. The `quiver.*` names are the function names.
`binop` now returns an overload set instead of a `sol::object` lambda, so the existing call sites
(`expression.cpp:93-100`, `:165-170`) compile unchanged. The exact `operand_error` body above, with the `unm` overload,
compiled this session with Release flags (`expr_proto_final.cpp`: exit 0, 0 warnings with the pragma). Its runtime
texts are [ASSUMED] until the tracer test runs (A2).

### Tests to add (`LuaExpressionTest`, file-backed sandbox, existing `prelude()` `fill`/`fill_by_row`)

| Test | Pins |
|------|------|
| `ExtraArgumentsThrow` | `quiver.abs(e, 99)` → `Cannot abs: too many arguments (expected 1, got 2)`; `quiver.gt(e, f, 3)` → `Cannot gt: too many arguments (expected 2, got 3)` |
| `FileAggregate` | `f:aggregate('row', 'sum')` on a raw file, saved and read back (6.0 per col, as in `AggregateDimensionSum`) |
| `FileAggregateAgents` | `f:aggregate_agents('mean')` → label `mean`, value 15.0 |
| `FileSelectAgents` / `FileRenameAgents` | `f:select_agents({'v2'})`, `f:rename_agents({v1='alpha'}):get_metadata():get_labels()` |
| `FileSaveKeepsFileOpen` | `f:save('expr_out')`; `assert(f:is_open())`; `f:read(...)` still works; output equals input |
| `FileSaveGuards` | `f:save('../out')` → `Cannot save: path '../out' escapes the database directory`; `f:save('expr_a')` on f=`expr_a` → `Cannot save: output path collides with input file`; a writer `w = db:open_file('expr_w','w',make_md())`, `w:save('expr_out')` → `Cannot open_file: file is already open for writing` |
| `FileGetMetadata` | `f:get_metadata():get_labels()` on a raw file |
| `ExpressionGetMetadata` | `e:get_metadata()` works; `e:metadata()` → `attempt to call a nil value (method 'metadata')` |
| `ExpressionOfFileIsAnExpression` | `local e = quiver.expression(f); assert(e.is_open == nil and e.get_file_path == nil and e.aggregate ~= nil)` |
| optional `FileAndExpressionKeepTableIndex` | `type(getmetatable(f).__index) == 'table'` and the same for `e`: pins the hot-path decision against a later switch back to the tag |

The count is the planner's choice. Record `Lua*` = 477 + N, equal in Debug and Release.

## Benchmark Protocol (LUA-06)

Phase 7 baseline (STATE.md PR notes; `07-02-SUMMARY.md`) [VERIFIED: .planning/STATE.md]: `f:write` 1M median
**2174 ms**, `f:read` 1M median **2082 ms**. Five interleaved runs after one warm-up. The binaries are in
`build/perf-phase7/` (`COMMIT` = `f2769c3af094c430a31ef0b7cabdf686b22ced06`, `SHA256SUMS` present). The run-to-run
spread was about ±10% (1857-2255 ms).

1. Build the tag form in Release (`cmake --build build/release`). Copy `quiver_cli.exe`, `libquiver.dll` and `libquiver_c.dll` to `build/perf-phase8-tag/`.
2. Switch to the traits form, rebuild Release, and run all three interleaved:
   `bash build/abstract-check/perf/bench.sh phase7=build/perf-phase7/quiver_cli.exe tag=build/perf-phase8-tag/quiver_cli.exe traits=build/release/bin/quiver_cli.exe`
3. Decision rule (proposal, [ASSUMED]): land traits if the tag's median for either workload exceeds Phase 7's by ≥2%.
   Otherwise the tag satisfies the requirement and may land. The spike predicts +4-5% for the tag and ~0% for traits.
4. Record all six medians, the raw runs (`runs.txt`) and the form that landed in the phase SUMMARY and the STATE.md PR notes.

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `sol::object` operands + `to_expression` | typed `const AbstractExpression&` + fallback | this phase | sol2 checks; the same Pattern 1 text |
| extra args silently ignored | `too many arguments (expected N, got M)` | this phase | BREAKING |
| `e:metadata()` (Expression) vs `f:get_metadata()` (file) | `get_metadata` on both | this phase | BREAKING for `e:metadata()` |
| `quiver.expression(f):aggregate(...)` | `f:aggregate(...)` also works | this phase | additive |

## Docs and CHANGELOG

- `bindings/js/src/lua-api.ts:879-919`:
  - say a file *is* an expression (every method and operator works on `f` directly, and `quiver.expression(f)` stays as an explicit conversion);
  - list `f:aggregate/aggregate_agents/select_agents/rename_agents/save/get_metadata`;
  - replace `e:metadata()` with `e:get_metadata()`;
  - add the arity rule (extra arguments to `quiver.*` expression functions and operators raise `Cannot <op>: too many arguments (expected N, got M)`);
  - extend the sandbox bullet at `:112` (`expr:save`) to "`save` on a file or expression".
  - Run `cd bindings/js && bun test test/lua-api-sync.test.ts` (6 tests).
- CHANGELOG `[0.13.0] — unreleased`, `### Changed`: two **BREAKING** lines.
  - Lua `e:metadata()` is renamed `e:get_metadata()`; call `get_metadata()`.
  - Extra arguments to the twelve `quiver.*` expression functions and to the operator metamethods raise `Cannot <op>: too many arguments (expected N, got M)` instead of being ignored; drop the extra argument.
  - Also add an `### Added` subsection (none exists yet in `[0.13.0]`): Lua files accept `aggregate`, `aggregate_agents`, `select_agents`, `rename_agents` and `save` directly, plus `get_metadata` on both, and `quiver.expression` is no longer needed.
- `src/AGENTS.md:697-701`: replace the `binop`/`to_expression` sentence with the overload + `operand_error` description. Also update `:720` (`expr:save`) and the layout bullet (`:661-666`) about the file's indexer-attached methods. Root `AGENTS.md:831`: `expr:metadata()` → `expr:get_metadata()`. The rest of the docs pass (the AbstractExpression design decision, cross-layer rows) is Phase 9 DOC-01.

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | The 2% threshold for "costs measurably" | Benchmark Protocol | Low. The requirement only says "measurably", and the spike predicts 4-5%. |
| A2 | The exact `operand_error` body reproduces all 13 pins at runtime (it compiled this session; the runtime texts were verified by the design-study spike with an equivalent rule) | Code Examples | Medium. The first tracer task must run the two pinned tests before anything else. |
| A3 | The traits form works at **runtime** in the real DLL (compile verified this session; runtime verified only for the tag form, in the design-study absprobe) | Pattern 1 | Medium. The tracer test (raw-file method + `quiver.abs(f)`) proves it. The fallback is the tag form with its cost recorded. |
| A4 | Fixing root `AGENTS.md:831` in Phase 8 rather than Phase 9 | Docs | Low. A one-word edit. |

## Open Questions

1. **`f:save` on a writer names `open_file`** (Phase 7 review IN-02).
   - Known: core throws `Cannot open_file: file is already open for writing: <path>`.
   - Unclear: whether to rethrow it as `Cannot save: ...`. That is a C++ core change, outside this phase's Lua scope.
   - Recommendation: pin the current text in `FileSaveGuards` and leave the core alone.
2. **Linux evidence.** Criterion 5 asks only for Windows Debug/Release counts.
   - Recommendation: run the Phase 7 Docker GCC 13 / Clang 18 harness once (`build/abstract-check/linux_*.txt` pattern), because the traits block is a compiler-sensitive explicit specialization. Expected `Lua*` there is 475 + N (474 + N passed, 1 skip).
3. **WR-01** (non-pure `get_metadata` base body). It does not affect Lua: both registered types return safe references, and the Lua lambda copies the result into a `BinaryMetadata` value. No action this phase.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| MSVC | build, C4702 check | ✓ | 14.51.36231 | — |
| CMake / Ninja | build | ✓ | 4.3.1 / 1.13.2 | — |
| `build/release` tree | Release suites, bench, warnings | ✓ | configured | — |
| `build/perf-phase7/` binaries | benchmark baseline | ✓ | COMMIT f2769c3 | — |
| Bun | lua-api sync test, JS suite | ✓ | 1.3.14 | — |
| Julia / Dart / uv (Python) | six-suite gate | ✓ | 1.11.9 / 3.13.4 / 0.12.3 | — |
| Docker | optional Linux check | ✓ | 29.6.2 | skip; PR CI covers Linux |

Missing dependencies: none.

## Security Domain

| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | no | — |
| V3 Session Management | no | — |
| V4 Access Control | yes (filesystem) | `resolve_sandboxed_path` gates `f:save` exactly as `e:save` (strict containment, `:memory:` rejected) |
| V5 Input Validation | yes | sol2 typed checks (SOL_SAFE_FUNCTION_CALLS / SOL_SAFE_USERTYPE stay on) + the Pattern 1 fallback; with `SOL_SAFE_GETTER=0`, nothing reads an unchecked operand |
| V6 Cryptography | no | — |

| Pattern | STRIDE | Mitigation |
|---------|--------|------------|
| Path escape through the new `f:save` | Tampering / Elevation | the same single sandbox gate, tested (`f:save('../out')`) |
| Overwriting an input through `save` | Tampering | core output-collision check, tested on a raw file |
| Writing a file that is open for writing | Tampering | process-wide `write_registry`, tested with a writer handle |
| Unchecked userdata cast | Tampering | `class_check` before `class_cast`; never `SOL_USE_UNSAFE_BASE_LOOKUP` |

## Sources

### Primary (HIGH confidence)
- sol2 v3.5.0, `build/_deps/sol2-src/include/sol/`:
  - `forward.hpp:216-265`;
  - `usertype_storage.hpp:111-125, 180-271, 376-401, 459-557, 559-585, 623-660, 690-780, 1060-1140`;
  - `stack_check_unqualified.hpp:193-210, 500-559`;
  - `stack_get_unqualified.hpp:887-925`;
  - `call.hpp:150-260`;
  - `inheritance.hpp:30-140`;
  - `examples/source/overloading_with_fallback.cpp`.
- Lua 5.4.8 `lvm.c:1555-1566`.
- Quiver: `src/lua_runner/{expression.cpp,binary.cpp,internal.h}`, `src/expression/{expression.cpp,expression_file.cpp}`, `src/binary/binary_file.cpp`, `include/quiver/expression/{abstract_expression.h,expression.h}`, `tests/test_lua_expression.cpp`, `tests/test_lua_runner.h`, `bindings/js/test/lua-api-sync.test.ts`, `bindings/js/src/lua-api.ts`, `src/CMakeLists.txt`, `cmake/CompilerOptions.cmake`, `CHANGELOG.md`.
- This session's compile spike: `scratchpad/expr_proto*.cpp` compiled with `build/release` flags. Results: traits 0 errors; unsuppressed 34 × C4702; pragma 0; same-TU noinline 34; out-of-TU helper 0.
- Live counts: `quiver_tests --gtest_list_tests --gtest_filter=Lua*` gives 477 in 12 suites (`LuaExpressionTest` 28); `LuaRunnerCApiTest` 27.

### Secondary (MEDIUM confidence)
- `.planning/research/DESIGN-STUDY.md` (Compiled spike + Observed error texts): runtime texts and the tag's +80-110 ns/call. Measured in an earlier session, not re-run here.
- `.planning/phases/07-abstractexpression-in-c/07-02-SUMMARY.md`, `07-REVIEW.md` (baselines, IN-02, WR-01).

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH. Vendored source read; nothing new is installed.
- Architecture: HIGH. Compiled in the real tree with Release flags. Runtime of the traits form is A3.
- Pitfalls: HIGH. C4702 counts measured this session; the `__index` cost comes from source plus the earlier spike.

**Research date:** 2026-10-04
**Valid until:** 2026-11-03 (stable: pinned sol2/Lua versions)
