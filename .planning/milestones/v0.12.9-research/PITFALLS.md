# Pitfalls Research

**Domain:** Splitting a 2,539-line sol2 (v3.5.0) / Lua 5.4.8 binding file into per-domain TUs, then renaming an FFI-exposed class (`LuaRunner` -> `quiver::Sandbox`) across C++ / C API / Julia / Dart / Python / JS / CLI
**Researched:** 2026-10-02
**Confidence:** HIGH overall. sol2 and Lua claims are read from the vendored sources (`build/_deps/sol2-src/include/sol/`, `build/_deps/lua-src/src/`). Repo claims come from the files themselves. Where a claim relies on training knowledge instead, it is marked MEDIUM.

Phase keys used below: **P1 tests-first**, **P2 split**, **P3 dedupe**, **P4 fixes**, **P5 rename**.

## Three findings the map did not have

1. **`SOL_SAFE_FUNCTION=1` does nothing in sol2 v3.5.0.** sol2 recognises only `SOL_SAFE_FUNCTIONS` (plural), `SOL_SAFE_FUNCTION_OBJECTS` and `SOL_SAFE_FUNCTION_CALLS` (`version.hpp:372-407`). Searching the whole sol2 tree finds no `SOL_SAFE_FUNCTION_I_` and no `SOL_SAFE_FUNCTION)`. So src/AGENTS.md:708 and the comment at `lua_runner.cpp:1495` describe a protection that does not exist. The Release argument-check gap (C1) is therefore wider than the docs imply: `checked = detail::default_safe_function_calls` (`call.hpp:471`, `forward_detail.hpp:34-39`) is `false` in every Release TU. The PROJECT.md constraint keeps the define, so keep it, but the P4 docs must stop claiming it protects anything. Once `SOL_ALL_SAFETIES_ON` is set, the question no longer matters.
2. **Turning on `SOL_ALL_SAFETIES_ON` also turns on `SOL_PRINT_ERRORS` in Release** (`version.hpp:623-636`). sol2's default exception handler then writes `[sol2] An exception occurred: <msg>` to `std::cerr` for every C++ exception that crosses a binding (`trampoline.hpp:49-53`). That includes every Pattern 1 error a script catches with `pcall`. The published natives would then print to stderr inside Julia, Python, Dart and JS hosts. Debug builds already do this (DEFAULT_ON), which is why nobody has noticed. Fix: define `SOL_PRINT_ERRORS=0` next to `SOL_ALL_SAFETIES_ON=1`. An explicit define wins (`version.hpp:623-628`).
3. **PROJECT.md says to regenerate `bindings.dart`. bindings/dart/AGENTS.md:27-43 says regenerating it today is a breaking change.** The pinned ffigen 20.1.1 rewrites the whole file and turns `quiver_error_t` and the other int-constant classes into Dart `enum`s, which breaks hub's comparisons. Every recent C API change was hand-added "in the file's existing style". `scripts/generator.bat` runs the Dart generator too. In P5, regenerate Julia only and hand-edit the six Dart entries in `bindings.dart` (the 4 functions at 3337-3412, the `quiver_lua_runner` opaque class at 3504 and the `quiver_lua_runner_t` typedef at 3506), plus their uses in `bindings/dart/lib/src/lua_runner.dart:26,33`.

## Critical Pitfalls

### Pitfall 1: Tests-first phase pins undefined behaviour, or crashes Release CI

**What goes wrong:**
P1 adds tests for T2: non-string keys, and userdata or non-table payloads, on `upsert_time_series_row`, `update_time_series_files`, `create_element`/`update_element` and `file:read`. A test that passes a number or string where the code takes an unchecked `sol::table` behaves differently per build:
- **Release:** `lua_next` runs on a non-table. `gettable` asserts `ttistable` only through `api_check` (`lapi.c:725-728`, `lapi.c:1253-1259`), and `api_check` is a no-op because `LUA_USE_APICHECK=OFF` (`llimits.h:122-126`, `build/CMakeCache.txt:503`). This is UB, and plausibly a segfault (LOW, unverified: the concrete outcome depends on memory layout and was not reproduced), which would kill the whole `quiver_tests` process and drops every other result in that run.
- **Debug:** sol2's argument check rejects the call (it is on by default in Debug), so the test passes locally.

A userdata payload "works" in both builds (it appears to yield no keys; LOW, unverified, since it is UB through `hvalue()` on a `Udata*`), so a test that asserts today's behaviour pins the silent-clear bug as a contract. For C2, Release turns key `1` into column `"1"`, while Debug raises a raw sol2 panic. No assertion is true in both builds.

**Why it happens:** Local development is Debug MSVC (`_DEBUG` -> `SOL_DEBUG_BUILD`, `version.hpp:247-251`). The C++ suites run in Release only in the CI matrix (`ci.yml:24-25`). The one local Release consumer is the Dart hook (`hook/build.dart`, `buildMode: BuildMode.release`).

**How to avoid:**
- P1 pins only behaviour that is defined in both builds:
  - the 1,000,000 key cap (T1)
  - non-string keys where the code already checks them (`string_key` and `collect_group_columns` sites)
  - the outcomes P4 will keep
- The C1/C2/C5 tests land in P4 together with the fix, and they assert the new Pattern 1 text.
- Before merging P1, run the new tests in Release locally (`build/release`, the `release` preset).

**Warning signs:** a new test passes in Debug and fails or crashes only in the `Build (…, Release)` CI jobs. Or a test asserts "no rows written" after passing userdata.

**Phase to address:** P1 (scope), P4 (the tests that pin the fixes). **Confidence:** HIGH.

---

### Pitfall 2: The JS sync test passes vacuously after the split

