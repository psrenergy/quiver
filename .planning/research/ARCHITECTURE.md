# Architecture Research

**Domain:** Splitting an embedded-Lua (sol2 v3.5.0, Lua 5.4.8) binding layer across translation units in a shipped C++20 library (Quiver), plus a cross-layer rename `LuaRunner` -> `quiver::Sandbox`
**Researched:** 2026-10-02
**Confidence:** HIGH for the sol2 mechanics (checked in the vendored source), MEDIUM for the line budgets (estimated from the cluster ranges in LUA-RUNNER-MAP.md, not measured after a split)

Line references of the form `usertype.hpp:NN` point into `build/_deps/sol2-src/include/sol/`. `lua_runner.cpp:NN` refers to `src/lua_runner.cpp` at commit bdf9087. Where this file disagrees with LUA-RUNNER-MAP.md, the reason is given. That file's critic section was taken as authoritative.

## Standard Architecture

### System Overview

```
┌──────────────────────────────────────────────────────────────────────────┐
│ Hosts: C API quiver_lua_runner_* (src/c/) · CLI · Julia/Dart/Python/JS     │
└──────────────────────────────┬───────────────────────────────────────────┘
                               │ LuaRunner(Database&) / run(script) -> JSON
┌──────────────────────────────┴───────────────────────────────────────────┐
│ src/sandbox/sandbox.cpp   (Pimpl owner, the only file that knows Impl)    │
│   Impl { Database& db; RunHandles handles; sol::state lua; }               │
│   ctor: open_libraries -> nil dofile/loadfile -> quiver table ->           │
│         new_usertype<Database> ONCE -> bind_* (below) -> lua["db"]=&db     │
│   run(): GcGuard -> safe_script -> encode_return_json                      │
├──────────────────────────────────────────────────────────────────────────┤
│ Per-domain binders, each one registers AND implements its slice            │
│ ┌─────────┐┌─────────┐┌──────────┐┌───────────┐┌──────────────┐┌───────┐┌────────┐
│ │db_core  ││db_read  ││db_write  ││db_metadata││db_time_series││csv    ││binary  │
│ │bind&    ││bind&    ││bind&     ││bind&      ││bind&         ││lua,   ││lua,bind│
│ │         ││         ││          ││           ││              ││bind,  ││ns,db,  │
│ │         ││         ││          ││           ││              ││handles││handles │
│ └────┬────┘└────┬────┘└────┬─────┘└─────┬─────┘└──────┬───────┘└───┬───┘└───┬────┘
├──────┴──────────┴──────────┴────────────┴─────────────┴────────────┴────────┴──┤
│ src/sandbox/internal.h  (quiver::sandbox_internal; inline + templates)     │
│   RunHandles · binder declarations · converters (to_lua_table, lua_cell_as,│
│   lua_to_value, table_to_element, ...) · option walk · group-decoder decls │
├──────────────────────────────────────────────────────────────────────────┤
│ sol2-free leaves:  path_policy.cpp (resolve_sandboxed_path)                │
│ sol2 leaf:         return_json.cpp (encoder; internals in an anonymous ns) │
├──────────────────────────────────────────────────────────────────────────┤
│ Core: Database, BinaryFile/CSVConverter/BinaryMetadata, Expression,        │
│       csv_read::Reader / csv_write::Writer (csv-parser stays behind these) │
└──────────────────────────────────────────────────────────────────────────┘
```

### Component Responsibilities

