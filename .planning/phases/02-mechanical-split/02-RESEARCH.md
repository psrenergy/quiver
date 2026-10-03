# Phase 2: Mechanical Split - Research

**Researched:** 2026-10-02
**Domain:** Splitting one sol2 (v3.5.0) / Lua 5.4.8 binding TU into per-domain TUs in a C++20 shared library, with no behaviour change
**Confidence:** HIGH. The proposed layout was built as a throwaway prototype from the current source, compiled with the real `quiver` flags (MSVC Debug, `/W4`, zero warnings), and linked into a scratch `libquiver.dll`. The **unmodified** test executables passed against it: `Lua*` 444/444 in 12 suites, `LuaRunnerCApiTest` 27/27, full `quiver_tests` 1410/1410. GCC/Clang and Release were not run on the prototype.

<user_constraints>
## User Constraints

There is no CONTEXT.md: the user chose to plan without discuss-phase. The locked constraints below are quoted from REQUIREMENTS.md, ROADMAP.md, PROJECT.md and STATE.md. The discretion items are settled here as recommendations.

### Locked Decisions (verbatim from REQUIREMENTS.md, Phase 2)
- **SPLIT-01**: `src/lua_runner.cpp` is replaced by `src/lua_runner/`: `lua_runner.cpp`, `internal.h`, `return_json.cpp`, `path_policy.cpp`, `db_core.cpp`, `db_read.cpp`, `db_write.cpp`, `db_metadata.cpp`, `db_time_series.cpp`, `csv.cpp`, `binary.cpp`. Each binder registers and implements its own slice, and no file is over ~450 lines.
- **SPLIT-02**: There is exactly one `new_usertype<Database>` in the folder (grep count is 1). Binders receive `sol::usertype<Database>& bind` and the `quiver` table as `ns`. The ctor order is preserved: `open_libraries` → nil `dofile`/`loadfile` → `quiver` table → binders → `lua["db"] = &db`.
- **SPLIT-03**: Instance state lives in a `RunHandles` (writer registry, binary-file registry, `path_has_open_writer`, close-at-exit). It is held by the heap-allocated `Impl` and declared before `lua`. The three `[this]` captures become `[&handles]` / `[&db]`. `GcGuard` is still declared before `result`, with close then exactly one `collect_garbage()`.
- **SPLIT-04**: The JS sync test reads every source file under `src/lua_runner/` in sorted order, parses each one separately (resetting `current` at each file boundary), and extracts the same method set as before the split.
- **SPLIT-05**: `/bigobj` (MSVC) and `-Wa,-mbig-obj` (MinGW) apply to the whole target. All sol2 TUs are in the `quiver` target with identical PRIVATE defines, listed explicitly (no glob). Each file has its own NOLINT pair, and `scripts/tidy.bat` and clang-format 22.1.8 are clean on the new files.
- **SPLIT-06**: No test expectation changes. All C++ Lua tests (`--gtest_filter=Lua*`: 428 at `bdf9087` plus the Phase 1 pins), all 27 C API tests, and the Julia, Dart, Python and JS suites pass unmodified.
- **SPLIT-07**: Every citation of `src/lua_runner.cpp` is updated to the new paths: the AGENTS.md files, `src/csv/*` comments, `cmake/Platform.cmake`, `bindings/dart/hook/build.dart`, the tests, and the `lua-api.ts` maintainer header. Re-derive the list with `git grep`. The C API translation unit `src/c/lua_runner.cpp` and `test_c_api_lua_runner.cpp` keep their names.

Also locked (ROADMAP Phase 2 success criteria, STATE.md, PROJECT.md Constraints):
- Baseline to reproduce exactly: `quiver_tests --gtest_filter=Lua*` = **444 tests / 12 suites**, `LuaRunnerCApiTest` = **27** (STATE.md, observed at `570c2c1`; re-confirmed this session against `build/bin`).
- The 17 variadic `Database` pairs become `bind.set_function` in this phase. The four non-`Database` usertypes stay variadic, one name per line.
- The code moved into `src/lua_runner/` carries no planning-ID comments.
- The PR notes the clean-build time of the `quiver` target before and after (recorded, not gated).
- After this phase the Dart suite runs only after deleting `bindings/dart/.dart_tool/hooks_runner/` and `.dart_tool/lib/`.
- `SOL_SAFE_NUMERICS`/`SOL_NO_NIL` (and the no-op `SOL_SAFE_FUNCTION`) stay PRIVATE on `quiver`. Write `sol::lua_nil`. csv-parser headers are never included from `src/lua_runner/`.
- One PR per phase into master, green on its own. No version bump, and no planning ID in any code or test comment.

### Claude's Discretion (settled here as recommendations)
| Item | Recommendation |
|------|----------------|
| Internal namespace | `quiver::lua_internal` (justification under Architecture Patterns) |
| Include spelling | `#include "lua_runner/internal.h"` |
| Include guard | `QUIVER_SRC_LUA_RUNNER_INTERNAL_H` |
| Binder names | `bind_core`, `bind_read`, `bind_write`, `bind_metadata`, `bind_time_series`, `bind_csv`, `bind_binary`, one per `db_*.cpp`/`csv.cpp`/`binary.cpp` |
| Shared-helper placement | Templates, `is_lua_boolean`, `lua_to_value` and `csv_options_entries` are `inline` in `internal.h`. `table_to_element` and the group decoder are declared in `internal.h` and defined once in `db_write.cpp`. Everything else is file-local |
| NOLINT | Fix the check name (see Pitfall 1). One pair per TU that has a by-value sol2 parameter |
| Commit sequence | 10 commits in 3 sequential plans (Commit Sequence section) |

### Deferred Ideas (OUT OF SCOPE for Phase 2)
- Dedupe M1, M3-M16 (Phase 3). Only the M2 half (17 pairs to `bind.set_function`) lands here.
- `require_table` / `lua_string_key` / `optional_from_lua` / `SOL_ALL_SAFETIES_ON` and the C1-C8 fixes (Phase 4). The `SOL_SAFE_FUNCTION=1` deletion is SAFE-06, also Phase 4.
- `resolve_sandboxed_path` unit test and its sol2-free header question (Phase 5, TEST-01).
- The repo-wide planning-ID sweep (Phase 5). Phase 2 strips only the code it moves.
- The rename of `LuaRunner` in any layer (dropped from the milestone).
- Moving the sol2-free half of the JSON encoder to `src/json/` (v2).
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| SPLIT-01 | Folder of 11 files, each binder registers and implements its slice, ~450-line ceiling | Cluster-to-file table with current line ranges. Prototype line counts after clang-format: max 445 (`csv.cpp`), fallback stated |
| SPLIT-02 | One `new_usertype<Database>`, `bind`/`ns` params, ctor order | sol2 source confirms `set_function` is behaviour-neutral against the variadic pairs. Root-TU code example. Comment-literal trap (Pitfall 3) |
| SPLIT-03 | `RunHandles` in heap `Impl` before `lua`, `[&handles]`/`[&db]`, GcGuard unchanged | Prototype passes all four Phase 1 move pins and the `sizeof` static_assert |
| SPLIT-04 | Sync test reads the folder sorted, per-file `current` reset, same method set | Exact code change. Throwaway `surface.ts` gives an identical 107-entry set on the prototype |
| SPLIT-05 | Target-wide `/bigobj`, explicit sources, NOLINT pair per file, tidy + clang-format clean | CMake snippet. The NOLINT pairs are no-ops today (wrong check name, proven). Tidy baseline: 65 warnings in the monolith. Commands use pinned clang-format 22.1.8 via `uvx`, not VS's 22.1.3 |
| SPLIT-06 | No expectation changes; 444 + 27 + four binding suites | Per-commit and phase-gate command lists, Dart cache caveat |
| SPLIT-07 | Every citation updated; C API TU names kept | 48 stale sites found by three `git grep` checks, all classified. Allowed survivors listed |
</phase_requirements>

## Project Constraints (from AGENTS.md)

No `CLAUDE.md` exists (`.planning/config.json` points at `./.claude/CLAUDE.md`, which is absent). The project instructions are the nested `AGENTS.md` files. Directives that bind this phase:

- **Self-Updating:** keep the AGENTS.md nearest each change current (root, `src/`, `tests/`, `bindings/js/`, plus the Dart/Julia AGENTS.md lines that cite the path).
- **Delete unused code, do not deprecate.** No compatibility shim may be left at `src/lua_runner.cpp`.
- **Logic in C++; bindings thin; error messages defined only in C++/C API.** Every message string moves byte-for-byte.
- **Do Not Fix:** do not relocate `LUA_DB_API_REFERENCE` (only its maintainer header comment changes here). Do not "clean up" `tests/sandbox`. Do not drive-by fix lint in untouched JS files.
- **Design decisions kept byte-for-byte:** the sandbox gate and its check order (`open_file` validates mode before path), the 6-library stdlib, `dofile`/`loadfile` nil'd, the JSON return contract, GcGuard order with exactly one `collect_garbage()`, the writer registry's `is_closed()` guard, `is_lua_boolean` as the single boolean predicate, and `is<int64_t>()` before `is<double>()`.
- **Code style:** clang-format 22.1.8, 120 columns. `.gitattributes` forces LF. Pre-commit runs trailing-whitespace, EOF and LF checks.
- **Running Python locally:** `uv run ...` (plain `python` is not on PATH).
- **Memory notes:** quoted gtest filters break through `cmd //c`, so call the executables directly. At GSD gates, self-verify adversarially.

## Summary

