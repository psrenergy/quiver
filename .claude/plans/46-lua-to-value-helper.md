# 46 — Lua: one `lua_to_value` converter replaces four dispatch chains

**Batch** 5 · **Severity** medium · **Breaking** no (every error message stays byte-identical) · **Size** S · **Layers** C++ Lua runner (`src/lua_runner.cpp`), `src/AGENTS.md`
**Depends on** none · **Overlaps with** 47 (next plan: renames the `caller` strings these call sites pass; land 46 first), 44 (adds a key check in `collect_group_columns`, next to `columns_to_cpp_rows`), 48 (other decoders in the same file)

## Why

`src/lua_runner.cpp` converts a Lua value to a `quiver::Value` with the same five-way chain (`nil`
→ NULL, boolean → INTEGER 1/0, `int64_t`, `double`, `std::string`, or throw) written out **four
times**:

1. `lua_table_to_value_map` (currently ~L1400-1427, used by `upsert_time_series_row`):
   ```cpp
            if (val.is<sol::lua_nil_t>()) {
                result[key] = nullptr;
            } else if (is_lua_boolean(val)) {
                result[key] = val.as<bool>() ? int64_t{1} : int64_t{0};
            } else if (val.is<int64_t>()) {
                result[key] = val.as<int64_t>();
            } else if (val.is<double>()) {
                result[key] = val.as<double>();
            } else if (val.is<std::string>()) {
                result[key] = val.as<std::string>();
            } else {
                throw std::runtime_error("Cannot lua_table_to_value_map: column '" + key +
                                         "' has unsupported Lua type");
            }
   ```
2. `table_to_element`'s scalar branch (~L1456-1469), which ends with
   `throw std::runtime_error("Cannot table_to_element: attribute '" + k + "' has unsupported Lua type");`.
3. `lua_table_to_values` (~L1713-1735, used by the query methods), which ends with
   `"Cannot lua_table_to_values: parameter #" + std::to_string(i) + " has unsupported Lua type"`.
4. `columns_to_cpp_rows` (~L2028-2050, used by the six group writers), which ends with
   `"Cannot " + caller + ": column '" + column.name + "' has unsupported Lua type"`.

All four throws share the shape of the existing checked converter `lua_cell_as<T>` (~L1361):
`"Cannot " + caller + ": " + what + " has unsupported Lua type"`. The boolean-as-1/0 policy, which
root AGENTS.md calls "A Lua boolean is INTEGER 1/0 on every write path", is therefore restated in
four places.

Principles: simplicity and readability. One converter states the policy once.

## Constraints and decisions

- **Maintainer notes (binding):**
  - Every message stays byte-for-byte the same. No test changes.
  - Build each "what" string once per column, outside the cell loops.
  - Land before plan 47, which changes the `caller` strings these sites pass.
- Keep `lua_cell_as<T>` for the typed paths (arrays, dimensions, file paths). `lua_to_value` is its
  `Value`-typed sibling: same arguments, same message shape.
- Do not add a public `Element::set(name, const Value&)` just for this. `table_to_element` keeps a
  small `std::visit`.
- Keep `table_to_element`'s **array** branch unchanged. Only the scalar branch is one of the four
  chains.
- `pairs` never yields a `nil` value. The `nil` arm is still needed for `lua_table_to_values`, where
  an interior `nil` is read by index.

## Changes — `src/lua_runner.cpp`

### 1. Add `lua_to_value` next to `lua_cell_as` (after it, ~L1361-1390)

```cpp
    // The one Lua-value -> Value conversion: nil -> NULL, boolean -> INTEGER 1/0 (the cross-layer
    // write policy), then int64, double, string; anything else is Pattern 1 naming the slot. The
    // Value-typed sibling of lua_cell_as<T>, with the same arguments and message shape.
    static Value lua_to_value(const sol::object& v, const std::string& caller, const std::string& what) {
        if (v.is<sol::lua_nil_t>()) {
            return nullptr;
        }
        if (is_lua_boolean(v)) {
            return v.as<bool>() ? int64_t{1} : int64_t{0};
        }
        if (v.is<int64_t>()) {
            return v.as<int64_t>();
        }
        if (v.is<double>()) {
            return v.as<double>();
        }
        if (v.is<std::string>()) {
            return v.as<std::string>();
        }
        throw std::runtime_error("Cannot " + caller + ": " + what + " has unsupported Lua type");
    }
```
The `is<int64_t>()`-before-`is<double>()` order is load-bearing: `SOL_SAFE_NUMERICS=1` makes
`is<int64_t>()` false for a Lua float (see `src/AGENTS.md`). Keep it.

### 2. `lua_table_to_value_map`

Replace the if/else chain inside the loop with:
```cpp
        for (auto& pair : t) {
            auto key = pair.first.as<std::string>();
            result[key] = lua_to_value(pair.second, "lua_table_to_value_map", "column '" + key + "'");
        }
```
Keep the function's comment about alphabetical column order. Delete the "Surface typos..." comment
inside the removed `else`. `lua_to_value` states the policy.

### 3. `table_to_element` — scalar branch only

