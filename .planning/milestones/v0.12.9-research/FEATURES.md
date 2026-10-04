# Feature Research

**Domain:** Embedded-Lua (sol2 v3.5.0 over Lua 5.4.8) scripting host inside a C++20 library with FFI bindings. The milestone restructures and renames the host; it adds no features.
**Researched:** 2026-10-02
**Confidence:** HIGH for sol2/Lua behaviour (read from the vendored source, file:line below; nothing was executed). MEDIUM for the perf expectations (not measured). LOW for the ecosystem naming survey (web search only).

Paths: `sol/…` = `build/_deps/sol2-src/include/sol/`. `lua/…` = `build/_deps/lua-src/`. `lua_runner.cpp:N` = `src/lua_runner.cpp` at `bdf9087`. "Map" = `.planning/research/LUA-RUNNER-MAP.md`, and its critic section wins where the two disagree.

## How well-built embedded-Lua hosts handle the behaviours this milestone touches

### 1. Argument type validation: host messages vs sol2's raw errors

**Lua's own convention.** `luaL_check*` raises `bad argument #N to 'fn' (table expected, got number)` (`lua/src/lauxlib.c:189`, `:194-203`). Two things hold in every build: the check always runs, and the message names the function the caller called. Quiver's equivalent is Pattern 1, `Cannot <op>: <param> must be a table`. It already exists at `w:write_row` (`lua_runner.cpp:905`), `read_csv_stream`'s `on_row` (`:831-834`) and 9 hand-written sites.

**What sol2 does instead (verified):**
- **Debug builds check arguments.** `SOL_SAFE_FUNCTION_CALLS` defaults on only when `SOL_DEBUG_BUILD` is detected (`sol/version.hpp:394-407`, `:236-257`). The check is `multi_check` before the call (`sol/stack.hpp:194-196`), and a failure produces `stack index N, expected X, received Y … (bad argument into 'R(Args…)')` with a demangled C++ signature (`sol/error_handler.hpp:86-89`, `:143-157`). That text is neither Pattern 1 nor stable across compilers.
- **Release builds do not check arguments at all.** A `sol::table` parameter is built from whatever is on the stack. `SOL_SAFE_REFERENCES` is off, so the constructor's own check (`sol/table_core.hpp:346-351`) is compiled out. Iteration then calls `lua_next` → `gettable`, which does `hvalue(t)` behind an `api_check` (`lua/src/lapi.c:725-728`, `:1253-1259`). Lua is built with `LUA_USE_APICHECK=OFF` (`build/CMakeCache.txt:503`), so a number or string passed as a table is undefined behaviour, not an error.
- **Even the Debug table check is loose.** `loose_table_check` accepts any userdata (`sol/stack_check_unqualified.hpp:41-52`). `obj.is<sol::table>()` goes through the same checker (`:472-473`). **`is<sol::table>()` is therefore not a valid guard**; only `get_type() == sol::type::table` is. That matters at the value-level sites the map lists (`lua_runner.cpp:1600`, `:2184`), where a userdata cell silently becomes an all-NULL column.
- **`self` is unchecked in Release.** For a member-pointer binding the check exists only under `SOL_SAFE_USERTYPE` (`sol/call.hpp:485-492`), and its text is sol2's raw `sol: received nil for 'self' argument (use ':' …)`. Lambda bindings take `Database& self` as an ordinary argument, so they inherit the Release no-check described above.
- **A dead define.** `src/CMakeLists.txt:62-66` sets `SOL_SAFE_FUNCTION=1`, but sol2 v3.5.0 recognises only `SOL_SAFE_FUNCTIONS`, `SOL_SAFE_FUNCTION_OBJECTS` and `SOL_SAFE_FUNCTION_CALLS` (`sol/version.hpp:372-407`). A grep for `SOL_SAFE_FUNCTION` with no suffix finds nothing in sol2, so the define is a no-op. src/AGENTS.md:708 repeats the claim. `SOL_NO_NIL` is real (`version.hpp:522`). The PROJECT constraint "stay PRIVATE" still applies, but the requirement should say what the define is meant to do.

**What good hosts do:** validate explicitly at the boundary with a message that names the operation, and treat library-level safety switches as a crash backstop, never as the user-facing error. This milestone's plan already follows that: `require_table` for the message, `SOL_ALL_SAFETIES_ON` as the backstop.

