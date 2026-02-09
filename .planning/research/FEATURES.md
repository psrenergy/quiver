# Feature Research

**Domain:** C++20 library with C API and multi-language FFI bindings (Julia, Dart, Lua)
**Researched:** 2026-02-09
**Confidence:** HIGH (codebase analysis + established FFI design principles from Botan, CppCon hourglass pattern, multi-language binding practitioners)

## Feature Landscape

### Table Stakes (Users Expect These)

Features that any well-refactored C++ library with multi-language bindings must have. Missing these means the library feels broken or untrustworthy.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| **Complete API parity across all layers** | If C++ can do it, C/Julia/Dart/Lua must too. Partial coverage forces users to drop to a lower layer, defeating the purpose of bindings. | MEDIUM | Current gap: `is_healthy`, `path` have C API but no Dart/Julia high-level method. Lua missing CSV operations. Several methods only partially bound. |
| **Uniform naming convention per layer** | Each layer follows its language's idiom consistently. C++ uses `snake_case`, C uses `quiver_` prefix + `snake_case`, Dart uses `camelCase`, Julia uses `snake_case`. Inconsistencies signal amateur design. | LOW | Currently good within individual layers. Cross-layer mapping should be documented in one canonical table. |
| **Uniform error handling per layer** | C++ throws exceptions, C returns `quiver_error_t` + `quiver_get_last_error()`, bindings translate to native exceptions. Every function in every layer must follow its layer's convention without exception. | LOW | Currently solid. Dart `check()` and Julia `check()` both correctly translate C error codes to native exceptions. Maintain this. |
| **Explicit resource lifecycle (create/destroy pairs)** | Every allocated resource must have a matching free function. No ambiguity about ownership. | LOW | Already implemented: `quiver_database_close`, `quiver_free_*` functions. Ensure new features follow same pattern. |
| **Source file decomposition by responsibility** | No single file should exceed ~500 lines. Monolithic files make review, testing, and navigation painful. | MEDIUM | `database.cpp` is 1934 lines, `src/c/database.cpp` is 1612 lines. These must be split by operation category (CRUD, metadata, time series, query, CSV). |
| **Test coverage for every public API function** | Every C++ method, C API function, and binding method must have at least one test. Tests are the contract. | MEDIUM | Test structure exists and is well-organized by category. Must maintain this discipline as files are split. |
| **Matching free functions for every allocation** | Every `quiver_read_*` that returns allocated memory must have a documented `quiver_free_*` counterpart. | LOW | Already implemented. Keep the pattern. |
| **QUIVER_REQUIRE macro for null-pointer guards** | Every C API function taking a pointer param must validate it before dereferencing. | LOW | Pattern exists in codebase. Audit for completeness during refactor. |
| **Thread-local error storage** | `quiver_get_last_error()` must be thread-local so concurrent callers do not clobber each other's error state. | LOW | Already implemented with thread-local storage. |
| **Binary error codes (OK/ERROR)** | C API returns only 0 or 1. Detailed info via `quiver_get_last_error()`. Simple, universal, FFI-friendly. | LOW | Already implemented. Do not add richer error codes -- they leak C++ semantics into the C layer. |
| **Opaque handle pattern for all C API types** | `quiver_database_t`, `quiver_element_t`, `quiver_lua_runner_t` are forward-declared opaque structs. Internal layout never exposed. | LOW | Already implemented correctly. |
| **Shared test schemas** | All test layers (C++, C API, Julia, Dart, Lua) use the same SQL schema files from `tests/schemas/`. | LOW | Already implemented. Prevents schema drift between test suites. |

### Differentiators (Competitive Advantage)

