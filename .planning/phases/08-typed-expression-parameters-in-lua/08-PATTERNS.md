# Phase 8: Typed Expression Parameters in Lua - Pattern Map

**Mapped:** 2026-10-04
**Files analyzed:** 9
**Analogs found:** 9 / 9 (every file is a modification; the analog is mostly the file's own current code)

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|-------------------|------|-----------|----------------|---------------|
| `src/lua_runner/expression.cpp` | binding (sol2 usertype + ns functions) | request-response (Lua call -> C++ -> Expression) | itself (`binop`, `bind_expression_operators`, `new_usertype<Expression>`) | exact |
| `src/lua_runner/internal.h` | shared header (helpers + traits) | n/a | itself: forward decl block `:26-35`, `lua_type_error` `:208-218` | exact |
| `src/lua_runner/binary.cpp` | binding | request-response | itself `:164-190` (delete one entry) | exact |
| `tests/test_lua_expression.cpp` | test | request-response | `SaveEscapeThrows`, `OperandErrorsReportTheLeftmostBadOperand` (`:575-582`) | exact |
| `bindings/js/src/lua-api.ts` | docs constant | n/a | itself `:908-919` expression block | exact |
| `CHANGELOG.md` | docs | n/a | `## [0.13.0] — unreleased` / `### Changed` BREAKING entries `:8-14` | exact |
| `src/AGENTS.md` | docs | n/a | itself `:697-701` (binop/to_expression sentence), `:661-666`, `:720` | exact |
| `AGENTS.md` (root) | docs | n/a | Lua expression paragraph (`expr:metadata()`) | exact |
| benchmark harness | script (reuse, no new file) | batch | `build/abstract-check/perf/bench.sh`, `release_warnings.sh`, `eq.sh` | exact (reuse) |

## Pattern Assignments

### `src/lua_runner/expression.cpp` (binding, request-response)

**Analog:** current file. Keep: includes `:1-15`, `parse_aggregate_op` `:68-85`, NOLINT pair `:87-88`/`:174`, the `select_agents`/`rename_agents` bodies `:130-148`, the operator table shape `:91-102`, the `ns.set_function(...)` call sites `:152-170`.

**Delete** `is_number` (`:25-27`), `to_expression` (`:29-41`), the `sol::object` `binop` (`:43-65`). Do not name them in comments (criterion 1 grep).

**Current binop to replace** (lines 49-65):
```cpp
template <typename Op>
auto binop(const char* operation) {
    return [operation](const sol::object& lhs, const sol::object& rhs) -> Expression {
        const bool lnum = is_number(lhs);
        const bool rnum = is_number(rhs);
        if (lnum && !rnum) { return Op{}(lhs.as<double>(), to_expression(rhs, operation)); }
        if (!lnum && rnum) { return Op{}(to_expression(lhs, operation), rhs.as<double>()); }
        auto a = to_expression(lhs, operation);
        auto b = to_expression(rhs, operation);
        return Op{}(a, b);
    };
}
```
Replacement: `sol::overload((A,A), (A,double), (double,A), [operation](sol::variadic_args args){ return operand_error(operation, 2, true, args); })` plus `operand_error`, `unary_metamethod`, `unary_function` exactly as in 08-RESEARCH.md "Code Examples" (compiled skeleton). Call sites `:93-100`, `:165-170` stay textually unchanged.

**Unary metamethods to retype** (lines 97, 101):
```cpp
type[sol::meta_function::unary_minus] = [](sol::object a, sol::object) { return -to_expression(a, "unm"); };
type[sol::meta_function::bitwise_not] = [](sol::object a, sol::object) { return !to_expression(a, "bnot"); };
```
-> `unary_metamethod("unm", [](const A& a){ return -a; })` (arity 2, Lua passes operand twice).

**Methods to share** (lines 114-148, today `Expression& self`): rewrite as named lambdas taking `const AbstractExpression& self`, defined ABOVE `new_usertype<Expression>` (sync-test Pitfall 6), list by quoted name in `new_usertype<Expression>` with `"metadata"` renamed `"get_metadata"`, then attach to the file via indexer next to `bind_expression_operators(binary_file_type)` (`:108`):
```cpp
binary_file_type["save"] = save;   // indexer, not .set_function( (sync test accepts only bind./ns.)
binary_file_type["get_metadata"] = get_metadata;
// aggregate, aggregate_agents, select_agents, rename_agents
```
Sandbox gate to keep verbatim inside `save` (line 115):
```cpp
[&db](Expression& self, const std::string& path) { self.save(resolve_sandboxed_path(db, "save", path)); },
```

**ns functions to retype** (lines 152-162): `expression/abs/sqrt/log/exp` -> `unary_function(name, [](const A& o){...})`; `ifelse` -> 3-arg overload + `operand_error("ifelse", 3, false, args)`.

**C4702 guard**: MSVC `#pragma warning(push)/(disable : 4702)` before line 1, `pop` after line 15 (research Pitfall 2). No `[[noreturn]]` on `operand_error`.

---

### `src/lua_runner/internal.h` (shared header)

**Analog:** forward-decl block (lines 26-35):
```cpp
namespace quiver {

class BinaryFile;

namespace csv_write {
```
Add BEFORE this `namespace quiver {` a separate block declaring `AbstractExpression`, `BinaryFile`, `Expression`, then at global scope:
```cpp
SOL_BASE_CLASSES(quiver::BinaryFile, quiver::AbstractExpression);
SOL_BASE_CLASSES(quiver::Expression, quiver::AbstractExpression);
SOL_DERIVED_CLASSES(quiver::AbstractExpression, quiver::BinaryFile, quiver::Expression);
```
Error helper the fallback must use, unchanged (lines 208-218):
```cpp
inline std::runtime_error lua_type_error(const std::string& operation, const std::string& what,
                                         const char* expected, const sol::object& got) {
    return std::runtime_error("Cannot " + operation + ": " + what + " must be " + expected + ", got " + lua_type_name(got));
}
```
Tag form (benchmark build only): `sol::base_classes, sol::bases<AbstractExpression>(),` after `sol::no_constructor,` in both `new_usertype` calls, no traits.

---

### `src/lua_runner/binary.cpp` (binding)

**Analog:** itself, `new_usertype<BinaryFile>` (lines ~164-190). Delete only:
```cpp
        "get_metadata",
        [](BinaryFile& self) -> BinaryMetadata { return self.get_metadata(); },
```
Keep `read`/`write` with `BinaryFile& self` (Do Not Fix: hot path). Keep `binary_file_type` passed to `bind_expression` (signature `internal.h:335` unchanged).

---

### `tests/test_lua_expression.cpp` (test)

**Analog for error tests:** `OperandErrorsReportTheLeftmostBadOperand` (lines 575-582):
```cpp
TEST_F(LuaExpressionTest, OperandErrorsReportTheLeftmostBadOperand) {
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);
    const std::string tail = ": operand must be an expression or a binary file, got ";
    expect_lua_error(lua, "return quiver.gt('a', {})", "Cannot gt" + tail + "string");
```
Copy for `ExtraArgumentsThrow` (needs a file-backed `e`/`f` via `prelude()` + `fill(...)`).

**Analog for file-backed/sandbox tests:** `SaveEscapeThrows` (~line 360):
```cpp
    expect_lua_error(lua, prelude() + R"(
        fill('expr_a', 1.0, 1.0)
        local fa = db:open_file('expr_a', 'r')
        local expr = quiver.expression(fa) * 2.0
        expr:save('../out')
    )", "Cannot save: path '../out' escapes the database directory");
```
Use for `FileSaveGuards` (drop `quiver.expression`, call `fa:save` directly; pin `Cannot save: output path collides with input file` and `Cannot open_file: file is already open for writing`). Positive tests use `lua.run(prelude() + R"(... assert(...) ...)")` like `ComparisonFreeFunctions`.

`expect_lua_error` (`tests/test_lua_runner.h:48-55`) is substring match. Rename `:metadata()` at lines 238, 240, 302 to `:get_metadata()`. Lines 556-582 (two pinned bodies) must have no diff. Append new tests after line 582. Full test list: research table "Tests to add".

---

### `bindings/js/src/lua-api.ts` (docs constant)

**Analog:** expression block (lines 908-919), e.g.
```
local e = (quiver.expression(r) + 10.0) * 2.0        -- files auto-wrap; scalars either side
e:save(out_path); e:metadata()                       -- save path is sandboxed like db:open_file
```
Change `e:metadata()` -> `e:get_metadata()`; state files are expressions (`r:aggregate(...)`, `r:save(...)`); add arity rule line; sandbox bullet (~`:112`) to "`save` on a file or expression". Verify: `bun test test/lua-api-sync.test.ts`; `grep -n ':metadata()'` empty.

---

### `CHANGELOG.md`

**Analog** (lines 8-14):
```
## [0.13.0] — unreleased

### Changed

- **BREAKING** **Lua table arguments are type-checked.** A value other than a table passed where
  a Lua method takes a table now raises `Cannot <op>: <argument> must be a table, got <type>`:
```
Add two BREAKING bullets in that `### Changed` (`e:metadata()` -> `e:get_metadata()`; extra args -> `Cannot <op>: too many arguments (expected N, got M)`), and a new `### Added` subsection in `[0.13.0]` (insert before `### Fixed` at line 73) for file expression methods. No planning IDs, no version bump.

---

### `src/AGENTS.md` / `AGENTS.md`

Replace `src/AGENTS.md:697-701` sentence (`binop<Op>(name)` ... `to_expression(o, operation)` names the operation ...) with the overload + `operand_error` description; update layout bullet `:661-666` (file methods attached by `bind_expression` via indexer) and `:720` (`expr:save`). Root AGENTS.md: `expr:metadata()` -> `expr:get_metadata()` in the Lua expression paragraph. Rest is Phase 9.

---

### Benchmark / warnings (no new files)

Reuse: `bash build/abstract-check/perf/bench.sh phase7=build/perf-phase7/quiver_cli.exe tag=build/perf-phase8-tag/quiver_cli.exe traits=build/release/bin/quiver_cli.exe`; `build/abstract-check/release_warnings.sh` (baseline 0 lines); `build/abstract-check/eq.sh` (`[true,true,false]`); `build/layout-check/golden.sh` (recapture with `GOLDEN_CHANGE=1` / `--capture`). Baseline: write 2174 ms, read 2082 ms.

## Shared Patterns

### Operand/arity error text
**Source:** `src/lua_runner/internal.h:208-218` (`lua_type_error`)
**Apply to:** every fallback in expression.cpp. Expected string `"an expression or a binary file"`, what `"operand"`. Too-many text is Pattern 1: `"Cannot " + op + ": too many arguments (expected N, got M)"`.

### NOLINT pair
**Source:** `expression.cpp:87-88` / `:174`. Keep wrapping all by-value sol2 lambdas.

### Sync-test receivers
Only `ns.set_function("x"` / `bind.set_function("x"`; usertype method names one quoted name per line after `new_usertype<T>`; shared lambdas defined above `new_usertype<Expression>`.

### Sandbox
`resolve_sandboxed_path(db, "save", path)` is the single gate for both `f:save` and `e:save`.

## No Analog Found

None. All mechanisms (traits macros, `sol::variadic_args` fallback) are new to the repo but have a compiled skeleton in 08-RESEARCH.md "Code Examples" and Pattern 1; use those.

## Metadata

**Analog search scope:** `src/lua_runner/`, `tests/test_lua_expression.cpp`, `tests/test_lua_runner.h`, `bindings/js/src/lua-api.ts`, `CHANGELOG.md`, `src/AGENTS.md`
**Files scanned:** 8
**Pattern extraction date:** 2026-10-04