**What goes wrong:**
`bindings/js/test/lua-api-sync.test.ts:10` reads the single file `src/lua_runner.cpp`. After the split it has to read every file in the folder, and each of these mistakes passes silently:
- **Missed file or extension.** Globbing only `*.cpp` misses a registration that sits in a header template. Missing one TU drops its names from `dbMethods`. The reverse check ("no documented db:/quiver. name has been removed", lines 83-93) does catch a missing `db:`/`quiver.` name. It has no equivalent for usertype methods, though. `BinaryFile`, `BinaryMetadata` and `CsvWriter` are read with `?? []` (line 76), so a missing file or a usertype that was never parsed simply contributes no names.
- **Concatenating files without resetting `current`.** Pass 2 (lines 29-45) never closes a usertype at `);`. A bare `"word",` line near the top of the next file is then attributed to the last usertype that was open (critic note 4).
- **Unstable file order.** `readdirSync` order depends on the OS and filesystem. NTFS happens to be alphabetical, but ext4 makes no promise (MEDIUM, Node docs). The `open_libraries(` assertion takes the first match in the combined text (line 96). On ubuntu-latest a comment mentioning `open_libraries(` in another file can be read first, and the stdlib check then compares the wrong list or an empty one.

**Why it happens:** the test was written for one file, and its meta-guards (lines 54-60) only check `dbMethods > 40`, `quiverFns > 10` and `Expression.size > 0`.

**How to avoid:**
- Read `src/sandbox/**/*.{cpp,h}`, sorted, and parse each file separately so that `current` resets per file.
- Assert that the folder has at least N files, so the test fails if the path is wrong.
- Assert exactly one `open_libraries(` match in total.
- Add Pass 2 meta-guards that each of the four non-`Database` usertypes (`BinaryFile`, `BinaryMetadata`, `Expression`, `CsvWriter`) parsed with at least one method. Do **not** guard `Database` in Pass 2: after the split all 17 of its variadic pairs become `bind.set_function` (ARCHITECTURE Pattern 1), Pass 2 resets on `.set_function(` (test lines 39-42), so `Database` has no Pass 2 entries and the guard would always fail. `dbMethods.size > 40` covers it (ARCHITECTURE Pattern 6).
- Add a reverse check for usertype methods as well.
- Update the docs that describe the path in the same PR: src/AGENTS.md:83 and 659-665, bindings/js/AGENTS.md:37, tests/AGENTS.md:172-176, lua-api.ts:3 and 6.

**Warning signs:** the sync test is green even though the PR moved code and never edited the test file. Usertype counts printed in a debug run come out as 0.

**Phase to address:** P2 (rewrite the test alongside the move; the meta-guards before any move). **Confidence:** HIGH.

---

### Pitfall 3: clang-format joins a short registration onto one line, and Pass 2 skips it

**What goes wrong:**
Pass 2 relies on two things:
- the bound name sitting alone on its own line, matched by `/^\s*"([a-z_][a-z0-9_]*)",\s*$/`;
- the `new_usertype<…>(` line itself being skipped with `continue` (line 33-35).

With `BinPackArguments: false` and `AllowAllArgumentsOnNextLine: false` (`.clang-format`), a call that fits in 120 columns is printed on one line, and then every pair in it is invisible to Pass 2. P3's member-pointer rewrites shorten calls (M3, M11 `&CsvWriter::write_row`, M14 functors). For example, `lua.new_usertype<CsvWriter>("CsvWriter", sol::no_constructor, "write_row", &CsvWriter::write_row, "close", &CsvWriter::close);` is about 126 columns before indent, so it still wraps today, but only by a few columns. A slightly shorter usertype, or a rename such as `CsvWriter` -> `Writer`, puts it on one line. The `?? []` meta-guard gap (Pitfall 2) then hides the loss. Pass 1 is safe from this, because its regex is multiline and line shape does not matter.

**Why it happens:** the test depends on how clang-format happens to lay out the code. clang-format runs in CI and in pre-commit, after the author has looked at the file.

**How to avoid:**
- Register usertype methods through `.set_function(` on the usertype handle, or `t["x"] = …`. A `set_function` call resets `current`, though, so if usertype methods move to that form, extend Pass 1's regex to cover them.
- Or keep Pass 2 and make it parse the pairs on the `new_usertype` line too.
- Either way, add the per-usertype guards from Pitfall 2.
- Run `scripts/format.bat` before you run the sync test, never after.

**Warning signs:** after `format.bat`, a `new_usertype` call sits on one line. Or a usertype's method set shrinks in a debug print.

**Phase to address:** P2 (harden the test), P3 (every member-pointer rewrite). **Confidence:** HIGH.

---

### Pitfall 4: sol2 configuration drifts between TUs (ODR) and between builds

**What goes wrong:**
sol2's behaviour switches come from preprocessor macros evaluated in each TU. Many of the functions they change are `inline` non-templates or templates whose arguments stay the same while their bodies change:
- `default_exception_handler`, under `#if SOL_IS_ON(SOL_PRINT_ERRORS)` (`trampoline.hpp:49`)
- the self check under `SOL_SAFE_USERTYPE` (`call.hpp:485-503`)
- `stack::get` under `SOL_SAFE_GETTER` (`stack_core.hpp:1138, 1163`)
- `basic_table_core`'s constructor under `SOL_SAFE_REFERENCES` (`table_core.hpp:347-350`)

Two TUs in the same binary that see different macro values are an ODR violation. The linker keeps one COMDAT copy at random, and neither MSVC nor ld reports it. Concrete ways this could happen here:
1. Trying `SOL_ALL_SAFETIES_ON` on one TU through `set_source_files_properties(... COMPILE_DEFINITIONS)` to measure its cost.
2. A unit test for an internal helper that includes `src/sandbox/*.h`, and therefore sol2, in `quiver_tests` without the PRIVATE defines. Hidden visibility (`cmake/Platform.cmake:40-42`) keeps this a different-configuration copy rather than cross-library interposition on shared builds. With `QUIVER_BUILD_SHARED=OFF` it becomes a real one-binary ODR violation. It also breaks the "no test includes sol2" claim (src/AGENTS.md:741) that makes PRIVATE defines complete coverage.
3. Adding a sol2 TU to `quiver_c` or `quiver_cli` instead of `quiver`.