`src/lua_runner.cpp` is byte-identical to `bdf9087` (`git diff --stat bdf9087 -- src/lua_runner.cpp` prints nothing; 2,539 lines), so every line range in LUA-RUNNER-MAP.md and ARCHITECTURE.md still applies. ARCHITECTURE.md's design survives the dropped rename with only name changes: folder `src/lua_runner/`, root TU `src/lua_runner/lua_runner.cpp` (the `git mv` target), and namespace `quiver::lua_internal`. This session went further than restating it. I built the split as a throwaway prototype in the scratchpad: slice by current line ranges, de-indent out of `Impl`, convert the 17 pairs, run clang-format 22.1.8. Then I compiled it with the exact `quiver` compile command and linked it in place of `lua_runner.cpp.obj`. The existing test binaries passed against it unmodified (444 / 27 / 1410). The sync-test parse over the prototype folder extracts the identical 107-entry surface (71 `db:`, 15 `quiver.`, 21 usertype methods).

Measured sizes after clang-format: `csv.cpp` 445, `binary.cpp` 367, `db_write.cpp` 299, `db_time_series.cpp` 280, `db_read.cpp` 233, `db_core.cpp` 229, `internal.h` 227, `return_json.cpp` 222, `db_metadata.cpp` 159, `lua_runner.cpp` 146, `path_policy.cpp` 62. Only `csv.cpp` is near the ceiling (5 lines of margin), so the planning-ID strip there must be line-neutral or better.

Three findings change the plan:
1. **The four existing NOLINT pairs suppress nothing.** They name `performance-unnecessary-value-parameter`, but clang-tidy's check is `performance-unnecessary-value-param`. Proven on a minimal file: the wrong name does not suppress, the right one does. Tidy on today's monolith reports 65 warnings in `lua_runner.cpp`: 50 of that check (32 inside the "suppressed" blocks) plus 15 others. "Tidy clean" is therefore not reachable by moving code alone.
2. **The SPLIT-02 grep counts comments.** My prototype's ctor comment spelled `new_usertype<Database>`, so the count came out 2. The same goes for `open_libraries(`, which the sync test counts across every file. Comments must paraphrase.
3. **The local `clang-format` on PATH is VS's 22.1.3, and so is the one the CMake `format` target picks**, not the pinned 22.1.8. Use `uvx --from clang-format==22.1.8 clang-format`, as CI does.

**Primary recommendation:** Ten commits in three sequential plans, in this order: `git mv` + CMake + sync test, then a comment-only ID strip, then de-class in place, then six extraction commits, then docs. Every commit builds and passes `Lua*` + C API + the sync test, with a before/after surface diff. The six-suite run, the Release `Lua*` run, tidy, clang-format and build-time measurement are phase-gate items.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Lua `db:`/`quiver.*` surface registration | Binding TUs (`src/lua_runner/*.cpp`) | JS sync test (doc contract) | Each binder registers and implements its slice. The sync test parses the registrations textually |
| State lifecycle (`sol::state`, registries, GcGuard) | Root TU (`lua_runner.cpp`, `LuaRunner::Impl`) | — | Only the Pimpl owner knows `Impl`. Heap address stability is what keeps captured references valid across a move |
| Lua-to-C++ value conversion | Shared header (`internal.h`) | `db_write.cpp` (out-of-line defs) | Templates must be visible to every instantiating TU. The single boolean predicate must exist once |
| Filesystem containment gate | `path_policy.cpp` | — | The single security choke point, with no sol2 types in its logic |
| Return-value JSON encoding | `return_json.cpp` | — | A leaf with one exported entry point. Internals stay in its anonymous namespace |
| Validation, SQL, errors | C++ core (`Database`, `BinaryFile`, `Expression`) | — | Unchanged. Bindings stay thin |
| sol2 configuration (defines, `/bigobj`) | Build system (`src/CMakeLists.txt`, `quiver` target) | — | PRIVATE target-wide, never per file (ODR) |

## Standard Stack

No new libraries. The phase uses only what is vendored or installed.

### Core (in-repo, versions verified this session)
| Component | Version | Purpose | Source |
|-----------|---------|---------|--------|
| sol2 | v3.5.0 (vendored, `build/_deps/sol2-src`) | Lua binding | [CITED: AGENTS.md Build System] |
| Lua | 5.4.8 | Interpreter | [CITED: AGENTS.md Build System] |
| CMake / Ninja | 4.3.1-msvc1 / 1.13.2 | Build | [VERIFIED: `cmake --version`, `ninja --version`] |
| MSVC | 19.51.36256 | Local compiler | [VERIFIED: `cl` banner] |
| clang-format | **22.1.8 via `uvx`** (the VS-bundled one is 22.1.3, do not use it) | Format gate | [VERIFIED: `uvx clang-format@22.1.8 --version`; VS `clang-format.exe --version` = 22.1.3] |
| clang-tidy / run-clang-tidy | LLVM 22.1.3 (VS bundle) | Lint gate | [VERIFIED: `clang-tidy --version`] |
| Bun | 1.3.14 | JS sync test | [VERIFIED: `bun --version`] |

**Installation:** none.

## Package Legitimacy Audit

Not applicable. This phase installs no external packages.

| Package | Registry | Age | Downloads | Source Repo | Verdict | Disposition |
|---------|----------|-----|-----------|-------------|---------|-------------|
| (none) | — | — | — | — | — | — |

**Packages removed due to [SLOP] verdict:** none
**Packages flagged as suspicious [SUS]:** none

## Architecture Patterns

### System Architecture Diagram

```
host (C API quiver_lua_runner_run / CLI / Julia / Dart / Python / JS)
   │  LuaRunner(Database&) ; run(script)
   ▼
lua_runner.cpp ── LuaRunner::Impl { Database& db; RunHandles handles; sol::state lua; }
   │  ctor: open_libraries → nil dofile/loadfile → ns = create_named_table("quiver")
   │        → bind = new_usertype<Database> (once) → bind_core, bind_read, bind_write,
   │          bind_metadata, bind_time_series, bind_csv(lua,bind,handles),
   │          bind_binary(lua,bind,ns,db,handles) → lua["db"] = &db
   │  run(): GcGuard armed → lua.safe_script(script)
   ▼
script calls db:x(...) / quiver.y(...) / obj:m(...)
   ▼
sol2 dispatch (usertype storage) ──► per-domain TU wrapper (db_*.cpp / csv.cpp / binary.cpp)
                                        │ converters in internal.h (lua_to_value, lua_cell_as, ...)
                                        │ file ops ──► path_policy.cpp resolve_sandboxed_path ──► (reject | resolved path)
                                        │ handles   ──► RunHandles (open_writers / open_binary_files)
                                        ▼
                                 C++ core (Database / BinaryFile / Expression / csv_read / csv_write)
   ◄── first return value ── return_json.cpp encode_return_json ──► std::string JSON
   ◄── GcGuard dtor: handles.close_open_writers(); lua.collect_garbage() (exactly once)
```

### Recommended Project Structure (prototype line counts after clang-format 22.1.8)

```
src/lua_runner/
├── lua_runner.cpp      # 146  LuaRunner::Impl (ctor order, the one Database usertype), RunHandles bodies, run()/GcGuard   (git mv of src/lua_runner.cpp)
├── internal.h          # 227  quiver::lua_internal: RunHandles, binder decls, converters, option walk, group-decoder decls
├── return_json.cpp     # 222  run()'s JSON encoder (anonymous namespace) + encode_return_json
├── path_policy.cpp     #  62  resolve_sandboxed_path
├── db_core.cpp         # 229  bind_core: 22 methods (info, transactions, dry runs, count, describe, query, migrations, export/import_csv)
├── db_read.cpp         # 233  bind_read: 14 methods (bulk + by-id readers)
├── db_write.cpp        # 299  bind_write: 11 methods; defines table_to_element + columnar group decoder
├── db_metadata.cpp     # 159  bind_metadata: 8 methods
├── db_time_series.cpp  # 280  bind_time_series: 10 methods
├── csv.cpp             # 445  bind_csv: read_csv, read_csv_stream, write_csv + CsvWriter usertype
└── binary.cpp          # 367  bind_binary: open_file/bin_to_csv/csv_to_bin, BinaryMetadata/BinaryFile/Expression usertypes, 15 quiver.*
```
[VERIFIED: prototype built from the current `src/lua_runner.cpp`, formatted with clang-format 22.1.8, `wc -l`. The planning-ID strip will move counts by a few lines at most]

Method distribution: 22 + 14 + 11 + 8 + 10 + 3 + 3 = 71 `bind.set_function` registrations, no duplicate key. [VERIFIED: multiline grep over the prototype; baseline `before.txt` has 71 distinct `db:` names]

### Cluster-to-file assignment (line ranges in the current file, identical to `bdf9087`)