| Component | Responsibility | `db:` / Lua surface it owns | ~LOC at split -> after dedupe |
|-----------|----------------|-----------------------------|-------------------------------|
| `sandbox.cpp` | `LuaRunner::Impl` (later `Sandbox::Impl`), the ctor sequence, `RunHandles` method bodies (`path_has_open_writer`, `close_open_writers`), Pimpl special members, `run()` + `GcGuard` | the `db` global, the stdlib set | ~170 -> ~150 |
| `internal.h` | Everything more than one TU needs: `RunHandles`, the seven binder declarations, `resolve_sandboxed_path` / `encode_return_json` declarations, the Lua<->C++ converters (cluster 17 minus the TS-only pair), the strict-option walk (cluster 7), the columnar-decoder declarations (`GroupColumn`, `collect_group_columns`, `columns_to_cpp_rows`, `join_column_names`), and later `require_table` / `lua_string_key` / `optional_from_lua` | none | ~310 -> ~360 (fixes add helpers) |
| `return_json.cpp` | Cluster 2 as is. Exposes one `std::string encode_return_json(const sol::object&)`. `append_json*` and the caps stay in its anonymous namespace | the return contract | ~215 |
| `path_policy.cpp` | Cluster 15, `resolve_sandboxed_path` (renamed `resolve_contained_path` in the rename phase) | the directory-containment gate | ~80 |
| `db_core.cpp` | Info, transactions, dry runs, element count, describe x3, query x3 (+ `lua_table_to_values`), `validate_migrations`, `export_csv` / `import_csv` + `parse_csv_options` | 22 methods: `is_healthy current_version path begin_transaction commit rollback in_transaction transaction begin_dry_run end_dry_run in_dry_run dry_run number_of_elements describe describe_collection summarize_collection query_string query_integer query_float validate_migrations export_csv import_csv` | ~245 -> ~190 (M1, M3, M5) |
| `db_read.cpp` | Clusters 19, 22, 23 | 14: `read_element_ids`, `read_{scalar,vector,set}_{integers,floats,strings}`, `read_{scalars,vectors,sets}_by_id`, `read_element_by_id` | ~250 -> ~150 (M4, M7) |
| `db_write.cpp` | Clusters 18, 26, 27: element CRUD, relations, vector/set group writers, and the **definition** of the columnar decoder | 11: `create_element update_element update_element_by_label delete_element delete_element_by_label update_relation update_relation_by_label update_{vector,set}_group[_by_label]` | ~250 -> ~230 |
| `db_metadata.cpp` | Cluster 20 + the metadata half of 24. `scalar_metadata_lua` / `group_metadata_lua` stay file-local, since every caller (lua_runner.cpp:1776-2108) is a metadata wrapper | 8: `get_{scalar,vector,set,time_series}_metadata`, `list_scalar_attributes list_vector_groups list_set_groups list_time_series_groups` | ~165 -> ~110 (M8) |
| `db_time_series.cpp` | Clusters 25, 28, 29, 30, plus `value_to_lua_object` and `lua_table_to_value_map`, which are file-local: their only callers are 2134, 2153, 2433 and 2447 | 10: `read_time_series_group read_time_series_row update_time_series_group[_by_label] upsert_time_series_row[_by_label] has_time_series_files list_time_series_files_columns read_time_series_files update_time_series_files` | ~290 -> ~250 |
| `csv.cpp` | Lua-only CSV I/O: clusters 4, 6, 8, the read half of 16, 9d and 9e. `CsvWriter` and `new_usertype<CsvWriter>` | 3 + 2: `read_csv read_csv_stream write_csv`, `w:write_row w:close` | **~445** -> ~380 (M9-M12) |
| `binary.cpp` | Clusters 10-14 plus 9c: the BinaryMetadata/BinaryFile/Expression usertypes, `bind_expression_operators<T>`, the `quiver.*` free functions | 3 `db:` (`open_file bin_to_csv csv_to_bin`) + 3 usertypes + 15 `quiver.*` | ~400 -> ~360 (M14, M15) |

The total is 71 `db:` methods: 22 + 14 + 11 + 8 + 10 + 3 + 3. That matches the 17 inline pairs plus the 54 `bind.set_function` calls the critic counted (LUA-RUNNER-MAP.md, correction 1).

## Recommended Project Structure

```
src/sandbox/
├── internal.h          # shared vocabulary: RunHandles, binder decls, converters, option walk
├── sandbox.cpp         # Impl, ctor order, RunHandles bodies, run()/GcGuard   (git mv of lua_runner.cpp)
├── return_json.cpp     # run()'s return-value encoder
├── path_policy.cpp     # the single filesystem gate (no sol2 types in its logic)
├── db_core.cpp         # info, transactions, dry runs, describe, query, migrations, export/import_csv
├── db_read.cpp         # bulk + by-id readers
├── db_write.cpp        # element CRUD, relations, vector/set group writers, columnar decoder
├── db_metadata.cpp     # get_*_metadata, list_* groups
├── db_time_series.cpp  # TS read/write/upsert, TS files
├── csv.cpp             # read_csv, read_csv_stream, write_csv, CsvWriter
└── binary.cpp          # BinaryMetadata, BinaryFile, Expression, quiver.*, open_file/bin_to_csv/csv_to_bin
```

Include it as `#include "sandbox/internal.h"`. The `quiver` target's PRIVATE include directory is `src/`, and that is the existing `#include "csv/csv_read.h"` convention.

### Structure Rationale

- **The 11-file layout holds with one content move and no added file** (HIGH for the sizing below; LOW / unverified that it is "the user's" layout: no `.planning` file records an 11-file layout, and PROJECT.md says only "a src/sandbox/ folder of small per-domain files"). The one move: `export_csv` / `import_csv` and their `parse_csv_options` decoder (lua_runner.cpp:666-684 and 1362-1405, about 65 lines) go to **`db_core.cpp`, not `csv.cpp`**.
  - **Evidence:** clusters 4 + 6 + 8 + read-half-16 + 9d + 9e already total ~415 lines before includes (26 + 129 + 56 + 43 + 108 + 52). Adding export/import would put `csv.cpp` at ~500 on day one, over the ~450 ceiling.
  - **It is also the cleaner boundary.** Export/import are `Database` methods decoding `CSVOptions`. `csv.cpp` then means only "the Lua-only CSV I/O that has no counterpart in any other layer", which is the root design decision on `db:read_csv*` / `db:write_csv`.
  - Even so, `csv.cpp` starts closest to the ceiling, so it is the first dedupe target.
- **One internal header, not several** (HIGH).
  - Templates (`to_lua_table` x3, `lua_cell_as<T>`, `lua_table_to_vector<T>`, later `optional_from_lua<T>`) must be visible in every TU that instantiates them.
  - The non-template helpers they call (`is_lua_boolean`, used by `lua_cell_as` at 1503) must be at least declared there.
  - The repo already solves this problem this way for `Database`: `src/database_internal.h` (213 lines) is `namespace quiver::internal` plus `inline` free functions plus templates, shared by seven TUs (six `database_*.cpp` files and `type_validator.cpp`; the other `database_*.cpp` files include only `database_impl.h`). Following that precedent beats inventing a `convert.cpp`.
  - Escape hatch: if `internal.h` passes ~450 after the fixes phase, move the non-template bodies (`lua_to_value`, `table_to_element`, `require_dense_array`, the option walk) into a `convert.cpp` and leave declarations behind. That is not needed at split time (~310).
