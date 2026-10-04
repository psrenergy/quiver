<!-- Generated 2026-10-02 by a read-only mapping workflow (5 section readers + touch-point sweep + synthesis + completeness critic). Line numbers refer to src/lua_runner.cpp at commit bdf9087. -->
# Lua runner map (`src/lua_runner.cpp`, 2539 lines)

## Current shape

Everything after the includes and the JSON encoder is one `struct LuaRunner::Impl` (238–2489). Almost all of it is `static`. Instance state is just `db`, `lua`, the two registries, the three `bind_*` methods and `close_open_writers`. Only three lambdas capture `this`: `open_file` (761), `write_csv` (876) and `expr:save` (1032).

| # | Cluster | Lines | ~LOC | Impl state? | Summary |
|---|---|---|---|---|---|
| 1 | Includes | 1–35 | 35 | n | One include block for every subsystem: csv, binary, expression, database, utils, sol2. |
| 2 | JSON return encoder | 36–236 | 200 | n | Anonymous namespace. `kMaxReturnDepth` 32 / `kMaxReturnBytes` 64 MiB, `append_json_string` (UTF-8 validation), `append_json_double`, `append_json_table`, `append_json`. Its only caller is `run()` at 2535. `append_json_string` and `append_json_double` use no sol2 types. |
| 3 | Runner core: state + ctor | 237–259 | 23 | **y** | `Database& db` and `sol::state lua`. The ctor runs `open_libraries` (6 libs), nils `dofile`/`loadfile`, creates the `quiver` table, then `bind_database` → `bind_binary` → `bind_expression`, then `lua["db"] = &db`. |
| 4 | `CsvWriter` handle (data only) | 260–285 | 26 | n | `shared_ptr<csv_write::Writer>`, `next_row_index`, `header_width`. Its behaviour lives at 899–942, about 620 lines away. |
| 5 | Run-scoped handle registry | 286–342 | 57 | **y** | `open_writers` (path + weak_ptr), `open_binary_files`, `path_has_open_writer`, and `close_open_writers`, which closes CSV writers **and** BinaryFiles. |
| 6 | CSV cell/header marshalling | 343–471 | 129 | n | `csv_max_integer_key` (1,000,000 cap), `csv_row_cells_from_lua`, `csv_cell_to_string`, `csv_header_from_lua`. |
| 7 | Strict-option walk | 472–532 | 61 | n | `csv_options_entries` (despite the name, it also backs `quiver.metadata` at 1143), `table_entries`, `string_key`. |
| 8 | CSV option decoders (write half) | 533–588 | 56 | n | `csv_separator_from_lua`, `write_csv_options_from_lua`. |
| 9 | `bind_database` | 589–944 | 356 | **y** | 9a 593–684: variadic `new_usertype<Database>` with 24 inline pairs. 9b 687–750: 54 `bind.set_function` lines. 9c 752–782: sandboxed file ops (`open_file` captures `this`). 9d 784–891: `read_csv`/`read_csv_stream`/`write_csv` (`write_csv` captures `this`). 9e 893–943: `CsvWriter` usertype, with `write_row`'s ~40 lines of logic inline. |
| 10 | `bind_binary` | 945–1017 | 73 | **y** (`lua`) | `BinaryMetadata` and `BinaryFile` usertypes, operator metamethods on BinaryFile, `quiver.metadata*`. |
| 11 | `bind_expression` | 1018–1096 | 79 | **y** (`lua`, `db` via `[this]` at 1032) | `Expression` usertype and `quiver.expression/abs/sqrt/log/exp/ifelse/gt…neq`. |
| 12 | Binary decode/marshal helpers | 1097–1178 | 82 | n | `lua_table_to_dim_map`, `metadata_string`, `metadata_array<T>`, `build_metadata_from_lua`, `dimension_to_lua`. |
| 13 | Expression operator dispatch | 1179–1276 | 98 | n | `BinOp`, `is_number`, `to_expression`, `apply_binop`, `binop_dispatch`, `bind_expression_operators<T>`. |
| 14 | `parse_aggregate_op` | 1277–1296 | 20 | n | String → `ExpressionAggregate::Operation`. Sound as is. |
| 15 | `resolve_sandboxed_path` | 1297–1357 | 61 | n | The single filesystem gate. Uses only `std::filesystem` and `Database::path()`; no sol2. |
| 16 | CSV option decoders (read/import half) | 1358–1448 | 91 | n | `parse_csv_options` (export/import), `read_csv_options_from_lua`. |
| 17 | Lua↔C++ converters | 1449–1636 | 188 | n | `to_lua_table` ×3, `is_lua_boolean`, `lua_cell_as<T>`, `lua_to_value`, `lua_table_to_vector<T>`, `value_to_lua_object`, `lua_table_to_value_map`, `require_dense_array`, `table_to_element`. |
| 18 | Element writes | 1637–1704 | 68 | n | create/update/update_by_label, `relation_target_from_lua`, update_relation ×2. |
| 19 | Scalar/vector bulk readers | 1705–1769 | 65 | n | 6 readers + `read_element_ids_lua`, all `to_lua_table(lua, db.X(...))`. |
| 20 | Metadata | 1770–1882 | 113 | n | 3 list wrappers, `lua_data_type_name`, `get_scalar_metadata_lua`, `scalar_metadata_lua`, `group_metadata_lua`, get vector/set. |
| 21 | Query | 1883–1936 | 54 | n | `lua_table_to_values` and 3 identical `query_*_lua`. |
| 22 | By-id composites | 1937–2053 | 117 | n | `read_scalars/vectors/sets/element_by_id_lua`. |
| 23 | Set bulk readers | 2054–2087 | 34 | n | 3 more copies of cluster 19's body. |
| 24 | Time-series metadata/list | 2088–2111 | 24 | n | `get_time_series_metadata_lua`, `list_time_series_groups_lua`. |
| 25 | Time-series readers | 2112–2156 | 45 | n | Row-shaped → column-oriented transpose; per-element row read. |
| 26 | Columnar group decoder | 2157–2257 | 101 | n | `GroupColumn`, `collect_group_columns`, `columns_to_cpp_rows`, `join_column_names`, `group_rows_from_lua`. |
| 27 | Vector/set group writers | 2258–2307 | 50 | n | 4 one-line delegations. |
| 28 | `time_series_rows_from_lua` | 2308–2394 | 87 | n (takes `Database&`) | Dimension columns are the row-count authority, found via `get_time_series_metadata`. |
| 29 | TS group writers + upsert | 2395–2449 | 55 | n | 4 one-line delegations. |
| 30 | TS files + end of `Impl` | 2450–2489 | 40 | n | list/read/update `time_series_files`. |
| 31 | Pimpl special members | 2490–2497 | 8 | **y** | Defaulted out of line. A move moves only the `unique_ptr<Impl>`, so a captured `this` (an `Impl*`) stays valid. |
| 32 | `LuaRunner::run` + `GcGuard` | 2498–2539 | 42 | **y** | `GcGuard` is declared before `result`. It calls `close_open_writers()` and then exactly one `collect_garbage()`. The Pattern 3 wrap uses the prefix "Failed to run Lua script:". |