Features that set a well-designed multi-language library apart. Not expected, but make the library feel polished and professional.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| **Canonical API mapping table** | A single document (in CLAUDE.md or a dedicated file) mapping every C++ method to its C API, Julia, Dart, and Lua equivalents. Makes parity gaps instantly visible. Automated gap detection possible. | LOW | No competing library does this well. Most rely on ad-hoc documentation. This is Quiver's "intelligence over documentation" advantage. |
| **Mechanical binding generation** | Julia and Dart bindings auto-generated from C header via `generator.bat`. Eliminates hand-written FFI boilerplate drift. | LOW | Already implemented. Most libraries require manual FFI. This is a significant advantage -- protect it. |
| **Consistent file decomposition mirroring across layers** | C++ has `database_read.cpp`, C API has `c/database_read.cpp`, Julia has `database_read.jl`, Dart has `database_read.dart`. Same logical split at every layer. | MEDIUM | Partially there (bindings already split by operation). C++ and C API are monolithic. Aligning them to match bindings creates navigational symmetry. |
| **Convenience methods in bindings only** | Bindings provide `readAllScalarsById`, `readAllVectorsById`, `readVectorGroupById` -- these are pure binding-layer conveniences that compose lower-level operations. Logic stays in C++, convenience in bindings. | LOW | Julia and Dart already do this. Good pattern. Document it as intentional policy. |
| **Version query at C API level** | `quiver_version()` already exists. Bindings should expose it for runtime compatibility checks. | LOW | C API has it. Julia/Dart FFI wrappers have the binding. Verify high-level wrapper exposes it. |
| **DateTime as a binding-layer concern** | C++ and C API handle dates as strings. Julia/Dart bindings convert to native DateTime types. Type enrichment happens at the binding layer, keeping C++ simple. | LOW | Already implemented in both Julia (`string_to_date_time`) and Dart (`stringToDateTime`). Document as explicit pattern. |
| **Parameterized query support with typed params** | C API uses parallel arrays (`param_types[]`, `param_values[]`) for typed SQL parameters. Bindings marshal native types to these arrays. | LOW | Implemented in C API and all bindings. Complex FFI pattern done well. |
| **Unified GroupMetadata across vector/set/time_series** | Single `GroupMetadata`/`quiver_group_metadata_t` type with `dimension_column` field distinguishing time series from vectors/sets. Reduces type proliferation. | LOW | Already implemented. Elegant design choice. |
| **Schema validation at database creation** | `schema_validator.cpp` validates schema structure before database creation. Catches errors early with clear messages. | LOW | Already exists. Surface validation errors through all layers consistently. |
| **RAII transaction guards** | `TransactionGuard` and `with_transaction()` lambda pattern for safe transaction management. | LOW | Already in C++ layer. Not exposed through C API/bindings (correct -- this is an implementation detail). |
| **Blob module completion** | `blob.cpp` is currently a stub (empty function bodies). Completing this adds binary data support. | HIGH | Current stub has `Blob::Impl` with `std::iostream`, `file_path`, `BlobMetadata`. Needs full implementation + C API + bindings. |
| **API surface documentation via `describe()`** | `describe()` prints schema info to stdout. Available at all layers. Useful for debugging and discovery. | LOW | Already bound to C API, Julia, Dart, Lua. |

### Anti-Features (Commonly Requested, Often Problematic)

Features that seem good but would harm the library during this refactoring phase.

