# 48 — Lua: strict option decoders for `export_csv`/`import_csv`, `quiver.metadata` and `rename_agents`

**Batch** 5 · **Severity** medium · **Breaking** yes — a script with a misspelled or wrong-typed key in these tables now throws instead of silently getting defaults · **Size** M · **Layers** C++ Lua runner, C++ Lua tests (Debug + Release), `bindings/js/src/lua-api.ts`, `src/CLAUDE.md`, CHANGELOG
**Depends on** none · **Overlaps with** 16 (aggregation parser near `rename_agents`), 51 (operator tables next to `rename_agents`), 46/47 (other converters in the same file), 43/44 (lua-api.ts CSV and metadata sections), 11 (`BinaryMetadata::from_element` length checks; this plan validates the Lua side before calling it)

## Why

`src/lua_runner.cpp` has strict decoders for `db:read_csv`/`db:read_csv_stream`/`db:write_csv`.
They share `csv_options_entries(options, operation, allowed)` (currently ~L433-465), which collects
every entry first and then rejects any non-string key ("option key must be a string") and any
unknown key ("unknown option 'x'"). Their comments explain why: sol2's optional getters never raise
on a type mismatch, and the unchecked getters are silent in Release (`SOL_SAFE_GETTER` is off).

Three decoders in the same file skip that treatment.

1. **`parse_csv_options(sol::optional<sol::table>)`** (~L1241-1263), used by `db:export_csv` and
   `db:import_csv` (~L596-618):
   ```cpp
        if (auto fmt = t.get<sol::optional<std::string>>("date_time_format")) {
            options.date_time_format = *fmt;
        }
        if (auto enums = t.get<sol::optional<sol::table>>("enum_labels")) {
            enums->for_each([&](sol::object attr_key, sol::object attr_value) {
                auto attr_name = attr_key.as<std::string>();
                ...
                    locale_value.as<sol::table>().for_each(
                        [&](sol::object k, sol::object v) { label_map[k.as<std::string>()] = v.as<int64_t>(); });
   ```
   - `{ date_format = "%Y" }` (typo) is ignored.
   - `date_time_format = 5` is ignored.
   - A positional or non-table options argument is treated as "no options", because the parameter
     is `sol::optional<sol::table>`.
   - Inside `enum_labels`, a non-table level or a non-integer code goes through unchecked
     `as<T>()`: a silent 0/"" in Release, a raw sol2 panic in Debug. These throws also happen
     inside `for_each` lambdas, which the collect-then-validate rule forbids.
2. **`build_metadata_from_lua`** (~L1062-1073), behind `quiver.metadata{...}`. It reads eight keys
   through `lua_opt_string` / `lua_opt_string_vector` / `lua_opt_int64_vector` (~L1043-1059), which
   use `t.get<sol::optional<...>>(key)`:
   - An unknown key such as `dimension_size = {3}` is ignored.
   - `unit = 5` silently becomes the default `""`.
   - `labels = "v1"`, a string instead of a table, silently becomes an empty vector.
3. **`rename_agents`** (~L983-989):
   ```cpp
                for (auto& kv : mapping) {
                    pairs.emplace_back(kv.first.as<std::string>(), kv.second.as<std::string>());
                }
   ```
   Both halves are unchecked `as<std::string>()`, so `{ v1 = true }` is a Release-silent empty
   string.

Principle: consistency within one file, and loud failure at a trust boundary. A script is
untrusted input; see `src/CLAUDE.md` "Three guards in the Lua layer's decoders".

## Constraints and decisions

- **Maintainer notes (binding):**
  - BREAKING, with a CHANGELOG entry.
  - Keep the collect-then-validate rule: never throw from inside a `for_each` lambda.
  - Run the negative tests on a Release build as well (`SOL_SAFE_GETTER` is off there).
  - Update lua-api.ts and the `src/CLAUDE.md` `parse_csv_options` line.
- Reuse `csv_options_entries` for all three top-level tables. Its name says "csv". Renaming it to
  `lua_options_entries` is optional; if you rename it, rename every call site and the
  `src/CLAUDE.md` mention in the same change.
- Use the same messages as the strict CSV decoders: `Cannot <op>: options must be a table`,
  `Cannot <op>: option key must be a string`, `Cannot <op>: unknown option '<k>'`. Add
  `Cannot <op>: option '<k>' must be a <type>` for value types.