**How the clusters depend on each other.** Leaves first, no cycles:

- **Base layer, used by everything:** converters (17). The strict-option walk (7) also has no in-file dependencies.
- **Decoders and helpers, built on 7 and 17:**
  - CSV marshalling (6) → `is_lua_boolean`, `utils::append_number`
  - option decoders (8, 16) → 7, `lua_cell_as`
  - binary helpers (12) → 7, 17 (`table_to_element`, `lua_table_to_vector`)
  - group decoders (26, 28) → `lua_to_value`
  - operator dispatch (13) and the sandbox gate (15) depend on nothing in the file.
- **Wrappers (18–25, 27, 29, 30):** depend on 17, on the metadata converters inside 20, and on 26/28.
- **Binders:**
  - `bind_database` (9) → 4, 5, 6, 8, 15, 16 and every wrapper
  - `bind_binary` (10) → 12, 13, 17
  - `bind_expression` (11) → 13, 14, 15, 17 and `Impl::db`
- **Top:** the ctor (3) calls 9 → 10 → 11; `run` (32) uses 2 and 5.

The current order compiles only because of class scope. These forward references must be declared first once the code is split:

- `csv_cell_to_string` (414) → `is_lua_boolean` (1487)
- `list_scalar_metadata_lua` (1771) and `get_scalar_metadata_lua` (1818) → `scalar_metadata_lua` (1829)
- every `bind_*` (590+) → wrappers defined after it

## Natural seams

**The split is mechanical, with zero behaviour change.** Every helper is already a free function in disguise (`static`, explicit `Database&`/`sol::state_view&`). Each binder needs only `(sol::state&, Database&, RunHandles&)`. Proposed minimum layout under `src/lua/` (src/AGENTS.md:83 already anticipates "a `src/lua/` folder would also have to take `lua_runner.cpp`"):

| File | Takes clusters | ~LOC | Notes |
|---|---|---|---|
| `src/lua/lua_runner.cpp` (name per Q1) | 2, 3, 5, 15, 31, 32 | ~400 | Owns `Impl` (state, ctor, `RunHandles` member), `run()`/`GcGuard`. The encoder stays in its anonymous namespace. `resolve_sandboxed_path` is defined here and declared in the header. `open_libraries(` must stay in exactly one file. |
| `src/lua/lua_internal.h` | 7, 17, metadata converters from 20, `RunHandles`, binder declarations | ~300 | **The one shared header every binder TU needs.** It is mandatory because `to_lua_table`, `lua_cell_as<T>` and `lua_table_to_vector<T>` are templates. Use a named internal namespace with `inline`, not an anonymous namespace in a header. It also hosts the new `require_table` / `lua_string_key` / `optional_from_lua` helpers (§3). |
| `src/lua/bind_database.cpp` | 9a–9c, 18–25, 30 | ~600 now, ~450 after §3 dedup | **Every `bind.set_function` for `db:` stays in this one file**, so the whole `db:` surface is still readable in one place. Implementations of the CSV and group writers live in their own TUs and are only registered here. |
| `src/lua/group_writes.cpp` | 26, 27, 28, 29 | ~300 | The most policy-dense code: the anti-silent-clear rule and the row-count authority. |
| `src/lua/csv.cpp` | 4, 6, 8, 16, 9d implementations, 9e | ~420 | `CsvWriter` and its `new_usertype<CsvWriter>` move together, and `write_row`/`close` become members. Only `write_csv` needs `RunHandles&`. |
| `src/lua/bind_binary.cpp` | 10, 11, 12, 13, 14 | ~420 | **Binary and expression must share a TU** (or move `bind_expression_operators<T>` into the header). The template is instantiated for both `BinaryFile` (1007) and `Expression` (1075), and `to_expression` accepts a `BinaryFile`. `expr:save` needs `const Database&` for the sandbox gate; capture a reference to the `Impl`-owned `Database&`, never the outer object. |

**What must stay together or in order:**
- The ctor sequence: `open_libraries` → nil `dofile`/`loadfile` → `create_named_table("quiver")` before `bind_binary`/`bind_expression` (both read `lua["quiver"]`) → binders → `lua["db"] = &db` last.
- `GcGuard` declared before `result`, with `close_open_writers` and then one `collect_garbage`, both in `run()`.
- `RunHandles` must live inside the heap-allocated `Impl`. Binder lambdas will capture `RunHandles&`, and that reference survives a `LuaRunner` move only because `Impl` never moves. This holds today for the `this` captures and must keep holding.
- `write_csv` and the `CsvWriter` usertype must be registered on the same `sol::state`. `open_file` returns `shared_ptr<BinaryFile>`, so `BinaryFile`'s usertype must be registered before any script runs (it already is).

**Optional extra seams, deferred until there is a reason:**
- The sol2-free half of the encoder (`append_json_string`, `append_json_double`) could move to `src/json/`, the destination src/AGENTS.md:86-88 names. `append_json`/`append_json_table` take `sol::object`, and a format folder must never see a sol2 type (the src/csv rule). Splitting the encoder across two folders buys nothing until a second JSON consumer exists (Q3).
- `resolve_sandboxed_path(const std::string& db_path, …)` in its own sol2-free TU would make it directly unit-testable. Today it is covered through Lua (`DeviceNamePathIsReportedWithPrefix` ×2 and the LuaBinaryTest sandbox cases). Not needed for the split.

**Compile-time effects:**
- **`/bigobj`:** src/CMakeLists.txt:70-74 sets `/bigobj` / `-Wa,-mbig-obj` on `lua_runner.cpp` alone. Each new sol2 TU needs it too. One target-wide generator expression (`target_compile_options(quiver PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/bigobj>)`, plus the MinGW branch) replaces the per-file list so it cannot rot. It is harmless on non-sol2 TUs.
- **Total vs. wall-clock cost:** every TU re-parses `sol.hpp`, so total CPU goes up. Ninja wall-clock and incremental rebuilds should improve, because editing a reader no longer recompiles the `BinaryFile`/`Expression` usertypes. The heaviest instantiation (`usertype<Database>` with ~80 functions) stays in `bind_database.cpp`. None of this is measured: time `cmake --build build --target quiver` before and after. Precompiled headers only if that measurement says so.
- **Defines:** `SOL_SAFE_NUMERICS`, `SOL_SAFE_FUNCTION` and `SOL_NO_NIL` are `PRIVATE` on `quiver` (src/CMakeLists.txt:62-66), so new TUs inherit them as long as they stay in the `quiver` target. src/AGENTS.md's claim "lua_runner.cpp is the only TU that includes sol2" must become "only `src/lua/` TUs". The point of the claim, that PRIVATE gives full coverage because no test includes sol2, still holds.