- **`binary.cpp` keeps binary and expression together** (HIGH).
  - `bind_expression_operators<T>` (1252-1275) is instantiated for both `BinaryFile` (1007) and `Expression` (1075).
  - `to_expression` accepts a `BinaryFile`.
  - `binop_dispatch` / `apply_binop` (cluster 13, 98 lines) serve both.
  - Splitting them would push all of cluster 13 into `internal.h`, which every TU parses, for no locality gain. At ~400 lines the shared TU fits.
- **`path_policy.cpp` stays its own file** even at ~80 lines (MEDIUM).
  - It is the security gate. Its logic uses only `std::filesystem` and `Database::path()`.
  - Isolating it makes its review surface obvious and leaves room for a later direct unit test.
  - It includes `internal.h` for its declaration, which costs one extra `sol.hpp` parse. A separate sol2-free `path_policy.h` would avoid that, but it is not worth an extra header until someone wants the direct unit test.
- **`git mv src/lua_runner.cpp src/sandbox/sandbox.cpp` first, then carve out of it** (HIGH). `git log --follow` then keeps the history of the biggest piece. The class is still `LuaRunner` until the rename phase, and that interim name mismatch is harmless.

## Architectural Patterns

### Pattern 1: Create the `Database` usertype exactly once; pass `sol::usertype<Database>& bind` into every binder

**What:** `sandbox.cpp` calls `auto bind = lua.new_usertype<Database>("Database");` once. Each `bind_*` function takes it by reference and calls `bind.set_function("name", ...)`.

**Why it works (HIGH):**
- `basic_usertype::set` does not write into the C++ object. It looks up `u_detail::maybe_get_usertype_storage<T>(lua_state())`, which reads a **global** (`_G`, not the registry) keyed by `usertype_traits<T>::gc_table()`, i.e. `"sol." + demangle<T>() + ".♻"` (`usertype.hpp:85-90`, `usertype_storage.hpp:857, 911-920`; `get_field<true>` is `lua_getglobal`, `stack_field.hpp:110-113`). Only the metatables (`sol.<T>`, `sol.<T*>`, `sol.<const T>`, …) live in the registry.
- So any `sol::usertype<Database>` reference on the same `lua_State` reaches the same storage, from any TU.
- Because the storage is a `_G` entry, scripts can see it (`pairs(_G)` lists `sol.Database.♻`) and overwrite it. No design note should describe it as hidden registry state.

**Why "exactly once" (HIGH):**
- `register_usertype` begins with "STEP 0: tell the old usertype (if it exists) …" and calls `clear_usertype_storage<T>(L_)` (`usertype_storage.hpp:991-993`). On a first registration that returns early, because no storage exists yet (`:932-940`).
- That wipes the storage and nils every registry metatable name (`clear_usertype_registry_names`, `usertype_storage.hpp:806-826`).
- A second `new_usertype<Database>` in another TU would silently delete every method registered before it.

**The 17 variadic pairs become `bind.set_function` at split time** (HIGH). A variadic `new_usertype<Database>(...)` call cannot span files, so the split forces this part of M2.
- It is behaviour-neutral:
  - The variadic path is `new_usertype(key, enrollments)` followed by `tuple_set`, which calls `this->set(key, value)` (`table.hpp:63-80`, `usertype.hpp:47-50`).
  - `set_function` is `set(key, as_function_reference(fx))` (`usertype.hpp:62-67`).
  - Both land in `usertype_storage<T>::set`. The enrollment defaults are identical: no constructor argument means `default_constructor = true`, `destructor = true` and flags `all` in both routes (`table.hpp:64-69`, `types.hpp:1540-1550`).
- The file already registers `Database` methods both ways with no observed difference.
- Bonus: `Database` stops depending on the sync test's fragile Pass 2.

**Load-bearing names:** the parameter must be literally `bind`, and the `quiver` table literally `ns`. Pass 1 is `/\b(bind|ns)\.set_function\(\s*"name"/`.

```cpp
// sandbox.cpp -- Impl ctor (order is a constraint, see Pattern 4)
auto ns = lua.create_named_table("quiver");
auto bind = lua.new_usertype<Database>("Database");
sandbox_internal::bind_core(bind);
sandbox_internal::bind_read(bind);
sandbox_internal::bind_write(bind);
sandbox_internal::bind_metadata(bind);
sandbox_internal::bind_time_series(bind);
sandbox_internal::bind_csv(lua, bind, handles);
sandbox_internal::bind_binary(lua, bind, ns, db, handles);
lua["db"] = &db;

// db_read.cpp
void bind_read(sol::usertype<Database>& bind) {
    bind.set_function("read_element_ids", &read_element_ids_lua);
    ...
}
```

**Trade-off:**
- The order among the seven binders does not matter: each registers distinct keys, and `set` is per key.
- The two that create usertypes (`csv`, `binary`) only need to run before any script, which every ctor ordering guarantees.
- A key registered twice would be "last one wins" silently. The sync-test guard in Pattern 6 makes accidental duplicates visible in review rather than in code.

### Pattern 2: Named internal namespace + `inline`; anonymous namespaces only inside `.cpp`, never for usertypes

