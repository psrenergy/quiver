# Stack Research

**Domain:** Embedded Lua scripting layer (sol2 v3.5.0 over Lua 5.4.8) inside a C++20 library with a C API and FFI bindings. Covers splitting `src/lua_runner.cpp` into `src/sandbox/` TUs and Release type-safety hardening.
**Researched:** 2026-10-02
**Confidence:** HIGH for sol2 and Lua behaviour (read from the vendored source and, where noted, compiled locally). LOW for runtime cost (inferred from source; not measured yet). LOW for the local compile-time measurements (not reproduced).

Ground truth for this file is the vendored source under `build/_deps/sol2-src/include/sol/` (written `sol/` below) and `build/_deps/lua-src/` (written `lua/`). No new dependency is needed. The work is configuration, plus one CMake block.

## Recommended Stack

### Core Technologies (unchanged versions, pinned in `cmake/Dependencies.cmake`)

| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| sol2 | v3.5.0 (`sol/version.hpp:31-34`) | C++↔Lua binding | Already vendored and load-bearing. Do not bump it during a behaviour-neutral split, because macro semantics are version-specific (see the `SOL_SAFE_FUNCTION` finding below). |
| Lua (lua-cmake wrapper) | `lua-cmake/v5.4.8.0`, compiled as **C** (`LUA_LANGUAGE=C`, `build/CMakeCache.txt:486`) | Interpreter | Unchanged. Compiling Lua as C means `lua_error`/`luaL_error` are `longjmp`s, which shapes the safety advice below. |
| CMake | ≥ 3.26 (project floor) | Build | Everything below works at 3.26. No generator expressions or 3.28 features are needed. |
| MSVC 19.51 (VS 18) / GCC / Apple clang | as in CI (`ci.yml:23-25`: ubuntu x64+arm, windows-latest, macos-latest × Release+Debug) | Compilers | Windows CI uses the default VS generator, so it is MSVC. The MinGW branch is local-only. |

### sol2 safety configuration (the core decision)

**Recommended `src/CMakeLists.txt` block** (replaces lines 61-74):

```cmake
# sol2 settings. PRIVATE on `quiver` reaches every src/sandbox/ TU, and only those TUs include sol2.
target_compile_definitions(quiver PRIVATE
    SOL_ALL_SAFETIES_ON=1   # Release gets the same argument/self/table checks the Debug CI matrix already runs
    SOL_PRINT_ERRORS=0      # ALL_SAFETIES would otherwise print every caught error to std::cerr in the host
    SOL_SAFE_NUMERICS=1     # explicit, although ALL_SAFETIES implies it; src/AGENTS.md calls it load-bearing
    SOL_NO_NIL=1            # portability guard: sol::nil can clash with ObjC's nil on Apple
)

# sol2's template instantiations exceed COFF's default section limit. Target-wide, so no new TU can miss it.
if(MSVC)
    target_compile_options(quiver PRIVATE /bigobj)
elseif(WIN32)
    target_compile_options(quiver PRIVATE -Wa,-mbig-obj)
endif()
```

#### What each macro actually does in v3.5.0

The value of a `SOL_*` macro resolves in this order: explicit define, then `SOL_ALL_SAFETIES_ON`, then `SOL_DEBUG_BUILD`, then off. `SOL_DEBUG_BUILD` is set by MSVC `_DEBUG` (`/MDd`), or by GCC/Clang with `!NDEBUG && !__OPTIMIZE__` (`sol/version.hpp:240-257`).

