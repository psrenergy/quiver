# Stack Research

**Domain:** C++20 library refactoring with C FFI and multi-language bindings (Julia, Dart, Lua)
**Researched:** 2026-02-09
**Confidence:** HIGH

## Recommended Stack

### Core Technologies (Already In Place -- Keep)

| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| C++20 | std=c++20 | Core library standard | Already in use. Provides concepts, ranges, designated initializers, `std::format` candidates. No reason to move to C++23 -- the codebase is small and C++20 features are sufficient. **HIGH confidence.** |
| CMake | 3.21+ | Build system | Already in use with FetchContent. Well-supported across all platforms. `CMAKE_EXPORT_COMPILE_COMMANDS ON` already configured, which enables clang-tidy integration. **HIGH confidence.** |
| SQLite | 3.50.2 | Embedded database | Core dependency. Fetched via cmake FetchContent. No action needed. **HIGH confidence.** |
| spdlog | 1.17.0 | Logging | Already in use. Lightweight, header-only option available. No change needed. **HIGH confidence.** |
| sol2 | 3.5.0 | Lua C++ bindings | Already in use for Lua scripting layer. Mature library, actively maintained. **HIGH confidence.** |
| GoogleTest | 1.17.0 | C++ testing | Already in use. Standard choice for C++ testing. **HIGH confidence.** |

### Refactoring Tools (Add These)

| Tool | Purpose | Why Recommended |
|------|---------|-----------------|
| clang-tidy | Static analysis + automated refactoring | The single most important tool for this refactoring. Catches naming inconsistencies, modernization opportunities, performance issues, and bugprone patterns. Already have `compile_commands.json` generation via CMake. Add a `.clang-tidy` config file to codify project standards. **HIGH confidence -- industry standard.** |
| clang-format | Code formatting | Already in use (`.clang-format` exists, `format` CMake target defined). Continue using. No changes needed. **HIGH confidence.** |
| include-what-you-use (IWYU) | Header dependency cleanup | Run once during refactoring to identify unnecessary includes and missing forward declarations. Especially useful when splitting `database.cpp` (1934 lines) into smaller files. Do NOT integrate into CI -- use as a one-shot cleanup tool. **MEDIUM confidence -- useful but can produce false positives on Windows/MSVC.** |
| compile_commands.json | Tooling integration | Already generated via `CMAKE_EXPORT_COMPILE_COMMANDS ON`. Required by clang-tidy and IWYU. No changes needed. **HIGH confidence.** |

### Binding Generation Tools (Already In Place -- Refine)

| Tool | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| Dart ffigen | Latest | Generates `bindings.dart` from C headers | Run after every C API header change. Currently configured via `ffigen.yaml`. Keep YAML config -- Dart team has not yet deprecated it and YAML is simpler for this use case. **HIGH confidence.** |
| Julia Clang.jl Generators | Latest | Generates `c_api.jl` from C headers | Run after every C API header change. Currently uses `generator.toml` config with prologue/epilogue files. This is the correct approach. **HIGH confidence.** |
| `scripts/generator.bat` | - | Orchestrates both generators | Already exists and works. Run after any C header change. **HIGH confidence.** |

### Supporting Libraries (No Changes)

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| toml++ | 3.4.0 | TOML parsing | Schema/config parsing. Already in use. |
| Lua | 5.4.8 | Lua runtime | Scripting support via sol2. Already in use. |

## .clang-tidy Configuration

Use this configuration file for the refactoring. It enables checks relevant to C++20 code quality without generating noise from third-party headers.

**Recommended `.clang-tidy` contents:**