**Why it happens:** "it's just a define on one file" feels harmless, and defines live far from the code they change.

**How to avoid:**
- All sol2 macros go only in the `target_compile_definitions(quiver PRIVATE …)` block (`src/CMakeLists.txt:61-66`), never per source file.
- Measure the cost by flipping the flag for the whole target.
- Every `src/sandbox/*.cpp` goes in `QUIVER_SOURCES`.
- Test internals only through Lua scripts. If a sol2-free helper such as `resolve_contained_path` needs direct tests, give it its own sol2-free header.
- Add a CMake check or a test that no target other than `quiver` has `sol2` in its link interface.

**Warning signs:** a binding's behaviour changes depending on which TU last touched a helper. A `set_source_files_properties` call naming a `src/sandbox` file with anything other than `/bigobj`.

**Phase to address:** P2 (target and file layout), P4 (adding `SOL_ALL_SAFETIES_ON`). **Confidence:** HIGH.

---

### Pitfall 5: A second `new_usertype<Database>` silently wipes the first

**What goes wrong:**
The per-domain layout invites each TU to call `lua.new_usertype<Database>("Database", …)` for its own slice. sol2's registration starts with `clear_usertype_storage<T>(L_)`, whose comment says "tell the old usertype (if it exists) to fuck off" (`usertype_storage.hpp:989-993`), and then rebuilds the storage. Whichever binder runs last wins, and every `db:` method registered earlier disappears. Scripts then fail with "attempt to call a nil value", and only for the domains registered earlier.

**Why it happens:** `new_usertype` reads like "get or create". It is "replace".

**How to avoid:**
- Create `new_usertype<Database>` exactly once, in `bind_database.cpp`.
- Every other binder takes `sol::usertype<Database>& bind` as a parameter literally named `bind`. The sync test's Pass 1 depends on that name. A `sol::table& ns` parameter for `lua["quiver"]` follows the same rule.
- The same applies to `BinaryFile`, which gets operator metamethods from `bind_expression_operators<T>`.
- P2 adds a test that calls one method from every domain in a single script. The existing suites probably cover this already. Confirm that their count is unchanged.

**Warning signs:** `grep -c 'new_usertype<Database>' src/sandbox` returns more than 1.

**Phase to address:** P2. **Confidence:** HIGH.

---

### Pitfall 6: Shared-header definitions: anonymous namespaces, missing `inline`, duplicate `inline`, guard collisions

**What goes wrong:** the members of `Impl` were implicitly inline because they were defined in the class. Moving them out goes wrong in four ways:
- **No `inline` on a non-template in the header.** The link fails with LNK2005 or "multiple definition". This one is loud.
- **An anonymous namespace in the header.** Every TU gets its own copy. MSVC `/W4` warns C4505 and GCC `-Wall` warns `-Wunused-function` in every TU that does not use a helper, so the build fills with noise. A function-local `static` in such a helper becomes per-TU state.
- **The same `inline` helper copied into two TUs with different bodies, in one named namespace.** For example, two `string_key` copies whose messages differ. This is an ODR violation that nothing diagnoses. Which message a script sees then depends on link order. The worst variant is a TU-private struct such as `GroupColumn` defined twice in a named namespace with different layouts.
- **Include-guard or name collision.** An internal `src/sandbox/sandbox.h` guarded `QUIVER_SANDBOX_H` collides with the public `include/quiver/sandbox.h`, which will follow the existing `QUIVER_<NAME>_H` scheme. One of the two then expands to nothing. Separately, `#include "sandbox.h"` from inside `src/sandbox/` resolves to the local file before the include path.

**Why it happens:** mechanical cut-and-paste from a class body, done in parallel by several agents.

**How to avoid:**
- Shared helpers live in one `src/sandbox/*internal*.h` (named distinctly, for example `sandbox_internal.h` with guard `QUIVER_SANDBOX_INTERNAL_H`). They are `inline` or templates in a named namespace (`quiver::sandbox_detail`), as the map recommends.
- TU-private helpers and types go in that TU's anonymous namespace.
- A helper is defined once. Grep for duplicate function names across `src/sandbox` before merging.
- Note that sol2 strips `(anonymous namespace)` from type names (`demangle.hpp:39-47`). Two anonymous-namespace types with the same name, both registered as usertypes, would share one metatable key `sol.<name>`. That can only happen with `CsvWriter`, and only if it is ever duplicated.

**Warning signs:** C4505 or `-Wunused-function` warnings in new TUs. The same function name shows up in two `src/sandbox` files.

**Phase to address:** P2 (header shape), P5 (header and guard names). **Confidence:** HIGH (sol2/ODR mechanics), MEDIUM (the exact warning IDs).

---

### Pitfall 7: Self-registration or reordered binders break the ctor invariant

**What goes wrong:** "each TU registers its own slice" invites static registrar objects that push binder functions into a global list. Three problems follow:
- The order of static initialisation across TUs is unspecified. The ctor sequence is not optional: `open_libraries` -> nil `dofile`/`loadfile` -> `create_named_table("quiver")` -> binders -> `lua["db"] = &db` last (a convention, see below). `bind_binary` and `bind_expression` read `lua["quiver"]`.
- In a `QUIVER_BUILD_SHARED=OFF` static library, a TU whose only content is a registrar is dropped by the linker, and its domain silently never registers.
- A namespace-scope table such as `const std::map<std::string, Operation>`, used from another TU's static initialiser, is the classic static-initialisation-order fiasco.