### 2. Optional arguments: nil/none vs wrong type

**Lua's convention.** `luaL_opt(L,f,n,d)` is `lua_isnoneornil ? d : f(L,n)` (`lua/src/lauxlib.h:152`). Absent or nil gives the default, and **any other type is checked**, so a wrong type is an error.

**sol2's `sol::optional<T>` does something else, in both builds:**
- The optional checker returns true for none/nil. For anything else it returns `unqualified_check<T>(…, &no_panic)` and **discards the handler** (`(void)handler;`, `sol/stack_check_unqualified.hpp:361-373`).
- So a wrong type fails the check without raising. `call` ignores `multi_check`'s return value (`sol/stack.hpp:194-196`), and the getter produces `nullopt` (`sol/stack_get_unqualified.hpp:163-165` → `get_using` with `no_panic`, `sol/stack_check_get_unqualified.hpp:44-80`).
- Result: `db:bin_to_csv(p, "false")` aggregates, and `db:query_integer(sql, 5)` runs with no parameters, **in Debug as well as Release**. That is C5.
- **`SOL_ALL_SAFETIES_ON` does not fix C5.** This is the main correction to the Key Decision's expectations. The fix has to be the explicit `optional_from_lua<T>` that the map proposes (C5), with exactly `luaL_opt` semantics.
- Side effect: `check_types` stops at the first failure (`sol/stack_core.hpp:1107-1110`). Every parameter after a silently failed optional is therefore also unchecked, even in Debug. Converting all optional parameters to `sol::object` removes this as well.

### 3. Transaction / callback helpers (commit failure → rollback)

**Every other Quiver binding does the same thing:** begin, call `fn`, commit, all **inside** the protected region; on any failure, roll back best-effort (swallowing errors) and rethrow the original error. Julia `bindings/julia/src/database_transaction.jl:21-34`, Python `bindings/python/src/quiverdb/database.py:370-382`, Dart `bindings/dart/lib/src/database_transaction.dart:38-53`.

**Lua today** (`lua_runner.cpp:620-637`) commits after the `!result.valid()` block, with nothing around it, so a failed COMMIT leaves the transaction open (C4).

**The dry run** is already right: `end_dry_run` on the success path sits outside the catch, deliberately, matching Dart's `dryRun` (`database_transaction.dart:85-101`, "let a failure surface"). The shared helper (M1) must keep that asymmetry: commit failure → rollback, `end_dry_run` failure → surface.

Edge cases the helper must keep as they are:
- Inside a dry run, `commit` is absorbed, so nothing changes there (root decision).
- A script that calls `db:commit()` inside `fn` makes the outer commit throw today, and still will. The fix only adds the best-effort rollback, which then no-ops or fails silently.
- The error surfaced is the C++ commit error text. No Lua-side message is crafted.