```yaml
Checks: >
  -*,
  bugprone-*,
  -bugprone-easily-swappable-parameters,
  modernize-*,
  -modernize-use-trailing-return-type,
  performance-*,
  readability-identifier-naming,
  readability-redundant-*,
  readability-simplify-*,
  readability-container-size-empty,
  readability-else-after-return,
  misc-unused-using-decls,
  misc-redundant-expression,
  cppcoreguidelines-init-variables,
  cppcoreguidelines-slicing

CheckOptions:
  # Enforce project naming conventions
  - key: readability-identifier-naming.NamespaceCase
    value: lower_case
  - key: readability-identifier-naming.ClassCase
    value: CamelCase
  - key: readability-identifier-naming.StructCase
    value: CamelCase
  - key: readability-identifier-naming.FunctionCase
    value: lower_case
  - key: readability-identifier-naming.VariableCase
    value: lower_case
  - key: readability-identifier-naming.PrivateMemberSuffix
    value: '_'
  - key: readability-identifier-naming.ConstantCase
    value: lower_case
  - key: readability-identifier-naming.EnumConstantCase
    value: CamelCase
  - key: readability-identifier-naming.GlobalConstantCase
    value: lower_case
  - key: performance-unnecessary-value-param.AllowedTypes
    value: 'DatabaseOptions'

HeaderFilterRegex: 'include/quiver/.*\.h$|src/.*\.(h|cpp)$'
WarningsAsErrors: ''
FormatStyle: file
```

**Why these specific checks:**
- `bugprone-*` catches common mistakes (use-after-move, dangling references, infinite loops). Disable `bugprone-easily-swappable-parameters` because the API inherently has many `(collection, attribute)` string pairs -- this check would flag every function.
- `modernize-*` ensures C++20 idioms are used. Disable `modernize-use-trailing-return-type` because it conflicts with the existing code style and provides no safety benefit.
- `performance-*` catches unnecessary copies, moves, and allocations. Critical for a database wrapper.
- `readability-identifier-naming` enforces naming consistency -- the primary motivation for this refactoring.
- `HeaderFilterRegex` limits warnings to project headers only (not spdlog, sqlite3, sol2, etc.).

**Confidence: HIGH** -- these are well-established, non-controversial checks. The naming configuration matches the existing Quiver codebase conventions.

## Refactoring Strategy: Header-First, Top-Down

Use this specific approach for the refactoring. It is the correct order because bindings depend on C API, C API depends on C++ API, and tests validate everything.

### Layer 1: C++ Public Headers (define the contract)
1. Rename/reorganize methods in `database.h` first
2. Update `element.h`, `attribute_metadata.h`, etc. to match
3. C++ implementation follows headers

### Layer 2: C API Headers (FFI contract)
4. Update `include/quiver/c/database.h` to mirror C++ changes
5. Follow the established pattern: `quiver_database_{verb}_{noun}_{type}` naming
6. Update `src/c/database.cpp` implementation

### Layer 3: Auto-Generate FFI Bindings
7. Run `scripts/generator.bat` to regenerate `c_api.jl` and `bindings.dart`
8. Verify generated output compiles

### Layer 4: Hand-Written Binding Wrappers
9. Update Julia wrapper files (`database_read.jl`, `database_update.jl`, etc.)
10. Update Dart wrapper files (`database_read.dart`, `database_update.dart`, etc.)
11. Update Lua bindings in `lua_runner.cpp`

### Layer 5: Tests
12. Update C++ tests
13. Update C API tests
14. Update Julia tests
15. Update Dart tests

**Why this order:** Each layer depends only on the layer above it. Changing headers first means you know exactly what the final API looks like before touching implementation. Auto-generated bindings catch FFI mismatches immediately. Hand-written wrappers and tests come last because they are the consumers.

**Confidence: HIGH** -- this is the standard approach for multi-layer library refactoring.

## Alternatives Considered