**Suggested order:**
1. Add the missing tests (§3 T1, T2).
2. Do the mechanical split and update the sync test (§5). No behaviour change.
3. Deduplicate (§3.2).
4. Apply the behaviour fixes (§3.1), each with a CHANGELOG entry.
5. Rename (§4), which is BREAKING.

## Duplication and improvement opportunities

### 3.1 Behaviour-changing fixes (bugs and error paths)

**C1. Userdata accepted where a table is expected, which silently clears group data.**
- Kind: correctness. Behaviour change: yes (wrong types now throw).
- Evidence: sol2's table check is loose and accepts userdata, and iterating a userdata yields no keys. The file says so itself at 900–904, and fixed it for `w:write_row` only. Unchecked `sol::table` parameters remain at:
  - group writers: 2264, 2274, 2289, 2299, 2401, 2416
  - upsert row: 2431, 2441
  - `update_time_series_files`: 2476
  - element writes via `table_to_element`: 1638–1656
  - `metadata_from_element`: 1013
  - `select_agents`: 1051
  - `file:read`/`file:write`: 988, 994
  - query parameters, as `sol::optional<sol::table>`: 1896, 1911, 1926
- Worst case: `db:update_vector_group("C","g",id, some_file)` → `collect_group_columns` yields nothing → `return {}` (2244–2245) → the core clears the group. That is exactly the silent clear the anti-silent-clear rule exists to prevent (root "Time-series group NULLs…" decision).
- Fix: one `require_table(obj, caller, what)` helper, with the parameters changed to `sol::object`. It also replaces the 9 hand-written checks (517, 568, 581, 905, 1059, 1129, 1140, 1367, 1412).
- Not reproduced here, because this run was read-only. Pin it with a test first.

**C2. Map keys converted to strings without a type check.**
- Kind: correctness. Behaviour change: yes (Pattern 1 instead of a wrong column name or a raw panic).
- Evidence: `pair.first.as<std::string>()` with no `get_type()` check at 1569 (`lua_table_to_value_map`, used by upsert), 1598 (`table_to_element`), 1108 (`lua_table_to_dim_map`) and 2479 (`update_time_series_files`). The guarded versions are at 497–499 and 2176–2182.
- Effect in Release: key `1` becomes column `"1"` and `[true]` becomes `""`. In Debug it is a raw sol2 panic. This is exactly the hazard in src/AGENTS.md "Three guards", bullet 2.
- Fix: generalise `string_key` (527) into one `lua_string_key(key, caller, what)` and use it at all four sites. `lua_table_to_dim_map` should also collect entries before validating them, as `rename_agents` does at 1062–1069.

**C3. No sparse-extent cap in the vector/set group writers.** [DOCUMENTED DECISION — needs user]
- Kind: correctness (DoS from an untrusted script).
- Evidence: 2190 computes `extent = max(...)` with no bound. 2246–2248 sets `row_count` to the maximum extent. 2210–2214 allocates `vector<map>(row_count)` and NULL-fills every row. So `{value = {[1e9] = 1}}` builds 1e9 maps, or throws a raw `bad_alloc`. The CSV walk caps at 1,000,000 (364–370) for exactly this reason (src/AGENTS.md "Three guards", bullet 1). The time-series path is already safe: dimensions must be dense (2365–2373) and value columns may be no longer than them (2377–2384).
- Behaviour change: yes, an absurd extent now throws.
- Conflict: the documented rule that short or sparse value columns write NULL. A cap narrows that rule rather than replacing it, and the bound has to be chosen (Q4). Implement it as the shared key walk that `csv_max_integer_key` also uses.

**C4. `db:transaction` commits outside its failure path.**
- Kind: correctness.
- Evidence: 632 calls `self.commit()` after the `!result.valid()` block, with no try around it. So a COMMIT failure (for example SQLITE_BUSY) leaves the transaction open. Julia (`database_transaction.jl:22-35`) and Python commit inside the try and roll back on failure.
- Behaviour change: only when COMMIT fails. Inside a dry run, commit is absorbed, so nothing changes there.

**C5. `sol::optional<T>` where absence means something treats a wrong type as absent.**
- Kind: consistency. Behaviour change: yes (wrong types now throw).
- Evidence: `open_file` metadata (765), `bin_to_csv` aggregate (777), `file:read` `allow_nulls` (988), `aggregate`/`aggregate_agents` parameter (1036, 1044), `query_*` parameters (1896, 1911, 1926).
- Examples:
  - `db:bin_to_csv(p, "false")` aggregates.
  - `r:read(dims, 1)` reads with `allow_nulls=false`.
  - `db:query_integer(sql, 5)` runs with no parameters.
  - A plain table passed as `open_file` metadata throws BinaryFile's misleading "Metadata must be provided…".
- This breaks src/AGENTS.md's own rule: "A nullable argument whose absence *means* something takes `sol::object`".
- Fix: one `optional_from_lua<T>(obj, caller, what)` (nil/none → `nullopt`, else `lua_cell_as<T>`; `is<BinaryMetadata>()` for the usertype). It also removes the three hand-written ternaries (771, 1040, 1047).

**C6. `db:transaction` / `db:dry_run` take a typed `sol::protected_function` (621, 646).**
- Kind: consistency. Behaviour change: error text only.
- `db:transaction(42)` surfaces sol2's raw "expected function" text, which is not Pattern 1 and differs by build type. Fix with the `on_row` pattern at 831–834 (`sol::object` plus an explicit `sol::type::function` check).

**C7. `table_to_element` skips empty arrays (1603).** [DOCUMENTED DECISION — needs user]
- Kind: consistency.
- Evidence: in Lua, `db:update_element(c, id, {value_int = {}})` is a no-op, and a typo'd `{typo = {}}` is accepted silently. Python (`element.py:61-63`) and JS (`create.ts:26-28`) send a count-0 array, so the core clears the group.
- Behaviour change: yes.
- Conflict: it touches the "Lua has no row-aligned whole-group readers" decision. An all-NULL column reads back as `{}`, so a `read_vectors_by_id` → `update_element` round trip would go from skipping that column to clearing it or hitting a length mismatch (Q5).

**C8. `to_expression` names no public operation (1199).**
- Kind: consistency. Behaviour change: error text only.
- It throws "Cannot build expression: …", which breaks the root Pattern 1 rule that `{operation}` is the method the user called. Thread the caller name through (`abs`, `gt`, the metamethods). No test pins this text.