| Feature | Why Requested | Why Problematic | Alternative |
|---------|---------------|-----------------|-------------|
| **Rich C API error codes** | "Give me QUIVER_ERROR_NOT_FOUND, QUIVER_ERROR_CONSTRAINT, etc." | Leaks C++ exception taxonomy into C ABI. Every new error type is an ABI break. Bindings must update switch statements. | Keep binary OK/ERROR + `quiver_get_last_error()` for details. Error messages are strings, not codes. |
| **C++ templates in public API** | "Make `read_scalar<T>()` generic" | Cannot cross FFI boundary. C API must enumerate concrete types. Templates in the public header force template instantiation at the binding layer, violating "logic in C++" principle. | Keep type-specific methods: `read_scalar_integers`, `read_scalar_floats`, `read_scalar_strings`. |
| **Async/callback-based C API** | "Add async operations for non-blocking I/O" | SQLite is inherently single-connection-single-thread. Adding async API adds complexity with no real concurrency gain. Binding-layer async (Dart futures, Julia tasks) should wrap synchronous C calls. | Bindings can wrap sync calls in async if their language needs it. C API stays synchronous. |
| **SWIG-generated bindings** | "Auto-generate all bindings from C++ headers" | SWIG produces suboptimal FFI code, fights with custom memory management, and generates bindings that do not feel native. Current `ffigen`-based generators for Julia/Dart are better. | Keep current generator approach. Thin hand-written high-level wrappers over generated FFI layer. |
| **Backwards compatibility constraints** | "Do not break existing API during refactor" | CLAUDE.md explicitly states: "WIP project - breaking changes acceptable, no backwards compatibility required." Backwards compat would prevent fixing naming inconsistencies and API gaps. | Break freely. Fix everything. Document changes. |
| **Abstract base classes / interfaces in C++** | "Add Database interface for mocking" | Single concrete implementation. Interface adds vtable indirection, complicates Pimpl, serves no current purpose. | If testing needs mocking, test through the C API or use schema-based test databases (already the pattern). |
| **Error message localization** | "Translate error messages to user's language" | Error messages are developer-facing diagnostic strings. Localization adds complexity and translation maintenance burden. | Keep English error messages. They are debugging output, not UI strings. |
| **Exposing C++ implementation internals through C API** | "Let bindings access TransactionGuard, Impl, internal helpers" | Breaks encapsulation. C API is a public boundary. Internals should never leak. | If a binding needs transaction control, add explicit `quiver_database_begin_transaction()` / `commit()` / `rollback()` C API functions. |
| **Adding new features during refactoring** | "While we're in there, let's add blob support and new query types" | Mixing refactoring with feature development makes regressions harder to detect and bisect. | Complete the refactoring first (file splits, naming, parity). Then add blob module as a separate phase. |

## Feature Dependencies

```
[Source file decomposition]
    |
    +--requires--> [Consistent file naming across layers]
    |                  |
    |                  +--enables--> [Canonical API mapping table]
    |
    +--enables--> [Complete API parity]
                      |
                      +--requires--> [Every C++ method has C API function]
                      |                  |
                      |                  +--requires--> [Every C API function has binding methods]
                      |                                    |
                      |                                    +--requires--> [Generator re-run for FFI layer]
                      |                                    |
                      |                                    +--requires--> [High-level wrapper methods in Julia/Dart]
                      |
                      +--requires--> [Test for every public function]

[Blob module completion]
    |
    +--requires--> [Source file decomposition] (so blob fits cleanly into split structure)
    +--requires--> [Complete API parity pattern] (C++ -> C -> Julia/Dart/Lua)
    +--requires--> [Test coverage]

[Canonical API mapping table]
    |
    +--enables--> [Automated parity gap detection]
    +--enables--> [Generator validation]
```

### Dependency Notes

- **Source file decomposition must come first:** Splitting monolithic files is the foundation. Without it, adding API parity creates even larger monolithic files. Split first, then fill gaps.
- **API parity requires generator re-runs:** After C API changes, `bindings/julia/generator/generator.bat` and `bindings/dart/generator/generator.bat` must run to regenerate FFI layers. High-level wrappers are then added manually.
- **Blob module depends on everything else:** It is a new feature that should be added only after the refactoring establishes clean patterns. Adding it during refactoring mixes concerns.
- **Canonical API mapping table has no hard dependencies:** Can be created at any time, but is most useful after source decomposition reveals the full picture.
- **Test coverage is continuous:** Every file split, every new binding method, every parity gap filled must include tests. Not a separate phase -- it is part of every change.

## MVP Definition

### Launch With (v1 -- Refactoring Complete)

Minimum viable refactoring -- what is needed to call this codebase "well-structured."

- [x] **Source file decomposition** -- Split `database.cpp` and `src/c/database.cpp` into files mirroring the existing binding structure (create, read, update, delete, query, metadata, time_series, csv, relations)
- [x] **Complete API parity audit** -- Document every C++ method and its C/Julia/Dart/Lua equivalents. Identify gaps.
- [x] **Fill critical parity gaps** -- `is_healthy`, `path`, `current_version` exposed in all binding layers. CSV operations in Lua.
- [x] **Tests for every split file** -- Existing tests reorganized to match new file structure. No test regressions.

### Add After Validation (v1.x)

Features to add once core refactoring is verified working.