**What:** `internal.h` declares everything in `namespace quiver::sandbox_internal`. Shared non-templates are `inline`; templates are plain templates. File-local helpers in a `.cpp` may use an anonymous namespace. Any type that is given to `new_usertype` or returned to Lua lives in a named namespace.

**Why (HIGH):**
- An anonymous namespace in a header gives each TU its own copy of every function and type. An `inline` function in a named namespace that names one of those copies differs between TUs: an ODR violation, no diagnostic required.
- The sol2-specific reason is sharper: sol2 keys a usertype's storage (a `_G` global) and its registry metatables by the demangled type name. It uses `"sol." + demangle<T>()` (`usertype_traits.hpp:41-54`), and `demangle` **strips** `(anonymous namespace)` / `` `anonymous namespace' `` from the name (`demangle.hpp:39-47`, `113-119`).
- So two distinct anonymous-namespace types with the same qualified spelling share one registry key on one `lua_State`. sol2 then reinterprets one type's storage as the other's.
- Today `CsvWriter` is `LuaRunner::Impl::CsvWriter`, which is unique by nesting. After the split it must be `quiver::sandbox_internal::CsvWriter`.
- No test depends on the demangled name: a grep for `sol.quiver` / `Impl::CsvWriter` finds only two comments, `csv_write.h:55` and `test_lua_runner_write_csv.cpp:1423`, and both get updated.

**Namespace name:** use `quiver::sandbox_internal` (MEDIUM, a naming call). It mirrors `quiver::internal`, and it avoids a `quiver::sandbox` namespace that differs from the class `quiver::Sandbox` only by case.

### Pattern 3: Run-scoped state in a `RunHandles` member of the heap `Impl`; binders capture references to it

**What:** the only per-instance state besides `db` and `lua` is:
- the writer registry (`open_writers`)
- the binary-file registry (`open_binary_files`)
- `path_has_open_writer`, a const member that reads `open_writers` (critic correction 2)
- `close_open_writers`

Move these into one struct and keep it as an `Impl` member:

```cpp
// internal.h
namespace quiver::csv_write { class Writer; }   // weak_ptr<T> accepts an incomplete T
namespace quiver::sandbox_internal {
struct RunHandles {
    std::vector<std::pair<std::string, std::weak_ptr<csv_write::Writer>>> open_writers;
    std::vector<std::weak_ptr<BinaryFile>> open_binary_files;
    bool path_has_open_writer(const std::string& resolved_path) const;  // defined in sandbox.cpp
    void close_open_writers();                                          // defined in sandbox.cpp
};
}

// sandbox.cpp
struct LuaRunner::Impl {
    Database& db;
    sandbox_internal::RunHandles handles;   // BEFORE lua: destroyed after the state's closures
    sol::state lua;
    ...
};
```

The three `[this]` captures become:

| Site | Today | After the split |
|------|-------|-----------------|
| `open_file` (761) | `[this]` | `[&handles]` |
| `write_csv` (876) | `[this]` | `[&handles]` |
| `expr:save` (1032) | `[this]` | `[&db]` |

**Why it survives a move (HIGH):**
- `LuaRunner`'s move special members are defaulted and move only the `unique_ptr<Impl>` (lua_runner.cpp:2490-2497). `Impl`, and with it `handles`, never changes address.
- `db` is a borrowed reference to the host's `Database`, which the runner never moves.
- `[&handles]` / `[&db]` capture a reference *parameter* by reference. Since CWG 2011 (P0613R0, adopted 2017), an odr-use of such a capture refers to the entity the reference is bound to, not to the parameter. So the closures stay valid after `bind_csv` / `bind_binary` return.

**Why `handles` is declared before `lua` (MEDIUM, cheap hardening):**
- Members are destroyed in reverse order, so `sol::state` (via `lua_close`) finalizes all userdata and closures while `handles` is still alive.
- Today the registries are declared after `lua` (lines 288 and 294), which is harmless only because no finalizer touches them.
- This is not observable, so it fits the zero-behaviour split.

**Where definitions live:**
- `close_open_writers` needs complete `csv_write::Writer` and `BinaryFile`, so it is defined in `sandbox.cpp`. `csv_write.h` and `binary_file.h` include no csv-parser (`csv_write.h` includes only `<fstream> <string> <vector>`), so the "no csv-parser in `src/sandbox/`" rule holds.
- `GcGuard` stays in `run()` exactly as is: declared before `result`, `close_open_writers()` then exactly one `collect_garbage()`.

### Pattern 4: Registration order lives in one place and is checked by the sync test

The order (HIGH, all in the `Impl` ctor in `sandbox.cpp`):

1. **`open_libraries(base, string, table, math, coroutine, utf8)` comes first.** It must be the only `open_libraries(` text in the folder, because the sync test extracts its argument list.
2. **`lua["dofile"] = lua["loadfile"] = sol::lua_nil` comes after `open_libraries`,** because `base` defines them.
3. **`create_named_table("quiver")` comes before `bind_binary`,** which registers `ns.set_function(...)` on it.
4. **`new_usertype<Database>` runs exactly once, before any `bind_*`** (they need the `bind` handle). `lua["db"] = &db` stays last by convention.
   - Pushing a `Database*` before the usertype exists runs `undefined_metatable`, which `luaL_newmetatable`s a bare metatable (`stack_push.hpp:211-213`, `stack_core.hpp:789-793`).
   - That is harmless before the first and only `new_usertype<Database>`: STEP 0's `clear_usertype_storage<T>` returns early when no storage exists (`usertype_storage.hpp:932-940`), and `register_usertype` then fills the same registry metatable in place through `luaL_newmetatable` (`:1052`), so `db`'s methods resolve. This was checked by compile-and-run against the vendored sol2/Lua.
   - The real hazard is a **second** `new_usertype<Database>` after the push. It clears the storage and the registry names (Pitfall 5, Anti-Pattern 1).
   - Keeping `lua["db"] = &db` last is still a fine convention, because the ctor is then one ordered list to review. It is not a guard against an orphaned metatable.