Pushing `lua["db"]` before the first `new_usertype<Database>` is harmless. On the first registration nothing is cleared: `clear_usertype_storage<T>` returns early when no storage exists (`usertype_storage.hpp:932-940`), and `register_usertype` reuses and fills the metatable the early push created, through `luaL_newmetatable` (`:1052`). So `db` works; this was verified by compile-and-run against the vendored sol2/Lua. Only a later, second `new_usertype<Database>` clears and rebuilds it (Pitfall 5). Keeping `lua["db"] = &db` last is a readability convention, not a correctness requirement.

**How to avoid:**
- The `Sandbox::Impl` ctor calls `bind_database(…)`, `bind_csv(…)`, `bind_binary(…)` and so on explicitly, in the current order.
- No registrars.
- Constant tables are `constexpr std::array` or function-local statics, as sol2 itself does (`usertype_traits.hpp:34-54`).
- Keep `open_libraries(` in exactly one file, the ctor's (the sync test needs it).

**Phase to address:** P2. **Confidence:** HIGH.

---

### Pitfall 8: Drift in check order changes which error a call reports

**What goes wrong:** several orders are byte-for-byte contracts:
- `open_file` validates `mode` (line 767) before the containment check (770). `db:open_file("../x","z")` reports the mode error.
- CSV operations run containment, then option decoding, then the work (D-22).
- `CsvWriter:write_row` checks the type, then the closed state, then formats cells (WRITE-05).

Three planned changes can move an error:
- A P3 helper that does "resolve path first".
- A P4 `require_table` placed at the top of a lambda. Then `db:write_csv("../escape", 5)` reports a table error instead of the containment error.
- M10 (`csv_cell_to_string` via `lua_to_value`), if the dispatch order changes. `is<int64_t>()` must stay before `is<double>()`, and the boolean check must stay first.

M3 (member pointers) changes the Debug error for a dot call (`db.describe()`), from the generic argument mismatch to sol2's self text (`call.hpp:487-490`). No test pins either message, so it is acceptable, but list it as a known difference in the PR.

**How to avoid:**
- Put `require_table` and `optional_from_lua` exactly where today's decode or iteration happens, not at the top of the function.
- For each table-taking method with more than one validated argument, P1 adds one test with two bad arguments that pins which error wins.
- P3 PRs list every reordered check. The answer must be "none".

**Phase to address:** P1 (pins), P3, P4. **Confidence:** HIGH.

---

### Pitfall 9: The `SOL_ALL_SAFETIES_ON` backstop has side effects beyond safety

**What goes wrong:**
- **stderr noise in Release.** This is finding 2 above.
- **The backstop's own errors are `luaL_error` longjmps.** Lua is compiled as C (`LUA_LANGUAGE:STRING=C`, `CMakeCache.txt:486`), so `LUAI_THROW` is `longjmp` (`ldo.c:66, 73`). sol2's up-front argument checks run before any argument object is built (`stack.hpp:194-197`), so they are safe. A `SOL_SAFE_GETTER` failure inside a lambda body is not: an `.as<T>()` on the wrong type calls `type_panic_c_str` -> `luaL_error` (`error_handler.hpp:105-109`, `stack_core.hpp:1163`). That longjmps over live C++ frames, which per the C++ standard is UB. Whether their destructors run is platform-dependent (STACK.md cites MSVC docs saying `/EHsc` unwinds them while GCC/Clang do not; LOW, unverified against this repo). Where they are skipped, strings and vectors leak. So the backstop must never replace an explicit `get_type()` or `lua_cell_as<T>` check before an `.as<T>()`.
- **The loose table check still accepts userdata.** `is<sol::table>()`, `sol::optional<sol::table>` and a checked `sol::table` parameter all use `loose_table_check`, which accepts userdata (`stack_check_unqualified.hpp:41-52, 472-473`). `require_table` must test `obj.get_type() == sol::type::table`, the way `write_row` already does (line 905).
- **Raw sol2 text.** The self check gives `sol: received nil for 'self' argument…` (`call.hpp:487-490`). That is not Pattern 1. It is fine as a backstop, but it belongs in the CHANGELOG.
- **Builds converge on Debug semantics.** This is good. The CI Release C++ jobs then stop being an independent check, so the explicit tests matter more.

**How to avoid:**
- Define `SOL_ALL_SAFETIES_ON=1 SOL_PRINT_ERRORS=0` together, target-wide.
- Add a host test that runs a failing `pcall` script and asserts that nothing reaches stderr (C++ with a captured stderr, or Python `capfd`).
- `require_table` uses `get_type()`.
- Grep for `.as<` in `src/sandbox` and confirm each one is guarded by a type check.

**Phase to address:** P4. **Confidence:** HIGH.

---

### Pitfall 10: Lifetime of run state and GC order during the split

**What goes wrong:** today, `[this]` captures (761, 876, 1032) stay valid across a `LuaRunner` move only because `Impl` lives on the heap and never moves. The split risks the following:
- Binder lambdas capture `RunHandles&` (or `Context&`) that live somewhere other than inside the heap `Impl`, for example a local in the ctor or a `Sandbox` member. That is a dangling reference after the move.
- `GcGuard` is declared after `result`. Collection then runs while `result` still anchors the script's userdata (D-46).
- The single `collect_garbage()` is "hardened" into a loop or a double call. The documented contract is exactly one.
- `close_open_writers` is moved into a `RunHandles` destructor. Handles outlive individual runs, so that closes writers at the wrong time and never between runs.
- The registry keeps `shared_ptr` instead of `weak_ptr` "to simplify". A dropped writer then never finalises and keeps its path busy.
- M13's `erase_if` prunes entries whose writer is closed but still alive. `path_has_open_writer` is meant to skip closed writers (`is_closed()`), not forget them. Pruning only `expired()` entries is safe.
- Member order inside `Impl`. Today `lua` is declared before the registries, so the registries are destroyed first. No finalizer touches them, which is fine. If a future `__gc` does, it reads destroyed memory. Declare `RunHandles` before `sol::state lua` so the state is torn down first.