| Recommended | Alternative | Why Not |
|-------------|-------------|---------|
| clang-tidy for static analysis | cppcheck | clang-tidy has deeper AST understanding, auto-fix support via `-fix`, and direct CMake integration. cppcheck finds different bugs but cannot enforce naming conventions or auto-refactor. Use clang-tidy as primary, cppcheck as optional secondary. |
| Manual C API + ffigen/Clang.jl generators | SWIG for all bindings | SWIG generates bindings for many languages from C++ headers directly, but produces non-idiomatic code in target languages. Quiver's current approach (thin C API + language-specific generators + hand-written wrappers) produces much more natural bindings. SWIG would lose the "homogeneity" principle. |
| Keep current Dart ffigen YAML config | Switch to Dart API config | YAML config works, is simpler, and is still supported. The Dart team's eventual migration to API-based config is not yet required. Switch only when forced. |
| Keep Clang.jl Generators for Julia | CBinding.jl / CBindingGen.jl | Clang.jl is the established tool in the Julia ecosystem for this purpose. CBinding.jl is more ambitious but less mature and adds runtime overhead. The current `generator.toml` + prologue/epilogue approach works well. |
| sol2 for Lua bindings | Raw Lua C API | sol2 provides type-safe, ergonomic C++ to Lua bindings with minimal overhead. Raw Lua C API would require manual stack management and is error-prone. sol2 is the right choice. |
| Keep `new`/`delete` in C API | Use arena allocator for C API allocations | The C API allocation pattern (`new[]` + matching `quiver_free_*`) is simple and correct. An arena allocator would add complexity for marginal performance gain in a database wrapper (SQLite is the bottleneck, not allocation). |

## What NOT to Do

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| C++20 modules for the core library | Module support is still inconsistent across compilers (MSVC, GCC, Clang have different levels of support). The codebase targets MSVC on Windows. Modules would break the FFI generator toolchain which parses traditional headers. | Keep traditional headers. Modules are not ready for libraries with C FFI layers. **HIGH confidence.** |
| Refactoring all layers simultaneously | Changing C++ API, C API, and all bindings in one commit creates untestable, unreviewable changesets. A single naming mistake propagates through 4 layers before anyone catches it. | Refactor one layer at a time, run full test suite (`scripts/build-all.bat`) after each layer. |
| Custom clang-tidy checks | Writing custom checks via LibTooling is powerful but overkill for this project. The built-in checks cover naming, performance, and bug detection. Custom checks add build complexity (need to build clang from source or as plugin). | Use built-in clang-tidy checks. If a pattern is not caught by existing checks, add a code review rule instead. |
| Changing the C API error model | The current binary error code + thread-local error string pattern (`QUIVER_OK`/`QUIVER_ERROR` + `quiver_get_last_error()`) is the standard C FFI pattern used by SQLite, libgit2, and most successful C libraries. Switching to structured error types or error code enums would break all bindings and add complexity without benefit. | Keep the current error model. It works correctly and is well-understood by all binding layers. **HIGH confidence.** |
| Auto-generating the hand-written wrapper layers | The Julia wrapper (`database_read.jl`) and Dart wrapper (`database_read.dart`) files contain idiomatic conversions (marshalling, type mapping, exception handling) that cannot be auto-generated from headers alone. Attempting to generate them would produce non-idiomatic code. | Keep hand-written wrappers. Auto-generate only the raw FFI layer (`c_api.jl`, `bindings.dart`). The hand-written wrappers are thin -- updating them after a rename is straightforward. |
| include-what-you-use in CI | IWYU produces false positives, especially on Windows with MSVC standard library headers. Running it in CI would create noisy failures. | Run IWYU manually during the refactoring phase, review suggestions, apply selectively. |
| Changing `QUIVER_REQUIRE` macro pattern | The variadic macro for null-checking C API arguments is working and readable. Replacing it with a function template or `if constexpr` adds no benefit -- macros are appropriate here because they need stringification (`#a`) and early return. | Keep the macro. It is correct, readable, and serves its purpose. |

## Refactoring-Specific Patterns

### Pattern: Rename in C++ Header, Propagate Down

**When:** Renaming a public C++ method (e.g., `read_scalar_integers` to `read_scalars_integer`)

**Approach:**
1. Rename in `include/quiver/database.h`
2. Build C++ library -- compiler errors show every call site
3. Fix `src/database.cpp` implementation
4. Fix C++ tests
5. Rename corresponding C API function in `include/quiver/c/database.h`
6. Fix `src/c/database.cpp`
7. Fix C API tests
8. Run `scripts/generator.bat` -- regenerates FFI bindings
9. Update Julia/Dart hand-written wrappers
10. Run `scripts/build-all.bat` -- confirms everything passes