5. **The seven `bind_*` calls can run in any order relative to each other** (Pattern 1). Keep the order shown above anyway: it matches the reading order of the reference doc, and the ctor is the one place to review it.

### Pattern 5: Where `CsvWriter` must be complete

`write_csv` returns `std::unique_ptr<CsvWriter>` (876). Registering that lambda instantiates sol2's unique-usertype pusher and deleter for `CsvWriter`, so the type must be complete in whichever TU calls `bind.set_function("write_csv", ...)`.

**Resolution (HIGH):** `csv.cpp` registers `write_csv` itself, through its `bind` parameter. So `CsvWriter` and `new_usertype<CsvWriter>` both live privately in `csv.cpp` (in `sandbox_internal`, per Pattern 2), and `internal.h` never sees it.
- Only `csv_write::Writer` is forward-declared in the header, for `RunHandles`.
- This avoids the critic's alternative of hoisting `CsvWriter` into `internal.h`.

### Pattern 6: Sync test reads the folder, resets `current` per file, and enforces the load-bearing names

`bindings/js/test/lua-api-sync.test.ts` changes **in the same commit as the `git mv`** (line 10 hardcodes the path, and `readFileSync` throws ENOENT otherwise):

```ts
const SANDBOX_DIR = join(__dirname, "..", "..", "..", "src", "sandbox");
const SOURCES = readdirSync(SANDBOX_DIR).filter((f) => /\.(cpp|h)$/.test(f)).sort()
  .map((f) => readFileSync(join(SANDBOX_DIR, f), "utf8"));
const CPP = SOURCES.join("\n");                       // Pass 1 + open_libraries
for (const src of SOURCES) {
  let current = "";                                   // reset at every file boundary
  for (const line of src.split("\n")) { /* Pass 2 body unchanged */ }
}
```

Guards to add (HIGH value, a few lines each):
- **Every `X.set_function(` in the folder has `X` in {`bind`, `ns`}.** That turns the load-bearing names from convention into a check. A TU whose parameter was renamed `b` otherwise drops out of Pass 1 silently and the doc check passes vacuously.
- **Exactly one `open_libraries(` across the folder.** Today the test takes the first match, and a comment in `internal.h` that mentions `open_libraries(` would sort first and break it.
- **`usertypeMethods.get(T)?.size > 0` for `BinaryFile`, `BinaryMetadata`, `Expression` and `CsvWriter`.** Today only `Expression` is guarded, and the others are skipped through `?? []`. Do **not** guard `Database` in Pass 2: after Pattern 1 it has no Pass 2 entries, and `dbMethods.size > 40` covers it.
- Optional (MEDIUM): fail when a `src/sandbox/` file exceeds a hard cap (say 500 lines). The test already reads every file, and this keeps the core value ("small enough for an agent") from rotting.

**Keep the non-`Database` usertypes variadic, one name per line.** Pass 2 depends on it, and `BinaryFile` / `BinaryMetadata` / `Expression` / `CsvWriter` are only tracked by Pass 2.
- Registering their methods with `type.set_function(...)` would make Pass 2 reset and Pass 1 ignore them.
- A short member-pointer pair that fits in 120 columns falls off one-name-per-line. The M11 `CsvWriter` form is ~134 columns, so it still wraps, but only just.

### Pattern 7: Uniform sol2 configuration across every TU

`SOL_SAFE_NUMERICS`, `SOL_SAFE_FUNCTION` and `SOL_NO_NIL`, and in the fixes phase `SOL_ALL_SAFETIES_ON=1`, stay `target_compile_definitions(quiver PRIVATE …)` (src/CMakeLists.txt:62-66), never per-source.

**Why (HIGH):**
- sol2's safety switches change the bodies of inline templates. Examples: `SOL_SAFE_USERTYPE` gates the `self` nil check in member-pointer calls (`call.hpp:485-492`), and `SOL_SAFE_FUNCTION_CALLS` feeds `detail::default_safe_function_calls` (`forward_detail.hpp:34-39`).
- Two TUs compiled with different values contain different definitions of the same inline functions: an ODR violation where the linker keeps one at random.
- The defaults depend on `SOL_DEBUG_BUILD` (`version.hpp:240-256`), which is why Debug-tested behaviour diverges in Release CI.
- PRIVATE on `quiver` gives complete coverage because no test or binding includes sol2. src/AGENTS.md's "only TU that includes sol2" sentence becomes "only `src/sandbox/` TUs".

**`/bigobj`:** replace the per-file `set_source_files_properties(lua_runner.cpp …)` (src/CMakeLists.txt:70-74) with a target-wide `if(MSVC) target_compile_options(quiver PRIVATE /bigobj) elseif(WIN32) target_compile_options(quiver PRIVATE -Wa,-mbig-obj) endif()`. That keeps the current `MSVC` / `WIN32` predicate, so clang-cl behaves as today. It is harmless on non-sol2 TUs and cannot rot when a file is added. List the eleven files explicitly in `QUIVER_SOURCES`; the repo does not glob.

