# 51 — Lua: register the eight operator metamethods once for `BinaryFile` and `Expression`

**Batch** 5 · **Severity** low · **Breaking** no (identical Lua behaviour) · **Size** S · **Layers** C++ Lua runner only
**Depends on** none · **Overlaps with** 16 (merges the two aggregation-op string parsers in the same area — the parser half of the original finding belongs to 16, not here), 45 (changes `db:open_file`'s holder to `shared_ptr<BinaryFile>`; the `BinaryFile` usertype itself is unchanged), 48 (rewrites `rename_agents` inside the `Expression` usertype)

## Why

`src/lua_runner.cpp` binds the same eight operator metamethods twice, verbatim: once in
`lua.new_usertype<BinaryFile>(...)` (currently ~L904, pairs at ~L926-942) and once in
`lua.new_usertype<Expression>(...)` (~L961, pairs at ~L990-1006):

```cpp
            sol::meta_function::addition,
            [](sol::object a, sol::object b) { return binop_dispatch(BinOp::Add, a, b); },
            sol::meta_function::subtraction,
            [](sol::object a, sol::object b) { return binop_dispatch(BinOp::Subtract, a, b); },
            sol::meta_function::multiplication,
            [](sol::object a, sol::object b) { return binop_dispatch(BinOp::Multiply, a, b); },
            sol::meta_function::division,
            [](sol::object a, sol::object b) { return binop_dispatch(BinOp::Divide, a, b); },
            sol::meta_function::unary_minus,
            [](sol::object a, sol::object) { return -to_expression(a); },
            // Logical ops (nonzero = true, NaN propagates, unitless): `&` / `|` / `~`.
            sol::meta_function::bitwise_and,
            [](sol::object a, sol::object b) { return binop_dispatch(BinOp::And, a, b); },
            sol::meta_function::bitwise_or,
            [](sol::object a, sol::object b) { return binop_dispatch(BinOp::Or, a, b); },
            sol::meta_function::bitwise_not,
            [](sol::object a, sol::object) { return !to_expression(a); });
```

Adding an operator (as `&`/`|`/`~` were added) means editing both lists in sync.

Principle: delete duplication. There is one operator table for "anything that behaves like an
expression".

## Constraints and decisions

- Only the operator half of the original finding. The duplicated aggregation-op parser is plan 16.
- `bindings/js/test/lua-api-sync.test.ts` parses `new_usertype<T>(` blocks and collects bare
  `"name",` lines as methods (Pass 2, ~L20-46). Moving the metamethod pairs out of the variadic
  list does not change any `"name",` line, so the sync test is unaffected. The metamethods are
  `sol::meta_function::...` entries, not names.
- sol2 v3 supports assigning metamethods to a usertype after creation:
  `ut[sol::meta_function::addition] = f;`.

## Changes — `src/lua_runner.cpp`

1. Add one helper in the same scope as `binop_dispatch` / `to_expression`, so it sees both. Put it
   just before the function that creates the two usertypes:
   ```cpp
    // The arithmetic, unary-minus and logical metamethods shared by every usertype that behaves
    // like an expression (BinaryFile auto-wraps to Expression). One table, so a new operator is
    // added once.
    template <typename T>
    static void bind_expression_operators(sol::usertype<T>& type) {
        type[sol::meta_function::addition] = [](sol::object a, sol::object b) { return binop_dispatch(BinOp::Add, a, b); };
        type[sol::meta_function::subtraction] = [](sol::object a, sol::object b) {
            return binop_dispatch(BinOp::Subtract, a, b);
        };
        type[sol::meta_function::multiplication] = [](sol::object a, sol::object b) {
            return binop_dispatch(BinOp::Multiply, a, b);
        };
        type[sol::meta_function::division] = [](sol::object a, sol::object b) { return binop_dispatch(BinOp::Divide, a, b); };
        type[sol::meta_function::unary_minus] = [](sol::object a, sol::object) { return -to_expression(a); };
        // Logical ops (nonzero = true, NaN propagates, unitless): `&` / `|` / `~`.
        type[sol::meta_function::bitwise_and] = [](sol::object a, sol::object b) { return binop_dispatch(BinOp::And, a, b); };
        type[sol::meta_function::bitwise_or] = [](sol::object a, sol::object b) { return binop_dispatch(BinOp::Or, a, b); };
        type[sol::meta_function::bitwise_not] = [](sol::object a, sol::object) { return !to_expression(a); };
    }
   ```
   If `binop_dispatch` / `to_expression` / `BinOp` are members of `Impl` (not free functions), make
   this a `static` member template in the same place. Check with
   `grep -n "binop_dispatch\|to_expression(sol\|enum class BinOp" src/lua_runner.cpp | head`.

2. In the `BinaryFile` registration: delete the eight `sol::meta_function::*` pairs and the
   "Arithmetic on files mirrors Julia" comment above them. Close the `new_usertype` call after the
   last method pair (`"get_file_path", [](BinaryFile& self) ...`), capture the result, and bind the
   operators:
   ```cpp
        auto binary_file_type = lua.new_usertype<BinaryFile>(
            ...all current method pairs, unchanged, ending with the get_file_path lambda...);
        // Arithmetic on files mirrors Julia: file_a + file_b, -file, file * 2.0 (auto-wrap to Expression)
        bind_expression_operators(binary_file_type);
   ```
3. In the `Expression` registration: delete the same eight pairs, capture the usertype, and call
   `bind_expression_operators(expression_type);`.

The method pairs, `sol::no_constructor` and ordering stay exactly as they are. Only the eight
metamethod pairs move.

## Tests

No behaviour change. The operators are covered by `tests/test_lua_expression.cpp` (arithmetic on
files and expressions, scalar on either side, `&`/`|`/`~`, unary minus). Check coverage with
`grep -n "&\|~\| - \|\*" tests/test_lua_expression.cpp | head`. If `BinaryFile`-side operators
(`file_a + file_b`, `-file`) are not exercised directly, add one test there using the fixture's
two-file setup:
```cpp
// file-level operators still dispatch after moving the metamethods out of the usertype list
lua.run(R"(
    local a = db:open_file('a', 'r'); local b = db:open_file('b', 'r')
    local e = (a + b) * 2.0 - (-a)
    e = (a & b) | ~a
    a:close(); b:close()
)");
```
Adapt the file names to what the fixture writes.

## Docs and changelog

None. Neither `src/AGENTS.md` nor lua-api.ts documents the registration mechanics.

## Verification

From the repo root:
1. `cmake --build build --config Debug`
2. `./build/bin/quiver_tests.exe --gtest_filter=LuaExpression*:LuaBinary*`
3. `bindings/js/test/test.bat test/lua-api-sync.test.ts` (must still find every `BinaryFile`/`Expression` method)
4. `bindings/julia/test/test.bat test_lua_runner.jl`
5. `scripts/format.bat`

## Acceptance criteria

- [x] `grep -c "sol::meta_function::addition" src/lua_runner.cpp` prints 1.
- [x] The Lua expression/binary suites and lua-api-sync pass.

## Pitfalls

- `sol::usertype<T>` is the return type of `new_usertype<T>`. Capture it by value (`auto x = ...`);
  it is a handle into the Lua registry.
- Keep the capture-less lambdas. `binop_dispatch` takes `sol::object`, so the templates compile for
  both `T`.

## Out of scope

- The aggregation-op parser duplication (plan 16).
- Adding operators.

## Implementation notes

Implemented on `rs/plan51` at `0da36ae`. Plans 43–50 were already merged there, so `git merge origin/master` was a no-op.

### What changed

- `src/lua_runner.cpp`: the eight `sol::meta_function::*` pairs are gone from both `new_usertype` lists. `bind_binary()` now captures `auto binary_file_type = lua.new_usertype<BinaryFile>(...)` and `bind_expression()` captures `auto expression_type = lua.new_usertype<Expression>(...)`. Each then calls the new `bind_expression_operators(...)`, which registers the eight metamethods once. The "Arithmetic on files mirrors Julia" comment now sits above the `BinaryFile` call. The method pairs, `sol::no_constructor` and their order are unchanged.
- `tests/test_lua_expression.cpp`: one new test, `LuaExpressionTest.OperatorMetamethodsOnFileAndExpression`.
- No CHANGELOG or AGENTS.md edits, as the plan says. `src/AGENTS.md`'s "`__band`/`__bor`/`__bnot` metamethods on the Expression and BinaryFile usertypes" is still true.

### Why sol2 behaves identically (checked in `build/_deps/sol2-src/include/sol`)

`new_usertype(key, args...)` registers the automagic enrollments first, then calls `ut.tuple_set(...)`, which calls `basic_usertype::set(key, value)` for each pair (`table.hpp` L63-81, `usertype.hpp` L48). `ut[meta_function::x] = f` goes through `usertype_proxy::operator=` → `tbl.set(key, value)`, the same `usertype_storage::set`. None of the automagic flags (`types.hpp` L1490) is an arithmetic or bitwise op, so running after creation cannot conflict with them.

### Drift from the plan

1. **Helper placement.** The plan says "just before the function that creates the two usertypes", but they are created in two functions (`bind_binary`, `bind_expression`). `BinOp`, `to_expression` and `binop_dispatch` are static members of `LuaRunner::Impl`, so `bind_expression_operators` is a `static` member template. It sits right after `binop_dispatch`, in the section headed "Expression operator dispatch (shared by Expression and BinaryFile metamethods)". Member bodies are complete-class context, so it can sit after its callers.
2. **NOLINT block.** The helper's lambdas take `sol::object` by value, like every other sol2 lambda in the file. It is wrapped in the file's usual `// NOLINTBEGIN(performance-unnecessary-value-parameter) ...` / `NOLINTEND` pair.
3. **Test.** The plan's sample test (`(a + b) * 2.0 - (-a)`, `(a & b) | ~a`) asserted no values and never used `/`. Before this plan, Lua `/` and unary `-` had no test on either usertype, and `- * /` had none with a file as the left operand. The new test loops over `{file = fa, expression = quiver.expression(fa)}` and checks all eight operators with values. Each usertype is the **left** operand against a number, because Lua tries the left operand's metamethod first; otherwise one usertype's table could stand in for the other's.
4. **JS verification command.** `bindings/js/test/test.bat test/lua-api-sync.test.ts` expands to `bun test test test/lua-api-sync.test.ts`, and the bare `test` filter matches every file, so it runs the whole JS suite. `cd bindings/js && bun test test/lua-api-sync.test.ts` runs the sync test alone. Both were run.

### Results

- **Baseline** (test added, source untouched): `quiver_tests.exe --gtest_filter=LuaExpression*:LuaBinary*` → **50/50 passed**, the new test included. This is a refactor, so the test must pass before the change.
- **After the refactor:** same filter **50/50**. Full `quiver_tests.exe` **1391/1391**. `quiver_c_tests.exe` **571/571**.
- **Mutation checks:**
  - Commenting out `bind_expression_operators(binary_file_type);` fails `FilePlusFile`, `LogicalOperators` and the new test with `attempt to perform arithmetic on a sol.sol::d::u<quiver::BinaryFile> value (local 'x')`.
  - Commenting out `bind_expression_operators(expression_type);` fails 8 tests, the new one included.
  - Both were restored and rebuilt, and the full suites above ran on the restored code.
- **JS:** `bindings/js/test/test.bat test/lua-api-sync.test.ts` → **242 pass / 0 fail** (the whole suite, see drift 4). `bun test test/lua-api-sync.test.ts` → **6/6**, re-run after formatting.
- **Julia:** `bindings/julia/test/test.bat test_lua_runner.jl` → LuaRunner **24/24**. It must be run from PowerShell or cmd. Through Git Bash's `cmd //c` it printed "The system cannot find the path specified."
- **`grep -c "sol::meta_function::addition" src/lua_runner.cpp`** → `1`.
- **`scripts\format.bat`** → exit 0.
  - clang-format only rewrapped the new helper's lambdas.
  - Biome again rewrote 43 JS files CRLF→LF with an empty `git diff --ignore-cr-at-eol`, and they were reverted with `git checkout -- bindings/js`.
  - uv's first Python build failed with a transient `Failed to update Windows PE resources` (uv trampoline), then built and passed on its own (ruff: 35 files unchanged).
  - No `.bat` file was touched.

### For later plans

- **The file's NOLINT spelling is wrong, and this plan copied it.** Every `NOLINTBEGIN/END(performance-unnecessary-value-parameter)` in `src/lua_runner.cpp` names a check that does not exist. The real one is `performance-unnecessary-value-param`, and clangd still reports it inside those blocks, e.g. `sol::object options` at the `read_csv*` lambdas. The new block uses the same spelling on purpose, so a later file-wide replace fixes all of them at once. No plan covers it (plan 79 is about `tidy.bat` itself), so it is left for the maintainer. Correcting the name would turn the blocks on, so the other lambdas' suppressed lint should be checked first.
- `bind_expression_operators` is now the one place to add a Lua operator metamethod. The comparison free functions (`quiver.gt/...`) and the `Expression`/`BinaryFile` method lists are unchanged.
