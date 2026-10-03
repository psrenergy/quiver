# Project Research Summary

**Project:** Quiver Sandbox Refactor
**Domain:** Restructuring an embedded-Lua host layer (sol2 v3.5.0 over Lua 5.4.8, compiled as C) inside a C++20 library with a C API and four FFI bindings, plus Release type-safety hardening and a cross-layer rename `LuaRunner` -> `quiver::Sandbox`
**Researched:** 2026-10-02
**Confidence:** HIGH (nearly every claim comes from the vendored sol2/Lua source or from repo files; perf numbers and the naming survey are the weak spots)

## Executive Summary

The milestone takes `src/lua_runner.cpp` (2,539 lines, almost all one `LuaRunner::Impl` of static functions) and splits it into about 11 per-domain TUs under `src/sandbox/`. Each TU registers and implements its own slice of the 71 `db:` methods and 4 usertypes. The work then dedupes the boilerplate, fixes the bugs C1-C8, and renames the class to `quiver::Sandbox` in every layer. There is no new feature and no new dependency. The whole stack change is one CMake block plus the explicit source list. `Impl` is free functions in disguise: its only instance state is `db`, `lua` and two handle registries. `src/database_internal.h` (a named namespace with `inline` functions and templates, shared by 7 TUs: six `database_*.cpp` plus `type_validator.cpp`) is the precedent the new `internal.h` copies.

The main finding is that **Release builds have undefined behaviour today, not wrong error messages.** In Release, sol2 does no argument checks: `SOL_SAFE_FUNCTION_CALLS` and `SOL_SAFE_REFERENCES` are on only in Debug, and `LUA_USE_APICHECK` is OFF. A number passed where a `sol::table` is expected therefore goes straight to `lua_next` as a `Table*`. The `SOL_SAFE_FUNCTION=1` define does nothing, because sol2 v3.5.0 only reads the plural and suffixed names. The fix has three layers, in this order:
1. Explicit `require_table` (testing `get_type() == sol::type::table`, never `is<sol::table>()`, which accepts userdata), `lua_string_key`, and `optional_from_lua` with `luaL_opt` semantics, all emitting Pattern 1.
2. Then `SOL_ALL_SAFETIES_ON=1` as a backstop, always paired with `SOL_PRINT_ERRORS=0`. Without that, every script error prints `[sol2] An exception occurred` to the host's stderr.
3. The cost measured in Release through `quiver_cli`.

The flag does **not** fix C5: `sol::optional<T>` swallows a wrong type as `nullopt` in every build.

The main risks are silent:
- A second `new_usertype<Database>` wipes every method registered before it.
- The JS sync test can pass vacuously: it hard-codes the file path, keeps `current` across files, skips usertypes through `?? []`, and depends on clang-format line shapes.
- Mixed sol2 macros across TUs are an undiagnosed ODR violation.
- Pin tests that assert today's UB will crash Release CI.
- Regenerating Dart's `bindings.dart` breaks downstream code.

Each has a cheap guard, listed below. Keep the phase order strict: pins, split, dedupe, fixes (backstop flag last), rename. One PR per phase, each green on its own.

## Key Findings

### Recommended Stack

No version changes. sol2 v3.5.0 and Lua 5.4.8 (as C) stay pinned; macro semantics are version-specific. The deliverable is one `src/CMakeLists.txt` block on the `quiver` target only (details in STACK.md).

**Core technologies and settings:**
- **`SOL_ALL_SAFETIES_ON=1` + `SOL_PRINT_ERRORS=0` + `SOL_SAFE_NUMERICS=1` + `SOL_NO_NIL=1`**, all `target_compile_definitions(quiver PRIVATE …)`, never per file (ODR). Release then matches the Debug configuration that 428 C++ Lua tests, 27 C API tests and three binding suites (Julia, Python, JS) already pass; Dart already builds Release through its hook.
- **Target-wide `/bigobj` (MSVC) / `-Wa,-mbig-obj` (MinGW)** under the existing `if(MSVC)/elseif(WIN32)` predicate. It replaces the per-file property, which would silently match nothing after the move.
- **Explicit `QUIVER_SOURCES`, no GLOB.**
- **Lua stays C and `LUA_USE_APICHECK` stays OFF.** APICHECK is an `assert`: it does nothing in Release and aborts the host in Debug. `LUA_LANGUAGE=CXX` is out of scope.
- **Measured build cost (one local MSVC run, LOW, unverified):** the single TU goes from 42.9 s to 52.8 s (+23%) with ALL_SAFETIES. A header-only sol2 TU costs about 2.1 s, so about 11 TUs add roughly 25 s of CPU, while incremental builds improve. No PCH (sccache reportedly does not cache MSVC PCH builds; unverified).
- **clang-format 22.1.8 via `uvx`.** Local is 22.1.3. Re-run `cmake` after adding files, because the `format` glob is evaluated at configure time.

