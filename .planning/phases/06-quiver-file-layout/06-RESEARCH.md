# Phase 6: Quiver File Layout - Research

**Researched:** 2026-10-04
**Domain:** Re-splitting the sol2 v3.5.0 / Lua 5.4.8 binder TUs in `src/lua_runner/` so they mirror the core's `src/database_*.cpp` files, plus `csv`, `binary` and `expression`, with no behaviour change
**Confidence:** HIGH. Every mapping below comes from files read this session at HEAD. HEAD's `src/`, `tests/`, `include/`, `cmake/` and `CMakeLists.txt` are identical to `da6f67b` (`git diff --stat da6f67b HEAD` over those paths is empty). The counts (86/71/15, `Lua*` 477, 1454, 543, sync test 6 pass) and the golden harness (`GOLDEN debug OK`, `GOLDEN release OK`) were re-run this session.

## Summary

This is a pure move with a ready-made precedent. v0.12.9 Phase 2 did the same kind of split, and Phase 3 built the verification tooling, which is still on disk in the gitignored `build/fixes-check/`: `golden.sh`, `prelude.lua`, 10 probe scripts, `debug_text.lua`, `wave_gate.sh` and the tidy counter. `build/split-check/surface.ts` holds the static surface lister. Against the current Debug and Release binaries the harness still passes. Today there are 9 sol2 TUs and 7 binders (`bind_core`, `bind_read`, `bind_write`, `bind_metadata`, `bind_time_series`, `bind_csv`, `bind_binary`). Afterwards there are 16 sol2 TUs and 14 binders. `csv.cpp` and `return_json.cpp` keep their names and content, and so do `path_policy.{h,cpp}`. Three files are renamed (`db_read`, `db_metadata`, `db_time_series`). `db_core.cpp`, `db_write.cpp` and `binary.cpp` are carved up.

The milestone design study (`.planning/research/DESIGN-STUDY.md` §File layout mapping) already has a per-name mapping table. LAYOUT-01's literal file list overrides it in two places, and the planner must follow LAYOUT-01. First, `describe`/`describe_collection`/`summarize_collection` get their own `database_describe.cpp` instead of being folded into `database.cpp`. Second, `get_time_series_metadata`/`list_time_series_groups` move to `database_time_series.cpp`. Because of the second override, `list_metadata_lua`/`get_metadata_lua` (templates) move into `internal.h` and the two `metadata_to_lua` overloads get `internal.h` declarations. That is about 22 lines of new `internal.h` content. Neither override is a problem: no name is registered twice, and registration order between binders is not observable (see Pitfall 4).

The study's proposal misses one trap, and it is the one that matters: **`binary.cpp` must keep `#include "quiver/expression/expression.h"` after the split, even though nothing in it names `Expression` any more.** sol2 adds `__lt`/`__le`/`__eq` to the `BinaryFile` metatable at `new_usertype<BinaryFile>` time, from `meta::supports_op_less<BinaryFile>` (`build/_deps/sol2-src/include/sol/usertype_core.hpp:131-149`). That trait is true only because ADL finds `quiver::operator<(const Expression&, const Expression&)` through the implicit `Expression(const BinaryFile&)`. If the include goes, `f < g` becomes an error and `f == g` turns into identity equality. That is a script-visible change, and no test pins it. A runtime surface probe (written and run this session, see Code Examples) detects it.

**Primary recommendation:** 3 sequential plans. (1) Copy the `build/fixes-check` harness to `build/layout-check`, add the runtime surface probe, rebuild Debug and Release at the base, and capture the baselines. Then commit a rename-only `git mv` of the three files plus the CMake rename, and a tracer move (`database_describe.cpp`). (2) Carve `db_core`, `db_write` and `binary`, one commit per source file, each gated. (3) Docs (24 grep hits to 0, plus the `binary.cpp`→`expression.cpp` citations the grep cannot see), the Release/tidy/six-suite gate, and no CHANGELOG entry.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Lua name registration (`bind.set_function`, `ns.set_function`, usertypes) | Lua binding layer (`src/lua_runner/*.cpp`, sol2) | — | Only file placement changes. Names, signatures and adapter bodies move verbatim. |
| Binder wiring and constructor order | `src/lua_runner/lua_runner.cpp` (`LuaRunner::Impl` ctor) | `internal.h` (declarations) | The one `new_usertype<Database>` and the stdlib/sandbox setup stay here. Only the binder calls change. |
| Shared marshalling helpers | `internal.h` (templates, inline, declarations) | defining `.cpp` (non-template bodies) | The existing src/AGENTS.md rule: internal.h holds only templates, `inline` and declarations. |
| Filesystem sandbox | `path_policy.cpp` (`resolve_sandboxed_path`) | each file-op binder (call site) | Untouched. 10 call sites move with their lambdas. |
| Business logic and error text | C++ core (`src/database_*.cpp`, `src/binary`, `src/expression`) | — | Not touched. The core files are only the naming authority for placement. |

## Project Constraints (from AGENTS.md / src/AGENTS.md)

No `CLAUDE.md` or project skills exist (`.claude/skills`, `.agents/skills` absent). Directives that bind this phase:

- **Self-Updating:** keep the nearest AGENTS.md current. Root `AGENTS.md` and `src/AGENTS.md` must describe the new files (LAYOUT-05).
- **Changelog:** only user-visible changes get an entry. This phase is internal with no Lua-visible change, so **no CHANGELOG entry** and no version bump (it stays 0.13.0). The design study reaches the same conclusion.
- **Philosophy:** delete unused code, do not deprecate. No compatibility shims (for example, no `bind_core` forwarding to the new binders).
- **Error messages:** every Pattern 1/2/3 string stays byte-identical. Moves are verbatim.
- **src/AGENTS.md Layout bullet (lines 648-661):** binder parameters are named `bind` (Database usertype) and `ns` (the `quiver` table). internal.h holds only templates, `inline` functions and declarations. Every other helper goes in an anonymous namespace nested in `quiver::lua_internal`. A type registered as a usertype stays in a named namespace. **Comments must not spell `new_usertype<Database>` or `open_libraries(`**, because the greps count both. There is one `NOLINTBEGIN/END(performance-unnecessary-value-param)` pair per TU with by-value sol2 parameters, and files stay at about 450 lines or fewer.
- **Each lua_runner TU includes only the headers it uses** (v0.12.9 Phase 2 decision; STATE.md).
- **clang-format is pinned to 22.1.8.** PATH has 22.1.3 (VS LLVM). Run `uvx --from clang-format==22.1.8 clang-format`.
- **Do Not Fix:** no drive-by lint fixes in files you only move.
- **No planning IDs in code or test comments** (ROADMAP overview). That covers `LAYOUT-0x`, `EXPR-`, `LUA-`, `JUL-`, `DOC-`, `Phase N`, `Pitfall N` and `RESEARCH.md`.
- **Never stage `.planning/config.json`** (the v0.12.9 orchestrator rule, still in force).
- **Python via `uv run python`.** Git Bash is the shell. Call test binaries directly, because quoted filters break through `cmd //c` (user memory).

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| LAYOUT-01 | 14 binder files, each Lua name in the file named after its core file; `return_json.cpp` and `path_policy.{h,cpp}` unchanged; `lua_runner.cpp`/`internal.h` changed only for binders and shared helpers; no file over ~450 lines | §Complete Mapping (all 86 + usertypes, core file:line verified), §Shared Helpers, §Projected Line Counts |
| LAYOUT-02 | 14 binders named after files, called in core order; `bind_binary` returns the `BinaryFile` usertype that goes to `bind_expression`; ctor-order invariants; one `new_usertype<Database>` | §Binder Declarations and Call Order (exact signatures), Pitfall 4 (order is not observable) |
| LAYOUT-03 | Behaviour-neutral: counts, all suites, lua-api sync, golden Debug/Release byte-identical | §Verification Harness (existing harness verified, plus a new runtime surface probe), Pitfall 1 (the expression.h include) |
| LAYOUT-04 | Explicit `QUIVER_SOURCES`, NOLINT pairs, clang-format 22.1.8, tidy at most 14, `git mv` for renames | §CMake, §NOLINT Placement, §Tidy Baseline (expected per-file redistribution), §Commit Sequencing |
| LAYOUT-05 | Every old-name citation updated; `git grep` 24 → 0 | §Citation Inventory (all 24 hits plus 6 `binary.cpp`→`expression.cpp` citations the grep cannot see) |
</phase_requirements>