**How to avoid:**
- `RunHandles` is a member of `Impl`.
- `run()` stays in one function with `GcGuard` first.
- P2 keeps `HandleFromAnEarlierRunIsClosed`, `WriterHeldInAGlobalIsClosedWhenRunReturns` and `UnclosedWriterHeldInAGlobalIsAlsoFlushedWhenRunReturns` green, and adds a move test: construct, move-assign, then `run` a script that calls `db:write_csv` and `db:open_file`.

**Phase to address:** P2, P3 (M13). **Confidence:** HIGH.

---

### Pitfall 11: Generated vs hand-maintained FFI files in the rename

**What goes wrong:**
- **Dart:** see finding 3. Regenerating `bindings.dart` rewrites the file and introduces breaking enums. The live ffigen config is the `ffigen:` block in **pubspec.yaml** (lines 26-38). `ffigen.yaml` is a dead duplicate, so editing only that file changes nothing (bindings/dart/AGENTS.md:22-26).
- **Julia:** the generator collects headers by `.h` glob (`generator.jl:27-31`), so a renamed header is picked up automatically. Its position in `c_api.jl` moves (alphabetical order), so the diff is larger than four functions. Any hunk not related to the rename means `c_api.jl` had drifted and has to be reviewed. Do not take it blindly.
- **Python:** `_c_api.py` cdefs (lines 398-404) are hand-maintained. `generator/generator.py:22` HEADERS also names `lua_runner.h`. CFFI ABI mode resolves symbols lazily, so a stale cdef name surfaces as `AttributeError` only at the first `Sandbox(...)`.
- **JS:** rename the `luaSymbols` keys in `loader.ts:199-204, 219` and leave the documented Bun FFI workarounds alone (Do Not Fix).
- **C API:** `QUIVER_REQUIRE(runner, …)` stringifies the parameter name (`internal.h:45-51`). Renaming `runner` / `out_runner` changes user-visible text from "Null argument: runner" to "…: sandbox". Nothing pins that text, but it is a message change that belongs in the CHANGELOG. Add `src/c/sandbox.cpp` to the `quiver_c` source list.

**How to avoid:** in P5, run the Julia generator only (not `scripts/generator.bat`) and review the diff. Hand-edit the six Dart entries in `bindings.dart` in the existing style (4 functions, the `quiver_lua_runner` opaque class, the `quiver_lua_runner_t` typedef), and their uses in `lua_runner.dart:26,33`. Use the Python generator only as a diff aid.

**Phase to address:** P5. **Confidence:** HIGH.

---

### Pitfall 12: Stale natives hide or fake failures

**What goes wrong:** each binding finds the rebuilt DLL differently.
- **Dart:** the native-assets hook keeps its own Release CMake build under `.dart_tool/hooks_runner/shared/quiverdb/build/<hash>/`. Its checksum does not cover the hook's defines (bindings/dart/AGENTS.md:79-85). After P2 (CMakeLists source list), P4 (new defines) or P5 (symbols), tests can run against the old DLL. Dart looks symbols up lazily, so only the `Sandbox` tests fail, with "Failed to lookup symbol 'quiver_sandbox_new'".
- **Bun:** `dlopen` with the full symbol table fails eagerly, so the whole suite errors at load.
- **Python:** dev mode resolves `libquiver_c` from PATH (`_loader.py`), and the `_libs/` bundle wins if one exists.
- **Julia:** `@ccall` is lazy.
- **Windows:** a Julia REPL, VS Code Python or a running Dart test holds `libquiver.dll` / `libquiver_c.dll` open. The rebuild then fails with LNK1104 or "permission denied", or worse, a partial rebuild leaves the old C DLL next to a new core DLL.

**How to avoid:**
- After P2, P4 and P5, delete `bindings/dart/.dart_tool/hooks_runner/` and `.dart_tool/lib/`.
- Close host processes before `build-all.bat`.
- Check symbol presence directly (`dumpbin /exports build/bin/libquiver_c.dll | findstr sandbox`) rather than inferring it from green tests.
- Remember that the Dart suite is the only local Release run, which makes it valuable for P4.

**Phase to address:** P2, P4, P5. **Confidence:** HIGH (Dart, Bun, Python), MEDIUM (Julia laziness detail).

---

### Pitfall 13: The rename sweep damages the other meanings of "sandbox"

**What goes wrong:** a regex replace of `LuaRunner` -> `Sandbox`, or a broad edit of "sandbox", hits things it should not:
- **Historical CHANGELOG entries.** Lines 199-202, 505, 819, 896 and 1061 must stay as they are.
- **Path-policy prose.** `resolve_sandboxed_path` (10 call sites), "sandboxed", and lua-api.ts lines 108, 647, 785, 870 and 905. These become "directory containment" (`resolve_contained_path`).
- **The stdlib meaning.** "`io` is absent from the sandbox" (lua-api.ts:695, 783).
- **The host-harness meaning.** lua-api.ts:162 "run_lua sandbox" is shipped LLM prompt text, so reword it.
- **Test fixtures.** `LuaSandboxTest` and its `sandbox` member are used at 137 sites. A local `quiver::Sandbox sandbox(db);` shadows the member in every derived `TEST_F`. That is a compile error where `sandbox / "x"` is used, and silent where only `.run` is called.
- **The scratch target.** `tests/sandbox` / `quiver_sandbox` is protected by Do Not Fix. Renaming it to `tests/scratch` / `quiver_scratch` is approved, but a stale `build/bin/quiver_sandbox.exe` stays behind and confuses grep.
- **Binding messages.** Python asserts `match="LuaRunner is closed"` (`test_lua_runner.py:93, 99`) and has the "not closed explicitly" `ResourceWarning`. JS and Dart have no test for their closed or disposed text, so a missed rename there passes. Dart's `LuaException` -> `SandboxException` touches `isA<LuaException>()` at three test sites and every downstream `catch`.
- **gtest suites.** Today 378 tests match `LuaRunner*`, plus 50 in `LuaBinaryTest` and `LuaExpressionTest`, 428 in total. After the rename, `Sandbox*` must match exactly 428 in `quiver_tests`, and the C API suite must still have 27. Suite names with underscores (`LuaRunner_ReadCsv`) go against googletest's FAQ advice (MEDIUM). The rename is the cheap moment to switch to `SandboxReadCsvTest`.