### Expected Features

**Must have (this milestone):**
- T1/T2 pin tests, limited to behaviour defined in both builds.
- The split into `src/sandbox/`, each file at most ~450 lines. The sync test scans the folder and guards all usertypes. NOLINT pairs move with their code.
- Dedupe M1-M16 with no behaviour change. M1 comes first so that C4/C6 land once.
- `require_table` at about 20 sites plus the value-level sites at 1600 and 2184. Parameters become `sol::object`.
- `lua_string_key` at 4 sites. An integral float key counts as an integer key and is rejected.
- `optional_from_lua` at 8 parameter sites.
- Function-parameter checks before any side effect.
- A failed COMMIT does a best-effort rollback and rethrows (parity with Julia, Python, Dart). An `end_dry_run` failure still surfaces.
- An empty array clears the group (C7), with `lua-api.ts:306-307` rewritten. Precisely: `update_element`/`update_element_by_label` with `{col = {}}` clear that group (and a misspelled empty column there throws "array 'x' does not match any vector, set, or time series table"); `create_element` still skips an empty array, because the core skips empty arrays when `delete_existing` is false (`database_impl.h:296-306`).
- C8 and the dead branches.
- Then the backstop flag with its measured cost.
- The rename to `quiver::Sandbox` in every layer with no aliases, "directory containment" (`resolve_contained_path`), and a scope statement saying what is **not** limited: instructions, memory, time, and globals persisting across `run()`.
- CHANGELOG `[0.13.0]` BREAKING entries.

**Should have (v1.x):**
- A "got <type>" suffix on type errors, at every site or none.
- A direct `resolve_contained_path` unit test through a sol2-free header.
- A Lua micro-benchmark kept in the repo.

**Defer (v2+, user decisions):**
- Resource limits: instruction hook, allocator cap, deadline.
- **Text-only `load`** (bytecode crash vector; flag to the user).
- `__close` on handles.
- A fresh `_ENV` per run.

**Anti-features (do not build):** `LuaRunner` aliases; new `db:` methods; a sparse-extent cap; a global sol2 error-rewriting handler; table-driven registration loops (they break sync-test Pass 1); a GC loop; SAVEPOINTs; renaming the `"Failed to run Lua script:"` prefix.

### Architecture Approach

`sandbox.cpp` is the original file moved with `git mv`. It owns `Impl { Database& db; RunHandles handles; sol::state lua; }`, with `handles` declared **before** `lua`. The ctor order is fixed: `open_libraries` -> nil `dofile`/`loadfile` -> `create_named_table("quiver")` -> **one** `new_usertype<Database>` -> seven explicit `bind_*` calls -> `lua["db"] = &db` last. Binders receive `sol::usertype<Database>& bind` and `ns`; those names are literal because the sync test matches them. The 17 variadic pairs become `bind.set_function`, which is behaviour-neutral per the sol2 source. Non-`Database` usertypes stay variadic, one name per line, for Pass 2.

**Major components:**
1. `internal.h` — `quiver::sandbox_internal` (named namespace, `inline` functions and templates): `RunHandles`, binder declarations, converters, option walk, columnar-decoder declarations, later the three check helpers. Move bodies to `convert.cpp` only past ~450 lines.
2. `sandbox.cpp` — `Impl`, ctor, `RunHandles` bodies, `run()` with `GcGuard` (before `result`; `close_open_writers` then exactly one `collect_garbage`).
3. Leaves: `return_json.cpp` (encoder) and `path_policy.cpp` (the single filesystem gate, no sol2 logic).
4. Domain binders:
   - `db_core.cpp`, 22 methods, **including `export_csv`/`import_csv`**.
   - `db_read.cpp`, 14 methods.
   - `db_write.cpp`, 11 methods; it also defines the columnar decoder `db_time_series` uses.
   - `db_metadata.cpp`, 8 methods.
   - `db_time_series.cpp`, 10 methods.
   - `csv.cpp`, ~445 lines, first dedupe target; owns `CsvWriter` privately and registers `write_csv` itself.
   - `binary.cpp`; BinaryFile and Expression stay together because they share `bind_expression_operators<T>`.
5. The three `[this]` captures become `[&handles]` (open_file, write_csv) and `[&db]` (expr:save). They stay valid across a move because `Impl` lives on the heap.

### Critical Pitfalls