## Standard Stack

No new dependency, and nothing is installed. Everything is already vendored or on the machine.

| Tool | Version (verified this session) | Used for |
|------|------|------|
| sol2 | v3.5.0 (`build/_deps/sol2-src`) | binding (unchanged) |
| Lua | 5.4.8 | runtime (unchanged) |
| CMake / Ninja | 4.3.1-msvc1 / 1.13.2 | build; `cmake --build build` (Debug), `cmake --build --preset release` (→ `build/release/`) |
| clang-format | 22.1.8 via `uvx --from clang-format==22.1.8 clang-format` | format gate |
| run-clang-tidy | VS LLVM (`C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin/run-clang-tidy`) | tidy gate |
| bun | 1.3.14 | lua-api sync test, `surface.ts` |
| Docker | 29.6.2 | optional GCC 14 syntax pass (`gcc:14`), as in `build/fixes-check/wave_gate.sh` |
| Julia / Dart / uv | 1.11.9 / 3.13.4 / 0.12.3 | six-suite run |

## Package Legitimacy Audit

Not applicable. This phase installs no external packages.

## Current Inventory (HEAD = `da6f67b` sources)

`wc -l src/lua_runner/*` [VERIFIED: wc this session]:

| File | Lines | Binder(s) / role | set_function count |
|------|-------|------------------|-----|
| `lua_runner.cpp` | 173 | `RunHandles` bodies, `LuaRunner::Impl` ctor (binder calls at :113-119), `run()` | 0 |
| `internal.h` | 317 | helpers; binder decls :295-301; `table_to_element`/`collect_group_columns`/`columns_to_cpp_rows` decls :305-311 | 0 |
| `return_json.cpp` | 225 | `encode_return_json` | 0 |
| `path_policy.h` / `.cpp` | 18 / 64 | `resolve_sandboxed_path` | 0 |
| `db_core.cpp` | 205 | `bind_core` (:133-202) | 22 `bind` |
| `db_read.cpp` | 129 | `bind_read` (:108-127) | 14 `bind` |
| `db_write.cpp` | 311 | `bind_write` (:294-308) | 11 `bind` |
| `db_metadata.cpp` | 91 | `bind_metadata` (:79-89) | 8 `bind` |
| `db_time_series.cpp` | 281 | `bind_time_series` (:261-278) | 10 `bind` |
| `csv.cpp` | 416 | `bind_csv` (:309-413), `CsvWriter` usertype | 3 `bind` |
| `binary.cpp` | 345 | `bind_binary` (:184-342): BinaryMetadata, BinaryFile, Expression usertypes, operators, `quiver.*` | 3 `bind` + 15 `ns` |
| **Total** | 2575 | 7 binders | **86 = 71 `bind` + 15 `ns`** |

Other base facts [VERIFIED this session]: 5 `new_usertype<` (Database, BinaryMetadata, BinaryFile, Expression, CsvWriter); 0 duplicate registered names; `surface.ts` lists 107 lines; `Lua*` 477 in 12 suites, `quiver_tests` 1454, `quiver_c_tests` 543; `bun test test/lua-api-sync.test.ts` gives 6 pass.

Current constructor (`lua_runner.cpp:91-121`) [VERIFIED: Read]:
```cpp
sol::table ns = lua.create_named_table("quiver");
// The only Database usertype: registering it again would clear every method bound before.
auto bind = lua.new_usertype<Database>("Database");
lua_internal::bind_core(bind);
lua_internal::bind_read(bind);
lua_internal::bind_write(bind);
lua_internal::bind_metadata(bind);
lua_internal::bind_time_series(bind);
lua_internal::bind_csv(lua, bind, handles);
lua_internal::bind_binary(lua, bind, ns, db, handles);
lua["db"] = &db;
```

## Complete Mapping (every registration → target)

The core file is the one that defines the C++ method, verified by `grep -noE '^[A-Za-z].*\bDatabase::[a-z_]+\(' src/database*.cpp` this session. Binder names are the LAYOUT-02 list.

### `database.cpp` — `bind_database` (13)
| Lua name | Now | Core |
|---|---|---|
| `is_healthy` | db_core.cpp:134 | src/database.cpp:129 |
| `current_version` | db_core.cpp:135 | src/database.cpp:239 |
| `path` | db_core.cpp:136 | src/database.cpp:243 |
| `begin_transaction` / `commit` / `rollback` / `in_transaction` | db_core.cpp:137-140 | src/database.cpp:319 / 334 / 345 / 330 |
| `transaction` (lambda over `run_in_scope`) | db_core.cpp:141-150 | **Lua-only composite** over database.cpp:319/334/345 → database.cpp (locked by LAYOUT-01) |
| `begin_dry_run` / `end_dry_run` / `in_dry_run` | db_core.cpp:151-153 | src/database.cpp:356 / 368 / 386 |
| `dry_run` (lambda over `run_in_scope`) | db_core.cpp:154-163 | **Lua-only composite** → database.cpp (locked) |
| `validate_migrations` (sandboxed lambda) | db_core.cpp:198-201 | src/database.cpp:268 (static) |

### `database_create.cpp` — `bind_create` (1)
| `create_element` (`create_element_lua`) | db_write.cpp:298 | src/database_create.cpp:5 |

### `database_read.cpp` — `bind_read` (15)
| `read_element_ids` | db_read.cpp:109 | src/database_read.cpp:264 |
| `read_scalar_{strings,integers,floats}` | db_read.cpp:111-113 | :26 / :6 / :16 |
| `read_vector_{integers,floats,strings}` | db_read.cpp:115-117 | :69 / :80 / :91 |
| `read_set_{integers,floats,strings}` | db_read.cpp:119-121 | :138 / :149 / :160 |
| `read_scalars_by_id` / `read_vectors_by_id` / `read_sets_by_id` / `read_element_by_id` | db_read.cpp:123-126 | **composites**: `list_*` (database_metadata.cpp:55/67/77) + `read_*_by_id` (database_read.cpp). They stay in database_read.cpp because they are read operations that already live there. |
| `number_of_elements` | **db_core.cpp:188** | src/database_read.cpp:270 → moves here (locked) |

### `database_update.cpp` — `bind_update` (8)
| `update_element` / `update_element_by_label` | db_write.cpp:300-301 | src/database_update.cpp:9 / :63 |
| `update_relation` / `update_relation_by_label` | db_write.cpp:302-303 | :71 / :119 |
| `update_vector_group` / `_by_label` | db_write.cpp:304-305 | :216 / :227 |
| `update_set_group` / `_by_label` | db_write.cpp:306-307 | :241 / :252 |

### `database_delete.cpp` — `bind_delete` (2)
| `delete_element` / `delete_element_by_label` (member pointers) | db_write.cpp:295-296 | src/database_delete.cpp:5 / :17 |