**Why:** The compiler is your refactoring tool. Each rename produces compiler errors that serve as a checklist. No rename is "done" until `build-all.bat` passes.

### Pattern: Add New API Surface (Parity Completion)

**When:** A C++ method exists but has no C API / binding equivalent

**Approach:**
1. Add C API declaration to `include/quiver/c/database.h`
2. Implement in `src/c/database.cpp` following the established try/catch/QUIVER_REQUIRE pattern
3. Add matching `quiver_free_*` if function allocates
4. Add C API test
5. Run `scripts/generator.bat`
6. Add Julia wrapper function
7. Add Dart wrapper function
8. Add binding tests
9. Expose in Lua via sol2 in `lua_runner.cpp`

### Pattern: Split Large Implementation File

**When:** `database.cpp` at 1934 lines should be split

**Approach:**
1. Identify logical groupings (already reflected in test file organization: lifecycle, create, read, update, delete, query, relations, time_series)
2. Move `Impl` methods to separate `.cpp` files, keeping the `Impl` struct definition in a shared internal header
3. Each new file includes only what it needs (this is where IWYU helps)
4. Build and test after each file extraction

**Key constraint:** The `Impl` struct definition must remain in exactly one place (likely a new `src/database_impl.h` internal header). All split `.cpp` files include it.

## Version Compatibility

| Package | Compatible With | Notes |
|---------|-----------------|-------|
| clang-tidy 18+ | CMake 3.21+, compile_commands.json | Requires `CMAKE_EXPORT_COMPILE_COMMANDS ON` (already set). MSVC builds need `-p` flag pointing to build directory. |
| Dart ffigen latest | Dart SDK 3.x | Uses `dart:ffi`. Requires libclang on the dev machine for header parsing. |
| Julia Clang.jl latest | Julia 1.9+ | Uses Clang.Generators module. Requires the built `libquiver_c.dll` to exist before generation (loads symbols). |
| sol2 3.5.0 | Lua 5.4.x, C++17/20 | sol2 is C++17 minimum. Compatible with C++20 builds. No issues expected. |

## Sources

- [Clang-Tidy documentation (LLVM)](https://clang.llvm.org/extra/clang-tidy/) -- check categories, configuration format
- [Clang-Tidy checks list](https://clang.llvm.org/extra/clang-tidy/checks/list.html) -- full check inventory
- [KDAB: Clang-Tidy Modernize](https://www.kdab.com/clang-tidy-part-1-modernize-source-code-using-c11c14/) -- modernize check usage patterns
- [Microsoft: Exploring Clang Tooling](https://devblogs.microsoft.com/cppblog/exploring-clang-tooling-part-1-extending-clang-tidy/) -- custom check development (reference, not recommended)
- [Dart ffigen package](https://pub.dev/packages/ffigen) -- Dart FFI binding generation docs
- [Dart C interop guide](https://dart.dev/interop/c-interop) -- official Dart FFI documentation
- [Julia Clang.jl](https://github.com/JuliaInterop/Clang.jl) -- Julia binding generator
- [sol2 documentation](https://sol2.readthedocs.io/) -- Lua C++ binding library
- [Botan FFI API](https://botan.randombit.net/handbook/api_ref/ffi.html) -- reference implementation of C FFI design patterns (opaque handles, error codes, free functions)
- [C++ Best Practices: Use the Tools Available](https://lefticus.gitbooks.io/cpp-best-practices/content/02-Use_the_Tools_Available.html) -- tool selection guidance
- [GNU Coding Standards: Libraries](https://www.gnu.org/prep/standards/html_node/Libraries.html) -- C API naming prefix conventions

---
*Stack research for: C++20 library refactoring with C FFI and multi-language bindings*
*Researched: 2026-02-09*