1. **Pin tests that hit Release UB or pin bugs.** A non-table passed to an unchecked `sol::table` segfaults Release CI. Pin only defined behaviour. Write the C1, C2 and C5 tests red-then-green with their fixes, asserting the Pattern 1 text. Run new tests under the `release` preset.
2. **Vacuous sync test.** Read `src/sandbox/*.{cpp,h}` sorted and parse each file separately so `current` resets. Add guards: minimum file count, exactly one `open_libraries(`, `X.set_function` only on `bind`/`ns`, at least one method for each of BinaryFile, BinaryMetadata, Expression and CsvWriter, and a reverse check for usertype methods. Do a mutation check. Run `format.bat` before the test.
3. **A second `new_usertype<Database>` wipes the first.** `grep -c` across the folder must return 1. No static registrars. Keep the ctor order exact.
4. **Macro drift and backstop side effects.** Define macros only on `quiver`; never on one file, even to measure; no test target includes sol2. Land the explicit checks before the flag, or `SOL_SAFE_GETTER` longjmps over live C++ destructors (UB on GCC/Clang). Add a stderr test. Every `.as<` needs a type check in front of it.
5. **Rename collateral and FFI.**
   - Never run `scripts/generator.bat`; it rewrites `bindings.dart` into breaking enums. Regenerate Julia only and review every hunk.
   - Hand-edit the six Dart entries in `bindings.dart` (4 functions, the opaque class and the typedef) plus their uses in `lua_runner.dart`; Python cdefs plus `generator.py:22`; and the JS `luaSymbols`. Verify no `quiver_lua_runner` name remains.
   - Rename from a whole-word list; leave CHANGELOG history alone; keep the test-local variable `lua`.
   - gtest counts: `Sandbox*` = 428, C API = 27.
   - Clear the Dart `.dart_tool/hooks_runner/` cache after P2, P4 and P5; check exports with `dumpbin`.

Also: check order is a byte-for-byte contract (`open_file` mode before path; containment before options; `write_row` type, then closed). Put `require_table` where today's decode happens. `QUIVER_REQUIRE` stringifies parameter names, so "Null argument: runner" changes; log it.

## Implications for Roadmap

### Phase 1: Pin behaviour (tests first)
**Rationale:** The split is provably neutral only against pins.
**Delivers:** T1 cap tests (`{[1000001]='x'}`, header `{[2e6]='a'}`), error-text pins, two-bad-argument order pins (incl. `open_file`), sync-test per-usertype guards on the current file, a move test (`write_csv` + `open_file`), build-time and Release Lua perf baselines (4 `quiver_cli` scripts, median of 5).
**Avoids:** Pitfall 1. No C1/C2/C5 pins.

### Phase 2: Mechanical split into `src/sandbox/`
**Rationale:** Later diffs should be local to one 200-400-line file.
**Delivers:** Commit order:
1. `git mv` + sync-test folder scan + target-wide `/bigobj`.
2. De-class statics in place.
3. `internal.h` + `RunHandles`.
4. Extract the leaves.
5. One `bind`, then domain TUs one per commit: metadata -> read -> write -> time_series -> core -> csv -> binary.
6. Doc path citations (AGENTS.md files, `csv_*` comments, `Platform.cmake:4`, `build.dart:54`).

Run tidy; one NOLINT pair per binder.
**Avoids:** Pitfalls 2-7 and 10. Zero test-expectation changes.

### Phase 3: Dedupe (M1-M16)
**Rationale:** Collapse duplicates so each fix lands once.
**Delivers:** M1 first, `csv.cpp` next, M4 with two adapters, M9 nil handling per caller, M3 member pointers (Debug dot-call text changes; note it in the PR). Each PR states "reordered checks: none".
**Avoids:** Pitfalls 3, 8, 10 (M13 prunes only `expired()`).

### Phase 4: Behaviour fixes + Release type safety
**Rationale:** Fixes need stable helper homes; the flag needs the explicit checks first.
**Delivers:**
- C2 -> C1 -> C5 -> C6/C4 (in M1) -> C7 (with reference text) -> C8.
- **Last:** `SOL_ALL_SAFETIES_ON` + `SOL_PRINT_ERRORS=0`, measured at 5% or less on bulk-read and `file:read`. If over, add `SOL_SAFE_GETTER=0 SOL_SAFE_STACK_CHECK=0`; never disable FUNCTION_CALLS or USERTYPE.
- Fix the `SOL_SAFE_FUNCTION` claim in docs.
- BREAKING entries for C1, C5, C7.

**Avoids:** Pitfalls 1, 8, 9. Verify in the CI Release matrix and through the local Dart run (the only local Release build).