- Convert leaf cells with `lua_cell_as<T>(obj, caller, what)`. It maps a boolean to 1/0 for integer
  targets, per the cross-layer boolean write policy.
- Operation names: `export_csv`, `import_csv`, `metadata` (for `quiver.metadata`), `rename_agents`.

## Changes — `src/lua_runner.cpp`

### 1. `db:export_csv` / `db:import_csv` take `sol::object options`

In both bindings (~L596-618), change the last parameter from `sol::optional<sol::table> options_table`
to `sol::object options` and pass `parse_csv_options(options, "export_csv")` /
`parse_csv_options(options, "import_csv")`.

### 2. Rewrite `parse_csv_options`

```cpp
    // Strict decoder for db:export_csv / db:import_csv options, the same collect-then-validate walk
    // as the read_csv/write_csv decoders: nil/missing means defaults; anything else must be a table
    // with only known keys of the right types.
    static CSVOptions parse_csv_options(const sol::object& options, const std::string& operation) {
        CSVOptions result;
        if (!options.valid() || options.get_type() == sol::type::lua_nil) {
            return result;
        }
        if (options.get_type() != sol::type::table) {
            throw std::runtime_error("Cannot " + operation + ": options must be a table");
        }
        const auto found = csv_options_entries(options, operation, {"date_time_format", "enum_labels"});

        if (const auto& fmt = found[0]) {
            if (fmt->get_type() != sol::type::string) {
                throw std::runtime_error("Cannot " + operation + ": option 'date_time_format' must be a string");
            }
            result.date_time_format = fmt->as<std::string>();
        }
        if (const auto& enums = found[1]) {
            // attribute -> locale -> { label = code }: every level collected before it is checked.
            for (const auto& [attr_key, attr_value] : table_entries(*enums, operation, "enum_labels")) {
                const auto attr = string_key(attr_key, operation, "enum_labels");
                auto& locales = result.enum_labels[attr];
                for (const auto& [loc_key, loc_value] :
                     table_entries(attr_value, operation, "enum_labels['" + attr + "']")) {
                    const auto locale = string_key(loc_key, operation, "enum_labels['" + attr + "']");
                    auto& labels = locales[locale];
                    const auto where = "enum_labels['" + attr + "']['" + locale + "']";
                    for (const auto& [label_key, code] : table_entries(loc_value, operation, where)) {
                        const auto label = string_key(label_key, operation, where);
                        labels[label] = lua_cell_as<int64_t>(code, operation, "code for label '" + label + "'");
                    }
                }
            }
        }
        return result;
    }
```

Add the two small helpers it uses, next to `csv_options_entries`:

```cpp
    // Collect a nested table's entries before any is checked (the LUA-09 collect-then-validate
    // rule), after checking the value really is a table.
    static std::vector<std::pair<sol::object, sol::object>>
    table_entries(const sol::object& value, const std::string& operation, const std::string& what) {
        if (value.get_type() != sol::type::table) {
            throw std::runtime_error("Cannot " + operation + ": option '" + what + "' must be a table");
        }
        std::vector<std::pair<sol::object, sol::object>> entries;
        value.as<sol::table>().for_each(
            [&](sol::object k, sol::object v) { entries.emplace_back(std::move(k), std::move(v)); });
        return entries;
    }

    static std::string string_key(const sol::object& key, const std::string& operation, const std::string& what) {
        if (key.get_type() != sol::type::string) {
            throw std::runtime_error("Cannot " + operation + ": keys of option '" + what + "' must be strings");
        }
        return key.as<std::string>();
    }
```
Check `CSVOptions::enum_labels`'s exact nested type in `include/quiver/options.h`, e.g.
`std::map<std::string, std::map<std::string, std::map<std::string, int64_t>>>`, and match it.
Check that `lua_cell_as<int64_t>`'s message shape reads well with
`what = "code for label 'x'"`.

### 3. `quiver.metadata{...}` — `build_metadata_from_lua`

Replace the three `lua_opt_*` helpers and `build_metadata_from_lua` with:

```cpp
    // Strict quiver.metadata{...} decoder: only the eight known keys, each of the right type.
    static BinaryMetadata build_metadata_from_lua(const sol::table& t) {
        static const std::string op = "metadata";
        const auto found = csv_options_entries(
            t, op, {"version", "initial_datetime", "unit", "labels", "dimensions", "dimension_sizes",
                    "time_dimensions", "frequencies"});

        const auto str = [&](size_t i, const char* key, const std::string& fallback) {
            if (!found[i]) {
                return fallback;
            }
            return lua_cell_as<std::string>(*found[i], op, std::string("field '") + key + "'");
        };
        const auto strings = [&](size_t i, const char* key) {
            if (!found[i]) {
                return std::vector<std::string>{};
            }
            if (found[i]->get_type() != sol::type::table) {
                throw std::runtime_error("Cannot " + op + ": field '" + key + "' must be a table");
            }
            return lua_table_to_vector<std::string>(found[i]->as<sol::table>(), op + ": field '" + key + "'");
        };

        Element el;
        el.set("version", str(0, "version", "1"));
        el.set("initial_datetime", str(1, "initial_datetime", ""));
        el.set("unit", str(2, "unit", ""));
        el.set("labels", strings(3, "labels"));
        el.set("dimensions", strings(4, "dimensions"));
        if (found[5]) {
            if (found[5]->get_type() != sol::type::table) {
                throw std::runtime_error("Cannot metadata: field 'dimension_sizes' must be a table");
            }
            el.set("dimension_sizes",
                   lua_table_to_vector<int64_t>(found[5]->as<sol::table>(), "metadata: field 'dimension_sizes'"));
        } else {
            el.set("dimension_sizes", std::vector<int64_t>{});
        }
        el.set("time_dimensions", strings(6, "time_dimensions"));
        el.set("frequencies", strings(7, "frequencies"));
        return BinaryMetadata::from_element(el);
    }
```
Keep the existing `lua_table_to_vector` `caller` strings (`"metadata: field 'x'"`), so messages
for bad cells stay as they are today. Check with `grep -n "metadata: field" tests/test_lua_binary.cpp`.
`csv_options_entries` takes `const sol::object&`; a `sol::table` converts implicitly. If it does
not compile, pass `sol::object(t)`.

### 4. `rename_agents`

```cpp
            "rename_agents",
            [](Expression& self, const sol::table& mapping) {
                std::vector<std::pair<sol::object, sol::object>> entries;
                mapping.for_each([&](sol::object k, sol::object v) { entries.emplace_back(std::move(k), std::move(v)); });
                std::vector<std::pair<std::string, std::string>> pairs;
                for (const auto& [k, v] : entries) {
                    const auto old_name = lua_cell_as<std::string>(k, "rename_agents", "key");
                    pairs.emplace_back(old_name, lua_cell_as<std::string>(v, "rename_agents", "value for '" + old_name + "'"));
                }
                return self.rename_agents(pairs);
            },
```
`lua_cell_as<std::string>` rejects a number. Check that: sol2's `is<std::string>()` is true for a
Lua number, so find out how `lua_cell_as<std::string>` decides by reading it (~L1361). If it
accepts numbers, add an explicit `get_type() == sol::type::string` check here instead, so that
`{ v1 = 5 }` throws.

## Tests (C++ Lua suites; run on Debug **and** Release)

- `tests/test_lua_runner_csv_export.cpp`, using `expect_lua_error` (from `tests/test_lua_runner.h`):
  - `db:export_csv("Collection", "", "out.csv", { date_format = "%Y" })` gives
    `"Cannot export_csv: unknown option 'date_format'"`
  - `db:export_csv("Collection", "", "out.csv", "x")` gives
    `"Cannot export_csv: options must be a table"`
  - `db:export_csv("Collection", "", "out.csv", { date_time_format = 5 })` gives
    `"Cannot export_csv: option 'date_time_format' must be a string"`
- `tests/test_lua_runner_csv_import.cpp`:
  - an `enum_labels` level that is not a table, `{ enum_labels = { status = 1 } }`, gives
    `"Cannot import_csv: option 'enum_labels' must be a table"`. Adjust the expected substring to
    the message your `table_entries` produces for that level.
  - a code that is a string, `{ enum_labels = { status = { en = { active = "one" } } } }`, gives
    the `lua_cell_as<int64_t>` message for `code for label 'active'`