| Macro | Defined at | Release today | What it gates (vendored source) | Fixes in this milestone |
|---|---|---|---|---|
| `SOL_ALL_SAFETIES_ON` | `version.hpp:314-322` | off | Turns on every row below, plus `SOL_PRINT_ERRORS` (`:623-637`). | Makes Release behave like Debug. |
| `SOL_SAFE_FUNCTION_CALLS` | `:394-408` | **off** | `detail::default_safe_function_calls` (`forward_detail.hpp:34-39`), which drives `multi_check<Args...>` before every bound call (`stack.hpp:190-197`). | **The main one.** It checks every lambda argument, including `Database& self` and every `const sol::table&`. Numbers, strings, booleans and nil passed as a table get a sol2 error instead of reaching `lua_next`. |
| `SOL_SAFE_USERTYPE` | `:340-354` | **off** | The `self` check for **member-pointer** bindings (`call.hpp:485-496`, `:519-529`, `:578-588`, `:817-827`). Without it the code is `*stack::unqualified_get<non_null<Ta*>>(L,1)`, a null or garbage dereference. | Needed once M3/M11 move registrations to member pointers (`&Database::describe`, `&CsvWriter::write_row`). |
| `SOL_SAFE_REFERENCES` | `:356-370` | off | A type check inside `sol::table` / `sol::protected_function` / `sol::userdata` constructors from a stack index (`table_core.hpp:331-375`, `protected_function.hpp:78-173`). | A second layer for `.as<sol::table>()` on an arbitrary reference. |
| `SOL_SAFE_GETTER` | `:324-338` | off | `stack::get` / `object::as<T>()` go through `check_get`, which panics through `type_panic_c_str` on a mismatch (`stack_core.hpp:1136-1175`). | Turns the Release-only silent `as<std::string>()` on a number or boolean key (C2) into a raw sol2 error, as in Debug today. |
| `SOL_SAFE_PROXIES` | `:410-424` | off | `protected_function_result::get` validity checks (`protected_function_result.hpp:130-145`). | Neutral. Every `get` in the file already follows a `valid()` test (lua_runner.cpp:624, 649, 857, 2525). |
| `SOL_SAFE_STACK_CHECK` | `:456-470` | off | `luaL_checkstack` before pushes (`stack_push.hpp`, ~60 sites). | Cheap insurance for the big `to_lua_table` returns. |
| `SOL_SAFE_FUNCTIONS` / `SOL_SAFE_FUNCTION_OBJECTS` | `:372-392` | off | Only aliases `sol::function` to `protected_function` (`forward.hpp:120-140`) and routes `state_view::script` to `safe_script` (`state_view.hpp:554`). | Neutral. The code uses `sol::protected_function` and `safe_script` explicitly, and `grep` finds no `sol::function` and no `.script(`. |
| `SOL_SAFE_NUMERICS` | `:426-440` | **on** (explicit) | It has no direct use. It feeds `SOL_NUMBER_PRECISION_CHECKS` (`:472-494`), which makes `is<int64_t>()` false for a float (`stack_check_unqualified.hpp:141-148`). | Keep it, for the `is<int64_t>()`-before-`is<double>()` policy. |
| `SOL_NO_NIL` | `:522-532` | **on** (explicit) | Removes `sol::nil` / `sol::type::nil` (`types.hpp:97-100`, `:720-722`). Not a safety flag. It does not interact with `ALL_SAFETIES`. | Keep it. Write `sol::lua_nil` and `sol::type::lua_nil`. |
| `SOL_PRINT_ERRORS` | `:623-637` | off in Release, **on in Debug and under ALL_SAFETIES** | `std::cerr << "[sol2] An exception occurred: ..."` on **every** C++ exception caught by the trampoline (`trampoline.hpp:47-55`), on panic, and on `script_throw_on_error` (`state_handling.hpp:52-70`, `:152-156`). | **Must be forced to 0.** Without that, every Pattern 1/2/3 error a script triggers writes to the stderr of the Julia/Python/Dart/JS host process. |

**Finding: `SOL_SAFE_FUNCTION=1` is a dead define.** `grep -rnw SOL_SAFE_FUNCTION` over the sol2 tree matches only `documentation/source/safety.rst:51,189` and one regression test. The code reads `SOL_SAFE_FUNCTIONS` (plural) or `SOL_SAFE_FUNCTION_OBJECTS` (`version.hpp:372-392`). The define has had no effect since it was added, and even the intended macro would be neutral here (row above). Recommendation: delete it in the same change that adds `SOL_ALL_SAFETIES_ON`, and fix `src/AGENTS.md:708`, which repeats the claim. **PROJECT.md:72 currently lists it as "stays", so deleting it needs the user's sign-off.** If the user wants it kept, it costs nothing, but the comment must stop claiming it does something. Confidence: HIGH (grep plus a compile with `/showIncludes` flags).