### Phase 5: Rename to `quiver::Sandbox` (BREAKING, 0.13.0)
**Rationale:** A mechanical sweep across ~50 files (re-derive the list with `git grep`) against stable fixes.
**Delivers:**
- `tests/scratch` (not breaking; may move earlier).
- Header and class; C API `quiver_sandbox_*`.
- Julia regeneration plus Dart/Python/JS hand-edits; the CLI.
- `SandboxException` and the closed/disposed texts, with new JS/Dart tests.
- `Sandbox*` suites without underscores.
- `resolve_contained_path`.
- `lua-api.ts` wording for the three meanings of "sandbox", plus the scope statement.
- Per-layer CHANGELOG list of renamed symbols.

**Avoids:** Pitfalls 11-14. No version bump.

The K1 planning-ID cleanup (49 lines) folds into Phase 2 for the Lua code and Phase 5 for the rest of the repo, or ships as its own small PR.

### Phase Ordering Rationale
- Each phase protects the next one: pins protect the split, the split localizes the dedupe, the dedupe makes each fix single (C4/C6 in M1).
- Inside Phase 4, the explicit checks come before the flag. Otherwise sol2's raw text pre-empts Pattern 1 and the getter longjmps.
- The rename goes last, so CHANGELOG entries and tests are written once and the sync test changes in two separate steps.

### Research Flags
Needs research:
- **Phase 4:** the perf protocol and threshold are unmeasured; about 20 check placements must preserve check order; the C7 fan-out wording.
- **Phase 5:** FFI mechanics differ per binding (Dart hand-edit, Python lazy symbols, JS eager load, Dart cache); ~50 files (re-derive with `git grep`) plus the fixture rename.

Standard patterns: Phases 1, 2, 3 (already enumerated and verified by the critic).

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | From vendored source with file:line references; compile cost measured. Runtime cost unmeasured (MEDIUM). |
| Features | HIGH | sol2/Lua behaviour from source. Naming survey LOW (web only). |
| Architecture | HIGH | Registry mechanics and capture lifetimes verified (CWG 2011). LOC budgets estimated. |
| Pitfalls | HIGH | Repo and sol2 facts verified. Some toolchain details from training knowledge (MEDIUM). |

**Overall confidence:** HIGH

### Gaps to Address

**Conflicts that need a user decision:**
- **`SOL_SAFE_FUNCTION=1`:** PROJECT.md says it "stays"; STACK says delete it; PITFALLS says keep it and fix the docs. Default: keep it with a corrected comment unless the user signs off on deletion.
- **Dart generation:** PROJECT.md says regenerate; `bindings/dart/AGENTS.md` says that is breaking. Correct the constraint to hand-editing.

**Naming (ARCHITECTURE.md is canonical):** `internal.h` (vs `sandbox_internal.h`) with guard `QUIVER_SANDBOX_INTERNAL_H`, included as `"sandbox/internal.h"`; namespace `quiver::sandbox_internal` (not `sandbox_detail`); files `db_*.cpp` (not `bind_database.cpp`).

**Other gaps:**
- Release runtime cost: baseline in Phase 1, decision in Phase 4.
- CHANGELOG `[0.12.9]` backfill is the maintainer's call. The auto-memory note pointing at `[0.12.9]` is stale; use `[0.13.0] — unreleased` with no bump.
- Text-only `load`: raise with the user as v2 hardening.
- The `CsvWriter` member-pointer form is about 134 columns, close to the 120-column wrap. Harden Pass 2 or the guards before M11.

## Sources

### Primary (HIGH confidence)
- Vendored sol2 v3.5.0: `version.hpp` macro cascade, `usertype_storage.hpp` STEP 0, `stack_check_unqualified.hpp`, `trampoline.hpp`, `call.hpp`, `demangle.hpp`.
- Vendored Lua 5.4.8: `lapi.c`, `luaconf.h.in`, `ldo.c`, `lauxlib`.
- Repo: `src/lua_runner.cpp`@bdf9087, `src/CMakeLists.txt`, `lua-api-sync.test.ts`, `lua-api.ts`, `bindings/dart/AGENTS.md`, binding transaction helpers, `ci.yml`, `CMakeCache.txt`.
- LUA-RUNNER-MAP.md; its critic corrections take precedence.
- CWG 2011 / P0613R0.
- Local MSVC 19.51 measurements.

### Secondary (MEDIUM confidence)
- sol2 safety docs; Microsoft Learn on longjmp and /EH; sccache PCH limitation; training-knowledge toolchain details.

### Tertiary (LOW confidence)
- Luau/mlua, PHP LuaSandbox, Mozilla lua_sandbox naming survey.

---
*Research completed: 2026-10-02*
*Ready for roadmap: yes*