## Data Flow

### Request Flow

```
host: runner.run(script)
  -> sandbox.cpp run(): GcGuard armed -> lua.safe_script(script)
     -> Lua: db:update_vector_group("C","g",1,{value={1,2}})
        -> sol2 dispatch via Database metatable (registry) / usertype storage (_G)
           -> db_write.cpp update_vector_group_lua(Database&, ...)
              -> internal.h group decoder / converters (lua_to_value, lua_cell_as)
              -> Database::update_vector_group  (C++ core: validation, SQL)
        <- (reads) internal.h to_lua_table -> sol::table
  <- first return value -> return_json.cpp encode_return_json -> std::string
  <- GcGuard dtor: handles.close_open_writers(); lua.collect_garbage()
```

### State Management

```
Impl (heap, address-stable)
 ├─ Database& db ............... borrowed; captured by expr:save as [&db]
 ├─ RunHandles handles ......... captured by open_file / write_csv as [&handles]
 │    open_writers, open_binary_files   (filled during run, cleared at run() exit)
 └─ sol::state lua ............. usertype storages are _G globals ("sol.<T>.♻"),
                                  metatables live in its registry;
                                  all binders write into it via bind / ns / lua
```

### Key Data Flows

1. **Write path:** Lua table -> `require_table` (fixes phase) -> converter in `internal.h` -> core writer. All validation messages are Pattern 1 from C++, so bindings pass them through unchanged.
2. **Read path:** core returns `vector<optional<T>>` -> `to_lua_table` keeps `nil` holes -> the script, or the JSON encoder at `run()` exit.
3. **File path:** script path -> `path_policy.cpp` gate (always before option decoding, D-22; except `open_file`, which validates `mode` first, byte-for-byte) -> core file API -> handle registered in `RunHandles` -> closed at `run()` exit.

## Scaling Considerations

Users are not the scaling axis here. What grows is the binding surface and the build.

| Scale | Architecture Adjustments |
|-------|--------------------------|
| Today (71 `db:` methods, 4 usertypes) | 11 files, one header. Adding a method touches one domain TU plus `lua-api.ts`. |
| +1 domain (e.g. a new subsystem) | Add `src/sandbox/<domain>.cpp`, one binder declaration in `internal.h`, one call in the ctor, one line in `QUIVER_SOURCES`. No other file changes. |
| `internal.h` > ~450 lines | Move the non-template converter bodies to `convert.cpp` and keep the declarations in the header. Do not split the header itself. |

### Scaling Priorities

1. **First bottleneck, build time** (unmeasured, MEDIUM).
   - Each of ~10 sol2 TUs re-parses `sol.hpp`, so total CPU goes up.
   - Ninja wall-clock and incremental rebuilds should go down, because editing a reader no longer recompiles the BinaryFile/Expression instantiations.
   - Time `cmake --build build --target quiver` (clean and incremental) before and after the split, and record the result. PCH stays out of scope unless that number is bad.
2. **Second bottleneck, Release dispatch cost of `SOL_ALL_SAFETIES_ON`** (unmeasured).
   - It adds argument checks on every bound call, including the binary `file:read` hot path.
   - `quiver_benchmark` only covers transactions, so the fixes phase needs an ad-hoc Lua loop over `file:read` and a bulk reader, Release, before and after.

## Anti-Patterns

### Anti-Pattern 1: Calling `new_usertype<Database>` in each domain TU
**What people do:** each file "opens" its own usertype for locality.
**Why it's wrong:** STEP 0 of `register_usertype` clears the previous storage (`usertype_storage.hpp:991-993`), so only the last binder's methods survive. No error is raised.
**Do this instead:** create the usertype once in `sandbox.cpp` and pass `sol::usertype<Database>& bind` (Pattern 1).