- [ ] **Canonical API mapping table in CLAUDE.md** -- Add when parity audit is complete
- [ ] **Automated parity gap detection** -- Script that parses C++ header and checks C API / binding coverage
- [ ] **Blob module completion** -- Implement `Blob` class, C API, all bindings, tests

### Future Consideration (v2+)

Features to defer until the refactored structure proves stable.

- [ ] **Transaction control through C API** -- `quiver_database_begin_transaction`, `commit`, `rollback` if scripting use cases need it
- [ ] **Batch operations** -- Bulk create/update for performance-critical binding use cases
- [ ] **Connection pooling** -- If multi-threaded usage patterns emerge

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---------|------------|---------------------|----------|
| Source file decomposition | HIGH | MEDIUM | P1 |
| Complete API parity | HIGH | MEDIUM | P1 |
| Fill parity gaps (is_healthy, path, etc.) | MEDIUM | LOW | P1 |
| Tests for every split file | HIGH | LOW | P1 |
| Canonical API mapping table | MEDIUM | LOW | P2 |
| Consistent file naming across layers | MEDIUM | LOW | P2 |
| Automated parity gap detection | LOW | MEDIUM | P3 |
| Blob module completion | MEDIUM | HIGH | P3 |
| Transaction control through C API | LOW | MEDIUM | P3 |

**Priority key:**
- P1: Must have for refactoring milestone
- P2: Should have, add when structure is stable
- P3: Nice to have, future milestone

## Competitor Feature Analysis

| Feature | SQLiteCpp | sqlite_modern_cpp | Quiver (Current) | Quiver (Target) |
|---------|-----------|-------------------|-------------------|-----------------|
| C++ API | Yes | Yes | Yes | Yes |
| C API for FFI | No | No | Yes | Yes (complete) |
| Multi-language bindings | No | No | Julia, Dart, Lua | Julia, Dart, Lua (parity) |
| Schema validation | No | No | Yes | Yes |
| Binding auto-generation | N/A | N/A | Partial (FFI layer) | Full (FFI + validation) |
| File decomposition | Good (by class) | Single header | Monolithic | Split by operation |
| Thread safety docs | Yes | Yes | Implicit | Should document |
| Error messages | C++ exceptions | C++ exceptions | Exceptions + C error codes | Same (consistent) |
| Migration support | No | No | Yes | Yes |
| Lua scripting | No | No | Yes | Yes (complete parity) |

Quiver's differentiator is the multi-language binding architecture. No comparable SQLite wrapper offers C++ + C API + Julia + Dart + Lua from a single codebase. The refactoring should strengthen this unique position by making the binding process more systematic and verifiable.

## Sources

- [Botan FFI C Binding Documentation](https://botan.randombit.net/handbook/api_ref/ffi.html) -- HIGH confidence, authoritative example of production FFI design
- [CppCon 2014: Hourglass Interfaces for C++ APIs (Stefanus DuToit)](https://isocpp.org/blog/2015/07/cppcon-2014-hourglass-interfaces-for-cpp-apis-stefanus-dutoit) -- HIGH confidence, foundational pattern for C++ FFI
- [Binding a C++ Library to 10 Programming Languages (Ash Vardanian)](https://ashvardanian.com/posts/porting-cpp-library-to-ten-languages/) -- MEDIUM confidence, practitioner experience report
- [Hourglass Interfaces (Brent Huisman)](https://brent.huisman.pl/hourglass-interfaces/) -- MEDIUM confidence, practical summary of hourglass pattern
- [C Wrappers for C++ Libraries and Interoperability](https://caiorss.github.io/C-Cpp-Notes/CwrapperToQtLibrary.html) -- MEDIUM confidence, tutorial with patterns
- [Layered Library Design (Big Book of Rust Interop)](https://nrc.github.io/big-book-ffi/patterns/layered.html) -- MEDIUM confidence, Rust-focused but pattern applies universally
- [Principles of FFI API Design (Edward Z. Yang)](http://blog.ezyang.com/2010/06/principles-of-ffi-api-design/) -- MEDIUM confidence, principled FFI design guidance
- Quiver codebase analysis (direct inspection of all layers) -- HIGH confidence

---
*Feature research for: C++ library with multi-language FFI bindings*
*Researched: 2026-02-09*