### 3.2 No behaviour change: deduplication and structure

- **M1. `db:transaction` and `db:dry_run` share one body.** 620–637 vs 645–662 differ only in commit/rollback vs `end_dry_run`. Fold them into one helper so C4 and C6 land once. The root "Dry runs live on Database" decision is unaffected, because the helper only sequences public calls.
- **M2/M3. One registration style; forwarders become member pointers.**
  - 24 methods are inline variadic pairs (593–684), the rest use `bind.set_function` (687–891), and `delete_element` and `create_element` sit in different styles. The "Group 1/9/10/10b/11/12" comments (601, 608, 611, 638, 663, 674) number a scheme whose other groups no longer exist.
  - 17 lambdas only forward to non-overloaded `Database` methods (database.h:42, 44, 52, 55, 223, 299, 307, 311, 313, 316, 338–341, 348–350, all verified single signatures). Replace them with `bind.set_function("describe", &Database::describe)` and so on.
  - Result: every `db:` name goes through sync-test Pass 1, and `Database` stops depending on Pass 2's line-shape heuristic.
- **M4. One adapter for 11 bulk readers.** 1706–1769, 2059–2087 and 2455–2462 all follow `to_lua_table(lua, db.X(c, a))`. Replace them with `template <auto Read> sol::table bulk_read_lua(...)`, keeping the `bind.set_function("read_scalar_strings", &bulk_read_lua<&Database::read_scalar_strings>)` line shape the sync test matches. Saves ~100 lines. The Do-Not-Fix entry about closure helpers is scoped to Dart/Python FFI, not this.
- **M5. Return `std::optional` directly.** In `query_*_lua` (1893–1936) and the `read_scalars_by_id` ternaries (1951, 1956, 1962): sol2 pushes `nullopt` as nil, and a nil assigned on a fresh table leaves the key absent, as today.
- **M6. `value_to_lua_object` (1549–1561) duplicates sol2's built-in `std::variant` pusher** (callers 2134, 2153). Optional, and only after a macOS CI run, because platform-default sol2 macros already bit this file once (the `SOL_NO_NIL` story).
- **M7. `read_vectors_by_id_lua` and `read_sets_by_id_lua` are identical** (1984–2003 vs 2012–2030). Make them one template; keep the documented by-column-name routing.
- **M8. Metadata wrappers.** 4 list loops (1771–1799, 2103–2111) and 3 get bodies (1864–1882, 2093–2101) collapse to one `group_list_lua` helper. Also rename `list_{scalar,vector,set}_metadata_lua` to the names they are bound under (734–736).
- **M9. Option decoders.**
  - The nil/non-table preamble is repeated ×3 (563–572, 1363–1370, 1407–1416). Fold it into the walk.
  - Rename `csv_options_entries` to `option_entries`; it also backs `quiver.metadata` (1143).
  - The entry-collect lambda is repeated ×3 (486–489, 520–523, 1062–1065).
  - The "option 'header' must be a table" check is duplicated (581–583 vs `table_entries` at 517–519).
  - `header_row` is type-checked twice with the same message (1428–1437); `is<int64_t>()` alone suffices under `SOL_SAFE_NUMERICS`.
  - Doc update: src/AGENTS.md names `csv_options_entries`.
- **M10. `csv_cell_to_string` (405–450) re-implements `lua_to_value`'s dispatch** (1517–1535): same order, same message shape. It could be `lua_to_value` + `std::visit`, keeping only the non-finite check (432–437) and the number formatting. That removes the second copy of an ordering the comment at 399–404 says must not change. Doc update: src/AGENTS.md says "`csv_cell_to_string` writes a boolean as 1/0".
- **M11. Move `CsvWriter` behaviour onto the struct.** `write_row` (899–940) and `close` become members, registered as `"write_row", &CsvWriter::write_row`. Doc update: src/AGENTS.md "lives entirely in the Lua-layer `CsvWriter` wrapper in `src/lua_runner.cpp`".
- **M12. One header rule for `read_csv` and `read_csv_stream`.** They spell "header is nil when empty" two ways (813–816 vs 850–852). One `header_object()` helper turns the "must not diverge" comment (844–849) into code. The comment at 809–812 is stale: it says no-header mode is "forward-looking", but `header_row = 0` has shipped.
- **M13. Registry hygiene.**
  - Entries are pruned only at `run()` exit (331, 341), so `path_has_open_writer` scans expired entries and dead `Writer` storage stays allocated until exit. `std::erase_if(expired)` before each `emplace_back` (888) and `push_back` (773) fixes both.
  - Rename `close_open_writers`, which also closes binary readers.
  - The two close loops (321–330, 332–341) are the same pattern.
- **M14. Drop `BinOp`.** `BinOp` (1184) and `apply_binop` (1202–1234) re-declare `ExpressionBinary::Operation` (expression_node.h:73) and its switch. Pass transparent functors instead (`std::plus<>`, …, `std::logical_and<>`/`std::logical_or<>`, which call the overloaded `&&`/`||`). Keep every `ns.set_function("gt", …)` a literal call, as the sync test requires.
- **M15. Name the slots in `build_metadata_from_lua`'s `found[0..7]`** (1143–1163). Two `metadata_array<std::string>` slots that swap would still compile.
- **M16. Merge duplicate messages.** "contain no rows" is thrown in both decoders (2250–2256, 2386–2393); move it into `columns_to_cpp_rows`. "has length X but expected Y" is spelled twice (2359–2363, 2378–2383).

### 3.3 Dead code and comments

- **D1. Unreachable branch.** `update_time_series_files_lua`'s nil branch (2480–2485) can never run, because a pairs walk never yields a nil value. Omitting a key already clears it (pinned by `UpdateTimeSeriesFilesReplacesTheWholeRow`).
- **K1. Unresolvable planning references.**
  - 49 lines carry planning IDs (`D-xx`, `LUA-xx`, `WRITE-xx`, `FMT-xx`, `TEST-xx`), plus `04-SOL2-DISPATCH-PROBE.md` (~404), `04-02-SUMMARY.md` (430) and `RESEARCH.md Q1` (2506).
  - None of them resolve in the repo (`.planning/` holds only `codebase/*.md`).
  - The WRITE-06 rationale is written three times (266–268, 315–319, 2500–2515), plus once in src/AGENTS.md.
  - Replace each ID with its one-line reason or the test that pins it. This goes against "Human-Centric" only in the sense that it fixes it; src/AGENTS.md uses the same IDs, so scope is Q7.

### 3.4 Test gaps to close before moving code