### `database_describe.cpp` — `bind_describe` (3)
| `describe` / `describe_collection` / `summarize_collection` (member pointers) | db_core.cpp:190-192 | src/database_describe.cpp:232 / 254 / 262 |

### `database_metadata.cpp` — `bind_metadata` (6, was 8)
| `get_scalar_metadata` / `get_vector_metadata` / `get_set_metadata` | db_metadata.cpp:80-82 | src/database_metadata.cpp:6 / 18 / 37 |
| `list_scalar_attributes` / `list_vector_groups` / `list_set_groups` | db_metadata.cpp:85-87 | :55 / :67 / :77 |

### `database_query.cpp` — `bind_query` (3)
| `query_string` / `query_integer` / `query_float` | db_core.cpp:194-196 | src/database_query.cpp:6 / 10 / 14 |

### `database_time_series.cpp` — `bind_time_series` (12, was 10)
| `has_time_series_files` | db_time_series.cpp:262 | src/database_time_series.cpp:349 |
| `read_time_series_group` / `read_time_series_row` / `read_time_series_files` | :264-266 | :97 / :289 / :368 |
| `update_time_series_group` / `_by_label` | :268-269 | :141 / :215 |
| `upsert_time_series_row` / `_by_label` | :270-271 | :229 / :275 |
| `update_time_series_files` | :272 | :415 |
| `list_time_series_files_columns` (multi-line) | :274-277 | :355 |
| `get_time_series_metadata` | **db_metadata.cpp:83** | src/database_time_series.cpp:77 → moves here (locked) |
| `list_time_series_groups` | **db_metadata.cpp:88** | src/database_time_series.cpp:67 → moves here (locked) |

### `database_csv_export.cpp` — `bind_csv_export` (1) / `database_csv_import.cpp` — `bind_csv_import` (1)
| `export_csv` | db_core.cpp:164-175 | src/database_csv_export.cpp:123 |
| `import_csv` | db_core.cpp:176-186 | src/database_csv_import.cpp:330 |

### `csv.cpp` — `bind_csv` (3 + CsvWriter usertype), unchanged
`read_csv`, `read_csv_stream`, `write_csv` (no Database method; core `src/csv/csv_read.cpp`/`csv_write.cpp`); `CsvWriter:write_row`, `CsvWriter:close`.

### `binary.cpp` — `bind_binary` (3 `bind` + 3 `ns` + 2 usertypes), returns `sol::usertype<BinaryFile>`
`open_file` (:187-201), `bin_to_csv` (:202-207), `csv_to_bin` (:208-210) [core `src/binary/binary_file.cpp`, `csv_converter.cpp`]; `BinaryMetadata` usertype (:212-239, 7 methods); `BinaryFile` usertype (:241-267: read, write, close, is_open, get_metadata, get_file_path); `quiver.metadata`, `quiver.metadata_from_toml`, `quiver.metadata_from_element` (:271-277; the last calls `table_to_element`, declared in internal.h).

### `expression.cpp` — `bind_expression` (12 `ns` + Expression usertype + operators on both usertypes)
`Expression` usertype (:280-318: save, metadata, aggregate, aggregate_agents, select_agents, rename_agents); `bind_expression_operators(expression_type)` (:319) **and** `bind_expression_operators(binary_file_type)` (moved from :269); `quiver.expression, abs, sqrt, log, exp, ifelse, gt, lt, gte, lte, eq, neq` (:321-339).

**Totals after the move:** bind 13+1+15+8+2+3+6+3+12+1+1+3+3 = 71, ns 3+12 = 15, total 86. Unchanged.

### Ambiguities (all resolved; none needs a user decision)
1. `transaction`/`dry_run`: Lua-only composites → `database.cpp` (LAYOUT-01 names "transactions, dry runs").
2. `validate_migrations` is a static member in `src/database.cpp:268` → `database.cpp` (locked).
3. The `*_by_id` composites read through `list_*` (metadata) but are reads → `database_read.cpp` (unchanged location).
4. Every `_by_label` form lives in the same core file as its id form (verified above) → same Lua file.
5. `read_csv`/`read_csv_stream`/`write_csv` have no Database counterpart → `csv.cpp` mirrors `src/csv/` (unchanged).
6. `open_file`/`bin_to_csv`/`csv_to_bin` are `db:` methods, but their core is `src/binary/` → `binary.cpp`.
7. `quiver.metadata_from_element` builds an Element → stays in `binary.cpp` (BinaryMetadata core) and reaches `table_to_element` through internal.h.
8. BinaryFile *operators* belong to the Expression surface (they call `Expression` operators) → registered by `bind_expression` on the usertype `bind_binary` returns.

## Shared Helpers: Where Each Lives

| Helper | Now | After | internal.h change? |
|---|---|---|---|
| `run_in_scope` | db_core.cpp:66-103 (anon) | database.cpp anon | no |
| `lua_table_to_values`, `query_*_lua` | db_core.cpp:105-129 (anon) | database_query.cpp anon | no |
| `string_key` | db_core.cpp:19-24 (anon) | database_csv_export.cpp anon | no |
| `parse_csv_options` | db_core.cpp:29-64 (anon), used by export **and** import | **defined in database_csv_export.cpp (named, `lua_internal`), declared in internal.h** | **+1 decl**: `CSVOptions parse_csv_options(const sol::object& options, const std::string& operation);` (`CSVOptions` is visible through `quiver/database.h:7` → `quiver/options.h`) |
| `require_dense_array` | db_write.cpp:26-38 (anon) | database_create.cpp anon | no |
| `table_to_element` | db_write.cpp:51-99 (named) | database_create.cpp (named) | no (decl already at internal.h:305; users: create, update, binary) |
| `join_column_names` | db_write.cpp:41-47 (anon) | database_update.cpp anon | no |
| `collect_group_columns`, `columns_to_cpp_rows` | db_write.cpp:101-159 (named) | database_update.cpp (named) | no (decls at internal.h:306-311; also used by database_time_series.cpp) |
| `create_element_lua` | db_write.cpp:163-166 | database_create.cpp anon | no |
| `update_element*_lua`, `relation_target_from_lua`, `update_relation*_lua`, `group_rows_from_lua`, `update_{vector,set}_group*_lua` | db_write.cpp:168-290 | database_update.cpp anon | no |
| `lua_data_type_name` | db_metadata.cpp:13-21 (anon) | database_metadata.cpp anon | no |
| `metadata_to_lua(ScalarMetadata)`, `metadata_to_lua(GroupMetadata)` | db_metadata.cpp:23-56 (anon) | **database_metadata.cpp, named `lua_internal`** (out of the anon namespace) | **+2 decls** |
| `list_metadata_lua<List>`, `get_metadata_lua<Get>` (templates) | db_metadata.cpp:58-75 (anon) | **internal.h** (templates must be in the header to serve both TUs; same precedent as `bulk_read_lua`/`collection_read_lua` at internal.h:89-103) | **+~18 lines** |
| `read_scalars_by_id_lua`, `read_groups_by_id`, `read_*_by_id_lua` | db_read.cpp:14-104 | database_read.cpp anon (unchanged) | no |
| time-series helpers (`value_to_lua_object`, `length_mismatch`, …) | db_time_series.cpp:19-257 | unchanged | no |
| `lua_table_to_dim_map`, `metadata_string`, `metadata_array`, `build_metadata_from_lua`, `dimension_to_lua` | binary.cpp:27-93 | binary.cpp anon. `build_metadata_from_lua` **stays above the usertypes** (sync-test pass 2, comment at :53-55) | no |
| `is_number`, `to_expression`, `binop<Op>`, `parse_aggregate_op`, `bind_expression_operators<T>` | binary.cpp:99-176 | expression.cpp anon (every caller ends up there) | no |
| `bulk_read_lua` | internal.h:93-97 (only db_read uses it) | **leave in internal.h** (moving it is optional churn; LAYOUT-01 wants internal.h minimal) | no |
| `RunHandles`, `to_lua_table`, `lua_cell_as`, `lua_to_value`, `optional_from_lua`, `option_entries`, … | internal.h | unchanged | no |