| Target | Content (current lines) | Linkage in target |
|--------|------------------------|-------------------|
| `lua_runner.cpp` | 238-259 Impl + ctor (rewritten); 298-342 `path_has_open_writer` / `close_open_writers` (become `RunHandles::` member definitions); 2491-2539 Pimpl members + `run()` | `Impl` in `quiver`; `RunHandles` bodies in `lua_internal` |
| `internal.h` | `RunHandles` (from 287-296); 7 binder decls; `resolve_sandboxed_path` + `encode_return_json` decls; 1450-1481 `to_lua_table` x3; 1483-1489 `is_lua_boolean`; 1491-1512 `lua_cell_as<T>`; 1514-1534 `lua_to_value`; 1536-1547 `lua_table_to_vector<T>`; 473-508 `csv_options_entries`; `table_to_element` decl; 2158-2168 `GroupColumn` + 3 decoder decls | templates; `inline` non-templates; plain decls |
| `return_json.cpp` | 37-236 (anonymous namespace kept whole) + new `encode_return_json` (6 lines) | anon + one named function |
| `path_policy.cpp` | 1298-1357 | named |
| `db_core.cpp` | anon: 510-532 `table_entries`/`string_key`, 1359-1396 `parse_csv_options`, 1884-1936 `lua_table_to_values` + `query_*_lua`. `bind_core`: pairs 602-607, 612-662, 664-683 (from the variadic call), 690-692, 740-755 | anon + binder |
| `db_read.cpp` | anon: 1706-1769, 1938-2053, 2055-2087. `bind_read`: 689, 694-704, 710-713 | anon + binder |
| `db_write.cpp` | anon: 1575-1591 `require_dense_array`; named: 1593-1636 `table_to_element`, 2170-2231 `collect_group_columns`/`columns_to_cpp_rows`/`join_column_names`; anon: 1638-1704 element writes + relations, 2233-2307 `group_rows_from_lua` + 4 group writers. `bind_write`: pairs 595-600 (from variadic), 687, 715-718, 721-724 | mixed |
| `db_metadata.cpp` | anon: 1801-1816, 1829-1862, 1771-1799, 1818-1827, 1864-1882, 2093-2111 (`scalar_metadata_lua` placed before its users). `bind_metadata`: 729-737 | anon + binder |
| `db_time_series.cpp` | anon: 1549-1573 `value_to_lua_object`/`lua_table_to_value_map`, 2113-2156, 2309-2488. `bind_time_series`: pair 609-610 (from variadic), 706-708, 719-720, 725-727, 738 | anon + binder |
| `csv.cpp` | named: 261-285 `struct CsvWriter`; anon: 344-372, **394-450 (`csv_cell_to_string`, moved above its caller)**, 374-392, 452-471, 534-588, 1398-1448. `bind_csv`: 784-943 | named struct + anon + binder |
| `binary.cpp` | anon: 1102-1178, 1184-1246, 1278-1296, 1248-1276 (`bind_expression_operators<T>`). `bind_binary`: 757-782, 955-1015, 1028-1094 (`ns` arrives as a parameter instead of `lua["quiver"]` at 951/1024) | anon + binder |

Users of each shared helper decide where it lives [VERIFIED: read of `src/lua_runner.cpp` this session]:
- `csv_options_entries`: `csv.cpp` (x2), `db_core.cpp`, `binary.cpp`, so it goes in the header.
- `table_entries` / `string_key`: only `parse_csv_options`, so `db_core.cpp` anon.
- `table_to_element`: `db_write.cpp` + `binary.cpp` (`metadata_from_element`), so declared in the header, defined in `db_write.cpp`.
- `require_dense_array`: only `table_to_element`.
- Group decoder: `db_write.cpp` + `db_time_series.cpp` (`time_series_rows_from_lua`, critic correction 3).
- `value_to_lua_object` / `lua_table_to_value_map`: time series only.
- `scalar_metadata_lua` / `group_metadata_lua` / `lua_data_type_name`: metadata only.