**Why `SOL_ALL_SAFETIES_ON` is low-risk for behaviour.** CI already runs the full test suite in **Debug** on all four OSes (`ci.yml:25`). In Debug every row above is `SOL_DEFAULT_ON` through `SOL_DEBUG_BUILD`. So `ALL_SAFETIES_ON` makes Release match a configuration that 428 C++ Lua tests, 27 C API tests and three binding suites (Julia, Python, Bun, via `build-cpp`'s default Debug) already pass. Dart is not one of them: its native-assets hook always builds Release (`bindings/dart/hook/build.dart:67`), in CI and locally, so Dart already runs against Release sol2 defaults. The only Debug/ALL_SAFETIES difference left is `SOL_ASSERT` (`assert.hpp:49,74`), which stays Debug-only. Confidence: HIGH.

**What `SOL_ALL_SAFETIES_ON` does NOT fix (so `require_table` is mandatory, not optional):**
- **Userdata passed where a table is expected.** The table checker is `loose_table_check` (`stack_check_unqualified.hpp:41-52`, used at `:472-473`), and it returns `true` for any userdata. Iteration then calls `lua_next` → `gettable`, which is `api_check(ttistable(t)); return hvalue(t);` (`lua/src/lapi.c:725-729`, `:1253-1259`). That reinterprets a `Udata*` as a `Table*` in **every** build type, since APICHECK is off. "Iterating a userdata yields no keys" is undefined behaviour. That it is harmless in practice is LOW confidence (unverified: the concrete outcome depends on memory layout and was not reproduced). `require_table` must test `obj.get_type() == sol::type::table`, never `obj.is<sol::table>()`.
- **The error text.** A safety failure produces `stack index N, expected table, received number: ... (bad argument into 'R(Args...)')`, with a **demangled C++ signature** (`error_handler.hpp:86-96`, `:143-157`). It is not Pattern 1, and it differs across MSVC, GCC and Clang. Tests must never assert on it beyond the `Failed to run Lua script:` prefix plus a stable substring such as `expected table`. The explicit `require_table` / `lua_string_key` / `optional_from_lua` checks give the Pattern 1 text. The macro is only the backstop for paths nobody wrapped, and for `self`.
- **`sol::optional<T>` parameters (C5).** These are always checked with `check_get`, whatever the macros say, and a mismatch becomes `nullopt`. That is exactly the bug. Fix it in code with `optional_from_lua`.

**Interaction to sequence carefully.** `SOL_SAFE_GETTER` changes Release behaviour at every still-unguarded `pair.first.as<std::string>()` (lua_runner.cpp:1108, 1569, 1598, 2479; C2). Today these produce a silent wrong column name; with the flag they produce a raw sol2 panic. The panic is a `luaL_error`, which is a `longjmp` because Lua is compiled as C, raised from *inside* a user lambda whose locals (`std::map`, `std::vector`, `std::string`) have non-trivial destructors. It is formally undefined behaviour. Per Microsoft's docs, MSVC unwinds destructors on `longjmp` under `/EHsc` unless the frame is `noexcept`, while GCC and Clang on Linux and macOS do not, which in practice would be a leak ([MS longjmp docs](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/longjmp?view=msvc-170); LOW, unverified: compiler/ABI behaviour not checkable from the vendored source). Debug already has this hazard today. **So land the explicit C1/C2 checks (`require_table`, `lua_string_key`) before or together with the flag**, so that the getter safety never fires on a reachable path. Confidence: HIGH on the mechanism (`ldo.c:66,73`, `error_handler.hpp:105-109`), LOW on per-platform consequences.

#### Cost

Measured locally (MSVC 19.51, `/O2 /Ob2 /DNDEBUG /MD`, the full current `src/lua_runner.cpp`, one TU, uncontended). LOW confidence (unverified): machine-specific and not reproduced; only small ALL_SAFETIES test TUs were re-compiled cleanly.

| Config | Compile time | `.obj` size | Warnings |
|---|---|---|---|
| today (`NUMERICS`, `FUNCTION`, `NO_NIL`) | 42.9 s | 33.2 MB | 0 |
| `+ SOL_ALL_SAFETIES_ON=1 SOL_PRINT_ERRORS=0` | 52.8 s (+23%) | 37.5 MB (+13%) | 0 |

So it compiles cleanly as is, at a noticeable but tolerable build cost. **Runtime cost has not been measured.** From the source, each bound call adds one `multi_check`: a `lua_type` per argument, and for a userdata `self`, `lua_getmetatable` plus up to about four `luaL_getmetatable` registry lookups with `lua_rawequal` (`stack_check_unqualified.hpp:524-540`). That is an estimated tens of nanoseconds per call (unverified estimate, LOW: the per-call work is confirmed in source, the figure is not measured). Against a SQLite statement (microseconds) it is noise. It can show on the binary hot path (`file:read(dims)` / `file:write` per cell in a Lua loop) and on expression-operator construction. The requirement to "measure perf" should be met with this protocol:

1. `cmake --preset release && cmake --build --preset release` (builds into `build/release/`). Do it once on master, then once with the flag. Use separate build dirs so the source tree is not touched.
2. Write four scripts, run through `build/release/bin/quiver_cli.exe` and timed from the host, because the sandbox has no `os.clock` (`os` is unloaded):
   - (a) 1e6 × `db:in_transaction()`, pure dispatch plus the `self` check
   - (b) 1e5 × `db:read_scalar_integers` on a 10-element collection
   - (c) 1e5-cell `file:read` / `file:write` loop on a `.qvr`
   - (d) 1e5 × `quiver.gt(e, 1)` / `e + 1` construction
3. Time with PowerShell `Measure-Command` (or `hyperfine` if present), median of 5. Record the numbers in the phase SUMMARY and in the CMake comment.

Proposed acceptance: ≤5% on (b) and (c). Report (a) and (d) for information only. If (c) regresses badly, the fallback is in "Stack Patterns by Variant". Confidence: LOW (not measured).

### Lua 5.4.8 build options (lua-cmake `v5.4.8.0`)

| Option | Current | Recommendation | Why |
|---|---|---|---|
| `LUA_USE_APICHECK` | OFF (`CMakeCache.txt:503`; `lua/cmake/LuaLanguage.cmake:20`) | **Leave OFF in every build.** | It expands to `#define luai_apicheck(l,e) assert(e)` (`lua/src/luaconf.h.in:724-727`). In Release, `NDEBUG` compiles `assert` out, so it does **nothing** for the build where the bug lives. In Debug it **aborts the host process** instead of raising a catchable error. It is a debugging aid for the C API, not a guard against untrusted scripts. The guard is `require_table` plus sol2's argument check, both before the Lua API call. |
| `LUA_LANGUAGE` | C | Leave it as C. | Switching to CXX would make `lua_error` throw, which cures the `longjmp`-over-destructors hazard. But it also requires `SOL_USING_CXX_LUA=1` (`sol/compatibility/lua_version.hpp:171`; otherwise sol2's `extern "C"` include mismatches the link) and changes `SOL_EXCEPTIONS_CATCH_ALL` defaults and yield mechanics. That is a cross-platform behaviour change, which is out of scope for a "restructure and fix" milestone. Explicit checks before any `as<T>()` remove the hazard more cheaply. |
| `LUA_USE_LONGJMP`, `LUA_USE_C89`, `LUA_USE_LTESTS` | OFF | Leave them. | `LUA_USE_LONGJMP` does not apply to C Lua (it only matters when Lua is compiled as C++). `LUA_USE_C89` applies only to C builds and restricts Lua to C89, which is not wanted (`lua/cmake/LuaLanguage.cmake:14-18`, `luaconf.h.in:42`). `LUA_USE_LTESTS` is a test-harness switch. |
| `LUA_TESTS=None`, `LUA_LINE_EDITOR=None` | FORCEd (`cmake/Dependencies.cmake:40-43`) | Leave them. | Already correct. Do not re-add `LUA_BUILD_INTERPRETER` / `LUA_BUILD_COMPILER`. They do not exist upstream (the comment at `Dependencies.cmake:28-39` explains), and the stale cache entries at `CMakeCache.txt:481,483` are left over from that. |

### Compiler and build settings for multiple sol2 TUs

| Setting | Recommendation | Rationale | Confidence |
|---|---|---|---|
| `/bigobj` (MSVC, also clang-cl through `if(MSVC)`) and `-Wa,-mbig-obj` (MinGW, `elseif(WIN32)`) | **Target-wide `target_compile_options(quiver PRIVATE …)` under the existing `if(MSVC)/elseif(WIN32)` predicate** (block above). Delete the per-file `set_source_files_properties(lua_runner.cpp …)` at `src/CMakeLists.txt:70-74`. | The per-file form names a path the split deletes, and each new sol2 TU would need to be listed by hand, so it rots. Target-wide is harmless on non-sol2 TUs: only the COFF header format changes. `quiver` has no C sources, so the flag never reaches a C compile. Use plain `if` and not a generator expression: it is the same predicate, it is shorter, and it needs no `$<CXX_COMPILER_ID>` and frontend-variant reasoning for clang-cl. The `quiver_c` target needs nothing, since `src/c/lua_runner.cpp` does not include sol2. | HIGH |
| Explicit `QUIVER_SOURCES` | List every `src/sandbox/*.cpp` explicitly (`src/CMakeLists.txt:2-41`). **Do not `GLOB`.** | The repo does not glob its build sources, and a glob would hide a forgotten file. | HIGH |
| Per-TU compile cost | Measured: a TU containing only `#include <sol/sol.hpp>` costs **2.1 s (Release) / 2.5 s (Debug)** on MSVC 19.51. With ~11 sandbox TUs that is about +25 s total CPU over today's single 43 s (Release) / ~70 s (Debug, contended, from `.ninja_log`) TU. Ninja wall-clock and incremental rebuilds should improve: the `new_usertype<Database>` instantiation stays in one TU, and editing a reader no longer recompiles the `BinaryFile`/`Expression` usertypes. | Record before and after with `awk -F'\t' '$4 ~ /quiver.dir/ {print $2-$1"\t"$4}' build/release/.ninja_log \| sort -rn \| head` and a clean `cmake --build --preset release --target quiver` timing. | LOW (local measurement, not reproduced; the ~70 s Debug figure does match `build/.ninja_log`), MEDIUM (projection) |
| Precompiled headers | **Not worth it, and out of scope.** | It would save about 2 s per TU, roughly 20 s of CPU in total. CI compiles through **sccache** (`.github/actions/build-cpp/action.yml:54-60`), and sccache reportedly does not cache MSVC `/Yc`/`/Yu` compiles (unverified, secondary web source), so PCH would turn Windows cache hits into misses. `target_precompile_headers` would also put sol2 macros into a PCH that has to match the PRIVATE defines exactly. | LOW |
| `csv` target's PUBLIC defines | Already reach every `quiver` TU (`-DCSV_ENABLE_THREADS=0 -DCSV_NO_SIMD=1` in compile_commands). | Harmless. The rule that `src/sandbox/` never `#include`s csv-parser headers is enforced only by review, because the include path is reachable. One `grep -rn "csv.hpp\|internal/" src/sandbox` in the phase's verify step is enough. | HIGH |

### Development Tools

| Tool | Purpose | Notes for new `src/sandbox/` files |
|------|---------|-------|
| clang-format **22.1.8** (CI: `uvx --from clang-format==22.1.8`, `ci.yml:204`) | Formatting | **The local toolchain is 22.1.3** (VS-bundled LLVM, the first `clang-format` on PATH, which is what the CMake `format` target's `find_program(clang-format-22 clang-format)` resolves to). Format new files with `uvx --from clang-format==22.1.8 clang-format -i src/sandbox/*.cpp src/sandbox/*.h` so they match CI byte for byte. Wrapping matters beyond style here: sync-test Pass 2 depends on each `new_usertype` name sitting alone on a line, which holds only while the call exceeds `ColumnLimit: 120` with `BinPackArguments: false`. The `format` target's `file(GLOB_RECURSE …)` (`CMakeLists.txt:92-98`) is evaluated at **configure** time, so re-run `cmake` after adding files or `format` will skip them. Style keys new files must satisfy: `WrapNamespaceBodyWithEmptyLines: Always`, `IncludeBlocks: Regroup` (main header → `"..."` → `<lib/...>` → `<std>`), `InsertBraces`, LF line endings. Spell project includes `"sandbox/x.h"`, root-relative like `"csv/csv_read.h"`, because `src/` is the PRIVATE include dir. |
| clang-tidy (local 22.1.3, through `scripts/tidy.bat`) | Lint | The file regex `[\\/]src[\\/](?!binary)` already covers `src/sandbox/`, and `HeaderFilterRegex` covers its internal header. Consequences: (1) **A `NOLINTBEGIN` must close in the same file.** clang-tidy reports an unmatched BEGIN/END as an error, so each binder TU gets its own `NOLINTBEGIN/END(performance-unnecessary-value-parameter)` around its sol2 lambdas (today at lua_runner.cpp:591/685, 953/1016, 1026/1095, 1248/1276). (2) Helpers that move into the shared header and are not sol2 entry points should take `const sol::object&`. A by-value `sol::object` in a header is reported once per including TU. (3) The shared header must use a **named** internal namespace with `inline` functions, not an anonymous namespace: an anonymous namespace in a header gives one copy per TU and breaks template ODR. Naming follows `.clang-tidy`: `lower_case` functions and namespaces, `CamelCase` types. Do not add a `src/sandbox/.clang-tidy` that disables the check: it would also silence it in the non-binding helpers, where it is useful. |
| `bindings/js/test/lua-api-sync.test.ts` | Doc↔source sync | It is not part of the C++ toolchain, but it parses C++ source text, so it constrains formatting. Covered in ARCHITECTURE/PITFALLS. |

## Installation

No packages. The whole stack change is the `src/CMakeLists.txt` block above, plus listing the new TUs in `QUIVER_SOURCES`. For the perf protocol:

```bash
cmake --preset release && cmake --build --preset release      # build/release/, tests + C API on
uvx --from clang-format==22.1.8 clang-format --dry-run --Werror src/sandbox/*.cpp src/sandbox/*.h
scripts/tidy.bat                                               # needs a configured build/compile_commands.json
```

## Alternatives Considered

| Recommended | Alternative | When to Use Alternative |
|-------------|-------------|-------------------------|
| `SOL_ALL_SAFETIES_ON=1` + `SOL_PRINT_ERRORS=0` | Only `SOL_SAFE_FUNCTION_CALLS=1 SOL_SAFE_USERTYPE=1 SOL_SAFE_REFERENCES=1` | Use it if the perf protocol shows `SOL_SAFE_GETTER` or `SOL_SAFE_STACK_CHECK` costing measurably on the binary hot path. These three cover argument types, `self` and table construction, which is the whole of C1 plus `self`. Downside: Release then differs from Debug again, in the getter, so a class of bug only Debug can see comes back. Default to ALL. |
| Explicit `require_table` / `lua_string_key` / `optional_from_lua` | Rely on the sol2 macros alone | Never. The macros cannot reject userdata as a table (`loose_table_check`), cannot fix `sol::optional` swallowing a wrong type, and their messages are compiler-dependent and not Pattern 1. |
| Lua compiled as C | `LUA_LANGUAGE=CXX` + `SOL_USING_CXX_LUA=1` | Only in a future milestone that needs exception-correct unwinding through Lua frames, for example if a test shows a leak from a sol2 panic inside a lambda. It changes sol2's exception and yield configuration on every platform. |
| `if(MSVC)/elseif(WIN32)` + `target_compile_options` | Generator expression `$<$<CXX_COMPILER_ID:MSVC>:/bigobj>` | It is equivalent, but harder to read, and `CXX_COMPILER_ID` is `Clang` for clang-cl, which changes today's predicate. |

## What NOT to Use

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| `LUA_USE_APICHECK=ON` | It is an `assert`, so it does nothing under `NDEBUG` and aborts the host in Debug (`luaconf.h.in:724-727`). | `require_table` before any iteration. |
| `SOL_ALL_SAFETIES_ON` without `SOL_PRINT_ERRORS=0` | It writes `[sol2] An exception occurred: …` to the host's stderr for every script error (`trampoline.hpp:47-55`), and the library is embedded in Julia, Python, Dart and JS processes. | Define both. |
| `obj.is<sol::table>()` as the table test | `true` for userdata (`stack_check_unqualified.hpp:41-52`). It then leads to UB in `lua_next` (`lapi.c:725-729`). | `obj.get_type() == sol::type::table`. |
| `sol::nil`, `sol::type::nil` | They do not exist with `SOL_NO_NIL=1`. On Apple sol2 also defaults them off when `__MAC_OS_X_VERSION_MAX_ALLOWED`, `__OBJC__` or a `nil` macro is visible (`version.hpp:522-532`), which depends on include order (unverified that it is "never"; moot here, since `SOL_NO_NIL=1` is explicit). | `sol::lua_nil`, `sol::type::lua_nil`. |
| `sol::function` (unqualified alias) | Its meaning flips with `SOL_SAFE_FUNCTION_OBJECTS` (`forward.hpp:120-130`), which `ALL_SAFETIES` turns on. | Keep the explicit `sol::protected_function` / `sol::object` with a `type::function` check (C6). |
| Asserting full sol2 error text in tests | It includes demangled signatures that differ per compiler (`error_handler.hpp:143-157`). | Assert the Pattern 1 text from the explicit checks, or the `Failed to run Lua script:` prefix plus a stable substring. |
| Per-source `COMPILE_DEFINITIONS` / `COMPILE_OPTIONS` for sol2 macros | Macros change inline template bodies, so mixing values across TUs is an ODR violation. | One `target_compile_definitions(quiver PRIVATE …)`. |
| PCH, unity builds | Out of scope. sccache reportedly does not cache MSVC PCH (unverified). A unity build would undo the split's incremental-rebuild gain and merge the TUs' anonymous namespaces. | Nothing. Just measure. |
| Bumping sol2 (3.5.0 → newer) or Lua during this milestone | Macro names and semantics are version-specific (the dead `SOL_SAFE_FUNCTION` shows it), and the split must stay behaviour-neutral. | A separate milestone, if ever. |

## Stack Patterns by Variant

**If the perf protocol shows ≤5% on (b) and (c):** ship `SOL_ALL_SAFETIES_ON=1 SOL_PRINT_ERRORS=0` and record the numbers in the CMake comment.

**If (c), the binary per-cell loop, regresses by more than 5%:** keep `ALL_SAFETIES_ON`, but add explicit `SOL_SAFE_GETTER=0 SOL_SAFE_STACK_CHECK=0`. Explicit defines win over `ALL_SAFETIES` (`version.hpp:324-338`, `:456-470`). The C1/C2 explicit checks already cover what the getter safety would have caught. Re-measure. Do **not** turn off `SOL_SAFE_FUNCTION_CALLS` or `SOL_SAFE_USERTYPE`: those two are the `self` and table backstop the requirement asks for.

**If a phase ends before the C1/C2 explicit checks land:** do not flip the flag in that phase (see the `SOL_SAFE_GETTER` sequencing note). The flag belongs in the fixes phase, last, after `require_table` and `lua_string_key` cover every site.

## Version Compatibility

| Package | Compatible With | Notes |
|-----------|-----------------|-------|
| sol2 v3.5.0 | Lua 5.4.8 (C) | `SOL_LUA_VERSION_I_` defaults to 504 (`compatibility/lua_version.hpp`). The sol2 CMake cache value `SOL2_LUA_VERSION=5.4.4` is unused, because the `sol2` target is `INTERFACE` include-only (`sol2-src/CMakeLists.txt:97-102`) and Lua comes from lua-cmake's `lua_library`. |
| sol2 v3.5.0 | MSVC 19.51 `/std:c++20 /permissive- /EHsc` | Locally observed (not reproduced, LOW): the current file compiles with 0 warnings both with and without `SOL_ALL_SAFETIES_ON` (sol2 is `/external:I … /external:W0`). Small ALL_SAFETIES test TUs were re-compiled cleanly. |
| sol2 v3.5.0 | Apple clang | `SOL_NO_NIL=1` is required (the ObjC `nil` clash). macOS CI is the only place a `sol::nil` slip would surface without it. |
| clang-format 22.1.8 | `.clang-format` 22-only keys | 21.x rejects the config. The local 22.1.3 accepts it but may differ in patch-level output, so use `uvx` with the pin. |
| `PROPAGATE_EXCEPTIONS` | off for PUC Lua (`lua_version.hpp:153-157`) | That is why `multi_check` actually runs when `SOL_SAFE_FUNCTION_CALLS` is on (`stack.hpp:194-197`). Do not define `SOL_EXCEPTIONS_SAFE_PROPAGATION`: it would skip the argument checks *and* let C++ exceptions cross C Lua frames. |

## Sources

- Vendored sol2 v3.5.0, `build/_deps/sol2-src/include/sol/`: `version.hpp:31-35, 240-257, 314-532, 623-637`; `forward_detail.hpp:34-39`; `stack.hpp:146-205`; `call.hpp:470-600, 805-835`; `table_core.hpp:325-375`; `lua_table.hpp:45-90`; `stack_core.hpp:1130-1175`; `stack_check_unqualified.hpp:41-52, 125-190, 472-473, 492-540`; `error_handler.hpp:86-165`; `trampoline.hpp:25-175`; `state_handling.hpp:28-160`; `protected_function_result.hpp:120-145`; `forward.hpp:120-140`; `state_view.hpp:548-562`; `types.hpp:97-100, 715-722`; `compatibility/lua_version.hpp:120-175`; `documentation/source/safety.rst:51`. HIGH.
- Vendored Lua 5.4.8 / lua-cmake v5.4.8.0, `build/_deps/lua-src/`: `src/lapi.c:725-729, 1253-1267`; `src/luaconf.h.in:55-62, 715-727`; `src/llimits.h:106-126`; `cmake/LuaLanguage.cmake:1-24`; `src/CMakeLists.txt:60-160`. HIGH.
- Repo: `src/CMakeLists.txt:1-87`, `cmake/Dependencies.cmake:27-55`, `cmake/CompilerOptions.cmake`, `build/CMakeCache.txt:30-67, 481-512, 574`, `build/compile_commands.json` (the lua_runner.cpp command line), `build/.ninja_log`, `.clang-format`, `.clang-tidy`, `scripts/tidy.bat`, `CMakeLists.txt:91-106`, `.github/workflows/ci.yml:15-45, 191-204`, `.github/actions/build-cpp/action.yml:45-64`, `src/AGENTS.md:258-280, 700-745`. HIGH.
- Local measurements (2026-10-02, MSVC 19.51.36256 x64): compiles of the real `src/lua_runner.cpp` with and without `SOL_ALL_SAFETIES_ON`, and of a header-only sol2 TU. LOW (machine-specific, not reproduced); treat as relative numbers.
- [Microsoft Learn: longjmp](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/longjmp?view=msvc-170) and [/EH exception handling model](https://learn.microsoft.com/en-us/cpp/build/reference/eh-exception-handling-model?view=msvc-170): MSVC unwinds destructors on `longjmp` under `/EHsc`, except in `noexcept` frames; this is non-portable. LOW (web, unverified against the repo).
- [sccache MSVC support (deepwiki summary)](https://deepwiki.com/mozilla/sccache/4.3-msvc-support): sccache does not cache MSVC PCH compiles. LOW (web, secondary, unverified).

---
*Stack research for: sol2/Lua scripting-layer split and Release type-safety hardening (Quiver Sandbox refactor)*
*Researched: 2026-10-02*