Why the templates go to internal.h and are not duplicated: v0.12.9 Phase 3 (dedupe) made "each repeated binding pattern lives in one helper" a written rule (src/AGENTS.md:662). Two private copies of `get_metadata_lua` would undo it. Moving a template out of an anonymous namespace does not change its function type, so sol2's Debug "bad argument into '<signature>'" text (types only) cannot change. The golden `debug_text.lua` re-checks that anyway.

## Binder Declarations and Call Order

`internal.h:295-301` is replaced by these lines. Parameter names `bind`/`ns` are load-bearing for the sync test.
```cpp
void bind_database(sol::usertype<Database>& bind);
void bind_create(sol::usertype<Database>& bind);
void bind_read(sol::usertype<Database>& bind);
void bind_update(sol::usertype<Database>& bind);
void bind_delete(sol::usertype<Database>& bind);
void bind_describe(sol::usertype<Database>& bind);
void bind_metadata(sol::usertype<Database>& bind);
void bind_query(sol::usertype<Database>& bind);
void bind_time_series(sol::usertype<Database>& bind);
void bind_csv_export(sol::usertype<Database>& bind);
void bind_csv_import(sol::usertype<Database>& bind);
void bind_csv(sol::state& state, sol::usertype<Database>& bind, RunHandles& handles);
sol::usertype<BinaryFile> bind_binary(sol::state& state, sol::usertype<Database>& bind, sol::table& ns, RunHandles& handles);
void bind_expression(sol::state& state, sol::table& ns, sol::usertype<BinaryFile>& binary_file_type, Database& db);
```
- `bind_binary` drops `Database& db`, because only `expr:save` captured it and that moves to `bind_expression`. Declaring a function that returns `sol::usertype<BinaryFile>` with `BinaryFile` only forward-declared (internal.h:28) is legal, and the study's scratch split compiled this shape.
- `bind_expression` keeps `[&db]` for `save` (it captures the referent, exactly as today).
- The `BinaryFile` usertype parameter may only get `[sol::meta_function::...] =` assignments. A `binary_file_type.set_function(` would fail the sync test's `setFns.length === .set_function( count` assertion.
- clang-format will wrap the `bind_binary` declaration (it is over 120 columns). Let the pinned formatter decide.

Constructor (`lua_runner.cpp`). Only the binder lines change:
```cpp
lua_internal::bind_database(bind);
lua_internal::bind_create(bind);
lua_internal::bind_read(bind);
lua_internal::bind_update(bind);
lua_internal::bind_delete(bind);
lua_internal::bind_describe(bind);
lua_internal::bind_metadata(bind);
lua_internal::bind_query(bind);
lua_internal::bind_time_series(bind);
lua_internal::bind_csv_export(bind);
lua_internal::bind_csv_import(bind);
lua_internal::bind_csv(lua, bind, handles);
auto binary_file_type = lua_internal::bind_binary(lua, bind, ns, handles);
lua_internal::bind_expression(lua, ns, binary_file_type, db);
lua["db"] = &db;
```
`open_libraries` → nil `dofile`/`loadfile` → `load` wrapper → `quiver` table → `new_usertype<Database>` stay exactly as at `lua_runner.cpp:92-112`.

## CMake (`src/CMakeLists.txt:17-28`)

Replace the lua_runner block with this alphabetical list, which is the existing convention inside the block:
```
    lua_runner/binary.cpp
    lua_runner/csv.cpp
    lua_runner/database.cpp
    lua_runner/database_create.cpp
    lua_runner/database_csv_export.cpp
    lua_runner/database_csv_import.cpp
    lua_runner/database_delete.cpp
    lua_runner/database_describe.cpp
    lua_runner/database_metadata.cpp
    lua_runner/database_query.cpp
    lua_runner/database_read.cpp
    lua_runner/database_time_series.cpp
    lua_runner/database_update.cpp
    lua_runner/expression.cpp
    lua_runner/internal.h
    lua_runner/lua_runner.cpp
    lua_runner/path_policy.cpp
    lua_runner/path_policy.h
    lua_runner/return_json.cpp
```
The SOL_* defines (`src/CMakeLists.txt:74-81`, PRIVATE on `quiver`) and `/bigobj` (:89-93) are target-wide, so listing a file is all it takes [VERIFIED: Read]. Object paths stay distinct from the core's same-named files (`quiver.dir/lua_runner/database_read.cpp.obj` vs `quiver.dir/database_read.cpp.obj`). The root `CMakeLists.txt:92` `GLOB_RECURSE` (the format target) picks up new files without edits. Nothing in `tests/CMakeLists.txt` changes (only `path_policy.cpp` is referenced there, at :58).

## Projected Line Counts

Estimated from the source ranges above. The binary/expression figures come from the design study's compiled scratch split.

| File | Est. lines | NOLINT pair? |
|---|---|---|
| `lua_runner.cpp` | ~180 | no (none today) |
| `internal.h` | ~345 (317 + 7 decl lines + 1 `parse_csv_options` + 2 `metadata_to_lua` + ~18 templates) | no |
| `return_json.cpp` / `path_policy.{h,cpp}` | 225 / 18 / 64 (empty diff) | — |
| `database.cpp` | ~95 | optional (no by-value sol2 params) |
| `database_create.cpp` | ~100 | no |
| `database_read.cpp` | ~131 | no |
| `database_update.cpp` | ~237 | carried (wraps the moved `update_*_group_lua` block verbatim) |
| `database_delete.cpp` | ~15 | no |
| `database_describe.cpp` | ~16 | no |
| `database_metadata.cpp` | ~72 | no |
| `database_query.cpp` | ~50 | optional |
| `database_time_series.cpp` | ~285 | carried (git mv, untouched) |
| `database_csv_export.cpp` | ~80 | optional |
| `database_csv_import.cpp` | ~25 | optional |
| `csv.cpp` | 416 (unchanged) | **required** (by-value `sol::object` at :316, :342, :386) |
| `binary.cpp` | ~185 | no (after the split it has no by-value sol2 params) |
| `expression.cpp` | ~176 | **required** (by-value `sol::object` at binary.cpp:171, 175, 321-326 today) |

csv.cpp, at 416, is the largest file. No file comes near 450.

## NOLINT Placement

`grep -nE '(\(|, )(sol::(object|table|protected_function|function)) [a-z_]+' src/lua_runner/*.cpp | grep -v 'const sol::'` [VERIFIED this session] finds by-value sol2 parameters **only** in `binary.cpp` (unary-minus/bnot lambdas and `quiver.expression/abs/sqrt/log/exp/ifelse`, all of which go to expression.cpp) and `csv.cpp`. So LAYOUT-04's "every file whose functions take sol2 args by value" means **`expression.cpp` and `csv.cpp`**. The pairs in `db_core.cpp`, `db_write.cpp` and `db_time_series.cpp` wrap only `const&` code and suppress nothing.

Recommendation (lowest diff, rule-compliant):
- `expression.cpp`: one pair around the helper block, from `bind_expression_operators` through the end of `bind_expression`, the same span as binary.cpp:161-343 today.
- `csv.cpp`: unchanged.
- `database_time_series.cpp` (git mv) and `database_update.cpp` (verbatim block move): keep their existing pairs where the moved text already has them.
- Files carved out of `db_core.cpp` and `db_write.cpp`'s create/delete parts: leaving the pair out is correct (no by-value params). Copying it along is harmless. Pick one and state it in the SUMMARY.
- **The real gate:** tidy must report `performance-unnecessary-value-param` = 0 (the `uvp` counter in `build/fixes-check/wave_gate.sh:29`). That catches a missing pair. clang-tidy does not report unused NOLINTs.