### Anti-Pattern 2: Anonymous namespace in `internal.h`, or for any usertype struct
**What people do:** wrap shared helpers in `namespace { }` to "keep them private".
**Why it's wrong:** it creates per-TU duplicates (ODR hazards for every `inline` that names them). For usertypes, sol2's demangler erases the anonymous token, so distinct types can collide on one registry key (`demangle.hpp:39-47`).
**Do this instead:** use `namespace quiver::sandbox_internal` with `inline` / templates. Anonymous namespaces are fine for file-local helpers inside a `.cpp` (for example `return_json.cpp`'s `append_json*`).

### Anti-Pattern 3: A `Context` struct passed as `ctx.bind.set_function(...)`
**What people do:** bundle `lua`, `bind`, `ns`, `db` and `handles` into one parameter object.
**Why it's wrong:** Pass 1 happens to still match `ctx.bind.set_function` (`\b` falls between `.` and `b`), but only by accident. It also hands every binder state it does not need, which hides which TUs touch `RunHandles`. Today only `csv` and `binary` do.
**Do this instead:** give each binder explicit parameters, only the ones it uses, with the literal names `bind` and `ns`.

### Anti-Pattern 4: Mixing behaviour fixes into the split
**What people do:** "while I'm moving it", add `require_table`, flip `SOL_ALL_SAFETIES_ON`, or reorder an `open_file` check.
**Why it's wrong:**
- The split is only provably safe because the 428 + 27 tests stay green with **zero** expectation changes.
- `SOL_ALL_SAFETIES_ON` turns on `SOL_SAFE_GETTER` too (`version.hpp:324-337`). That changes the Release outcome of every still-unguarded `as<std::string>()` on a non-string key (C2) from a silent wrong name to a panic, which is a behaviour change.
**Do this instead:** follow the phase order below. The backstop flag lands last in the fixes phase, after the explicit `require_table` / `lua_string_key` checks, with its perf measurement.

### Anti-Pattern 5: Pinning C1/C2 "current behaviour" in the tests-first phase
**What people do:** write a test asserting what `db:update_vector_group(..., 42)` does today.
**Why it's wrong:** in Release, today's behaviour is undefined. A `sol::table` parameter is not checked (`SOL_SAFE_REFERENCES` / `SOL_SAFE_FUNCTION_CALLS` are default-off outside debug, `version.hpp:356-407`), and `lua_next` runs on a non-table with `LUA_USE_APICHECK` off. A pin test would crash or flake in Release CI.
**Do this instead:**
- The tests-first phase pins only **defined** behaviour the refactor could break:
  - T1: the 1,000,000 key cap (row `{[1000001]='x'}`, header `{[2e6]='a'}`)
  - existing error texts in the moved code
  - check orders (`open_file` mode before path)
  - the strengthened sync-test guards
- The C1/C2 tests are written red-then-green **with** their fixes.

## Integration Points

### External / Cross-layer

| Consumer | Integration | Notes |
|----------|-------------|-------|
| C API (`src/c/lua_runner.cpp`, later `src/c/sandbox.cpp`) | wraps the public class | Stays in `src/c/`, not `src/sandbox/`. Untouched by the split, renamed in the rename phase. |
| CLI (`src/cli/main.cpp:3,124`) | constructs the class | Rename phase only. |
| JS sync test | reads `src/sandbox/*.{cpp,h}` | Changes in the `git mv` commit (Pattern 6). |
| `scripts/tidy.bat` | lints `src/**` except `src/binary` | The new TUs and `internal.h` are linted (`HeaderFilterRegex` covers `src/`). Each TU needs its own `NOLINTBEGIN/END(performance-unnecessary-value-parameter)` pair around its sol2 lambdas, because a pair cannot span files. They are at 591/685, 953/1016, 1026/1095 and 1248/1276 today. |
| Docs citing `src/lua_runner.cpp` | text | Root/src/tests/js AGENTS.md, `bindings/dart/AGENTS.md:65` (the `to_chars` floor list), `bindings/julia/AGENTS.md:158`, `csv_read.h:5,12`, `csv_write.h:4,12,18,22,55`, `csv_write.cpp:26,48`, `cmake/Platform.cmake:4`, `bindings/dart/hook/build.dart:54`, `lua-api.ts:3,6`, `tests/test_database_ui_metadata.cpp:60`, `tests/test_lua_runner_write_csv.cpp:22`. Update them in the split commit (Self-Updating rule). Re-derive with `git grep -n 'lua_runner\.cpp'` (skip `CHANGELOG.md` history and `src/c/lua_runner.cpp` references). |

### Internal Boundaries

| Boundary | Communication | Notes |
|----------|---------------|-------|
| `sandbox.cpp` <-> binders | function calls with `bind` / `ns` / `lua` / `db` / `handles` refs | The only place that knows `Impl`. |
| domain TU <-> `internal.h` | inline converters, templates | One-way: domain TUs never include each other. |
| `db_time_series.cpp` -> `db_write.cpp` | columnar decoder declared in `internal.h`, defined in `db_write.cpp` | The one cross-domain definition dependency (`time_series_rows_from_lua` uses `collect_group_columns` / `join_column_names` / `columns_to_cpp_rows`, 2324/2388/2393). Explicit through the header. |
| `binary.cpp` -> `internal.h` option walk + `table_to_element` | inline | `quiver.metadata` (1143) and `metadata_from_element` (1014) share them with csv/db_write. That is why both live in the header. |
| `return_json.cpp` / `path_policy.cpp` | one exported function each | Leaves. No dependency on any domain TU. |

## Suggested Build Order

These are phases (one PR each, green on its own). Within the split, the steps are commits.

1. **Tests first** (no source moves).
   - T1: the key-width cap tests.
   - Error-text pins for the moved code.
   - Pin `open_file`'s mode-before-path order.
   - Strengthen the sync test's usertype guards against the current single file.
   - Record build-time and Release perf baselines.
2. **Split** (zero behaviour change, every commit builds and passes all six suites):
   1. `git mv src/lua_runner.cpp src/sandbox/sandbox.cpp`, update `QUIVER_SOURCES`, make `/bigobj` target-wide, and switch the sync test to the folder with per-file `current` reset and the `bind`/`ns` receiver guard.
   2. In place, turn `Impl`'s `static` members into free functions in `quiver::sandbox_internal`. This is the "de-class" diff, with no file moves. The forward references the critic listed (389->405, 414->1487, 1109/1120/1132->1501/1539, `bind_*`->wrappers) surface here and are fixed by declaration order.
   3. Create `internal.h` (clusters 7 and 17, the decoder declarations, `RunHandles`, the binder declarations). Introduce `RunHandles` in `Impl` before `lua`, and replace the three `[this]` captures.
   4. Extract the leaves: `return_json.cpp`, then `path_policy.cpp`.
   5. Create `bind` in the ctor, convert the 17 variadic pairs to `bind.set_function`, then extract domain TUs one per commit: `db_metadata` -> `db_read` -> `db_write` (defines the decoder) -> `db_time_series` -> `db_core` -> `csv` -> `binary`.
      - The stateless ones go first.
      - `csv` and `binary` go last because they take `handles` / `db` / `ns`.
   6. Update src/AGENTS.md (file map, "only `src/sandbox/` TUs include sol2", the `csv_options_entries`/`close_open_writers` citations) and the other path citations listed above.
3. **Dedupe** M1-M16 (no behaviour change):
   - M1 (one transaction/dry-run helper) first, so C4/C6 land once.
   - M4 needs two adapters (critic correction 11).
   - M9's nil handling stays per caller (critic correction 12).
   - `csv.cpp` first, because it starts at the ceiling.
4. **Fixes**, each with its own test and CHANGELOG line:
   - C2 `lua_string_key`
   - C1 `require_table` at every table parameter
   - C5 `optional_from_lua`
   - C6, C4, C7 (empty array clears, plus the reference text), C8
   - **last:** `SOL_ALL_SAFETIES_ON=1` next to the existing defines, with the measured Release cost recorded
5. **Rename** (BREAKING, 0.13.0):
   1. The scratch target `tests/sandbox` -> `tests/scratch` / `quiver_scratch`, which frees the name. This step is not BREAKING and could move earlier.
   2. Public header `include/quiver/sandbox.h` / class `Sandbox`.
   3. C API `quiver_sandbox_*` + `include/quiver/c/sandbox.h`, then regenerate Julia's `c_api.jl` only (run the Julia generator directly; `scripts/generator.bat` also runs the Dart one). Hand-edit `bindings/dart/lib/src/ffi/bindings.dart` in its existing style, because regenerating it with the pinned ffigen 20.1.1 rewrites the file and turns the int-constant classes into enums (bindings/dart/AGENTS.md:27-43; PITFALLS finding 3). PROJECT.md:75's "regenerated, never hand-edited" is wrong for Dart. Hand-edit the Python cdefs and the JS `loader.ts` symbols.
   4. The bindings, the CLI, the test suites' `Sandbox*` prefix (`LuaSandboxTest` -> `SandboxFileTest`), `resolve_sandboxed_path` -> `resolve_contained_path`, the docs and the CHANGELOG.
   - `src/sandbox/sandbox.cpp` already has its final name, so this phase moves no sandbox file.

**Ordering rationale:**
- The split must precede dedupe and fixes so that each later diff is local to one ~200-400-line file.
- Fixes precede the rename so their CHANGELOG entries and tests are written once, against stable names. The rename then becomes a pure mechanical sweep: 50 non-`.planning` files contain `LuaRunner` (54 counting C API symbol-only files with `quiver_lua_runner`, 66 counting `lua_runner` file and path mentions). Re-derive the list with `git grep -l` at rename time rather than reusing a count.
- The safety flag is last within fixes, because it changes Release semantics globally (Anti-Pattern 4).

## Sources

- Vendored sol2 v3.5.0, `build/_deps/sol2-src/include/sol/`:
  - `usertype.hpp:47-50,62-67,85-116` (set/set_function route through registry storage)
  - `usertype_storage.hpp:806-826,836-864,911-920,932-940,991-993,1052` (storage is a `_G` global, metatable names are in the registry; STEP 0 clears an existing usertype and is a no-op on the first registration; `luaL_newmetatable` reuses an existing metatable)
  - `stack_field.hpp:110-113` (`get_field<true>` / `set_field<true>` are `lua_getglobal` / `lua_setglobal`)
  - `table.hpp:36-81` (`new_usertype` variants, enrollment defaults)
  - `types.hpp:1540-1550` (automagic defaults)
  - `usertype_traits.hpp:41-54` and `demangle.hpp:39-47,113-119` (registry names; anonymous-namespace token stripped)
  - `stack_push.hpp:211-213` and `stack_core.hpp:781-795` (pointer push creates a bare metatable, which the first `new_usertype` then fills)
  - `stack_check_unqualified.hpp:41-52` (loose table check accepts userdata)
  - `version.hpp:240-256,314-407` (debug detection, `SOL_ALL_SAFETIES_ON` cascade)
  - `call.hpp:485-492` (`SOL_SAFE_USERTYPE` self check)
  - `forward_detail.hpp:34-39` (safe function calls default)
  — HIGH
- `src/lua_runner.cpp` at bdf9087 (line references throughout), `src/CMakeLists.txt:1-74`, `src/database_internal.h` (in-repo precedent: `quiver::internal` + inline), `src/csv/csv_read.h` / `csv_write.h` include lists, `bindings/js/test/lua-api-sync.test.ts`, `.clang-tidy` — HIGH
- `.planning/research/LUA-RUNNER-MAP.md` (cluster ranges, critic corrections 1-14 and missing items 1-8) and `.planning/PROJECT.md` — HIGH
- [CWG Issue 2011](https://cplusplus.github.io/CWG/issues/2011.html) (reference captured by reference denotes the referent; P0613R0) and [eel.is [expr.prim.lambda.capture]](https://eel.is/c++draft/expr.prim.lambda.capture) — HIGH (official)

---
*Architecture research for: sol2 binding layer split + LuaRunner -> Sandbox rename (Quiver)*
*Researched: 2026-10-02*
