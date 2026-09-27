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

None. Neither `src/CLAUDE.md` nor lua-api.ts documents the registration mechanics.

## Verification

From the repo root:
1. `cmake --build build --config Debug`
2. `./build/bin/quiver_tests.exe --gtest_filter=LuaExpression*:LuaBinary*`
3. `bindings/js/test/test.bat test/lua-api-sync.test.ts` (must still find every `BinaryFile`/`Expression` method)
4. `bindings/julia/test/test.bat test_lua_runner.jl`
5. `scripts/format.bat`

## Acceptance criteria

- [ ] `grep -c "sol::meta_function::addition" src/lua_runner.cpp` prints 1.
- [ ] The Lua expression/binary suites and lua-api-sync pass.

## Pitfalls

- `sol::usertype<T>` is the return type of `new_usertype<T>`. Capture it by value (`auto x = ...`);
  it is a handle into the Lua registry.
- Keep the capture-less lambdas. `binop_dispatch` takes `sol::object`, so the templates compile for
  both `T`.

## Out of scope

- The aggregation-op parser duplication (plan 16).
- Adding operators.