## Tidy Baseline (14) and How It Redistributes

The baseline is measured by `build/fixes-check/wave_gate.sh:25-32`: `run-clang-tidy -p build -quiet 'src[\\/]lua_runner[\\/]'`, then `grep -E 'src.lua_runner.*warning:' | sort -u | wc -l` must be ≤ 14, with `performance-unnecessary-value-param` = 0 and `clang-diagnostic-error` = 0. The header warnings (`options.h`, `element.h`, `expression_node.h`, `datetime.h`) are outside that filter. The 14 [VERIFIED: `build/fixes-check/tidy.txt`]:

| Now | Check | After the move |
|---|---|---|
| return_json.cpp ×4 (:22, :27 ×2, :34) | identifier-naming ×3, implicit-widening | return_json.cpp ×4 (file untouched) |
| lua_runner.cpp ×2 (:62, :73) | bugprone-empty-catch | lua_runner.cpp ×2 |
| csv.cpp ×2 (:44, :70) | identifier-naming | csv.cpp ×2 |
| db_core.cpp ×1 (:95, `run_in_scope`'s inner catch) | bugprone-empty-catch | **database.cpp ×1** |
| binary.cpp ×3 (:88-90, `dimension_to_lua`'s `dim.time->`) | bugprone-unchecked-optional-access | binary.cpp ×3 |
| binary.cpp ×1 (:112, `to_expression`'s `return Expression(...)`) | modernize-return-braced-init-list | **expression.cpp ×1** |
| binary.cpp ×1 (:192, `"mode must be \"r\" or \"w\""`) | modernize-raw-string-literal | binary.cpp ×1 |

Expected after: still 14, the same 14 (check, code line) pairs, under new file names. Compare by check name plus source text, not by file:line. Do not fix any of them; that would be a drive-by. The new TUs only appear in `compile_commands.json` after CMake reconfigures, which `cmake --build build` does on its own when `src/CMakeLists.txt` changes. `scripts/tidy.bat` is the official entry point. `wave_gate.sh`'s direct call is the same tool with the `src/lua_runner/` filter.

## Citation Inventory (LAYOUT-05)

`git grep -nE 'db_core|db_read|db_write|db_metadata|db_time_series|bind_core|bind_write' da6f67b -- ':!.planning' | wc -l` = **24** [VERIFIED]:

| File:line | Fix |
|---|---|
| bindings/js/src/lua-api.ts:3 | "`bind_core` through `bind_binary`" → "`bind_database` through `bind_expression`" (comment only; the sync test does not read it) |
| src/AGENTS.md:52-56 (5 lines) | file-map block → the 14 new lines (see below) |
| src/AGENTS.md:265 | "`csv.cpp`, `internal.h`, `db_write.cpp`" → `database_update.cpp` (home of `collect_group_columns`) |
| src/AGENTS.md:665 | `read_groups_by_id` (`db_read.cpp`) → `database_read.cpp` |
| src/AGENTS.md:667 | `metadata_to_lua` / `list_metadata_lua` / `get_metadata_lua` (`db_metadata.cpp`) → templates in `internal.h`, overloads in `database_metadata.cpp`; the four `get_*`/`list_*` group methods now span `database_metadata.cpp` and `database_time_series.cpp` |
| src/AGENTS.md:670 | `run_in_scope` (`db_core.cpp`) → `database.cpp` |
| src/AGENTS.md:687 | `length_mismatch` (`db_time_series.cpp`) → `database_time_series.cpp` |
| src/AGENTS.md:697 | date_time_format / keys-of-option texts (`db_core.cpp`) → `database_csv_export.cpp` |
| src/AGENTS.md:699 | `target_label` (`db_write.cpp`) → `database_update.cpp` |
| src/CMakeLists.txt:19-23 (5) | new QUIVER_SOURCES block |
| src/lua_runner/db_core.cpp:133, db_write.cpp:294 | files deleted or renamed |
| src/lua_runner/internal.h:295, 297 | new decls |
| src/lua_runner/lua_runner.cpp:113, 115 | new calls |

**Stale citations the gate grep does NOT catch** (they name `binary.cpp` or binder counts) [VERIFIED: git grep this session]:
- `AGENTS.md:74`: "via sol2 (`src/lua_runner/binary.cpp`)" → `binary.cpp` and `expression.cpp`.
- `AGENTS.md:743`: "one file per domain" → reword, e.g. "one file per core file it binds (`database_*.cpp`) plus `csv`, `binary`, `expression`".
- `src/AGENTS.md:46`: "one file per domain" (same rewording); `:58` binary.cpp line → split into binary.cpp and expression.cpp lines.
- `src/AGENTS.md:649`: "hands it to the seven binders" → fourteen. Add that `bind_binary` returns the `BinaryFile` usertype, which goes to `bind_expression`.
- `src/AGENTS.md:682`: `binop<Op>(name)` (`binary.cpp`) → `expression.cpp`.
- `src/AGENTS.md:999` (Expression section): `src/lua_runner/binary.cpp` → `src/lua_runner/expression.cpp`. `:929` (Binary section) stays `binary.cpp`.
- Possibly add a `parse_csv_options` home note at src/AGENTS.md:758 (defined in `database_csv_export.cpp`, declared in `internal.h`).

Verified unaffected (names kept): `cmake/Platform.cmake:4`, `bindings/dart/hook/build.dart:55`, `bindings/dart/AGENTS.md:65`, `AGENTS.md:90/157/223/449-450/904`, `src/csv/*` comments, `tests/CMakeLists.txt:58,69`, `tests/test_sandboxed_path.cpp:1`, `tests/AGENTS.md`, `bindings/julia/AGENTS.md:160`, `bindings/js/AGENTS.md:39`, `lua-api-sync.test.ts` (recursive glob, names no files). There are no hits in `.github/`, `docs/`, `scripts/` or `CHANGELOG.md`. In-file comments that travel with the code stay true: binary.cpp:53-55, db_core.cpp:198 ("like the file I/O in binary.cpp", which moves to database.cpp), csv.cpp:310.

**Basename ambiguity:** `database_read.cpp` and the others now exist in `src/`, `src/c/` and `src/lua_runner/`. Docs must give the full path. Note that `AGENTS.md:449` / `Platform.cmake:4` "`database_csv_export.cpp`" means the core file, and stays correct because the Lua files there carry a `lua_runner/` prefix.

Suggested `src/AGENTS.md` file-map block (replaces :52-58):
```
    database.cpp          # bind_database: info, transactions, dry runs (run_in_scope), validate_migrations
    database_create.cpp   # bind_create: create_element; table_to_element
    database_read.cpp     # bind_read: bulk + by-id readers, number_of_elements
    database_update.cpp   # bind_update: update_element, relations, vector/set group writers; group decoder
    database_delete.cpp   # bind_delete: delete_element(_by_label)
    database_describe.cpp # bind_describe: describe, describe_collection, summarize_collection
    database_metadata.cpp # bind_metadata: get_{scalar,vector,set}_metadata, list_* ; metadata_to_lua
    database_query.cpp    # bind_query: query_string/integer/float
    database_time_series.cpp # bind_time_series: time-series read/write/upsert/files, its metadata + list
    database_csv_export.cpp  # bind_csv_export: export_csv; parse_csv_options
    database_csv_import.cpp  # bind_csv_import: import_csv
    csv.cpp               # bind_csv: read_csv, read_csv_stream, write_csv, CsvWriter
    binary.cpp            # bind_binary: open_file/bin_to_csv/csv_to_bin, BinaryMetadata, BinaryFile, quiver.metadata*
    expression.cpp        # bind_expression: Expression, operators on Expression and BinaryFile, quiver.* expression functions
```

## Common Pitfalls

### Pitfall 1: Dropping `expression.h` from `binary.cpp` silently changes `f < g` / `f == g`
**What goes wrong:** after the split, nothing in binary.cpp names `Expression`, so a reviewer or include-cleaner removes `#include "quiver/expression/expression.h"` (binary.cpp:8). The build still passes and every test is green.
**Why:** `new_usertype<BinaryFile>` runs sol2's `insert_default_registrations<BinaryFile>` (`usertype_core.hpp:126-149`), which registers `__lt`/`__le` only if `meta::supports_op_less<BinaryFile>` holds. It registers a real `__eq` only if `supports_op_equal` holds, and falls back to `no_comp` otherwise. Those traits are true only through ADL on `quiver::operator<(const Expression&, const Expression&)` via the implicit `Expression(const BinaryFile&)`. Observed this session: the `BinaryFile` metatable has `__lt`, `__le` and `__eq`; `BinaryMetadata` has only `__eq`.
**How to avoid:** keep the include in binary.cpp with a one-line comment saying why (sol2's automatic comparison metamethods on BinaryFile see the Expression operators). Phase 8's `EQ-01` deferral depends on today's behaviour staying.
**Warning signs:** the runtime surface probe's `BinaryFile` list loses `__lt:function` / `__le:function`.

### Pitfall 2: Sync-test parse traps
- Pass 1 (`lua-api-sync.test.ts:22-26`): every `.set_function(` must be on a receiver named `bind` or `ns`. `bind_expression`'s quiver-table parameter **must** be named `ns`.
- Pass 2 (:36-53) resets per file and treats any bare `"name",` line after a `new_usertype<…>` line as a method of that usertype. `build_metadata_from_lua`'s wrapped key list must stay above the usertypes in binary.cpp. No new file may put bare quoted lines below a `new_usertype`.
- The floor guard requires the BinaryFile, BinaryMetadata, Expression and CsvWriter usertypes to parse non-empty.
- `open_libraries(` and `new_usertype<Database>` must each appear once across the folder. A comment that spells either breaks the count.

### Pitfall 3: Rename detection lost
**What goes wrong:** `git log --follow src/lua_runner/database_metadata.cpp` stops at the move, because the commit's diff falls under git's 50% similarity threshold. db_metadata loses 2 registrations, its anonymous namespace and ~18 template lines, roughly 25% of the file.
**How to avoid:** make the first code commit a **rename-only** commit: `git mv` of the three files plus only the three CMake path lines. It builds and passes, since binder names do not change. Edit in later commits. Optionally also `git mv src/lua_runner/db_write.cpp src/lua_runner/database_update.cpp` in the carve commit; ~76% is kept, so git detects it. `db_core` → `database.cpp` keeps under 50%, so do not bother.
**Verify:** `git log --follow --oneline -- src/lua_runner/database_metadata.cpp` lists commits from before the rename (`da6f67b`).

### Pitfall 4: Treating binder order as behaviour
It is not, apart from the fixed constraints. No name is registered twice (verified: `uniq -d` is empty). sol2 resolves usertypes at call time (csv.cpp registers `CsvWriter` after `write_csv`). The only data dependency, `bind_binary` → `bind_expression`, is enforced by the return value. The one theoretical observable is Lua `pairs()` iteration order over the `Database` metatable or the `quiver` table, which can change with insertion order. Lua leaves that order unspecified, no test or doc pins it, and the probes sort. **[ASSUMED]** that this is acceptable as "not script-observable". The probes sort keys, so it cannot fail a gate.

### Pitfall 5: Stale binaries in the baseline or the six-suite run
- `build/bin` and `build/release/bin` are from 2026-10-03, older than the 10-04 checkout of the same sources. Rebuild **both** at the base before capturing any baseline.
- Dart: delete `bindings/dart/.dart_tool/hooks_runner/` and `bindings/dart/.dart_tool/lib/` before the six-suite run. The hook cache does not notice source-list changes (ROADMAP overview). Close any Julia/Python/Dart host holding `libquiver.dll`.

### Pitfall 6: Wrong clang-format
PATH has 22.1.3. Use `uvx --from clang-format==22.1.8 clang-format --dry-run --Werror src/lua_runner/*.cpp src/lua_runner/*.h`. Format new files with `-i` through the same command before committing. Long declarations (`bind_binary`) will wrap.

### Pitfall 7: Planning IDs in comments
Moved code is already clean. New comments, for example the expression.h include note, must not cite `LAYOUT-0x`, `Phase 6`, `Pitfall 1` or `RESEARCH.md`. Use `build/fixes-check/gate.sh`'s regex extended with `LAYOUT|EXPR|LUA|JUL`.

## Verification Harness

### What exists [VERIFIED this session]
- `build/fixes-check/golden.sh <debug|release> [--capture]` runs 10 probe scripts (`scripts/{binary,core,csv,handles_run1,handles_run2,options,read_all_types,read_collections,write,write_multi_dim}.lua`, each starting with `-- schema: <file>.sql`, with `prelude.lua` spliced in) through `quiver_cli --schema tests/schemas/valid/<schema> <db> <script>`. Debug also runs `debug_text.lua`. It byte-compares stdout against `baseline/debug`, and **Release must match the Debug baseline**. This session: `GOLDEN debug OK`, `GOLDEN release OK` against the current binaries, about 25 s.
- `build/split-check/surface.ts`: the static surface (sync-test parse), `bun run build/split-check/surface.ts src/lua_runner`, 107 lines at base.
- `build/fixes-check/gate.sh` / `wave_gate.sh`: per-commit and per-wave gates (counts, sync test, surface diff, set_function/usertype/open_libraries counts, the name==member check, clang-format, ≤450 lines, planning-ID grep, tidy, GCC 14 syntax pass). Their `444` base and `BASE` file are Phase 3/4 values. Copy and adjust them; do not run them as they are.

### Recommended for Phase 6 (`build/layout-check/`, gitignored)
1. `cp -r build/fixes-check/{golden.sh,prelude.lua,scripts,debug_text.lua} build/layout-check/` and change `HERE=build/fixes-check` to `build/layout-check` in golden.sh.
2. Add `scripts/surface.lua` (Code Examples). It dumps every string key of the Database, BinaryMetadata, BinaryFile, Expression and CsvWriter metatables plus the `quiver` table, with value types. That covers metamethods, which neither `surface.ts` nor the sync test sees. Tested this session: 4090 bytes, identical in Debug and Release.
3. Write `git rev-parse HEAD > build/layout-check/BASE`. Prove the base equals `da6f67b`: `git diff --quiet da6f67b HEAD -- src include tests cmake CMakeLists.txt`.
4. Rebuild: `cmake --build build --config Debug` and `cmake --build --preset release --target quiver_cli quiver_tests quiver_c_tests`. Then `bash build/layout-check/golden.sh debug --capture`, `bash build/layout-check/golden.sh release` (must be OK), and `bun run build/split-check/surface.ts src/lua_runner > build/layout-check/surface-base.txt`. Also save the sorted name list:
   ```bash
   cat src/lua_runner/*.cpp src/lua_runner/*.h | tr '\n' ' ' \
     | grep -oE '\b(bind|ns)\.set_function\(\s*"[a-z_]+"' | sed -E 's/\.set_function\(\s*"/ /; s/"$//' | sort \
     > build/layout-check/names-base.txt     # 86 lines
   ```
   For a git-object baseline that ignores the working tree, run the same pipeline over `git show da6f67b:src/lua_runner/<f>` for each file.
5. Per-commit gate (`build/layout-check/gate.sh`): Debug build; `Lua*` 477 in 12 suites; `SandboxedPathTest` 11; `LuaRunnerCApiTest` 27; the sync test; `surface.ts` diff empty; names diff empty; `.set_function(` = 86, `bind.` = 71, `ns.` = 15; `new_usertype<Database>` = 1; `open_libraries(` = 1; the name==member grep (`gate.sh:39-44`); clang-format 22.1.8; each file ≤450; `git diff --quiet $BASE -- src/lua_runner/return_json.cpp src/lua_runner/path_policy.h src/lua_runner/path_policy.cpp`; the planning-ID grep; `golden.sh debug`.
6. Phase gate (`wave_gate.sh`-style): Release build, `Lua*` 477 and C API 27 in Release, `golden.sh release`, full `quiver_tests` 1454 and `quiver_c_tests` 543 in Debug and Release, tidy ≤14 with uvp=0, the GCC 14 Docker syntax pass (cheap, and it catches a missing include that MSVC tolerates), then the six suites via `cmd //c 'scripts\test-all.bat'` after deleting the Dart caches, and the 24→0 grep.

Also check that the sandbox call sites survive: `grep -o 'resolve_sandboxed_path(\(self\|db\), "[a-z_]*"' src/lua_runner/*.cpp | sed 's/.*"\(.*\)"/\1/' | sort` must list the same 10 operations as at base (open_file, bin_to_csv, csv_to_bin, save, read_csv, read_csv_stream, write_csv, export_csv, import_csv, validate_migrations). The tests pin each one as well.

## Commit Sequencing and Plan Decomposition

Coarse granularity. Everything touches `internal.h`, `lua_runner.cpp` and `src/CMakeLists.txt`, so the plans are **sequential, one wave each**, and parallel work buys nothing. Each commit builds and passes the per-commit gate. Every binder call is placed at its **final** position the moment it is introduced, so the order converges without a separate reorder commit.

**Plan 06-01: harness + rename + tracer**
- T1: `build/layout-check` harness, base rebuild (Debug and Release), baseline capture (golden Debug and Release, runtime surface, `surface.ts`, names list, tidy output, the 24-hit grep list). No repo commit, since `build/` is gitignored. Record the counts in the SUMMARY.
- T2: commit "rename" with `git mv db_read.cpp database_read.cpp`, `db_metadata.cpp database_metadata.cpp`, `db_time_series.cpp database_time_series.cpp`, plus the three CMake lines. Gate, then verify that `git log --follow` crosses the rename.
- T3: tracer commit. Create `database_describe.cpp` + `bind_describe` (decl, call between `bind_delete`'s slot and `bind_metadata`, CMake line), remove the three lines from `bind_core`, and move `number_of_elements` into `bind_read`. Run the full gate including golden Release. This proves the whole pipeline end to end.

**Plan 06-02: carve the three multi-domain files**
- T1: `db_core.cpp` → `database.cpp` (`bind_database`, `run_in_scope`), `database_query.cpp`, `database_csv_export.cpp` (+ `parse_csv_options` decl in internal.h), `database_csv_import.cpp`. Delete `db_core.cpp` and `bind_core`. Can be one commit or one per file.
- T2: time-series metadata pair → `database_time_series.cpp`. Move the `list_metadata_lua`/`get_metadata_lua` templates to internal.h and the `metadata_to_lua` decls to internal.h, with the definitions named in `database_metadata.cpp`.
- T3: `db_write.cpp` → (`git mv` to) `database_update.cpp` (`bind_update`), plus new `database_create.cpp` (`bind_create`, `table_to_element`, `require_dense_array`) and `database_delete.cpp` (`bind_delete`). `bind_write` is gone.
- T4: `binary.cpp` → `binary.cpp` (`bind_binary` returns the usertype, drops `Database&`, **keeps the expression.h include with a comment**) + `expression.cpp` (`bind_expression`, operators for both usertypes, NOLINT pair). Golden plus the runtime surface probe are the decisive checks here.

**Plan 06-03: docs + phase gate**
- T1: root AGENTS.md (:74, :743), src/AGENTS.md (file map, :265, :649, :665, :667, :670, :682, :687, :697, :699, :999, optionally :758), `lua-api.ts:3`. `git grep -nE 'db_core|db_read|db_write|db_metadata|db_time_series|bind_core|bind_write' -- ':!.planning'` must return 0, and `git grep -n 'lua_runner/binary.cpp' -- AGENTS.md src/AGENTS.md` may name it only where the BinaryFile/BinaryMetadata/file I/O side is meant.
- T2: phase gate: Release, tidy (the 14 redistribution above), GCC 14 syntax pass, full suites Debug and Release, six suites after the Dart cache delete. No CHANGELOG entry. Record the SUMMARY facts: renames, the tidy set, "reordered checks: none", and "no Lua-visible change (golden + runtime surface identical)".

## Code Examples

### Runtime surface probe (tested this session, Debug == Release)
```lua
-- schema: collections.sql
-- Runtime surface: every string key of each usertype's metatable (methods + metamethods) and of
-- the quiver table, sorted. Identical output = identical Lua-visible registration set.
local R = {}
local function keys(t)
  local out = {}
  if type(t) ~= "table" then return { "<" .. type(t) .. ">" } end
  for k, v in pairs(t) do
    if type(k) == "string" then out[#out + 1] = k .. ":" .. type(v) end
  end
  table.sort(out)
  return out
end
local md = quiver.metadata({
  initial_datetime = "2025-01-01T00:00:00", unit = "MW", labels = { "v1" },
  dimensions = { "stage" }, dimension_sizes = { 2 }, time_dimensions = { "stage" }, frequencies = { "monthly" },
})
local f = db:open_file("surface_probe", "w", md)
local e = quiver.expression(f)
local w = db:write_csv("surface_probe.csv")
R.Database = keys(getmetatable(db))
R.BinaryMetadata = keys(getmetatable(md))
R.BinaryFile = keys(getmetatable(f))
R.Expression = keys(getmetatable(e))
R.CsvWriter = keys(getmetatable(w))
R.quiver = keys(quiver)
w:close()
f:close()
return R
```
`golden.sh` splices `prelude.lua` after line 1. The probe does not use the prelude helpers, so it works either way. Observed `BinaryFile` keys at base: `__add __band __bnot __bor __div __eq __gc __index __le __lt __mul __name __newindex __pairs __sub __type __unm class_cast class_check close get_file_path get_metadata is_open new read write`.

### `bind_binary` tail and `bind_expression` head (shape)
```cpp
// binary.cpp
// Kept although nothing here names Expression: sol2 derives BinaryFile's automatic __lt/__le/__eq
// from the Expression operators (through Expression(const BinaryFile&)) when the usertype is made.
#include "quiver/expression/expression.h"
...
sol::usertype<BinaryFile> bind_binary(sol::state& state, sol::usertype<Database>& bind, sol::table& ns, RunHandles& handles) {
    ... // open_file, bin_to_csv, csv_to_bin, BinaryMetadata usertype (verbatim)
    auto binary_file_type = state.new_usertype<BinaryFile>(/* verbatim */);
    ns.set_function("metadata", ...);              // verbatim, x3
    return binary_file_type;
}

// expression.cpp
void bind_expression(sol::state& state, sol::table& ns, sol::usertype<BinaryFile>& binary_file_type, Database& db) {
    // Arithmetic on files mirrors Julia: file_a + file_b, -file, file * 2.0 (auto-wrap to Expression)
    bind_expression_operators(binary_file_type);
    auto expression_type = state.new_usertype<Expression>(/* verbatim, [&db] save */);
    bind_expression_operators(expression_type);
    ns.set_function("expression", ...);            // verbatim, x12
}
```

### internal.h additions (shared metadata helpers)
```cpp
sol::table metadata_to_lua(sol::state_view& lua, const ScalarMetadata& attribute);
sol::table metadata_to_lua(sol::state_view& lua, const GroupMetadata& metadata);

// list_scalar_attributes / list_{vector,set,time_series}_groups: one metadata table per entry.
template <auto List>
sol::table list_metadata_lua(Database& db, const std::string& collection, sol::this_state s) { /* verbatim db_metadata.cpp:60-68 */ }

// get_{scalar,vector,set,time_series}_metadata: the one named attribute or group.
template <auto Get>
sol::table get_metadata_lua(Database& db, const std::string& collection, const std::string& name, sol::this_state s) { /* verbatim :72-75 */ }

CSVOptions parse_csv_options(const sol::object& options, const std::string& operation);
```

## Runtime State Inventory

| Category | Items Found | Action Required |
|----------|-------------|-----------------|
| Stored data | None. No database or file stores a binder or file name. Lua names are unchanged. | none |
| Live service config | None | none |
| OS-registered state | None | none |
| Secrets/env vars | None | none |
| Build artifacts | (1) `build/` and `build/release/` objects for `db_*.cpp` are orphaned after the rename. Ninja ignores them, so this is harmless. (2) The Dart hook cache (`bindings/dart/.dart_tool/hooks_runner/`, `.dart_tool/lib/`) holds its own native build and does not notice the source-list change. (3) `compile_commands.json` must be regenerated (automatic on build) before tidy. | Delete the Dart caches before the six-suite run. Rebuild Debug and Release before the gates. |

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| MSVC + CMake + Ninja | build | ✓ | CMake 4.3.1, Ninja 1.13.2 | — |
| clang-format 22.1.8 (uvx) | format gate | ✓ | 22.1.8 | — |
| run-clang-tidy (VS LLVM) | tidy gate | ✓ | path verified | — |
| bun | sync test, surface.ts | ✓ | 1.3.14 | — |
| Docker | GCC 14 syntax pass (optional) | ✓ (CLI 29.6.2; daemon not probed) | — | skip; PR CI covers Linux |
| Julia / Dart / uv (Python) | six suites | ✓ | 1.11.9 / 3.13.4 / 0.12.3 | — |
| `build/release/bin/quiver_cli.exe` | Release golden | ✓ (stale; rebuild) | — | — |

No missing dependencies.

## Security Domain

`security_enforcement` is on (ASVS L1). The phase adds no input surface. The only security-relevant risk is losing a check during the move.

| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V5 Input Validation | yes | The existing Pattern 1 decoders (`require_table`, `lua_cell_as`, `option_entries`, `optional_from_lua`) move verbatim; golden `options.lua`/`debug_text.lua` byte-compare their texts |
| V12 Files and Resources | yes | `resolve_sandboxed_path` stays the single gate. Its 10 call sites must survive the move (grep above). `path_policy.{h,cpp}` has an empty diff. `SandboxedPathTest` 11 plus the per-operation sandbox tests |
| V2/V3/V4/V6 | no | — |

| Threat | STRIDE | Mitigation |
|--------|--------|-----------|
| A moved file-op lambda loses its `resolve_sandboxed_path` call (path escape) | Elevation / Tampering | Verbatim moves; sandbox call-site grep (10 ops); existing escape tests per op |
| Lua stdlib/sandbox setup reordered (`dofile`/`loadfile`/`load` wrapper) | Elevation | Constructor lines :92-112 untouched; `open_libraries(` count = 1; the `load` text-mode tests |

## State of the Art

| Old | New | Impact |
|-----|-----|--------|
| 7 binders by domain (`db_core` mixes info/tx/count/describe/query/csv/migrations) | 14 binders, one per core file plus csv/binary/expression | A change to `src/database_X.cpp` has an obvious `src/lua_runner/database_X.cpp` counterpart |
| 9 sol2 TUs, long pole `binary.cpp` 42 s | 16 sol2 TUs (+7) | About +70 s CPU on a clean Debug build. Wall time should fall to about 20-25 s because binary/expression split the long pole (design study §sol2 TU cost, measured for +6 TUs). Not a gate. |

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | Lua `pairs()` order over the `Database` metatable / `quiver` table is not part of "nothing script-observable" | Pitfall 4 | Low. If the user disagrees, binder order would have to reproduce today's registration order exactly, which conflicts with LAYOUT-02's core order. |
| A2 | Dropping the now-dead NOLINT pairs from carved files (those without by-value sol2 params) is acceptable, as is copying them along | NOLINT Placement | Nil. tidy uvp=0 is the real gate either way. |
| A3 | Linux GCC/Clang Docker builds are not required for Phase 6 (not in its success criteria); a GCC 14 syntax pass is enough locally, with PR CI as backstop | Verification | Low. A missing include caught late in CI. |
| A4 | No CHANGELOG entry (internal, no user-visible change) | Project Constraints | Low. The design study concurs. AGENTS.md requires entries only for user-visible changes. |

## Open Questions

1. **Should the carved files carry the old (dead) NOLINT pairs?** Recommendation: no. Only `expression.cpp` and `csv.cpp` need them; the time-series and update files keep theirs because they are verbatim. State the choice in the SUMMARY.
2. **Should `bulk_read_lua` move from internal.h to `database_read.cpp`?** Recommendation: no. LAYOUT-01 wants internal.h changed only for binders and shared helpers the moves need.

## Sources

### Primary (HIGH, read or run this session)
- `src/lua_runner/{internal.h,lua_runner.cpp,db_core.cpp,db_read.cpp,db_write.cpp,db_metadata.cpp,db_time_series.cpp,binary.cpp}` (full) and `csv.cpp` (:1-60, :300-416)
- `src/CMakeLists.txt:1-100`, `.clang-tidy`, `scripts/tidy.bat`, `include/quiver/expression/expression.h:1-120`, `include/quiver/database.h` includes
- `build/_deps/sol2-src/include/sol/usertype_core.hpp:126-160` (automagic comparison registration)
- `bindings/js/test/lua-api-sync.test.ts:1-120`, `bindings/js/src/lua-api.ts:1-12`
- `build/fixes-check/{golden.sh,gate.sh,wave_gate.sh,linux.sh,tidy.txt}`, `build/split-check/surface.ts`
- Commands: wc, the set_function counts, the core method grep, `git grep` (24 hits), golden Debug/Release runs, the runtime surface probe Debug/Release, the sync test, gtest list counts

### Secondary
- `.planning/research/DESIGN-STUDY.md` §File layout mapping (lines 215-526) and `SUMMARY.md`. They are superseded where LAYOUT-01 differs (describe, time-series metadata).
- `.planning/milestones/v0.12.9-phases/03-dedupe/03-RESEARCH.md` (harness design), `05-path-policy-test-and-docs/05-RESEARCH.md:553` (tidy baseline method)

## Metadata

**Confidence breakdown:**
- Mapping and helpers: HIGH. Every registration and core definition was read or grepped this session.
- Verification harness: HIGH. It exists and passes now. The new surface probe was run in Debug and Release.
- Pitfall 1 (expression.h include): HIGH on mechanism (sol2 source read, metatable observed). Not reproduced by actually deleting the include.
- Line counts: MEDIUM. They are estimates from source ranges and the design study's compiled scratch split.

**Research date:** 2026-10-04
**Valid until:** until the next change to `src/lua_runner/` (the mapping is exact against `da6f67b`)