**`fn` validation.** Good hosts check that the callback is a function before starting the transaction, so `db:transaction(42)` must not open one. Today:
- the typed `sol::protected_function` parameter means Debug raises sol2's `expected function` text;
- Release builds the function object unchecked and calls it after `begin_transaction` (Lua's "attempt to call a number value" error, then a rollback).

Fix: the `on_row` pattern (`sol::object` + `get_type() == function` + Pattern 1), placed before `begin_*`.

### 4. Empty collections in writes

**Cross-layer precedent.** C++, Python (`element.py:60-63`) and JS (`create.ts:26-28`) send a count-0 array, so the core clears the group. Lua skips it (`lua_runner.cpp:1603`), and the shipped reference says so (`bindings/js/src/lua-api.ts:306-307`).

The user changed the C7 decision: an empty array clears. Consequences the requirement must state:
- **Fan-out.** `update_element` routes an array by column name to every matching group. An empty array therefore clears every group that shares that column name (root "Whole-group writers" note). Same as the other bindings, but new for Lua.
- **Round trip.** An all-NULL column read through `read_vectors_by_id` comes back as `{}`. Feeding it back into `update_element` used to skip that column and now clears it (N NULL rows become 0 rows). This is the known gap in the Lua whole-group reader decision, not a new one.
- **Typos.** Only `update_element` / `update_element_by_label` now throw on `{typo = {}}`, with `Cannot update_element: array 'typo' does not match any vector, set, or time series table in collection '<c>'` (not "attribute not found"). `create_element` still skips a misspelled empty array silently in the C++ core, after C7 as before (`database_impl.h:296-306`: the `values.empty() && !delete_existing` skip runs before the table lookup, and create passes `delete_existing=false`, `database_create.cpp:20`).
- **Changed, not unchanged:** `{value_int = {}}` as the *only* key. Today it throws only because Lua drops the empty array (`lua_runner.cpp:1603`), so the Element is empty. After C7 the Element holds one empty array (`element.cpp:57-58`), so `db:update_element(c, id, {value_int = {}})` no longer throws "at least one attribute" (`database_update.cpp:16-17`). It clears that group, as Python and JS already do. Only `create_element` still throws, with `Cannot create_element: element must have at least one scalar attribute` (`database_create.cpp:10-11`). The C7 requirement and its test should pin the clear, not the throw. (This corrects map critic, item 8.) The group writers' anti-silent-clear rule (named-but-empty throws, `{}` clears) is a different API and stays.
- Lua cannot tell an empty array from an empty map. `{}` here is the empty array, consistent with the JSON encoder's `{}` → `[]`.

### 5. Map-key type checking

- Lua keys can be any non-nil, non-NaN value, and an integral float key is normalised to an integer (`lua/doc/manual.html:271`). So `{[1.0]=x}` is the integer key 1 and must be rejected like `{[1]=x}`.
- Lua's own manual warns against `lua_tolstring` on a key mid-traversal (`manual.html:4649-4653`). Checking `get_type() == string` before `as<std::string>()` is both the correct check and the safe one.
- The guarded pattern already exists at `collect_group_columns` (`lua_runner.cpp:2176-2182`) and `string_key` (`:527-531`). Four sites lack it: `:1569`, `:1598`, `:1108`, `:2479`. One `lua_string_key(key, caller, what)`, then key-before-value ordering at each site. Do not add a collect-first rule (map critic, item 6: range-for cleans up in its destructor).

### 6. Handle / registry lifecycle at script exit

What good hosts do:
- **Deterministic close at the end of the run**, with `__gc` only as a fallback. Lua's GC gives no timing guarantee, and a handle held by a global is never unreachable.
- Lua 5.4 adds to-be-closed variables (`lua_toclose`, `lua/src/lua.h:361`) for script-controlled scoping.

Quiver already does the first: the `weak_ptr` registry plus `GcGuard`, which closes writers and BinaryFiles and then calls `collect_garbage()` exactly once (`lua_runner.cpp:2498-2539`), pinned by `WriterHeldInAGlobalIsClosedWhenRunReturns` and `HandleFromAnEarlierRunIsClosed`.

What this milestone must keep:
- `RunHandles` lives inside the heap-allocated `Impl`, so captured references survive a `Sandbox` move.
- Declaration order: `GcGuard` before `result`.
- One `collect_garbage`, not a loop.
- The same-path guard keys on `is_closed()`.

What it may add: prune expired entries before each insert (M13), and a name that covers both kinds of handle (it closes binary readers too).

`__close` support is a new feature. sol2 v3.5.0 has no `meta_function` for it (a grep for `__close` in `sol/` finds nothing), so it would need a raw `"__close"` key. Deferred (v2).

### 7. Naming: "Sandbox" vs runner/engine, and the two meanings of "sandbox"

**Survey (web, LOW confidence):**
- Luau's "sandbox" (`luaL_sandbox`/`luaL_sandboxthread`, luau.org/sandbox) means read-only libraries and globals plus a per-script proxy `_G`. mlua's `Lua::sandbox` mirrors it, Luau only.
- PHP's `LuaSandbox` class and Mozilla's `lua_sandbox` use the word to include **resource limits**: memory, CPU and instruction counts, output size.
- "Runner" / "engine" names (for example Redis's scripting engine) describe execution, not restriction.

**Implication.** A class called `Sandbox` leads readers, and especially the LLM that reads `LUA_DB_API_REFERENCE`, to expect resource limits that Quiver does not have. Table stakes for the rename:
- The header comment and the reference text say what the Sandbox restricts: six stdlibs, no `dofile`/`loadfile`, file operations confined to the database directory, return-size caps.
- They also say what it does **not** do: no instruction, memory or time limits, and Lua globals persist across `run()` calls on one instance.

**The two meanings.** In the shipped reference, "sandbox" means both the stdlib restriction ("`io` is deliberately absent from the sandbox", `lua-api.ts:695`, `:783`) and the path rule (`:108`, `:647`, `:785`, `:870`, `:905`). A third meaning, the host harness, appears at `:161-162`.
- The class name fits the stdlib meaning. The path rule becomes "directory containment" (`resolve_contained_path`), which is what PROJECT.md already decided.
- The host-harness sentence at `:162` must be reworded so the model does not attribute `run_lua` limits to `quiverdb.Sandbox`.

### 8. Sandbox-host capabilities people commonly expect (explicitly out of scope)

These are listed so the requirements can mark them out of scope or v2. None of them is built in this milestone.

| Capability | Standard mechanism | Quiver today | Disposition |
|---|---|---|---|
| Restricted stdlib | load only safe libs | Yes: base/string/table/math/coroutine/utf8 | Keep as is (design decision) |
| No file loaders | nil `dofile`/`loadfile` | Yes | Keep |
| Path containment | resolve against a root | Yes: db directory, strict, `:memory:` rejects | Keep; rename the wording |
| Output size cap | cap the return value | Yes: JSON 64 MiB / depth 32; CSV width 1,000,000 | Keep; pin the width cap (T1) |
| Instruction budget | `lua_sethook(L, f, LUA_MASKCOUNT, n)` (`lua/src/lua.h:454`, `:468`) | No | Out of scope (v2). The host limits scripts (PROJECT, C3 rationale) |
| Memory cap | custom `lua_Alloc` via `lua_setallocf` (`lua.h:359`) | No. `string.rep` is bounded only by `MAXSIZE` (`lua/src/lstrlib.c:157`) | Out of scope (v2) |
| Wall-clock timeout / cancel | deadline checked in the count hook | No | Out of scope (v2) |
| Bytecode refusal | `load(s, name, "t")` | No. `load` defaults to `"bt"` (`lua/src/lbaselib.c:391`), `string.dump` is loaded, and "maliciously crafted bytecode can crash the interpreter" (`manual.html:8175-8179`) | Out of scope. **Flag it for the user as a v2 hardening item.** Text-only mode keeps "string-form `load` stays" intact, but it is a behaviour change |
| Read-only globals / fresh env per run | Luau `luaL_sandbox`, or a new `_ENV` per run | No. Globals persist across `run()` | Out of scope. Document it |
| GC control by scripts | remove `collectgarbage` | Script can call `collectgarbage` (`lbaselib.c:508`) | Out of scope |
| Warning sink | `lua_setwarnf` (`lua.h:322`) | Default (off) | Out of scope |

## Feature Landscape

### Table Stakes (this refactor must deliver)

| Feature | Why expected | Complexity | Notes |
|---|---|---|---|
| **Pin tests before moving code** (T1 width cap; T2 non-string keys and non-table payloads on `upsert_time_series_row`, `update_time_series_files`, `create/update_element`, `file:read`) | Zero-behaviour split is only provable against pins; Release UB means today's "behaviour" for non-tables is undefined, so T2 pins must target the *post-fix* Pattern 1 text or be added with the fix | LOW | HIGH confidence. Non-table pins can't assert current behaviour (UB in Release CI) — write them as expected-to-throw and land with C1 |
| **Explicit `require_table(obj, op, what)` at every table parameter, and the value-level sites** | Release is UB today (§1). Host-quality errors name the public operation | MEDIUM (~20 sites; parameters become `sol::object`) | HIGH. Must test `get_type() == sol::type::table`, **never** `is<sol::table>()` (loose check). Keep the existing message shape `Cannot <op>: <what> must be a table`. Replaces the 9 hand-written checks |
| **`SOL_ALL_SAFETIES_ON` backstop, with `SOL_PRINT_ERRORS=0`** | Makes Release behave like the tested Debug build for anything the explicit checks miss: `self` on dot-calls, typed scalar parameters | LOW to configure, MEDIUM to verify | HIGH (source). The flag also turns on `SOL_PRINT_ERRORS` (`version.hpp:625-635`). sol2 then writes `[sol2] An exception occurred: …` to `std::cerr` for **every** thrown Pattern 1 error (`sol/trampoline.hpp:50-54`). That is stderr noise inside Python/Julia/Dart/JS hosts. Debug builds already do this by default (`version.hpp:633`). Define `SOL_PRINT_ERRORS=0` explicitly. Fix or document the dead `SOL_SAFE_FUNCTION=1` |
| **Measured perf cost of the safeties** | The Key Decision requires a number | LOW | MEDIUM. `os` is not loaded, so a script cannot time itself. Time from the host: Release `quiver_cli` running a loop script (`file:read` per cell, bulk `read_scalar_*`, a `create_element` loop) before and after. Expect per-call O(#args) type checks, which is small next to SQLite work and most visible on the `file:read` hot path |
| **`optional_from_lua<T>` with `luaL_opt` semantics** (nil/none → absent, wrong type → Pattern 1) | `sol::optional` silently treats a wrong type as absent in **both** builds (§2). The backstop does not fix it | LOW–MEDIUM (8 parameter sites: `:765`, `:777`, `:988`, `:1036`, `:1044`, `:1896`, `:1911`, `:1926`) | HIGH. Use `is<BinaryMetadata>()` for the usertype case. Removes the ternaries at `:771`, `:1040`, `:1047`, which sit at three of those sites. Fixes C5 and the downstream unchecked parameters |
| **Function parameters checked with Pattern 1, before any side effect** (`db:transaction`, `db:dry_run`) | `db:transaction(42)` must not open a transaction or produce build-dependent text | LOW | HIGH. Copy the `on_row` pattern (`:831-834`) |
| **One transaction/dry-run helper; a failed COMMIT rolls back** | Parity with Julia, Python and Dart (§3) | LOW | HIGH. Rollback is best-effort and swallowed; rethrow the commit error. `end_dry_run` on success stays outside the catch |
| **Map keys type-checked before they become names** (one `lua_string_key`) | Today key `1` becomes column `"1"` and `true` becomes `""` in Release, and Debug raises a raw panic | LOW (4 sites) | HIGH. An integral float key is an integer key and is rejected too. Check the key before converting the value |
| **Empty array in `create_element`/`update_element` clears the group** | Parity with C++/Python/JS (user decision) | LOW code, MEDIUM docs | HIGH. Rewrite `lua-api.ts:306-307`. CHANGELOG must name the fan-out and the round-trip consequence (§4) |
| **Error messages name the public operation; dead branches removed** (C8, `lua_data_type_name` default, `apply_binop` throw, D1) | Pattern 1 contract: `{operation}` is the method the user called | LOW | HIGH. No test pins `"Cannot build expression"`, so add one |
| **Handle lifecycle preserved byte for byte across the split**: `RunHandles` in `Impl`, `GcGuard` order, one `collect_garbage`; M13 pruning | Deterministic close at run exit is the property scripts depend on (§6) | MEDIUM (ownership moves) | HIGH. Moving `path_has_open_writer` is required (map critic, item 2) |
| **Rename to `quiver::Sandbox` everywhere, with no aliases** (C++, C `quiver_sandbox_*`, Julia/Dart/Python/JS, CLI, `SandboxException`, closed/disposed texts, `Sandbox*` gtest prefix, `tests/scratch`) | User decision; mechanical cross-layer naming rule | HIGH (50 non-`.planning` files contain `LuaRunner`, 54 with `quiver_lua_runner`, more once `lua_runner` file and path names are counted; re-derive with `git grep`. Julia's `c_api.jl` is regenerated; Dart's `bindings.dart` is hand-edited, see bindings/dart/AGENTS.md:27-43) | HIGH. Keep `"Failed to run Lua script:"`: tests assert it and it is still accurate |
| **Document what "Sandbox" does and does not restrict** (header + `LUA_DB_API_REFERENCE`) | The ecosystem meaning includes resource limits (§7), and an LLM reads this text | LOW | MEDIUM. Reword every "sandbox" in `lua-api.ts`: path → "directory containment", stdlib → "the Sandbox", `:162` host harness → unambiguous |
| **Sync test covers the folder and guards all four non-`Database` usertypes** (`BinaryFile`, `BinaryMetadata`, `Expression`, `CsvWriter`; `Database` is covered by `dbMethods.size > 40`) | The split breaks the hard-coded path, and three usertypes are skipped silently today | LOW–MEDIUM | HIGH. Reset `current` per file. Keep the parameter names `bind`/`ns`, unqualified `new_usertype<X>`, one method per line |

### Differentiators (valuable, deferrable)

| Feature | Value | Complexity | Notes |
|---|---|---|---|
| Actual Lua type in type errors (`… must be a table, got number`), as `luaL_typeerror` does | Faster diagnosis for script authors and LLMs | LOW code, MEDIUM churn | Changes the text of existing pinned messages. Do it for all sites or none. Defer |
| Argument position in messages | Matches Lua's `bad argument #N` | LOW | Quiver names parameters by role (`row`, `options`), which reads better. Skip |
| `resolve_contained_path` in its own sol2-free TU with direct unit tests | Tests the one filesystem gate without Lua | LOW | The rename touches it anyway. Optional |
| A small Release Lua micro-benchmark kept in the repo | Makes the safety-cost number reproducible | LOW | Do it ad hoc unless the cost turns out to be significant |
| Catch `is<sol::table>()` misuse in review or tidy | Stops the loose check from coming back | LOW | One comment beside `require_table` is enough |

### Anti-Features (deliberately NOT built)

| Feature | Why requested | Why problematic | Alternative |
|---|---|---|---|
| `LuaRunner` / `quiver_lua_runner_*` compatibility aliases | Softens a BREAKING change | Policy is delete, not deprecate (WIP project) | BREAKING entry in CHANGELOG `[0.13.0]` with a caller migration line per layer |
| New `db:` methods or Lua features | "While we're in there" | The milestone restructures; every new method costs five layers plus the reference | None |
| Sparse-extent cap on `update_vector_group`/`update_set_group` | DoS guard (C3) | User rejected it: the host limits scripts, and the shipped "sparse columns write NULL" rule stands | Note in out-of-scope |
| Relying on `SOL_ALL_SAFETIES_ON` alone for argument errors | Cheapest diff | Raw sol2 text with demangled C++ signatures, accepts userdata as a table, does nothing for wrong-type optionals (§2), and puts `std::cerr` output on every error | Explicit checks first; flag as backstop |
| Global custom sol2 `argument_handler` / exception handler that rewrites raw errors | One place for "nice" messages | It sees only stack indices and C++ types, not the public operation, so it cannot produce Pattern 1. Messages belong at the call site | `require_table` / `optional_from_lua` / `lua_string_key` |
| Using `is<sol::table>()` as the guard | It reads naturally | It accepts userdata (`stack_check_unqualified.hpp:472-473`) | `get_type() == sol::type::table` |
| "Hardening" `collect_garbage` into a loop, or relying on `__gc` for close | Feels more thorough | Pinned contract is close-then-one-collect. GC timing is not a contract | Keep the `weak_ptr` registry |
| SAVEPOINTs or nesting logic in `db:transaction` | Partial rollback | Rejected in the root design decisions (no SAVEPOINTs, dry-run absorption) | Keep begin/commit/rollback sequencing only |
| Instruction, memory or timeout limits; read-only globals; per-run fresh `_ENV` | "A Sandbox should have them" (§7) | A new feature in every layer; out of milestone scope | Document their absence. List as v2 |
| Text-only `load` | Closes the bytecode crash | Behaviour change outside this milestone's fix list; needs the user | Flag as a v2 hardening question |
| Table-driven registration loops (`for op in {gt, lt…}`) | Dedup | Breaks sync-test Pass 1, which matches literal `(bind\|ns).set_function("name"` | Literal calls; member pointers for forwarders (M3) |
| Moving the JSON encoder to `src/json/`; PCH | Tidiness, build speed | Out of scope until a second consumer or a measurement | Keep the encoder in the runner TU; target-wide `/bigobj` |
| Renaming `"Failed to run Lua script:"` to mention Sandbox | Naming consistency | Asserted by C API, read_csv and write_csv tests; still accurate | Keep |

## Feature Dependencies

```
Pin tests (T1, T2)
    └──before──> Mechanical split (shared header: require_table, optional_from_lua, lua_string_key homes)
                     └──before──> Dedupe (M1 txn helper, M3 member ptrs, M4 bulk adapters, M13 registry)
                                      └──before──> Behaviour fixes (C1 C2 C4 C5 C6 C7 C8, each with test + CHANGELOG)
                                                       └──before──> Rename to Sandbox (BREAKING) + reference/docs wording

C4 commit-rollback, C6 fn check ──land in──> M1 shared transaction helper
require_table (params -> sol::object) ──must precede──> SOL_ALL_SAFETIES_ON
SOL_ALL_SAFETIES_ON ──requires──> SOL_PRINT_ERRORS=0 (else stderr spam)
SOL_ALL_SAFETIES_ON ──changes──> Debug/Release self-error text for M3 member pointers
C7 empty-array clears ──interacts──> update_element by-column-name fan-out; Lua whole-group-reader decision
Rename ──requires──> "directory containment" rewording (resolve_contained_path), lua-api.ts sandbox sentences
Sync test folder scan ──required by──> split (ENOENT otherwise)
```

### Dependency Notes

- **`require_table` must land before the backstop.** With `SOL_ALL_SAFETIES_ON` on, a parameter still typed `sol::table` is rejected by sol2's `multi_check` before the lambda body runs (`stack.hpp:194-196`). Numbers and strings would get sol2's raw text and the Pattern 1 check would never run. Change each parameter to `sol::object` and add its check first, then turn the flag on. Pattern 1 tests then guard the order.
- **C4 and C6 belong in M1.** Fixing them before the dedupe means fixing two copies (`:620-637`, `:645-662`).
- **C5 is independent of the backstop.** No sol2 flag fixes it, so do not sequence it as "after the flag, check what's left".
- **The rename is last.** It is BREAKING across six layers. The split and dedupe must stay provably behaviour-neutral without rename noise in the diff, and the sync test changes twice: once for the folder, once for the name.
- **C7 conflicts with the fan-out.** Do not fold in the open "reject `matches.size() > 1`" fix. It is a separate breaking change the root AGENTS.md leaves open.

## MVP Definition

### Launch With (this milestone)

- [ ] T1/T2 pins: the split is only safe against pinned behaviour.
- [ ] Split into `src/sandbox/` with the sync test scanning the folder and guarding the four non-`Database` usertypes, a target-wide `/bigobj`, and the NOLINT blocks moved with their code. This is the core value (agent-editable files).
- [ ] Dedupe M1–M16 with no behaviour change.
- [ ] `require_table` + `optional_from_lua` + `lua_string_key` + function-parameter checks, then `SOL_ALL_SAFETIES_ON` with `SOL_PRINT_ERRORS=0` and the measured cost. This removes Release UB.
- [ ] Commit failure → rollback; empty array clears; operation-named messages; dead branches removed.
- [ ] Rename to `quiver::Sandbox` in every layer, with "directory containment" wording, a scope statement (what it does not limit), CHANGELOG `[0.13.0]` BREAKING, and every AGENTS.md updated.

### Add After Validation (v1.x)

- [ ] "got <type>" suffix on type errors, all at once, if script authors ask for it.
- [ ] A direct unit test for `resolve_contained_path`.

### Future Consideration (v2+)

- [ ] Resource limits (instruction-count hook, allocator cap, deadline). Only if Quiver ever runs scripts without a limiting host.
- [ ] Text-only `load`. Needs a user decision; it closes a known bytecode-crash vector.
- [ ] `__close` on `CsvWriter`/`BinaryFile` for `local w <close>`.
- [ ] A fresh environment per `run()`.

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---|---|---|---|
| Pin tests T1/T2 | HIGH | LOW | P1 |
| Split + sync-test update | HIGH | MEDIUM | P1 |
| `require_table` everywhere (C1) | HIGH (removes UB) | MEDIUM | P1 |
| `SOL_ALL_SAFETIES_ON` + `SOL_PRINT_ERRORS=0` + measurement | HIGH | LOW | P1 |
| `optional_from_lua` (C5) | MEDIUM | LOW | P1 |
| Map-key check (C2) | MEDIUM | LOW | P1 |
| Transaction helper + commit rollback + fn check (M1, C4, C6) | MEDIUM | LOW | P1 |
| Empty array clears (C7) | MEDIUM | LOW | P1 (user decision) |
| Dedupe M2–M16 | MEDIUM (agent-editability) | MEDIUM | P1 |
| Rename + scope documentation | MEDIUM | HIGH | P1 |
| C8 + dead code | LOW | LOW | P1 (cheap) |
| "got <type>" in messages | LOW | MEDIUM (churn) | P3 |
| Resource limits, text-only `load`, `__close`, fresh env | MEDIUM | HIGH | P3 (v2, user decision) |

## Competitor Feature Analysis

| Feature | Luau / mlua sandbox | PHP LuaSandbox / Mozilla lua_sandbox | Quiver after this milestone |
|---|---|---|---|
| Meaning of "sandbox" | Read-only stdlib and globals, per-script `_G` | Environment restriction plus memory, CPU and instruction limits | Restricted stdlib + directory containment + output caps; **no** resource limits, documented as such |
| Argument errors | `luaL_check*` style, names the function | Host-defined | Pattern 1 `Cannot <op>: …`, explicit checks; sol2 safeties as backstop only |
| Optional arguments | `luaL_opt`: wrong type is an error | n/a | `optional_from_lua` with `luaL_opt` semantics |
| Handle cleanup | GC / `__close` | Host-managed | Deterministic close at `run()` exit via registry, plus one GC pass |
| Bytecode loading | Luau has no `string.dump` path | Usually disabled | Still allowed (`"bt"`); v2 flag |

Sources for this table: luau.org/sandbox and mlua docs (LOW, web search); PHP manual LuaSandbox::setMemoryLimit and Mozilla lua_sandbox docs (LOW, web search). The Quiver column comes from the source and PROJECT.md (HIGH).

## Sources

- **Vendored sol2 v3.5.0** (HIGH): `sol/version.hpp:236-257, 314-640`; `sol/stack_check_unqualified.hpp:41-52, 361-373, 472-473`; `sol/stack.hpp:186-206`; `sol/stack_core.hpp:1107-1110, 1137-1153`; `sol/stack_get_unqualified.hpp:163-165`; `sol/stack_check_get_unqualified.hpp:44-80`; `sol/error_handler.hpp:86-157`; `sol/call.hpp:485-492`; `sol/table_core.hpp:346-351`; `sol/trampoline.hpp:50-54`; `sol/state_handling.hpp:63, 152`.
- **Vendored Lua 5.4.8** (HIGH): `lua/src/lauxlib.h:152`; `lua/src/lauxlib.c:179-209`; `lua/src/lapi.c:725-728, 1253-1265`; `lua/src/lua.h:322, 359, 361, 454, 468`; `lua/src/lbaselib.c:391, 508`; `lua/src/lstrlib.c:157`; `lua/doc/manual.html:271, 4649-4653, 8165-8179`; `build/CMakeCache.txt:503` (`LUA_USE_APICHECK=OFF`).
- **Quiver source** (HIGH): `src/lua_runner.cpp` (lines cited inline), `src/CMakeLists.txt:62-74`, `bindings/julia/src/database_transaction.jl:21-34`, `bindings/python/src/quiverdb/database.py:370-382`, `bindings/dart/lib/src/database_transaction.dart:38-101`, `bindings/js/src/lua-api.ts:108, 161-162, 306-307, 647, 695, 783-785, 870, 905`, `src/AGENTS.md:700-725`.
- **Official docs** (MEDIUM via WebFetch): [sol2 safety](https://sol2.readthedocs.io/en/latest/safety.html). Safeties are opt-in and on by default only in debug builds; no numeric cost is given.
- **Web** (LOW): [Luau sandboxing](https://luau.org/sandbox/), [mlua Lua struct](https://docs.rs/mlua/latest/mlua/struct.Lua.html), [rubenwardy, sol3 sandbox](https://blog.rubenwardy.com/2020/07/26/sol3-script-sandbox/), [PHP LuaSandbox::setMemoryLimit](https://www.php.net/manual/en/luasandbox.setmemorylimit.php), [Mozilla lua_sandbox](https://mozilla-services.github.io/lua_sandbox/sandbox.html), [lua-users wiki: Memory Allocation](http://lua-users.org/wiki/MemoryAllocation).

---
*Feature research for: embedded-Lua scripting host refactor (Quiver Sandbox)*
*Researched: 2026-10-02*