### Pattern 1: Names for the dropped-rename layout
- **Folder** `src/lua_runner/`, **root TU** `src/lua_runner/lua_runner.cpp` (locked by SPLIT-01).
- **Namespace `quiver::lua_internal`.** It mirrors the existing `quiver::internal` of `src/database_internal.h`. Every alternative is worse:
  - `quiver::lua` reads like the ubiquitous `lua` locals (`sol::state_view lua(s)`) and the `lua` member.
  - `quiver::lua_runner_internal` costs 7 more columns on 120-column lines.
  - `quiver::sandbox_internal` (ARCHITECTURE's pick) would reintroduce the dropped rename's vocabulary.

  Each TU opens `namespace quiver::lua_internal {`, so code moved from `Impl` keeps calling helpers unqualified.
- **Include** `#include "lua_runner/internal.h"`, resolved through the `quiver` target's PRIVATE `src/` include dir. Do not write `"internal.h"`: `src/c/internal.h` exists (`QUIVER_C_API_INTERNAL_H`), so the bare spelling is ambiguous to readers and to grep, even though the quote-include rule would pick the sibling. [VERIFIED: `ls src/c/internal.h`; compile command contains `-I...\quiver1\src`]
- **Guard** `QUIVER_SRC_LUA_RUNNER_INTERNAL_H`, matching `src/csv/csv_read.h`'s `QUIVER_SRC_CSV_CSV_READ_H`. [VERIFIED: `src/csv/csv_read.h:1-2`]

### Pattern 2: One `Database` usertype, binders take `bind` / `ns`
`set_function` is behaviour-neutral against a variadic pair. Variadic pairs go through `ut.tuple_set(...)`, which calls `this->set(key, value)`. `set_function(key, fx)` calls `set(key, as_function_reference(fx))`. The `function_arguments<Sig, P>` wrapper is unwrapped by `lua_call_wrapper<T, function_arguments<Sig,P>, is_index, is_variable, checked, boost, clean_stack>`, which forwards to `lua_call_wrapper<T, P, ...>` with the **same** template flags. `is_variable_binding` is false for both shapes. [VERIFIED: sol2 `usertype.hpp:47-50,62-67,105-115`, `call.hpp:882-898,929-955`, `types.hpp:333-349`]

The automagic enrollments are identical. The single-argument `new_usertype<Class>(key)` uses `constant_automagic_enrollments<>` (default `automagic_flags::all`, all members `true`). The variadic path gives Database `all`, `default_constructor = true` and `destructor = true`, because no `sol::no_constructor` is passed today. [VERIFIED: sol2 `table.hpp:33-75`, `types.hpp:1540-1553`]

`usertype<T>::set` writes into storage found through `maybe_get_usertype_storage<T>(L)`, so the `bind` reference passed to every binder reaches one storage. [VERIFIED: `usertype.hpp:85-100`]

### Pattern 3: Linkage rules
- **`internal.h`:** templates as plain templates; every non-template function defined there is `inline`; no `static`, no anonymous namespace. A `static` or anon-namespace function in a header gives each TU its own copy, and an `inline` that names it is an ODR violation.
- **Each `.cpp`:** everything not declared in `internal.h` goes in an anonymous namespace nested in `quiver::lua_internal`. One exception: a type registered with `new_usertype` (`CsvWriter`) stays in the **named** namespace. sol2 keys a usertype's storage and metatables by `"sol." + demangle<T>()`, and `demangle` strips `(anonymous namespace)`, `` `anonymous namespace' `` and `{anonymous}`. Two anon-namespace types with the same qualified spelling would therefore share keys. [VERIFIED: sol2 `demangle.hpp:39-47,110-120`, `usertype_traits.hpp:38-56`]
- **Never** define a helper with the same name in two TUs in the named namespace. Non-inline gives a loud LNK2005 (the prototype hit exactly this when two old member declarations leaked into the header as globals). Inline gives silent ODR.
- **Side effect to note in the PR:** `CsvWriter`'s registry key changes from `sol.quiver::LuaRunner::Impl::CsvWriter…` to `sol.quiver::lua_internal::CsvWriter…`. It is visible only via `pairs(_G)` and sol2's Debug type text. No test pins it. [VERIFIED: `git grep 'Impl::CsvWriter'` hits only `src/csv/csv_write.h:55` and a test comment]

### Pattern 4: `RunHandles` and the three captures
- `RunHandles` (struct in `internal.h`, member definitions in `lua_runner.cpp`) holds `open_writers`, `open_binary_files`, `path_has_open_writer() const` and `close_open_writers()`. The header forward-declares `quiver::BinaryFile` and `quiver::csv_write::Writer`; `weak_ptr` of an incomplete type is fine in a member declaration.
- Only `lua_runner.cpp` includes `csv/csv_write.h` and `quiver/binary/binary_file.h` for the bodies, and neither pulls csv-parser. [VERIFIED: prototype compiled]
- `Impl` declares `Database& db; lua_internal::RunHandles handles; sol::state lua;` in that order.
- Capture replacements:
  - `open_file` (761): `[this]` → `[&handles]`, and `open_binary_files.push_back` → `handles.open_binary_files.push_back`.
  - `write_csv` (876): `[this]` → `[&handles]`, and `path_has_open_writer(` → `handles.path_has_open_writer(`, `open_writers.emplace_back` → `handles.open_writers.emplace_back`.
  - `expr:save` (1032): `[this]` → `[&db]` (`db` is `bind_binary`'s `Database&` parameter, bound to `Impl::db`, i.e. the host database).
- Capturing a reference parameter by reference denotes the referent (CWG 2011), so the closures outlive the binder call. [CITED: ARCHITECTURE.md Pattern 3, CWG 2011] The prototype passed `LuaRunner_Lifecycle` (both freed-source move pins) under MSVC Debug. [VERIFIED: prototype test run]
- `GcGuard` keeps its position (before `result`). Its destructor becomes `impl.handles.close_open_writers(); impl.lua.collect_garbage();`.

### Pattern 5: In-TU ordering (class scope no longer hides forward references)
Inside each TU, place helpers before the binder and the binder last. With `internal.h` holding the shared converters, exactly one forward reference remains: `csv_row_cells_from_lua` (374-392) calls `csv_cell_to_string` (394-450). Move `csv_cell_to_string` above it. In `db_metadata.cpp`, put `lua_data_type_name` and `scalar_metadata_lua`/`group_metadata_lua` before the list/get wrappers. [VERIFIED: prototype compiled with exactly these two reorders]

### Pattern 6: CMake
- Replace `src/CMakeLists.txt:68-74` (per-file `set_source_files_properties(lua_runner.cpp ...)`) with target-wide options.
- Replace line 17 `lua_runner.cpp` with the explicit folder list.
- The PRIVATE `target_compile_definitions(quiver ...)` block at 61-66 is untouched.
- The C API entry `c/lua_runner.cpp` (line 131) keeps its name. [VERIFIED: `src/CMakeLists.txt` read this session; current text quoted under Code Examples]

### Anti-Patterns to Avoid
- **A comment that spells `new_usertype<Database>` or `open_libraries(`** breaks the SPLIT-02 grep count and the sync test's `open_libraries(` count of exactly 1 (Pitfall 3).
- **A `Context` struct passed to binders** (`ctx.bind.set_function`). Pass explicit parameters with the literal names `bind` and `ns`. [CITED: ARCHITECTURE.md Anti-Pattern 3]
- **Static registrar objects** (self-registration). Static-initialisation order is unspecified, and a registrar-only TU is dropped from a static library. The ctor calls the binders explicitly. [CITED: PITFALLS.md Pitfall 7]
- **Mixing fixes into the split.** No `require_table`, no `SOL_ALL_SAFETIES_ON`, no reordered checks, no member-pointer forwarders (M3, Phase 3), no `const&` conversion of by-value sol2 params (Phase 4 changes those params anyway).

## Commit Sequence

Config is `granularity: coarse`, `parallelization: true`. The extraction commits all edit `src/lua_runner/lua_runner.cpp` and `src/CMakeLists.txt`, so **every plan is sequential (no parallel waves)**.

**Per-commit checks (every commit):**
1. `cmake --build build --config Debug`.
2. `build\bin\quiver_tests.exe --gtest_filter=Lua*` = 444 passed, 12 suites.
3. `build\bin\quiver_c_tests.exe --gtest_filter=LuaRunnerCApiTest.*` = 27 passed.
4. In `bindings/js`: `bun test test/lua-api-sync.test.ts` passes.
5. Surface diff empty (throwaway script, Sync Test section).

Phase-gate-only items are listed in Validation Architecture.

| # | Plan | Commit | What changes | Extra check |
|---|------|--------|--------------|-------------|
| 0 | 02-01 | (no commit) | Record the **before** build time and save `before.txt` (surface of the monolith) | — |
| 1 | 02-01 | `git mv src/lua_runner.cpp src/lua_runner/lua_runner.cpp` + `src/CMakeLists.txt` (path at line 17; target-wide `/bigobj`) + sync test switched to the folder | No content edit to the moved file, so similarity is 100% | `git log --follow --oneline src/lua_runner/lua_runner.cpp` shows `e7c6358`, `b68e66b`, … |
| 2 | 02-01 | Comment-only: planning-ID strip (59 lines) + NOLINT check-name fix, in the moved file | Each ID replaced by its one-line reason or the pinning test name; the 6 `// Group N:` lines deleted | `git diff -U0 HEAD~1 -- src/lua_runner | grep -E '^[-+][^-+]' | grep -vE '^[-+]\s*//'` prints nothing |
| 3 | 02-01 | De-class in place (one file): helpers out of `Impl` into `quiver::lua_internal`, leaves first, `static` dropped, de-indented; `RunHandles`; captures; single `bind`; 17 pairs → `bind.set_function`; the 7 binder functions; `csv_cell_to_string` reorder | Biggest diff. Review with `git diff -w --color-moved=zebra --color-moved-ws=allow-indentation-change` | `grep -c 'new_usertype<Database>'` = 1; `grep -n '\[this\]'` empty |
| 4 | 02-02 | Extract `internal.h`, `return_json.cpp`, `path_policy.cpp` | Add the 2 `.cpp` files (and `internal.h`, see Open Question 3) to `QUIVER_SOURCES` | — |
| 5 | 02-02 | Extract `db_metadata.cpp`, `db_read.cpp` | Stateless binders | — |
| 6 | 02-02 | Extract `db_write.cpp` (defines `table_to_element` + group decoder), `db_time_series.cpp` | — | — |
| 7 | 02-02 | Extract `db_core.cpp` | — | — |
| 8 | 02-02 | Extract `csv.cpp` (`CsvWriter` in the named namespace) | — | `wc -l src/lua_runner/csv.cpp` ≤ ~450 |
| 9 | 02-02 | Extract `binary.cpp`; `lua_runner.cpp` is now Impl + RunHandles bodies + run | — | All 11 files present |
| 10 | 02-03 | Citations + AGENTS.md (Citation Inventory section) | Docs/comments only | the three citation checks print nothing |

**History preservation:**
- Commit 1 is a pure rename of the biggest piece, so `git log --follow` crosses it.
- Commit 3 re-indents about 2,200 lines, so plain `git blame` would attribute them to it. Use `git blame -w -C -C <file>`: `-w` ignores the re-indent, and `-C -C` follows lines moved between files in the same commit. That works because every extraction commit cuts from `lua_runner.cpp` in the commit that creates the new file.
- No `.git-blame-ignore-revs` exists in the repo. Adding one is optional and not recommended (YAGNI). [VERIFIED: no such file, `blame.ignoreRevsFile` unset]
- Commits 4-9 are reviewable as pure moves with `git show --color-moved=zebra --color-moved-ws=allow-indentation-change`. The only uncolored lines should be includes, namespace wrappers, binder headers, and the `handles.`/`ns` edits.

## Sync Test Change (SPLIT-04)

**Current code** [VERIFIED: `bindings/js/test/lua-api-sync.test.ts:5-6,10-11,29-45,55,57`]:
```ts
import { readFileSync } from "node:fs";
import { join } from "node:path";
...
const CPP_PATH = join(__dirname, "..", "..", "..", "src", "lua_runner.cpp");
const CPP = readFileSync(CPP_PATH, "utf8");
...
const usertypeMethods = new Map<string, Set<string>>();
let current = "";
for (const line of CPP.split("\n")) {
  const open = /new_usertype<(\w+)>/.exec(line);
  ...
}
...
describe("lua-api reference stays in sync with src/lua_runner.cpp", () => {
    // Meta-guard: if a reformat of lua_runner.cpp breaks the regexes above, fail loudly instead of
```

**Replacement** (lands in commit 1):
```ts
import { readdirSync, readFileSync } from "node:fs";
import { join } from "node:path";
...
const SRC_DIR = join(__dirname, "..", "..", "..", "src", "lua_runner");
// Sorted, so neither the parse nor the open_libraries( count depends on directory order.
const SOURCES = readdirSync(SRC_DIR)
  .filter((f) => /\.(cpp|h)$/.test(f))
  .sort()
  .map((f) => readFileSync(join(SRC_DIR, f), "utf8"));
const CPP = SOURCES.join("\n"); // Pass 1, the open_libraries( count and the stdlib list read every file
...
const usertypeMethods = new Map<string, Set<string>>();
for (const source of SOURCES) {
  // Reset per file: Pass 2 never closes a usertype at `);`, so an open one would bleed into the next file.
  let current = "";
  for (const line of source.split("\n")) {
    /* existing loop body, unchanged */
  }
}
...
describe("lua-api reference stays in sync with src/lua_runner/", () => {
    // Meta-guard: if a reformat of src/lua_runner/ breaks the regexes above, fail loudly instead of
```
Also update the line-17 comment: `ns` is the `quiver` table passed to `bind_binary`; `bind_expression` no longer exists.

**Interaction with the Phase 1 guards (unchanged code, still meaningful):**
- The four usertype floors stay valid. `BinaryFile`/`BinaryMetadata`/`Expression` parse from `binary.cpp`, `CsvWriter` from `csv.cpp`.
- `Database` gets no Pass 2 entries after the split, because `auto bind = lua.new_usertype<Database>("Database");` is a one-line call. `dbMethods.size > 40` covers it, and nothing guards `Database` in Pass 2 (correct, per PITFALLS Pitfall 2).
- `open_libraries(` must be exactly 1 across all files, comments included.
- The reverse check ("no documented `db:`/`quiver.` name has been removed") is what catches a file the glob missed. A file-count floor would be redundant: a wrong `SRC_DIR` throws ENOENT. Skip it.

**Before/after method-set diff (throwaway, never committed):** save this as `surface.ts` in a scratch directory.
```ts
// bun surface.ts <file-or-dir>  -- prints "kind name" per bound method; a dir is read like the new sync test
import { readFileSync, readdirSync, statSync } from "node:fs";
import { join } from "node:path";
const target = process.argv[2];
const files = statSync(target).isDirectory()
  ? readdirSync(target).filter((f) => /\.(cpp|h)$/.test(f)).sort().map((f) => join(target, f))
  : [target];
const out = new Set<string>();
for (const file of files) {
  const src = readFileSync(file, "utf8");
  for (const m of src.matchAll(/\b(bind|ns)\.set_function\(\s*"([a-z_][a-z0-9_]*)"/g))
    out.add(`${m[1] === "bind" ? "db" : "quiver"} ${m[2]}`);
  let current = "";
  for (const line of src.split("\n")) {
    const open = /new_usertype<(\w+)>/.exec(line);
    if (open) { current = open[1]; continue; }
    if (line.includes(".set_function(")) { current = ""; continue; }
    const pair = /^\s*"([a-z_][a-z0-9_]*)",\s*$/.exec(line);
    if (pair && current) out.add(`${current === "Database" ? "db" : current} ${pair[1]}`);
  }
}
console.log([...out].sort().join("\n"));
```
```bash
git show 9f0a4b9:src/lua_runner.cpp > "$SCRATCH/monolith.cpp"   # any pre-split commit
bun surface.ts "$SCRATCH/monolith.cpp" > before.txt              # 107 lines: 71 db, 15 quiver, 21 usertype
bun surface.ts src/lua_runner > after.txt
diff before.txt after.txt && echo IDENTICAL                      # must print IDENTICAL
```
[VERIFIED: run on the monolith (107 entries) and on the prototype folder (identical)]

**Mutation check (by hand, phase gate):** delete one `bind.set_function("describe", ...)` line in `db_core.cpp` and run `bun test test/lua-api-sync.test.ts`. "no documented db:/quiver. name has been removed" must fail. Revert with `git checkout -- src/lua_runner/db_core.cpp`.

## Citation Inventory (SPLIT-07)

Re-derived with three checks over today's tree. They found 48 distinct stale sites. [VERIFIED: `git grep` this session; outputs quoted by file:line]

```bash
# 1  full path                      -> must print nothing after commit 10
git grep -n 'src/lua_runner\.cpp' -- . ':!.planning' ':!CHANGELOG.md'
# 2  bare name meaning the core file -> must print nothing after commit 10
git grep -nE '(^|[^/_a-z])lua_runner\.cpp' -- . ':!.planning' ':!CHANGELOG.md' ':!src/c/AGENTS.md'
# 3  stale symbols                  -> must print nothing after commit 10
git grep -nE 'bind_database|LuaRunner::Impl::CsvWriter|Impl::(open_writers|open_binary_files|close_open_writers)|is_lua_boolean` in `Impl' -- . ':!.planning'
```

**UPDATE** (current hit → new target):
| File:line | New citation |
|-----------|--------------|
| `AGENTS.md:74` (Lua binds via sol2) | `src/lua_runner/binary.cpp` (or "`src/lua_runner/`") |
| `AGENTS.md:89` (`resolve_sandboxed_path`) | `src/lua_runner/path_policy.cpp` |
| `AGENTS.md:156` (JSON encoder "anonymous namespace") | `src/lua_runner/return_json.cpp` |
| `AGENTS.md:222` (`lua_to_value` / `lua_cell_as`) | `src/lua_runner/internal.h` |
| `AGENTS.md:448` (macOS `to_chars` floor list) | `lua_runner/return_json.cpp`, `lua_runner/csv.cpp` (the two TUs that call `utils::append_number` on a double) |
| `AGENTS.md:895` (raw `Database&`) | `src/lua_runner/lua_runner.cpp` |
| `src/AGENTS.md:46` (file map) | Replace with the 11-line `src/lua_runner/` block (Code Examples) |
| `src/AGENTS.md:83, 88` (`src/lua/` remark; encoder "in `lua_runner.cpp`'s anonymous namespace") | Drop the stale `src/lua/` aside; encoder lives in `src/lua_runner/return_json.cpp` |
| `src/AGENTS.md:102` (csv-parser never included by `lua_runner.cpp`) | "by any `src/lua_runner/` TU (all sol2 TUs; `/bigobj` is target-wide)" |
| `src/AGENTS.md:136` (`CsvWriter` wrapper) | `src/lua_runner/csv.cpp` |
| `src/AGENTS.md:159` (`weakly_canonical` idiom) | `src/lua_runner/path_policy.cpp` |
| `src/AGENTS.md:255` ("Three guards … (`src/lua_runner.cpp`)") | `src/lua_runner/` (`csv.cpp`, `internal.h`, `db_write.cpp`) |
| `src/AGENTS.md:280, 806, 809, 796` (`Impl::open_writers` / `Impl::open_binary_files` / `Impl::close_open_writers()`) | `RunHandles::…` (`src/lua_runner/internal.h`, bodies in `lua_runner.cpp`) |
| `src/AGENTS.md:632` ("Implementation conventions in `lua_runner.cpp`") | "in `src/lua_runner/`" + a layout bullet: one `Database` usertype in the ctor, binder params `bind`/`ns` are load-bearing for the sync test, `RunHandles` before `lua` |
| `src/AGENTS.md:662` (sync test "parses `lua_runner.cpp`") | "parses every `.cpp`/`.h` under `src/lua_runner/`" |
| `src/AGENTS.md:741` ("`lua_runner.cpp` is the only translation unit … that includes sol2") | "the `src/lua_runner/` TUs are the only ones that include sol2, and all of them are in the `quiver` target" |
| `src/AGENTS.md:765` (`is_lua_boolean` in `Impl`) | in `internal.h` |
| `src/AGENTS.md:821, 891` (binary / expression bound in `src/lua_runner.cpp`) | `src/lua_runner/binary.cpp` |
| `src/csv/csv_read.h:5, 12` | `src/lua_runner/csv.cpp`; "never included by any `src/lua_runner/` TU" |
| `src/csv/csv_write.h:4, 12, 18, 22` | `src/lua_runner/csv.cpp` |
| `src/csv/csv_write.h:55` (`LuaRunner::Impl::CsvWriter`) | `lua_internal::CsvWriter` (`src/lua_runner/csv.cpp`) |
| `src/csv/csv_write.cpp:26, 48` | `src/lua_runner/csv.cpp` |
| `src/CMakeLists.txt:17, 71, 73` | Explicit folder list; target-wide `/bigobj` |
| `cmake/Platform.cmake:4` | `lua_runner/return_json.cpp`, `lua_runner/csv.cpp` |
| `bindings/dart/hook/build.dart:54` | same as Platform.cmake |
| `bindings/dart/AGENTS.md:65` | same |
| `bindings/julia/AGENTS.md:158` | `src/lua_runner/lua_runner.cpp` |
| `bindings/js/AGENTS.md:37` | "every file under `src/lua_runner/`" |
| `bindings/js/src/lua-api.ts:3` ("Authority: `src/lua_runner.cpp` `bind_database()`") | "the binders under `src/lua_runner/` (`bind_core` … `bind_binary`)". A maintainer comment outside the exported string, not shipped text |
| `bindings/js/src/lua-api.ts:6` | "derives the bound surface from every file under `src/lua_runner/`" |
| `bindings/js/test/lua-api-sync.test.ts:10, 17, 55, 57` | Sync-test rewrite (commit 1) |
| `tests/AGENTS.md:177` | "it parses every `.cpp`/`.h` under `src/lua_runner/` (sorted, `current` reset per file)" |
| `tests/test_database_ui_metadata.cpp:60` | `src/lua_runner/path_policy.cpp`'s `resolve_sandboxed_path` |
| `tests/test_lua_runner_write_csv.cpp:22` | "uses on src/lua_runner/" |

**KEEP (C API TU and its test):**
- `src/CMakeLists.txt:131` `c/lua_runner.cpp`
- `src/c/AGENTS.md:38` (C API file listing)
- `tests/CMakeLists.txt:93`
- `tests/AGENTS.md:96, 136` (`test_c_api_lua_runner.cpp`)

**KEEP (still accurate):** `tests/test_lua_runner_write_csv.cpp:1515` ("sol::state on LuaRunner::Impl persists across run() calls"), since `Impl` still owns the state. `tests/AGENTS.md:43` ("run state inside its `Impl`").

**HISTORICAL (never edit):** `CHANGELOG.md:896`, all of `.planning/`.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Proving the move kept every binding | A committed snapshot or exact counts | The throwaway `surface.ts` diff + the existing sync test + Phase 1 floors | Phase 1 D-06 locked floors only; the snapshot would rot |
| Per-file sol2 flags | `set_source_files_properties` per TU | Target-wide `target_compile_options` / PRIVATE defines | ODR: sol2 macros change inline bodies |
| Registration discovery | Static registrar objects | Explicit binder calls in the ctor | Init-order fiasco; dropped registrar TUs in static builds |
| Shared helper copies | Copy a helper into each TU | One definition (header `inline`, or declared in the header and defined once) | Silent ODR; message text depends on link order |
| Format check | The CMake `format` target or PATH `clang-format` | `uvx --from clang-format==22.1.8 clang-format --dry-run --Werror` | Local and target binary is 22.1.3, not the pin |

**Key insight:** the split is safe because it is provable. Use pure moves (color-moved diffs), an identical surface diff, the same 444 + 27 tests, and a grep count of one. Any "while I'm here" edit forfeits that proof.

## Common Pitfalls

### Pitfall 1: The NOLINT pairs are no-ops (wrong check name)
**What goes wrong:** All four pairs read `NOLINTBEGIN(performance-unnecessary-value-parameter)`. The check is `performance-unnecessary-value-param`, so nothing is suppressed. Tidy on today's monolith reports 65 warnings in `lua_runner.cpp`: 50 `performance-unnecessary-value-param` (lines 621, 646, 790, 823x2, 876, 1077-1092, 1254-1274 and 2264-2441) plus 15 others.
**Proof:** On a minimal file, the wrong name gives "1 warning" and the right name gives "Suppressed 1 warnings (1 NOLINT)". [VERIFIED: clang-tidy 22.1.3 run on a scratch file; full tidy run on `src/lua_runner.cpp` with `-p build`]
**How to avoid:** In commit 2 (comment-only), correct the name in all four pairs. After extraction, give each TU that has a by-value sol2 parameter **one** pair: `db_core.cpp` (`transaction`/`dry_run` `fn`), `db_write.cpp` (the four `update_*_group*_lua` `sol::table columns`), `db_time_series.cpp` (the four TS writer wrappers), `csv.cpp` (`bind_csv`) and `binary.cpp` (`bind_expression_operators` + `bind_binary`). Each pair runs from just before the first such function to the end of the binder.
**Residual (pre-existing, unchanged by the split, recommended accepted):**
- 4 `readability-identifier-naming` on `k`-constants (41, 46, 53, 364) and 1 on parameter `header_width_` (283)
- 1 `bugprone-implicit-widening-of-multiplication-result` (46)
- 4 `bugprone-empty-catch` (325, 336, 628, 653; deliberate and commented)
- 3 `bugprone-unchecked-optional-access` (1173-1175; false positives after `is_time_dimension()`)
- 1 `modernize-raw-string-literal` (768)
- 1 `modernize-return-braced-init-list` (1197)

[VERIFIED: tidy baseline output]
**Warning signs:** tidy output containing `performance-unnecessary-value-param` for a file under `src/lua_runner/`.

### Pitfall 2: `csv.cpp` has 5 lines of margin
**What goes wrong:** The prototype `csv.cpp` is 445 lines. Planning-ID rewrites that add a line push it over.
**How to avoid:** Make every ID replacement in the CSV code line-neutral (replace the token with the reason in place). Measure with `wc -l` after clang-format in commit 8.
**Fallback, keeping SPLIT-01's file list:**
1. Collapse duplicated rationale that the strip touches anyway. The `CsvWriter` field comments (old 261-285) restate the run-exit mechanism that `RunHandles`/`close_open_writers` documents. `read_csv_stream`'s "must not diverge" paragraph (old 844-849) restates 809-812, which is also stale ("forward-looking"). That is roughly 10 lines.
2. If still over, move the data-only `struct CsvWriter` (about 20 lines) into `internal.h` next to `RunHandles`. It stays a named-namespace complete type, and `shared_ptr<csv_write::Writer>` is fine with the forward declaration.

Never move csv-only decoders into the header.

### Pitfall 3: Greps count comments
**What goes wrong:** SPLIT-02's "grep count is 1" for `new_usertype<Database>` and the sync test's `open_libraries(` count of exactly 1 both match comment text. The prototype's ctor comment made the count 2. A `"word",` comment line after an open usertype, or a `bind.set_function("x"` in a comment, would also enter the parsed surface.
**How to avoid:** Comments paraphrase ("the only Database usertype", "the stdlib list above"). Check with `grep -c 'new_usertype<Database>' src/lua_runner/*` totalling 1 and `grep -c 'open_libraries(' src/lua_runner/*` totalling 1.

### Pitfall 4: The wrong clang-format
**What goes wrong:** `where clang-format` and the CMake `format` target (`find_program(CLANG_FORMAT NAMES clang-format-22 clang-format)`) both find VS's **22.1.3**. CI runs 22.1.8 from PyPI. Output can differ, and a different line wrap can drop a usertype name out of Pass 2.
**How to avoid:** Use `uvx --from clang-format==22.1.8 clang-format -i <files>`. Run it **before** the sync test, never after. [VERIFIED: `.github/workflows/ci.yml:204`; local `--version` outputs]

### Pitfall 5: Forward references and ordering
**What goes wrong:** Class scope used to hide forward references. In a TU, `csv_row_cells_from_lua` → `csv_cell_to_string` fails to compile, and so would any wrapper placed above `scalar_metadata_lua`.
**How to avoid:** Pattern 5 ordering. This is loud (a compile error), not silent.

### Pitfall 6: Stale natives in the binding suites
**What goes wrong:** The Dart hook reuses a cached CMake build keyed by a checksum that does not cover source-list changes. Its tests can run against the pre-split DLL and pass vacuously. [CITED: `bindings/dart/AGENTS.md:79-85`]
**How to avoid:** Before the Dart suite at the gate, run `Remove-Item -Recurse -Force bindings\dart\.dart_tool\hooks_runner, bindings\dart\.dart_tool\lib` (both exist today) [VERIFIED: `ls bindings/dart/.dart_tool`]. Also close any Julia REPL or Python process holding `libquiver.dll` before rebuilding.

### Pitfall 7: Behaviour drift hidden in "mechanical" edits
**What goes wrong:** Any of these changes behaviour:
- reordering a check (the `open_file` mode check before the path check);
- turning `sol::protected_function fn` into `sol::object`;
- converting a forwarder to a member pointer (changes the Debug dot-call text; Phase 3);
- changing a `sol::table` param to `const sol::table&`.

**How to avoid:** In commits 4-9 the diff must be pure moves. Commit 3's only semantic edits are the documented ones: captures, `handles.` qualification, `ns` parameter, the variadic-to-`set_function` change, the binder cut and the reorder. The Phase 1 order pins (`LuaBinaryTest`, `LuaRunner_ReadCsv`, `_ExportCSV`, `_ImportCSV`, `_WriteCsv*`) catch reorders.

## Code Examples

### `src/CMakeLists.txt`: current text being replaced [VERIFIED: `src/CMakeLists.txt:17,61-74`]
```cmake
    lua_runner.cpp
...
# sol2 safety settings
target_compile_definitions(quiver PRIVATE
    SOL_SAFE_NUMERICS=1
    SOL_SAFE_FUNCTION=1
    SOL_NO_NIL=1
)

# MSVC needs /bigobj for sol2 heavy template instantiations
# Other Windows compilers need -Wa,-mbig-obj for the same reason
if(MSVC)
    set_source_files_properties(lua_runner.cpp PROPERTIES COMPILE_OPTIONS "/bigobj")
elseif(WIN32)
    set_source_files_properties(lua_runner.cpp PROPERTIES COMPILE_OPTIONS "-Wa,-mbig-obj")
endif()
```
Replacement (the defines block is untouched; same `MSVC`/`WIN32` predicate as today):
```cmake
    lua_runner/binary.cpp
    lua_runner/csv.cpp
    lua_runner/db_core.cpp
    lua_runner/db_metadata.cpp
    lua_runner/db_read.cpp
    lua_runner/db_time_series.cpp
    lua_runner/db_write.cpp
    lua_runner/internal.h
    lua_runner/lua_runner.cpp
    lua_runner/path_policy.cpp
    lua_runner/return_json.cpp
...
# sol2's template depth needs /bigobj (MSVC) or -Wa,-mbig-obj (other Windows compilers) in every
# src/lua_runner/ TU; set for the whole target so a new file cannot miss it.
if(MSVC)
    target_compile_options(quiver PRIVATE /bigobj)
elseif(WIN32)
    target_compile_options(quiver PRIVATE -Wa,-mbig-obj)
endif()
```

### `internal.h` skeleton (prototype, compiled)
```cpp
#ifndef QUIVER_SRC_LUA_RUNNER_INTERNAL_H
#define QUIVER_SRC_LUA_RUNNER_INTERNAL_H

#include "quiver/database.h"
#include "quiver/element.h"
#include "quiver/value.h"

#include <sol/sol.hpp>
// <algorithm> <cstdint> <initializer_list> <map> <memory> <optional> <stdexcept> <string>
// <string_view> <type_traits> <utility> <vector>

namespace quiver {

class BinaryFile;

namespace csv_write {

class Writer;

}

namespace lua_internal {

struct RunHandles {
    std::vector<std::pair<std::string, std::weak_ptr<csv_write::Writer>>> open_writers;
    std::vector<std::weak_ptr<BinaryFile>> open_binary_files;

    bool path_has_open_writer(const std::string& resolved_path) const;  // lua_runner.cpp
    void close_open_writers();                                          // lua_runner.cpp
};

void bind_core(sol::usertype<Database>& bind);
void bind_read(sol::usertype<Database>& bind);
void bind_write(sol::usertype<Database>& bind);
void bind_metadata(sol::usertype<Database>& bind);
void bind_time_series(sol::usertype<Database>& bind);
void bind_csv(sol::state& lua, sol::usertype<Database>& bind, RunHandles& handles);
void bind_binary(sol::state& lua, sol::usertype<Database>& bind, sol::table& ns, Database& db, RunHandles& handles);

std::string resolve_sandboxed_path(const Database& db, const std::string& operation, const std::string& path);
std::string encode_return_json(const sol::object& value);

// templates: to_lua_table x3, lua_cell_as<T>, lua_table_to_vector<T>   (moved verbatim)
// inline:    is_lua_boolean, lua_to_value, csv_options_entries          (moved verbatim + `inline`)

Element table_to_element(const std::string& caller, const sol::table& values);  // db_write.cpp

struct GroupColumn { /* moved verbatim from 2163-2168 */ };
std::vector<GroupColumn> collect_group_columns(const std::string& caller, const sol::table& columns);
std::vector<std::map<std::string, Value>> columns_to_cpp_rows(
    const std::string& caller,
    const std::vector<GroupColumn>& lua_columns,
    size_t row_count
);
std::string join_column_names(const std::vector<GroupColumn>& lua_columns);

}  // namespace lua_internal

}  // namespace quiver

#endif  // QUIVER_SRC_LUA_RUNNER_INTERNAL_H
```

### Root TU: `Impl` and `run()` (prototype, compiled; the comment avoids the counted literal)
```cpp
struct LuaRunner::Impl {
    Database& db;
    // Declared before `lua`: the state, and every closure that captured `handles`, is torn down first.
    lua_internal::RunHandles handles;
    sol::state lua;

    explicit Impl(Database& database) : db(database) {
        lua.open_libraries(
            sol::lib::base,
            sol::lib::string,
            sol::lib::table,
            sol::lib::math,
            sol::lib::coroutine,
            sol::lib::utf8
        );
        // Scripts may not load Lua source from disk; string-form load() stays available.
        lua["dofile"] = sol::lua_nil;
        lua["loadfile"] = sol::lua_nil;
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
    }
};

std::string LuaRunner::run(const std::string& script) {
    // (existing GcGuard rationale comment, planning IDs replaced)
    struct GcGuard {
        Impl& impl;
        ~GcGuard() {
            impl.handles.close_open_writers();
            impl.lua.collect_garbage();
        }
    } gc_guard{*impl_};

    auto result = impl_->lua.safe_script(script, sol::script_pass_on_error);
    if (!result.valid()) {
        sol::error err = result;
        throw std::runtime_error(std::string("Failed to run Lua script: ") + err.what());
    }
    if (result.return_count() == 0) {
        return {};
    }
    return lua_internal::encode_return_json(result.get<sol::object>(0));
}
```

### A converted variadic pair (in `bind_core`; lambda body moved verbatim)
```cpp
bind.set_function("transaction", [](Database& self, sol::protected_function fn) -> sol::object {
    self.begin_transaction();
    auto result = fn(std::ref(self));
    ...  // unchanged lines 623-636
});
```

### `src/AGENTS.md` file-map block (replaces line 46)
```
  lua_runner/             # LuaRunner (sol2): every Lua binding, one file per domain
    lua_runner.cpp        # LuaRunner::Impl (ctor order, the one Database usertype), RunHandles bodies, run()/GcGuard
    internal.h            # quiver::lua_internal: RunHandles, binder decls, converters, option walk, group-decoder decls
    return_json.cpp       # run()'s JSON encoder
    path_policy.cpp       # resolve_sandboxed_path, the single filesystem gate
    db_core.cpp           # bind_core: info, transactions, dry runs, count, describe, query, migrations, export/import_csv
    db_read.cpp           # bind_read: bulk + by-id readers
    db_write.cpp          # bind_write: element CRUD, relations, vector/set group writers; table_to_element, group decoder
    db_metadata.cpp       # bind_metadata: get_*_metadata, list_* groups
    db_time_series.cpp    # bind_time_series: time-series read/write/upsert, time-series files
    csv.cpp               # bind_csv: read_csv, read_csv_stream, write_csv, CsvWriter
    binary.cpp            # bind_binary: BinaryMetadata/BinaryFile/Expression, quiver.*, open_file/bin_to_csv/csv_to_bin
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| One 2,539-line TU, everything `static` in `struct LuaRunner::Impl` | Per-domain TUs, free functions in `quiver::lua_internal` | This phase | Edits touch one ≤450-line file |
| 17 variadic `Database` pairs + 54 `bind.set_function` | 71 `bind.set_function` across 7 binders | This phase | `Database` no longer depends on Pass 2's line-shape heuristic |
| Per-file `/bigobj` | Target-wide | This phase | Cannot rot when a file is added |

**Deprecated/outdated:** the `performance-unnecessary-value-parameter` spelling in NOLINT comments was never a valid check name.

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | GCC (Linux) and Apple Clang (macOS) compile the split the way MSVC did (only MSVC Debug was compiled and tested) | Summary, Pattern 3 | CI compile failure on one OS; fixable in-phase. The patterns are standard C++ [ASSUMED] |
| A2 | Release behaviour of the split equals Debug (the prototype ran Debug only) | Validation | A Release-only divergence; the gate runs the `release` preset `Lua*` and C API suites [ASSUMED] |
| A3 | Wall-clock build of `quiver` improves. Indicative only: the 10 prototype TUs compiled in 22 s with `/MP` on 14 cores, against 44-69 s Debug / 61 s Release for the monolith TU alone in `.ninja_log` | Build-time protocol | None (recorded, not gated) [ASSUMED for the after-number] |
| A4 | SC1's "each listed explicitly in the `quiver` target's sources" includes `internal.h`. No other header is listed in `QUIVER_SOURCES` today | Code Examples | Verifier dispute only; listing a header is harmless [ASSUMED] |
| A5 | SC4's "one NOLINT pair per binder" is satisfied by one pair per TU that has something to suppress (5 TUs), not a no-op pair in `db_read`/`db_metadata` | Pitfall 1 | Verifier dispute only [ASSUMED] |
| A6 | Correcting the NOLINT check name and accepting the 15 residual pre-existing warnings is what "tidy clean on the new files" means | Pitfall 1 | If the user wants zero warnings, those 15 need explicit suppressions or code edits outside "mechanical" [ASSUMED] |

## Open Questions

1. **May commit 2 correct the NOLINT check name?**
   - What we know: the pairs are no-ops; the fix is a comment edit with no behaviour change; without it, tidy reports 50 extra warnings on the new files.
   - Recommendation: yes, and say so in the PR body.
2. **What does "tidy clean" mean for the 15 pre-existing non-value-param warnings?**
   - Recommendation: record them as accepted baseline in the SUMMARY (list above). The gate is "no tidy warning on `src/lua_runner/` that is not in the baseline list". Renaming `kMaxReturnDepth` and the like would also churn AGENTS.md text that cites those names.
3. **List `internal.h` in `QUIVER_SOURCES`?**
   - Recommendation: yes, to satisfy SC1 literally. It is harmless (CMake does not compile `.h`) and shows up in IDEs.
4. **One NOLINT pair "per binder" or "per TU that needs one"?**
   - Recommendation: per TU that needs one (5), so that no no-op markers are added.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| CMake + Ninja | Build | ✓ | 4.3.1-msvc1 / 1.13.2 | — |
| MSVC `cl`/`link` | Build | ✓ | 19.51.36256 | — |
| clang-format 22.1.8 | Format gate | ✓ via `uvx` | 22.1.8 | **Do not** use PATH / VS 22.1.3 |
| clang-tidy + run-clang-tidy | Lint gate | ✓ | 22.1.3 (VS LLVM) | — |
| uv | Python, `uvx` | ✓ | — | — |
| Bun | JS sync test + JS suite | ✓ | 1.3.14 | — |
| Julia / Dart / Python toolchains | Six-suite gate | ✓ | — | `test-all.bat` passed all six on this machine in Phase 1 [CITED: 01-02-SUMMARY] |
| `build/release` preset tree | Release `Lua*` gate | ✓ | — | `cmake --preset release` if missing |

**Missing dependencies with no fallback:** none.

## Validation Architecture

> `workflow.nyquist_validation` is `false` in `.planning/config.json`. The orchestrator explicitly requested this section (brief item 9), so it is included.

### Test Framework
| Property | Value |
|----------|-------|
| Framework | GoogleTest v1.17.0 (C++, C API); `bun test` (JS sync test); per-binding suites via `scripts/test-all.bat` |
| Config file | `tests/CMakeLists.txt`; `bindings/js/package.json` (`"test": "bun test test"`) |
| Quick run command | `build\bin\quiver_tests.exe --gtest_filter=Lua* --gtest_brief=1` (about 23 s) |
| Full suite command | `scripts\test-all.bat` (after the Dart cache delete) |

### Success Criterion → Proof Command

| SC / Req | Proof | Expected |
|----------|-------|----------|
| SC1 / SPLIT-01: files exist, old file gone | `ls src/lua_runner.cpp` | No such file |
| | `ls src/lua_runner` | Exactly the 11 names |
| SC1: sizes | `wc -l src/lua_runner/*` | Every file ≤ ~450 |
| SC1: explicit sources, no glob | `grep -n 'lua_runner/' src/CMakeLists.txt` | 11 explicit lines |
| | `grep -n GLOB src/CMakeLists.txt` | Empty |
| SC2 / SPLIT-02: one Database usertype | `grep -c 'new_usertype<Database>' src/lua_runner/* \| awk -F: '{s+=$2} END {print s}'` | `1` |
| SC2: one stdlib call | `grep -c 'open_libraries(' src/lua_runner/* \| awk -F: '{s+=$2} END {print s}'` | `1` |
| SC2 / SPLIT-03: no `[this]` | `grep -rn '\[this\]' src/lua_runner` | Empty |
| SC2: capture forms | `grep -rnE '\[&handles\]\|\[&db\]' src/lua_runner` | 3 hits: `open_file`, `write_csv`, `save` |
| SC2: member order | Read `struct LuaRunner::Impl` | `db`, `handles`, `lua` in that order; GcGuard before `result` with close then one `collect_garbage()` |
| SC2: 17 pairs converted | `bun surface.ts src/lua_runner` | 71 `db` entries |
| | Multiline count of `bind.set_function("` across the folder | 71 |
| SC3 / SPLIT-04: same surface | `diff before.txt after.txt` | Empty |
| | `bun test test/lua-api-sync.test.ts` (in `bindings/js`) | 6 pass |
| SC3: mutation | Delete one `bind.set_function` line, re-run the sync test, revert | Fails, then passes |
| SC4 / SPLIT-06 (Debug) | `build\bin\quiver_tests.exe --gtest_filter=Lua*` | 444 tests, 12 suites, all pass |
| | `build\bin\quiver_c_tests.exe --gtest_filter=LuaRunnerCApiTest.*` | 27 pass |
| SC4 / SPLIT-06 (Release) | `cmake --build --preset release`, then `build\release\bin\quiver_tests.exe --gtest_filter=Lua*` | 444 pass |
| | `build\release\bin\quiver_c_tests.exe --gtest_filter=LuaRunnerCApiTest.*` | 27 pass |
| SC4: six suites | `Remove-Item -Recurse -Force bindings\dart\.dart_tool\hooks_runner, bindings\dart\.dart_tool\lib`, then `scripts\test-all.bat` | 6 x PASS |
| SC4: no expectation change | `git diff --stat <phase-base> -- tests bindings/*/test bindings/*/tests` | Only `bindings/js/test/lua-api-sync.test.ts` plus two comment lines in tests (`test_database_ui_metadata.cpp:60`, `test_lua_runner_write_csv.cpp:22`) |
| SC4 / SPLIT-05: format | `uvx --from clang-format==22.1.8 clang-format --dry-run --Werror src/lua_runner/*.cpp src/lua_runner/*.h src/csv/*.h src/csv/*.cpp tests/test_database_ui_metadata.cpp tests/test_lua_runner_write_csv.cpp` | Exit 0 |
| SC4: tidy | `uv run python "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\Llvm\x64\bin\run-clang-tidy" -p build -quiet "src[\\/]lua_runner[\\/]"`. Equivalent to `scripts/tidy.bat` restricted to the folder; `.clang-tidy` `HeaderFilterRegex: '(include[\\/]quiver[\\/]\|[\\/]src[\\/])'` covers `internal.h` | No `performance-unnecessary-value-param`; nothing outside the 15-item baseline |
| SC4: NOLINT pairing | `grep -c 'NOLINTBEGIN' src/lua_runner/*` equals `grep -c 'NOLINTEND' …` per file | ≤ 1 pair per file; every pair names `performance-unnecessary-value-param` |
| SC4: defines PRIVATE on `quiver` only | `git grep -n 'SOL_' -- '*CMakeLists.txt' cmake` | Only `src/CMakeLists.txt:61-66` |
| | `git grep -n 'set_source_files_properties' -- src/CMakeLists.txt` | Empty |
| SC4: build time (recorded) | See protocol below | Before/after numbers in the PR |
| SC5 / SPLIT-07 | The three citation checks | All empty |
| SC5: planning IDs | `git grep -nE '\b(D\|LUA\|WRITE\|FMT\|TEST\|PIN\|SPLIT\|DEDUP\|SAFE\|FIX\|DOC)-[0-9]+\b\|[0-9]{2}-[A-Z0-9-]+\.md\|RESEARCH\.md\|Phase [0-9]\|Pitfall [0-9]\|Group [0-9]+\|\b[CM][0-9]{1,2}\b' -- src/lua_runner` | Empty |
| Lifetime pins | Included in `Lua*`: `LuaRunner_Lifecycle.*` | 4 pass; the `sizeof(LuaRunner) == sizeof(void*)` static_assert compiles |

The ID regex matches 59 lines in today's monolith (51 IDs + 6 `Group N` + `Phase 2's` + `Pitfall 3`) with no false positives. [VERIFIED]

### Build-time measurement protocol (recorded, not gated)
Run once on the pre-split tree (commit 0) and once at the gate, on the same machine and build dir, with no other heavy load:
```powershell
# Debug tree `build` (Ninja). Deletes only quiver's object dir; dependency libraries stay built.
$n = (Get-Content build\.ninja_log).Count
Remove-Item -Recurse -Force build\src\CMakeFiles\quiver.dir
$t = Measure-Command { cmake --build build --config Debug --target quiver }
"quiver clean build: $([int]$t.TotalSeconds) s wall"
Get-Content build\.ninja_log | Select-Object -Skip $n | ForEach-Object {
    $f = $_ -split "`t"; [pscustomobject]@{ s = ([int]$f[1] - [int]$f[0]) / 1000; obj = $f[3] }
} | Sort-Object s -Descending | Select-Object -First 12   # slowest TUs = critical path
```
Report wall-clock, the slowest TU, and the sum over `quiver.dir/lua_runner*` objects (total CPU).
Baseline data already on disk: `src/CMakeFiles/quiver.dir/lua_runner.cpp.obj` took 44-69 s (Debug, `build/.ninja_log`), 48 s (`build/dev`) and 61 s (`build/release`). [VERIFIED: `.ninja_log` entries]

### Sampling Rate
- **Per commit:** the five per-commit checks (Commit Sequence section).
- **Per plan:** plus `uvx` clang-format `--dry-run --Werror` on touched files.
- **Phase gate:** everything in the SC table, including Release, the six suites, tidy, the mutation check and build time.

### Wave 0 Gaps
None. Existing tests cover every requirement. The phase adds no test, and `before.txt` must be captured before commit 1.

## Security Domain

`security_enforcement: true`, ASVS level 1. This phase moves code without changing behaviour. The security goal is that **no control weakens in transit**.

### Applicable ASVS Categories
| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | no | — |
| V3 Session Management | no | — |
| V4 Access Control | yes (filesystem containment for untrusted scripts) | `resolve_sandboxed_path` moved verbatim to `path_policy.cpp`; every file op still calls it, in the same order |
| V5 Input Validation | yes | Converters (`lua_to_value`, `lua_cell_as`, `csv_max_integer_key` 1,000,000 cap, option walks) moved verbatim; Phase 1 pins assert full messages |
| V6 Cryptography | no | — |
| V12 Files and Resources | yes | Sandbox gate; `:memory:` rejection; `dofile`/`loadfile` nil'd right after `open_libraries`; run-exit closing of handles (`RunHandles`) |
| V14 Configuration | yes | sol2 defines stay PRIVATE on `quiver`, target-wide; no per-file flags (ODR) |

### Known Threat Patterns for this change
| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Path gate duplicated or bypassed during the move | Elevation of privilege | One definition (`grep -rn weakly_canonical src/lua_runner` hits only `path_policy.cpp`); `DeviceNamePathIsReportedWithPrefix` x2 + escape tests in `Lua*` |
| Check order changed (`open_file` mode before path; containment before options) | Tampering / info disclosure | Phase 1 order pins; pure-move diffs |
| Dangling captured reference after a runner move | Denial of service (host crash) | `RunHandles` inside heap `Impl`; `LuaRunner_Lifecycle` freed-source pins + `sizeof` static_assert |
| Mixed sol2 macro values across TUs (silent ODR) | Tampering | Defines only at `src/CMakeLists.txt:61-66`; no `set_source_files_properties` |
| `dofile`/`loadfile` re-enabled by a reordered ctor | Elevation of privilege | Ctor order kept; nil-ing is the line right after `open_libraries` |

## Sources

### Primary (HIGH confidence)
- `src/lua_runner.cpp` (read in full this session; byte-identical to `bdf9087`). Throughout: line ranges, captures, NOLINT text, planning-ID lines.
- Vendored sol2 v3.5.0 `build/_deps/sol2-src/include/sol/`:
  - `table.hpp:33-75` (new_usertype routes)
  - `usertype.hpp:47-115` (tuple_set / set / set_function)
  - `call.hpp:882-898, 929-955` (function_arguments unwrapping, variable-binding traits)
  - `types.hpp:325-349, 1540-1553` (as_function_reference, automagic defaults)
  - `usertype_storage.hpp:663-690` (binding storage)
  - `demangle.hpp:38-47, 110-120`
  - `usertype_traits.hpp:38-56`
- Prototype (scratchpad, not committed): split from the current source, clang-format 22.1.8, compiled with the `quiver` compile command from `build/compile_commands.json` (MSVC Debug `/W4`, zero warnings), linked in place of `lua_runner.cpp.obj`. `quiver_tests` `Lua*` 444/444 (12 suites), full 1410/1410; `quiver_c_tests` `LuaRunnerCApiTest` 27/27.
- clang-tidy 22.1.3 run on `src/lua_runner.cpp` (`-p build`) and on a minimal NOLINT probe file.
- Repo files read this session:
  - `src/CMakeLists.txt`, `.clang-tidy`, `scripts/tidy.bat`, `.clang-format`, `cmake/CompilerOptions.cmake`, `cmake/Platform.cmake`
  - `bindings/js/test/lua-api-sync.test.ts`, `bindings/js/src/lua-api.ts:1-40`, `include/quiver/lua_runner.h`
  - `src/AGENTS.md`, `tests/AGENTS.md` (excerpts), `bindings/js/AGENTS.md:25-50`, `bindings/dart/AGENTS.md:62-90`, `bindings/dart/hook/build.dart:45-62`
  - `.github/workflows/ci.yml:191-204`, `build/.ninja_log`, `build/build.ninja:3420-3440`
- `git grep` citation inventory (three checks) and `git diff --stat bdf9087 -- src/lua_runner.cpp` (empty).
- `.planning/research/ARCHITECTURE.md`, `LUA-RUNNER-MAP.md` (critic pass), `PITFALLS.md`; Phase 1 CONTEXT and SUMMARYs.

### Secondary (MEDIUM confidence)
- CWG 2011 (reference captured by reference denotes the referent), as cited in ARCHITECTURE.md Pattern 3. Not re-fetched; the prototype's passing move pins corroborate it on MSVC.

### Tertiary (LOW confidence)
- None. No web or Context7 lookup was needed. Every library question was answered from the vendored sol2 source, which outranks external docs for this version.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH. No new dependencies; tool versions probed.
- Architecture: HIGH. The prototype compiled, linked and passed the unmodified suites (MSVC Debug). GCC/Clang/Release are not prototyped (A1/A2).
- Pitfalls: HIGH. NOLINT, comment-count, clang-format-version and ordering pitfalls were each reproduced this session.
- Line counts: HIGH for the prototype, MEDIUM for the final files (the ID-strip rewording shifts them by a few lines).

**Research date:** 2026-10-02
**Valid until:** until `src/lua_runner.cpp` changes (it is frozen until this phase), or 30 days.