Replace the chain from `} else if (is_lua_boolean(val)) {` through the final
`throw std::runtime_error("Cannot table_to_element: attribute '" ...);` with:
```cpp
            } else {
                std::visit(
                    [&](auto&& x) {
                        if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::nullptr_t>) {
                            element.set_null(k);
                        } else {
                            element.set(k, x);
                        }
                    },
                    lua_to_value(val, "table_to_element", "attribute '" + k + "'"));
            }
```
The branch before it (`if (val.is<sol::table>()) { ...array handling... }`) is unchanged, so this
`else` only sees non-table values. Check that `Element::set` has overloads for `int64_t`, `double`
and `std::string` (`include/quiver/element.h`). It does: `set(name, int64_t)`, `set(name, double)`,
`set(name, const std::string&)`. Also check that `element.set_null(k)` exists
(`grep -n set_null include/quiver/element.h`). The `nullptr` arm cannot be reached through `pairs`,
but the visitor must still compile for every alternative of `Value`. Check that `<type_traits>` and
`<variant>` are already included, via `value.h` or directly.

### 4. `lua_table_to_values`

```cpp
    static std::vector<Value> lua_table_to_values(const sol::table& parameters) {
        std::vector<Value> values;
        for (size_t i = 1; i <= parameters.size(); ++i) {
            // A skipped parameter would shift every later placeholder, so anything unsupported throws.
            values.push_back(lua_to_value(parameters[i], "lua_table_to_values", "parameter #" + std::to_string(i)));
        }
        return values;
    }
```
`parameters[i]` is a proxy. If `lua_to_value(const sol::object&, ...)` does not accept it
implicitly, write `sol::object val = parameters[i];` first, as the current code does.

### 5. `columns_to_cpp_rows`

```cpp
        for (const auto& column : lua_columns) {
            for (auto& row : cpp_rows) {
                row[column.name] = nullptr;
            }
            const std::string what = "column '" + column.name + "'";
            for (auto& cell : column.values) {
                const auto index = static_cast<size_t>(cell.first.as<int64_t>());
                cpp_rows[index - 1][column.name] = lua_to_value(cell.second, caller, what);
            }
        }
```
The existing chain has no `nil` arm, because `pairs` never yields `nil`. `lua_to_value`'s `nil` arm
therefore changes nothing here.

### 6. Comment at the top of the boolean-policy block

The comment near `is_lua_boolean` (~L1349-1350) lists where the boolean test is used. Update it to
say the `Value` mapping itself lives in `lua_to_value` (scalars, row upserts, query parameters,
group cells) and in `lua_cell_as` (typed arrays).

## Tests

No new tests: behaviour and messages are unchanged. These existing tests pin each message and must
pass unchanged:
- `tests/test_lua_runner_create.cpp` (~L148, ~L162): `table_to_element` attribute/array messages.
- `tests/test_c_api_lua_runner.cpp` (~L497).
- `tests/test_lua_runner_errors.cpp` (~L314).
- The boolean write tests (`grep -n "true\|false" tests/test_lua_runner_create.cpp | head`) and the
  query/upsert/group-writer suites.

Run them on Debug **and** Release. The float/int dispatch depends on sol2 safety macros that
differ by build type.

## Docs and changelog

- `src/AGENTS.md`, LuaRunner conventions:
  - Bullet "**A Lua boolean is INTEGER 1/0 on every write path** ... Every boolean test goes
    through the one predicate `is_lua_boolean`, used by `table_to_element` ..., `lua_table_to_value_map`
    ..., `lua_table_to_values` ..., `columns_to_cpp_rows` ..., and `lua_table_to_vector`". Change it
    to: "... through `is_lua_boolean`, inside the two converters: `lua_to_value` (the `Value`-typed
    one, behind `table_to_element`'s scalars, `lua_table_to_value_map`, `lua_table_to_values` and
    `columns_to_cpp_rows`) and `lua_cell_as<T>` (typed arrays via `lua_table_to_vector`)."
  - Bullet "**`lua_cell_as<T>(object, caller, what)` is the one checked Lua-value→C++
    conversion**". Add: "Its `Value`-typed sibling is `lua_to_value(object, caller, what)`, with the
    same message shape."
- No CHANGELOG entry (internal refactor, identical behaviour).

## Verification

From the repo root:
1. `cmake --build build --config Debug`
2. `./build/bin/quiver_tests.exe --gtest_filter=LuaRunner*`
3. `./build/bin/quiver_c_tests.exe --gtest_filter=*LuaRunner*`
4. Release tree (configure once as in tests/AGENTS.md):
   `cmake --build build-release --config Release` then
   `./build-release/bin/quiver_tests.exe --gtest_filter=LuaRunner*`
5. `bindings/js/test/test.bat test/lua-api-sync.test.ts` (it parses lua_runner.cpp)
6. `scripts/format.bat`

## Acceptance criteria

- [ ] `lua_to_value` exists. The four chains are gone (`grep -c "has unsupported Lua type" src/lua_runner.cpp`
      drops by three, leaving `lua_to_value`, `lua_cell_as` and any unrelated sites).
- [ ] All Lua suites pass on Debug and Release with unchanged messages.
- [ ] `src/AGENTS.md` bullets are updated.

## Pitfalls

- Keep the `int64_t`-before-`double` order.
- `table_to_element`'s array branch still throws its own "array '...' has unsupported element type"
  message. Do not route it through `lua_to_value`.

## Out of scope

- Renaming the internal `caller` strings to public method names (plan 47).
- `lua_table_to_values`' `t.size()` bound (plan 44 decision: leave it).