- **T1. The 1,000,000 width cap in `csv_max_integer_key` (364–370) is untested.** No test in `tests/test_lua_runner_write_csv.cpp` mentions it, and `SubOneIntegerRowKeyThrows` asserts only the prefix. A refactor that unifies key walks could drop the cap silently. Add tests for a row `{[1000001]='x'}` and a header `{[2e6]='a'}`.
- **T2. No tests for non-string keys or userdata payloads** on `upsert_time_series_row`, `update_time_series_files`, `create_element`/`update_element` and `file:read`. Today only `read_csv` options and the group writers have key-type tests. These pin C1 and C2.

## Rename blast radius (LuaRunner -> Sandbox)

53 tracked files (excluding `build/`) contain the word `LuaRunner`. 0.13.0 has not been tagged (latest tag is `v0.12.9`; CMakeLists is at 0.13.0), so the BREAKING entry needs **no further version bump**. It needs a new `## [0.13.0] — unreleased` section: CHANGELOG.md's top section is `[0.12.8]`, and there is no `[0.12.9]` section even though that tag exists. The memory note pointing plan entries at `[0.12.9]` is stale.

| Layer | Files and symbols | Count |
|---|---|---|
| C++ | `include/quiver/lua_runner.h`: guard `QUIVER_LUA_RUNNER_H`, `class LuaRunner`, ctor, dtor, deleted copy, move, `run`, `Impl`. Not included by `quiver.h`. `src/lua_runner.cpp`: `LuaRunner::Impl` and member definitions. `src/CMakeLists.txt:17, 71, 73, 131`. Comments: `src/csv/csv_read.h`, `csv_write.h:55` (`LuaRunner::Impl::CsvWriter`), `csv_write.cpp:11`, `cmake/Platform.cmake:4`, `bindings/dart/hook/build.dart:54`. | 7 + 8 + 4 + ~12 |
| C API | `include/quiver/c/lua_runner.h`: guard, `quiver_lua_runner_t`, `quiver_lua_runner_new/free/run/free_string`. `src/c/lua_runner.cpp`: `struct quiver_lua_runner { quiver::LuaRunner runner; }` and 4 definitions. | 9 + 10 |
| CLI | `src/cli/main.cpp:3` (include) and `:124` (`quiver::LuaRunner lua(db)`). No user-visible text names it. | 2 |
| Julia | `src/lua_runner.jl`: `mutable struct LuaRunner`, `run!`, `close!`, 4 `C.quiver_lua_runner_*` calls. `Quiver.jl:25` include (nothing exported). `src/c_api.jl` is **generated**: regenerate it, do not hand-edit. `test/test_lua_runner.jl`: `@testset "LuaRunner"` plus 14 nested testsets. | 13 + 1 + 10 + 16 |
| Dart | `lib/src/lua_runner.dart`: class, `StateError('LuaRunner has been disposed')`. `lib/quiverdb.dart:21` (`show LuaRunner`). `ffigen.yaml:11,17` **and** the duplicate block in `pubspec.yaml:32,38`. `lib/src/ffi/bindings.dart` is generated: regenerate it. `exceptions.dart:28` `LuaException` (rename optional). `test/lua_runner_test.dart`. | 13 + 2 + 4 + 30 + 1 + 27 |
| Python | `src/quiverdb/lua_runner.py`: class, "LuaRunner is closed", "LuaRunner was not closed explicitly". `__init__.py` (import + `__all__`). `_c_api.py:398-404` hand-maintained cdef. `generator/generator.py:22` HEADERS. `tests/test_lua_runner.py`: asserts `match="LuaRunner is closed"` at 93 and 99. | 11 + 3 + 6 + 1 + 25 |
| JS | `src/lua-runner.ts`: class, "LuaRunner is closed". `src/loader.ts:199-204, 219` (`luaSymbols`). `src/index.ts:16`. `src/lua-api.ts`: maintainer header 3, 6; **shipped prompt text** 23, 30. `test/lua-runner.test.ts`. `test/lua-api-sync.test.ts:10, 25, 53`. `test/package-entry.test.ts:19`. `README.md:195-200`. | 6 + 5 + 2 + 4 + 13 + 3 + 1 + 3 |
| C++ tests | `tests/CMakeLists.txt:33-49` (17 `test_lua_runner_*.cpp`) and `:92` (`test_c_api_lua_runner.cpp`). `tests/test_lua_runner.h`: guard, `LuaRunnerTest`, `LuaSandboxTest`, `expect_lua_error(quiver::LuaRunner&, …)`. 429 `quiver::LuaRunner` constructions and 428 TEST/TEST_F across 19 files. Suites (per inventory): `LuaRunnerTest` 204, `LuaRunnerAllTypesTest` 5, `LuaRunnerFkTest` 18, `LuaRunner_ExportCSV` 9, `_ImportCSV` 12, `_Migrations` 4, `_ReadCsv` 69, `_WriteCsv` 48, `_WriteCsvErrors` 9, `LuaBinaryTest` 26, `LuaExpressionTest` 24. C API: `LuaRunnerCApiTest` (27 tests). Comments in `test_database_ui_metadata.cpp:31, 60`. | ~772 + 29 + 26 + 111 |
| Docs | Root AGENTS.md (16 sites, including the "### LuaRunner Class" heading and the cross-layer rule at 746), src/AGENTS.md (23, including "## LuaRunner" at 620–816), src/c/AGENTS.md (6), tests/AGENTS.md (17; also the stale `'LuaRunner*'` filter and "291/291" at 143–144), the four binding AGENTS.md files (23), `.planning/codebase/*.md` (33). | ~118 |
| CHANGELOG | New BREAKING entry: class, C API symbols and header paths, and exports renamed in every binding, plus what a caller must change. Historical mentions (199–202, 505, 819, 896, 1061) stay. | 1 entry |

Test-suite naming: `--gtest_filter='LuaRunner*'` matches 378 tests and misses `LuaBinaryTest`/`LuaExpressionTest` (50). The rename should choose one prefix that covers all Lua suites.

**Name collisions and what each would need:**