- `tests/test_lua_binary.cpp`:
  - `quiver.metadata{ dimension_size = {3} }` gives `"Cannot metadata: unknown option 'dimension_size'"`
  - `quiver.metadata{ unit = 5, ... }` gives the `lua_cell_as<std::string>` message for `field 'unit'`
  - `quiver.metadata{ labels = "v1", ... }` gives `"Cannot metadata: field 'labels' must be a table"`
- `tests/test_lua_expression.cpp`: `expr:rename_agents({ v1 = true })` gives
  `"Cannot rename_agents: value for 'v1'"`.

Match each file's fixture and sandbox setup, including the paths `export_csv` needs inside the
sandbox. Copy a neighbouring positive test's setup, then change only the options table. Existing
positive tests (valid options, `quiver.metadata` with all keys, `rename_agents` valid) must still
pass.

## Docs and changelog

- `src/CLAUDE.md`: the bullet "`parse_csv_options(table)` is the single CSVOptions parser shared by
  `export_csv`/`import_csv`." becomes "`parse_csv_options(options, operation)` is the single strict
  CSVOptions decoder for `export_csv`/`import_csv`: `nil` means defaults, any other non-table and any
  unknown or wrong-typed key throws, with the same collect-then-validate walk (`csv_options_entries`)
  as the `read_csv`/`write_csv` decoders. `quiver.metadata{...}` and `expr:rename_agents` are decoded
  the same strict way."
- `bindings/js/src/lua-api.ts`:
  - The "CSV import / export" options paragraph: add "An options value that is not a table, an
    unknown key or a wrong-typed value throws (\`Cannot export_csv: unknown option '...'\`)."
  - The `quiver.metadata{...}` kwargs paragraph (`grep -n "quiver.metadata" bindings/js/src/lua-api.ts`):
    add "Only the eight keys above are accepted; an unknown key or a value of the wrong type throws."
  - Escape backticks as `\``.
- `CHANGELOG.md`, under `## [0.11.0] — unreleased` → `### Changed`:
  ```markdown
  - **BREAKING — Lua: `db:export_csv`/`db:import_csv` options, `quiver.metadata{...}` and
    `expr:rename_agents` reject unknown keys and wrong types.** A misspelled key (`date_format`,
    `dimension_size`) or a wrong-typed value (`unit = 5`, `labels = "v1"`, a boolean rename target)
    used to be ignored or silently replaced by a default, and in Release some became empty strings.
    They now throw a `Cannot <op>: ...` error, like the `read_csv`/`write_csv` options already did.
    *Adapt:* fix the key or value the error names.
  ```

## Verification

From the repo root:
1. `cmake --build build --config Debug`
2. `./build/bin/quiver_tests.exe --gtest_filter=LuaRunner*:LuaBinary*:LuaExpression*`
3. Release tree (configure once:
   `cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DQUIVER_BUILD_TESTS=ON -DQUIVER_BUILD_C_API=ON`),
   then `cmake --build build-release` and
   `./build-release/bin/quiver_tests.exe --gtest_filter=LuaRunner*:LuaBinary*:LuaExpression*`
4. `bindings/js/test/test.bat test/lua-api-sync.test.ts`
5. `bindings/julia/test/test.bat test_lua_runner.jl` (Julia runs Lua scripts that call `quiver.metadata`)
6. `scripts/format.bat`

## Acceptance criteria

- [ ] No `sol::optional<sol::table>` options parameter remains on `export_csv`/`import_csv`.
- [ ] No throw happens inside a `for_each` lambda in the three decoders.
- [ ] All negative tests pass on Debug and Release, and the positive tests still pass.
- [ ] lua-api.ts, src/CLAUDE.md and the CHANGELOG are updated.

## Pitfalls

- sol2's `is<std::string>()` is true for numbers, and `as<std::string>()` stringifies them. Type
  checks must use `get_type()`.
- `quiver.metadata` is also called from Julia tests via Lua scripts and from
  `tests/test_lua_expression.cpp` setups. A fixture that passes an extra, previously ignored key will
  now fail; fix the fixture. Find candidates with `grep -rn "quiver.metadata{" tests bindings`.
- Keep `lua_opt_*` only if something else still calls them. Otherwise delete them.

## Out of scope

- `db:read_csv`/`write_csv` decoders (already strict).
- The aggregation-op string parser (plan 16).