**How to avoid:**
- Rename symbols one at a time with a whole-word list, never a blanket regex.
- Exclude `CHANGELOG.md` history.
- Keep the test-local variable named `lua`.
- Add closed/disposed message tests to JS and Dart in P5.
- Compare gtest counts with `--gtest_list_tests` before and after. On Windows, call the exe directly, because quoted filters through `cmd //c` break (user memory note).

**Phase to address:** P5. **Confidence:** HIGH.

---

### Pitfall 14: CHANGELOG and version handling

**What goes wrong:**
- **Wrong section.** CMake and all four manifests are already at 0.13.0. The newest tag is `v0.12.9`, which was cut at 0.12.9. The CHANGELOG's newest section is `[0.12.8]`, and neither `[0.12.9]` nor `[0.13.0]` exists. The memory note saying "plan entries go under [0.12.9]" is stale.
- **An unneeded bump.** Running Bump Version would produce 0.14.0 for no reason.
- **Behaviour fixes logged as plain "Fixed".** Several P4 changes make a previously accepted call throw, or change data:
  - C7: `{col = {}}` now clears a group. Today it is skipped, so existing scripts that pass empty arrays will start deleting rows. A `read_vectors_by_id` -> `update_element` round trip of an all-NULL column now clears it.
  - C1/C5: `db:query_integer(sql, 5)` and `db:bin_to_csv(p, "false")` now throw.
  - C6: the error text changes.
  - Release dot-calls go from UB to an error.

**How to avoid:**
- Each phase PR adds `## [0.13.0] — unreleased` plus its compare link, if not already present, per root AGENTS.md "Versioning".
- No bump.
- C7, C1 and C5 get **BREAKING** entries that say what a script author must change.
- The rename entry lists every renamed symbol per layer: the C++ header and class, the C API symbols and header, `Quiver.Sandbox`, `Sandbox` / `SandboxException` in Dart, `quiverdb.Sandbox`, the JS `Sandbox` export, and the changed messages.
- Whether to backfill the missing `[0.12.9]` section is the maintainer's call. Flag it, do not decide it.

**Phase to address:** P4, P5. **Confidence:** HIGH.

## Moderate Pitfalls