| Collision | Where | What a `Sandbox` rename would require |
|---|---|---|
| `tests/sandbox/sandbox.cpp` and the `quiver_sandbox` executable target | tests/CMakeLists.txt:113-122; protected by root **Do Not Fix** ("Deleting or 'cleaning up' tests/sandbox") | A C API prefix of `quiver_sandbox_*` sits next to the `quiver_sandbox` target and `build/bin/quiver_sandbox.exe`. There is no linker clash, but grep and docs become ambiguous, and a `src/sandbox/` folder would mirror `tests/sandbox/` with a different meaning. Renaming the scratch target needs explicit user permission (Q2). |
| "sandbox" as the **path policy** | `resolve_sandboxed_path` (10 call sites); root AGENTS.md "Lua file operations are db-scoped and sandboxed" and the "sandbox decision"; src/AGENTS.md "**Filesystem sandbox**" and "This is LuaRunner policy only"; csv_read.h/csv_write.h; CHANGELOG 776 ("the Lua sandbox has no io") | Phrases become tautologies ("Sandbox policy", "sandboxed by the Sandbox"). Either rename the policy wording (e.g. "db-directory containment") or accept the overlap. The error messages never contain "sandbox", so no test text changes. |
| `LuaSandboxTest` fixture and its `sandbox` member | tests/test_lua_runner.h:26; `sandbox / "x"` at ~170 sites | Renaming `LuaRunnerTest` to `SandboxTest` would sit beside `LuaSandboxTest` meaning the opposite (in-memory vs file-backed). A local `quiver::Sandbox sandbox(db);` would **shadow** the fixture member in every derived TEST_F. Keep the local variable named `lua`, and rename the fixture (e.g. `LuaFileTest`). |
| `LuaRunnerCApiTest.SandboxViolationError` | test_c_api_lua_runner.cpp:162 | The test name becomes ambiguous; rename it to something like `PathEscapeError`. |
| A third meaning: the host harness's sandbox | lua-api.ts:162 ("disabled in the run_lua sandbox", explicitly "not from quiverdb") | This is shipped LLM prompt text. A class named Sandbox makes the sentence ambiguous to the model reading it. Reword it if the class is renamed. Lines 23 and 30 also name `LuaRunner` in the prompt. |

There are no language-level collisions: the repo has no existing `Sandbox` symbol in C++, Julia, Dart, Python or JS.

## Constraints a refactor must keep

**Sync test contract** (`bindings/js/test/lua-api-sync.test.ts`):
- **It reads one file.** Line 10 hardcodes `src/lua_runner.cpp`, and a rename or move makes `readFileSync` throw ENOENT, so every test fails. A split must change it to read every `.cpp` under the new folder and **reset `current` at each file boundary**; otherwise an open usertype bleeds into the next file.
- **Pass 1** is `/\b(bind|ns)\.set_function\(\s*"name"/`. The local variable names `bind` (the `new_usertype<Database>` result) and `ns` (`lua["quiver"]`) are load-bearing in every file, including function *parameters* named that way in other TUs. Do not switch to `bind["x"] = …` syntax, and do not use table-driven loops for `gt/lt/…` or the metamethods.
- **Pass 2** works because `new_usertype<\w+>` is unqualified (`new_usertype<quiver::X>` breaks it), and because each method name sits alone on its own line. That holds only while the call exceeds 120 columns (`.clang-format`: `ColumnLimit: 120`, `BinPackArguments: false`). A short member-pointer registration that fits on one line silently drops out of Pass 2. The proposed `CsvWriter` form is ~134 columns, so it still wraps, but only just. Any line containing `.set_function(` resets `current`.
- **The meta-guards cover too little.** They check `dbMethods > 40`, `quiverFns > 10` and `Expression.size > 0`. `BinaryFile`, `BinaryMetadata` and `CsvWriter` are skipped silently if missing (`?? []`). Add guards for all four usertypes as part of the split.
- **`open_libraries(`:** the first match in the source must still be the real call, and the list must equal the doc sentence "Loaded standard libraries: …".
- **Docs that describe the hardcoded path** must change with the test: src/AGENTS.md:83 and 659–665, bindings/js/AGENTS.md:37, tests/AGENTS.md:172–176, lua-api.ts:3 and 6.

**AGENTS.md text that cites `lua_runner.cpp` by path or name** must be updated in the same change (the Self-Updating rule):
- **Root AGENTS.md:**
  - `resolve_sandboxed_path` in `src/lua_runner.cpp` (sandbox decision)
  - the JSON encoder "in `src/lua_runner.cpp`, anonymous namespace"
  - "Lua binds the C++ classes directly via sol2 (`src/lua_runner.cpp`)"
  - the macOS 13.3 floor list of `to_chars` users (also `cmake/Platform.cmake:4` and `bindings/dart/hook/build.dart:54`)
- **src/AGENTS.md:**
  - file map lines 16 and 46
  - the src/csv rationale and the src/json destination (80–88)
  - "Three guards in the Lua layer's decoders (`src/lua_runner.cpp`)" (255) and the names it cites: `csv_options_entries`, `collect_group_columns`, `csv_max_integer_key`
  - the FMT-07 `CsvWriter` sentence
  - the "only TU that includes sol2" claim (741)
  - `close_open_writers` / `Impl::open_writers`
  - "## LuaRunner" (620–816)
- **Other files:** src/c/AGENTS.md:13, 38, 71, 110–111; tests/AGENTS.md:35–62, 79–83, 92, 143–144; and the comment in `csv_write.cpp:9` (the TEST-12 catalogue that `write_row`'s width error at 920–932 says to "reword both together").

**Do-Not-Fix items in reach:**
- Do not delete or "clean up" `tests/sandbox`.
- Do not relocate `LUA_DB_API_REFERENCE` out of `bindings/js/src/lua-api.ts`; editing its text is fine.
- Do not "simplify" the Bun FFI workarounds while renaming the `luaSymbols` keys in `loader.ts`.
- Do not drive-by fix lint in untouched JS files.

**Design decisions this code implements, which the refactor must keep byte-for-byte:**
- **Sandbox:** strict containment, root rejected, `:memory:` rejects all file ops, the try/catch around `weakly_canonical` (with the deliberate throws outside it), the check order sandbox → options decode → work (D-22), the 6-library stdlib set, `dofile`/`loadfile` nil'd.
- **JSON return contract:** first value only; `""` vs `"null"`; non-finite → null; keys exactly 1..n → array; sorted keys; duplicate-key rejection; UTF-8 validation; caps 32 / 64 MiB. Encoder errors are Pattern 1 "Cannot run:" and are **not** wrapped in the Pattern 3 prefix.
- **`run()` exit:** `GcGuard` declared before `result`; `close_open_writers` (writers **and** BinaryFiles) then exactly one `collect_garbage()`, which must not be "hardened" into a loop.
- **Writer registry:** same-path guard checks `is_closed()`, so reopening after close still truncates.
- **Write policy:** the boolean-is-1/0 policy via the single `is_lua_boolean`; `is<int64_t>()` before `is<double>()` everywhere.
- **Group writers:** anti-silent-clear (`{}` clears, named-but-empty throws); time-series dimension columns are the row-count authority.
- **Relations:** `update_relation` clears only on nil.
- **Readers:** no Lua whole-group readers; nil-hole NULL preservation on reads.
- **Converters:** `lua_table_to_vector` checks every cell; `require_dense_array` before element arrays; collect-then-validate option walks; Lua→C++ converters throw on unsupported types and never skip.
- **sol2 build settings:** `SOL_SAFE_NUMERICS`, `SOL_SAFE_FUNCTION` and `SOL_NO_NIL` stay PRIVATE on `quiver` (new TUs must stay in that target). Write `sol::lua_nil`, never `sol::nil`.
- **Ownership and errors:** `LuaRunner` borrows `Database&`; one C API error channel.

**Behaviour pinned by tests:**
- **Coverage:** 428 C++ Lua tests, 27 C API tests, plus the Julia, Dart, Python and JS suites.
- **Named pins:**
  - `UnclosedWriterHeldInAGlobalIsAlsoFlushedWhenRunReturns`, `WriterHeldInAGlobalIsClosedWhenRunReturns`, `HandleFromAnEarlierRunIsClosed`
  - `DeviceNamePathIsReportedWithPrefix` (ReadCsv and Binary)
  - `ReopeningSamePathTruncatesExistingContent`
  - `ReadVectorPreservesNullCellsAsNilHoles`
  - `StrayQuotesTokenizeAsTheImportPrePassAssumes`
  - `UpdateTimeSeriesFilesReplacesTheWholeRow`
  - `ReadRejectsNonIntegerDimension`
  - `SubOneIntegerRowKeyThrows`
- **Asserted message text:** "Failed to run Lua script:" (C API, read_csv and write_csv tests); "LuaRunner is closed" (Python, at 93 and 99).

## Open questions for the user

1. **What is the new name, and what does the rename cover?**
   - (a) `Sandbox` / `quiver_sandbox_*`: collides with the `quiver_sandbox` target, the path-policy wording and `LuaSandboxTest` (§4).
   - (b) `LuaSandbox` / `quiver_lua_sandbox_*`: collides much less.
   - (c) Keep the public `LuaRunner` and do only the internal split and file rename.

   (a) and (b) are BREAKING across C++, the C API and all four bindings, and land in 0.13.0. Should Dart's `LuaException` and the "is closed" / "disposed" / "not closed explicitly" texts in Python, Dart and JS follow the new name?
2. **If `Sandbox` is chosen,** may `tests/sandbox` / `quiver_sandbox` (protected by Do Not Fix) be renamed, or should it stay and the overlap be accepted?
3. **Folder and encoder home.** Should the split go to `src/lua/` with the JSON encoder staying in the runner TU, or should the encoder's sol2-free half move to `src/json/` as src/AGENTS.md:86-88 says it eventually would?
4. **Sparse-extent cap for `update_vector_group`/`update_set_group`** (C3) [DOCUMENTED DECISION]. Reuse the CSV bound of 1,000,000, or pick another?
5. **Empty array in `create_element`/`update_element`** (C7) [DOCUMENTED DECISION]. Should `{col = {}}` clear the group, as in C++, Python and JS, or keep today's silent skip?
6. **Bundle or separate the behaviour fixes?** Should the behaviour-changing fixes (C1, C2, C4, C5, C6, C8) ship in the same change as the zero-behaviour split, or as separate PRs, each with its own CHANGELOG entry?
7. **Planning IDs in comments** (K1). Strip `D-xx`/`LUA-xx`/`WRITE-xx` IDs from the Lua TUs only, or repo-wide (src/AGENTS.md uses them too)?
---

# Critic pass (corrections to the map above — these take precedence)

## Corrections

1. **9a and 9b counts are wrong.** The variadic `new_usertype<Database>` at 593–684 has 17 inline pairs, not 24. They are delete_element, delete_element_by_label, is_healthy, current_version, path, has_time_series_files, begin_transaction, commit, rollback, in_transaction, transaction, begin_dry_run, end_dry_run, in_dry_run, dry_run, export_csv and import_csv. Lines 687–750 hold 47 `bind.set_function` calls. The figure 54 is the total across 687–891 (`grep -c` = 54; the awk over 687–750 = 47). The "17 forwarders" figure in M3 is right.

2. **Instance state is not complete.** `path_has_open_writer` (303) is a non-static `const` member that reads `open_writers`, so it also has to move into `RunHandles`.

3. **The dependency graph is missing edges, so the forward-reference list is incomplete.**
   - Missing edges:
     - 8 → 6: `write_csv_options_from_lua` calls `csv_header_from_lua` at 584.
     - 16 → 8: `read_csv_options_from_lua` calls `csv_separator_from_lua` at 1421.
     - 28 → 26: `time_series_rows_from_lua` uses `collect_group_columns`, `join_column_names` and `columns_to_cpp_rows` (2324, 2388, 2393).
     - `table_to_element` is used by 10 (1014), not by 12.
   - Missing forward references:
     - 389 → 405
     - 1109, 1120 and 1132 → `lua_cell_as` (1501) and `lua_table_to_vector` (1539)
     - 1390 → 1501
     - `bind_binary` → 1102, 1139, 1167, 1253, 1593
   - All of these go away if 7 and 17 land in the header first.
   - `parse_csv_options` (cluster 16) must also be declared in the header. The `export_csv`/`import_csv` lambdas (672, 682) stay in `bind_database.cpp`.

4. **Two LOC estimates are off.** `bind_database.cpp` is about 750 lines (9a–9c ≈190 + 18–25 ≈520 + 30 ≈40), not ~600. `bind_binary.cpp` is about 350 lines (clusters 10–14), not ~420.

5. **C1 is understated: in Release there is no check at all.**
   - **Debug:** sol2's `loose_table_check` (`build/_deps/sol2-src/include/sol/stack_check_unqualified.hpp:41-52`) accepts userdata. That is the only case the draft covers.
   - **Release:** `SOL_SAFE_FUNCTION_CALLS` and `SOL_SAFE_REFERENCES` default to off outside `SOL_DEBUG_BUILD` (`version.hpp:394-407`, `table_core.hpp:347-348`), so a `sol::table` parameter is not checked at all.
   - **The file already says so:** comments at 1056 and 1137 read "sol2 does not check a table parameter in Release".
   - **What that leads to:**
     - Iteration is `lua_next` (`table_iterator.hpp:70`), and Lua is built with `LUA_USE_APICHECK=OFF` (`build/CMakeCache.txt:503`).
     - So in Release, `db:update_vector_group("C","g",1,42)` treats a number or string as a `Table*`. That is undefined behaviour and probably crashes the host, rather than giving a Pattern 1 error.
     - "Iterating a userdata yields no keys" is the same undefined behaviour, observed to be harmless by luck. It is not guaranteed.
     - CI runs Release (`ci.yml:25`), and no test passes a non-table to any of the listed parameters.
   - **Missed sites (value level):** `is<sol::table>()` at 1600 (`table_to_element`) and 2184 (`collect_group_columns`). A userdata column value silently becomes an all-NULL column.
   - **Unchecked `self`:** the `Database&` / `CsvWriter&` / `BinaryFile&` first parameter is also unchecked in Release, so a dot call (`db.describe()`, `w.write_row({})`) dereferences null.
   - **Cheaper option for the user to decide:** add `SOL_ALL_SAFETIES_ON=1` (or the three flags above plus `SOL_SAFE_USERTYPE`) next to `src/CMakeLists.txt:62-66`. That makes Release behave like the tested Debug build. `require_table` is still needed for userdata, and the messages stay sol2's raw text. It may cost speed on the binary read path.
   - Nothing here was executed.

6. **The C2 "collect first" suggestion for `lua_table_to_dim_map` is not needed.**
   - The LUA-09 collect-first rule protects sol2's `for_each`. There the key stays on the stack across the callback with no RAII cleanup (`table_core.hpp:575-590`).
   - A range-for iterator cleans up in its destructor (`table_iterator.hpp:113-120`). `collect_group_columns` (2172–2190), `csv_max_integer_key` (354–356), `table_to_element` and `lua_table_to_value_map` already throw in the middle of a range-for. Either the rule covers all five sites or none of them.

7. **C3 cites the wrong rule.** The vector/set sparse-column rule it would narrow is in shipped text at `lua-api.ts:397` ("Row count is the largest index any column reaches. Shorter or sparse columns write NULL"). The root decision at `AGENTS.md:240` is about time series only, which needs no cap. `extent` is computed at 2192, not 2190.

8. **C7: the evidence and the example are both off.**
   - The skip is documented in shipped prompt text at `lua-api.ts:306-307` ("Empty arrays are skipped … silently dropped"). That is the text it conflicts with. The stated reason there, that the type can't be inferred, does not hold: Python (`element.py:60-63`) and JS (`create.ts:26-28`) send a count-0 integer array.
   - `{value_int = {}}` as the only key is not a no-op. It throws "Cannot update_element: element must have at least one attribute to update" (`database_update.cpp:16-17`). The silent skip only happens when other keys are present.

9. **C8 has a sibling.** `lua_data_type_name` (1812–1814) puts an internal helper name in a Pattern 1 message, though only in an unreachable `default:`. `apply_binop`'s throw at 1233 is also unreachable.

10. **M3 is not quite "no behaviour change".** Switching to member pointers changes the Debug error for a dot call, from the generic argument mismatch to sol2's `self` text. No test pins either message. I confirmed there are no overloads at the cited `database.h` lines.

11. **M4 needs two adapters, not one.** `read_element_ids` (1766) and `list_time_series_files_columns` (2455) each take a single argument, so one template cannot cover all 11 readers. The real saving is about 85 lines.

12. **M9's folded preamble must stay per caller.** `quiver.metadata` treats nil as an error (1140–1141), while the three CSV decoders treat nil as defaults.

13. **T2 is slightly understated.** Key-type tests also exist for `rename_agents` (`test_lua_expression.cpp:316-327`) and `enum_labels` (`test_lua_runner_csv_import.cpp:162`). The four untested targets the draft lists are correct.

14. **Small line references.**
    - C1's `return {}` is at 2242–2243.
    - "RESEARCH.md Q1" is at 2505.
    - D1's unreachable branch is 2481–2482; 2483–2485 is the live `else`.
    - `sandbox /` appears at 137 sites (162 hits for the word), not ~170.

Confirmed accurate:
- The cluster ranges are contiguous and cover lines 1–2539.
- There are exactly three `[this]` captures (761, 876, 1032).
- M1, M5–M8, M10–M16, D1, K1 (49 lines with planning IDs) and T1 check out. M10's `lua_to_value` route gives the same "cell #M has unsupported Lua type" text that the `csv_write.cpp` TEST-12 catalogue lists.
- The description of the sync test is right.
- Test counts are right: 428, the suite breakdown, 378 matched by `LuaRunner*`, and 27 C API tests.
- Version, tag and CHANGELOG facts are right, and so is "53 files contain `LuaRunner`".

## Missing

1. **More path citations in the CSV sources.** `src/csv/csv_read.h:5,12`, `csv_write.h:4,12,18,22` and `csv_write.cpp:26,48` cite `src/lua_runner.cpp` by path and go stale on the split. The draft lists only `csv_write.h:55` and `csv_write.cpp:9/11`.

2. **More AGENTS.md citations.**
   - `src/AGENTS.md`:
     - 102 also states a constraint the split must keep: csv-parser headers must never be included by any `src/lua/` TU.
     - 159, 821 and 891 cite the path.
   - Root `AGENTS.md`:
     - 222 (`lua_to_value` / `lua_cell_as` "in `src/lua_runner.cpp`")
     - 895 ("raw `Database&` in `src/lua_runner.cpp`")
   - For the rename only: `bindings/js/AGENTS.md:111-112` names `quiver_lua_runner_run` and `quiver_lua_runner_free_string`.

3. **`CsvWriter` must be a complete type in `bind_database.cpp`.** `write_csv` returns `std::unique_ptr<CsvWriter>` (876). Registering it there instantiates sol2's pusher and deleter, so the struct has to live in `lua_internal.h`, not privately in `csv.cpp`. The alternative is to register `write_csv` from `csv.cpp` through a parameter literally named `bind`.

4. **Pass 2 does not close a usertype at its `);`.**
   - `current` stays open until the next `.set_function(` or `new_usertype` line. Any bare `"word",` line that follows a usertype call in the same file gets attributed to that usertype.
   - Example: `"metadata",` at 1145 inside `build_metadata_from_lua` would be picked up. It is safe today only because the `ns.set_function` at 1077 resets `current` first.
   - The split should keep that ordering, or make Pass 2 reset `current` at file boundaries and scan headers too.

5. **"Sandbox" already has two meanings in the shipped `lua-api.ts`, beyond line 162.**
   - The path policy: 108, 647, 785, 870, 905.
   - The stdlib restriction ("`io` is deliberately absent from the sandbox"): 695, 783.
   - A class named `Sandbox` matches the second meaning and collides with the first.

6. **The NOLINT blocks have to move with their code.** They are at 591/685, 953/1016, 1026/1095 and 1248/1276. `tidy.bat` covers all of `src` except `src/binary`, so the new `src/lua/` files will be linted.

7. **Check order to preserve byte-for-byte.** `open_file` validates `mode` (767) before the sandbox check (770). A shared "sandbox first" helper would change which error `db:open_file("../x","z")` reports.

8. **Some renames need no list edits.**
   - Julia's generator has no header list, so regenerating `c_api.jl` picks up a renamed header automatically.
   - Dart lists the header twice (`ffigen.yaml` and `pubspec.yaml`); Python lists it once (`generator.py:22`).
   - No Julia, Dart, Python or JS test runner names `test_lua_runner*` by file, so renaming those test files needs no list edits there.
   - Nothing in `.github`, `scripts` or `docs/` names the file or the class.