- **NOLINT blocks split or left behind.** The `NOLINTBEGIN/END(performance-unnecessary-value-parameter)` pairs at 591/685, 953/1016, 1026/1095 and 1248/1276 wrap lambdas. Cutting a block across two files leaves an unmatched `NOLINTBEGIN`, which recent clang-tidy reports as an error (MEDIUM). Lambdas that land outside a block get flagged. `tidy.bat` covers `src/sandbox`, because its regex excludes only `src/binary`, but tidy is **not** in CI (only cppcheck in pre-commit). So this rots silently unless P2 runs `scripts/tidy.bat`. Fix: one BEGIN/END pair per binder function, and run tidy in P2 and P3.
- **`/bigobj` per-file property points at a vanished path.** `set_source_files_properties(lua_runner.cpp …)` (`src/CMakeLists.txt:70-74`) silently matches nothing once the file moves. The new TUs may compile now and hit C1128 or "too many sections" later, as they grow, on Windows only. Replace it with the target-wide generator expression in P2.
- **csv-parser headers included from `src/sandbox/`.** That breaks the Pimpl reason recorded at src/AGENTS.md:101-103. A P3 `CsvWriter` member move must only include `src/csv/csv_write.h` / `csv_read.h`.
- **Forward references disappear with class scope.** For the list, see the map plus critic note 3. These failures are loud (they do not compile), but for the parallel agents in P2 they mean the shared header has to land first. Sequence the work: header first, then the TUs.
- **M6 (sol2's variant pusher replacing `value_to_lua_object`).** Platform-default macros differ on macOS (`SOL_NO_NIL` history, `version.hpp:522-531`). Do it only with a green macOS CI run, or skip it.
- **M4 `template <auto Read>` over member pointers.** If a `Database` reader gets an overload later, `&Database::read_scalar_strings` stops compiling. That is loud and acceptable. Keep the `bind.set_function("name", &bulk_read_lua<…>)` line shape for Pass 1.
- **Usertype type names change.** Moving `CsvWriter` out of `LuaRunner::Impl` changes the `sol.<demangled>` metatable key (`usertype_traits.hpp:41-42`) and any sol2 Debug text that embeds the type name. No test pins either one (checked by grep). Accept it.

## Minor Pitfalls

- **Line endings.** Files created on Windows get normalised by `.gitattributes` (`* text=auto eol=lf`), but the pre-commit `mixed-line-ending` hook still flags CRLF in the working tree. Run pre-commit before committing.
- **Duplicate base names** (`src/sandbox/database.cpp` next to `src/database.cpp`). CMake and gcov handle them, but they confuse humans and Codecov file lists. Prefer domain-prefixed names such as `bind_database.cpp`.
- **Docs that cite the old path.** `cmake/Platform.cmake:4`, `hook/build.dart:54` and `bindings/dart/AGENTS.md:65` (the macOS `to_chars` floor list), `bindings/julia/AGENTS.md:158`, the `src/csv/*` header comments plus `csv_write.cpp:26,48` (critic "Missing 1"), `tests/test_database_ui_metadata.cpp:60` and `tests/test_lua_runner_write_csv.cpp:22` name `src/lua_runner.cpp`. Update them in P2. The other choice is P5, but they are path facts, not name facts.

## Technical Debt Patterns

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| Rely on `SOL_ALL_SAFETIES_ON` instead of `require_table` | Fewer edits | Raw sol2 text (not Pattern 1). Userdata still accepted (loose check). `as<T>` failures longjmp over C++ frames | Never as the only check. Fine as a backstop |
| Keep Pass 2's line-shape heuristic unchanged | No test rewrite | Every P3 member-pointer edit can silently drop names | Only with per-usertype meta-guards added |
| Run `scripts/generator.bat` for the rename | One command | Rewrites `bindings.dart`, breaking enums in hub | Never in this milestone. Julia generator only |
| `SOL_ALL_SAFETIES_ON` per file, to measure | Isolated benchmark | ODR violation inside `libquiver` | Never. Flip the whole target |
| Copy a helper into each TU as `inline` | Each TU stays self-contained | Silent ODR; message text depends on link order | Never. Define it once, or in an anonymous namespace |

## Integration Gotchas

| Integration | Common Mistake | Correct Approach |
|-------------|----------------|------------------|
| Dart ffigen | Editing `ffigen.yaml`, or regenerating | Edit the pubspec.yaml `ffigen:` block (and the duplicate, for consistency). Hand-edit `bindings.dart` |
| Dart native assets | Trusting a cached DLL after a CMakeLists or define change | Delete `.dart_tool/hooks_runner/` and `.dart_tool/lib/` |
| Python CFFI | Renaming the class but not the cdefs or the `generator.py` HEADERS | Update `_c_api.py:398-404` and `generator.py:22`. Lazy symbol errors appear only on first use |
| Bun FFI | "Simplifying" the loader while renaming the keys | Rename `luaSymbols` keys only (Do Not Fix) |
| Julia | Assuming the regeneration diff is rename-only | Review every hunk. The header moves position under the glob |
| C API errors | Renaming parameters without noticing the text change | `QUIVER_REQUIRE` stringifies names. Log "Null argument: sandbox" in the CHANGELOG |

## Performance Traps

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|----------------|
| `SOL_ALL_SAFETIES_ON` per-call checks on the binary hot path (`file:read(dims)` per cell, `lua_table_to_vector`) | Slower Lua loops over `.qvr` reads in Release | Measure Release before and after with `quiver_cli` running a fixed script (per-cell `file:read` loop, `read_scalar_integers` loop, expression build). `quiver_benchmark` has no Lua workload | Large per-cell loops. Debug numbers are meaningless here (already checked) |
| More sol2 TUs compiled in parallel on MSVC | Long builds and high memory use with Ninja `-j` | Record `cmake --build build --target quiver` time before and after (PROJECT.md out-of-scope note). Lower `-j` if C1060 appears | Laptops and 16 GB CI runners (MEDIUM) |
| Registry scans expired entries (M13) | `path_has_open_writer` grows linear in writers opened per run | `erase_if(expired)` before each insert | Scripts that open thousands of writers in one run |

## Security Mistakes

| Mistake | Risk | Prevention |
|---------|------|------------|
| Release UB on non-table arguments (C1) treated as a "wrong error" bug | A host crash from untrusted script input. Scripts are untrusted by design | P4 `require_table` plus the backstop. Tests run in Release CI |
| A dedupe "resolve path first" helper that skips containment on one path, or reorders it after option decoding that touches the filesystem | Escape from the database directory | Every file operation still calls the single `resolve_contained_path`. The `DeviceNamePathIsReportedWithPrefix` tests and the binary escape tests stay green |
| Renaming `resolve_sandboxed_path` and leaving a second, older gate in place | Two policies that diverge | Rename in place with one definition. Grep shows no other `weakly_canonical` in `src/sandbox` |
| Moving `dofile`/`loadfile` nil-ing after a binder that runs Lua | Disk Lua loading is open during construction | The ctor order stays fixed (Pitfall 7) |

## UX Pitfalls

| Pitfall | User Impact | Better Approach |
|---------|-------------|-----------------|
| The C7 empty-array change shipped without reference text | LLM-authored scripts follow lua-api.ts:306-307 ("Empty arrays are skipped") and delete data | Update the reference text in the same PR. BREAKING entry |
| The shipped prompt still says `LuaRunner` (lua-api.ts:23, 30), or "sandbox" stays ambiguous (162) | The agent is told about a class name that no longer exists | Reword in P5. The sync test does not check prose |
| stderr spam from `SOL_PRINT_ERRORS` | Host apps print `[sol2]` lines for caught errors | `SOL_PRINT_ERRORS=0` |

## "Looks Done But Isn't" Checklist

- [ ] **Split:** `grep -c "new_usertype<Database>" src/sandbox/*` totals 1. `open_libraries(` appears once. `src/lua_runner.cpp` is deleted (no stale copy compiled by an old CMake cache).
- [ ] **Sync test:** reads the folder sorted, parses per file, and has Pass 2 guards for the four non-`Database` usertypes. Deliberately deleting one `bind.set_function` and one usertype pair makes it fail (mutation check; revert it afterwards).
- [ ] **Build flags:** `/bigobj` is set target-wide. `SOL_*` defines appear only on the `quiver` target. Release CI jobs are green on all four OSes, macOS included.
- [ ] **Tidy:** `scripts/tidy.bat` is clean on `src/sandbox`, with no unmatched NOLINT.
- [ ] **Dedupe:** the PR lists "reordered checks: none". The gtest count is unchanged.
- [ ] **Fixes:** `SOL_PRINT_ERRORS=0` is set with the backstop. The perf numbers are written into the PR or CHANGELOG. Every `.as<` in `src/sandbox` has a type check before it.
- [ ] **Rename:** `Sandbox*` lists 428 tests and the C API suite still has 27. `dumpbin /exports` shows `quiver_sandbox_*` and no `quiver_lua_runner_*`. The Dart cache was cleared before the Dart run. JS and Dart closed-message tests were added. Historical CHANGELOG lines are untouched.
- [ ] **Docs:** every AGENTS.md path citation from the map's list, plus `Platform.cmake:4`, `build.dart:54`, `bindings/dart/AGENTS.md:65`, `bindings/julia/AGENTS.md:158`, `src/csv/*.h` comments, `csv_write.cpp:26,48`, `tests/test_database_ui_metadata.cpp:60`, `tests/test_lua_runner_write_csv.cpp:22` and src/AGENTS.md:708 (the `SOL_SAFE_FUNCTION` claim), matches.

## Recovery Strategies

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| ODR from mixed macros | MEDIUM | Move all defines to the `quiver` target and do a clean rebuild (`clean-all.bat`). The bug class cannot be found by reading code |
| Usertype wiped by a second `new_usertype` | LOW | Turn the later call into `bind.set_function` on a passed handle |
| `bindings.dart` regenerated by accident | LOW | `git checkout bindings/dart/lib/src/ffi/bindings.dart`, then hand-edit the six entries (4 functions, opaque class, typedef) and `lua_runner.dart`'s uses |
| Sync test went vacuous and doc drift shipped | MEDIUM | Re-diff the `db:` surface against `lua-api.ts` by hand, add the guards, republish npm |
| Release crash from a P1 test | LOW | Move the test to P4 and assert the post-fix message |

## Pitfall-to-Phase Mapping

| Pitfall | Prevention Phase | Verification |
|---------|------------------|--------------|
| 1 UB pinned or Release crash | P1, P4 | New tests green in the local `release` preset and in the CI Release matrix |
| 2 Vacuous sync test | P2 | A mutation check fails the test. Per-usertype guards present |
| 3 clang-format joins a registration | P2, P3 | `format.bat`, then the sync test. Guards catch a shrunken set |
| 4 Macro drift / ODR | P2, P4 | Defines only on `quiver`. No test target links sol2 |
| 5 Double `new_usertype` | P2 | grep count of 1. Full Lua suites green |
| 6 Header definitions / guards | P2, P5 | No C4505 or `-Wunused-function`. Distinct guards |
| 7 Ctor order / registrars | P2 | The ctor reads as an explicit ordered list |
| 8 Check order | P1, P3, P4 | Two-bad-argument pin tests |
| 9 Backstop side effects | P4 | stderr test. `get_type()` in `require_table`. Perf recorded |
| 10 Lifetime / GC | P2, P3 | Named pins plus a move test |
| 11 FFI regeneration | P5 | `bindings.dart` diff limited to the six rename entries (4 functions, opaque class, typedef) plus `lua_runner.dart`; `grep quiver_lua_runner bindings/dart/lib` is empty. Julia diff reviewed |
| 12 Stale natives | P2, P4, P5 | Cache cleared. `dumpbin` symbol check |
| 13 Rename collateral | P5 | gtest counts. Closed-message tests in all four bindings |
| 14 CHANGELOG / version | P4, P5 | `[0.13.0] — unreleased` exists. BREAKING entries. Versions unchanged at 0.13.0 |

## Sources

- sol2 v3.5.0 (vendored, `build/_deps/sol2-src/include/sol/`):
  - `version.hpp:236-256, 314-436, 522-531, 623-646`
  - `forward.hpp:120-140`, `forward_detail.hpp:33-40`
  - `call.hpp:471-530`, `stack.hpp:186-200`, `stack_core.hpp:1136-1171`
  - `stack_check_unqualified.hpp:41-52, 472-473`, `error_handler.hpp:98-110`
  - `trampoline.hpp:42-72`, `usertype_storage.hpp:932-940, 985-1000, 1052`, `usertype.hpp:85-100`
  - `usertype_traits.hpp:32-54`, `demangle.hpp:39-124`, `table_core.hpp:340-352`, `table_iterator.hpp:70`
- Lua 5.4.8 (vendored, `build/_deps/lua-src/`):
  - `src/lapi.c:725-729, 1253-1265`, `src/llimits.h:122-126`, `src/ldo.c:58-77`
  - `cmake/LuaLanguage.cmake:1-22`, `doc/manual.html:4653`
- Repo:
  - `src/CMakeLists.txt:1-75`, `cmake/Platform.cmake:40-42`, `cmake/CompilerOptions.cmake`, `.clang-format`, `.clang-tidy`
  - `scripts/tidy.bat`, `scripts/generator.bat`, `.github/workflows/ci.yml`, `.github/actions/build-cpp/action.yml`
  - `bindings/js/test/lua-api-sync.test.ts`, `bindings/js/src/loader.ts:199-219`
  - `bindings/python/src/quiverdb/_c_api.py:398-404`, `_loader.py`, `bindings/python/generator/generator.py:15-23`
  - `bindings/julia/generator/generator.jl:27-31`, `bindings/dart/hook/build.dart`
  - `bindings/dart/AGENTS.md:20-85`, `src/c/internal.h:45-51`, `src/c/lua_runner.cpp`
  - `tests/test_lua_runner.h`, `tests/CMakeLists.txt`, `CHANGELOG.md`, `git tag`
- `.planning/research/LUA-RUNNER-MAP.md`, including its critic section, and `.planning/PROJECT.md`.
- MEDIUM, training knowledge, not fetched: googletest FAQ on underscores in test names; Node `readdirSync` ordering; clang-tidy unmatched-NOLINTBEGIN diagnostic; MSVC C4505 and C1060 behaviour.

---
*Pitfalls research for: sol2 binding split + cross-layer FFI class rename (Quiver Sandbox refactor)*
*Researched: 2026-10-02*
