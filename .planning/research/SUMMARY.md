# Project Research Summary

**Project:** Quiver v0.5 Code Quality Improvements
**Domain:** Multi-language FFI library (C++/C/Julia/Dart/Python/Lua)
**Researched:** 2026-02-27
**Confidence:** HIGH

## Executive Summary

Quiver v0.5 is a targeted code quality milestone for an existing C++/SQLite library with a C API and four language bindings (Julia, Dart, Python, Lua). Unlike greenfield projects, all work is refactoring and gap-closing within a well-understood codebase with no backwards compatibility requirements. Research identified six concrete improvements: fixing a layer inversion in `DatabaseOptions` type ownership, renaming a misscoped free function (`quiver_element_free_string`), adding Python `DataType` named constants, adding a Python `LuaRunner` binding, improving cross-binding test coverage for `is_healthy()`/`path()`, and auditing Python convenience helper tests. All changes are independently verifiable by running `scripts/test-all.bat`.

The recommended approach is to sequence changes by blast radius: C++-only architectural fixes first (zero binding impact), then C API surface changes that require generator re-runs across all bindings, then purely additive Python changes. This ordering minimizes the number of generator re-run passes and avoids compounding risk. The architecture is already well-structured with clear co-location conventions and existing conversion patterns (`CSVOptions` in `src/c/database_options.h`) that serve as templates for each change. The layer inversion fix leaves the C API struct layout (`quiver_database_options_t`) unchanged, so no binding-level changes are needed for that phase.

The key risks are cross-language partial-rename failures and Python CFFI ABI-mode struct layout mismatches. Both are detectable only by running all five test suites after every phase. The mitigation is explicit checklists and running `scripts/test-all.bat` as the final gate for any change that touches a C API header. The Python LuaRunner binding requires careful lifetime management: the Python wrapper must hold a reference to the `Database` object to prevent Python GC from collecting it while the runner is alive, and must use `quiver_lua_runner_get_error` (not the global `quiver_get_last_error`) for error retrieval.

## Key Findings

### Recommended Stack

The existing technology stack is validated and unchanged for v0.5. All work is architectural improvement and binding additions within the current stack. The critical insight from STACK.md: `CSVOptions` was already fixed correctly and serves as the implementation template for the `DatabaseOptions` fix — define C++ type independently, define C type independently, convert in `src/c/` implementation files only. The `static_assert` pattern for enum correspondence exists in the codebase and should be replicated for `LogLevel`.

**Core technologies:**
- **C++20**: Core library — validated; `enum class LogLevel` replaces the current C-enum alias
- **CFFI ABI-mode (Python)**: FFI without compiler at install time — correct choice; hand-maintained `_c_api.py` is the known maintenance tradeoff
- **sol2 (Lua via C++)**: Lua scripting — C API already complete (`quiver_lua_runner_new/free/run/get_error`); Python binding is the missing layer
- **Static assertions**: Compile-time safety for C/C++ enum correspondence — adopt the `static_assert(static_cast<int>(LogLevel::Debug) == QUIVER_LOG_DEBUG)` pattern in `src/c/database_options.h`

### Expected Features

All six improvements are table stakes for a library at this quality level. There are no new user-facing features; all work is consistency and correctness.

**Must have (table stakes):**
- **Consistent free function naming** — `quiver_element_free_string` is semantically wrong for freeing database query/read results; every mature C library (libgit2, curl, SDL2) scopes free functions to the allocating entity; fix: add `quiver_database_free_string` in `src/c/database_read.cpp`
- **Python `DataType` named constants** — 20+ bare integer literals (`0`, `1`, `2`, `3`, `4`) in `database.py`; `IntEnum` is the Python stdlib-endorsed pattern for C enum interoperability; eliminates magic numbers in `_marshal_params`, `_marshal_time_series_columns`, and 5 convenience helpers
- **Python LuaRunner binding** — Julia, Dart, and Lua all expose `LuaRunner`; Python is the only binding missing this; C API already exists and is well-tested
- **DatabaseOptions layer inversion fix** — C++ core (`include/quiver/options.h`) must not depend on C API headers (`include/quiver/c/options.h`); blocks adding C++-only fields to `DatabaseOptions` in the future
- **Cross-binding `is_healthy()`/`path()` tests** — Julia and Dart lack dedicated tests; Python has them; consistency is expected

**Should have (differentiators):**
- **`DataType` IntEnum used in `ScalarMetadata.data_type` field** — upgrade type annotation from `int` to `DataType` for IDE autocompletion; `IntEnum` is drop-in compatible with existing integer comparisons
- **`DatabaseOptions` with member initializers** — `bool read_only = false; LogLevel console_level = LogLevel::Info;` is idiomatic C++20; natural outcome of the layer inversion fix
- **Context manager for Python LuaRunner** — `with LuaRunner(db) as lua:` matches existing `Database` pattern

**Defer (v2+):**
- Exposing configurable `DatabaseOptions` in Python binding
- Generic `quiver_free` catch-all (wrong allocator model for Quiver; entity-scoped free is correct)
- CFFI API-mode (requires C compiler at install time; current ABI-mode is correct tradeoff)

### Architecture Approach

All three architectural changes follow patterns already established in the codebase. The `CSVOptions` pattern is the template for `DatabaseOptions`. The alloc/free co-location convention determines that `quiver_database_free_string` belongs in `src/c/database_read.cpp`. The Dart `LuaRunner` binding is the reference implementation for Python's. No new patterns are introduced.

**Major components and changes:**
1. **`include/quiver/options.h`** — Remove C API include; define C++ `LogLevel` enum class and `DatabaseOptions` struct independently
2. **`src/c/database_options.h`** — Add `convert_database_options()` and `static_assert` enum correspondence guards
3. **`src/c/database.cpp`** — Call `convert_database_options()` before constructing `quiver_database` (3 factory functions)
4. **`src/database.cpp`** — Update spdlog level mapping from `quiver_log_level_t` to `quiver::LogLevel`
5. **`include/quiver/c/database.h` + `src/c/database_read.cpp`** — Add `quiver_database_free_string` declaration and implementation
6. **All binding generators + hand-written usage files** — Re-run Julia, Dart, Python generators; update 8 usage sites across bindings
7. **`bindings/python/src/quiverdb/lua_runner.py`** — New file; 4 C API functions wrapped; `self._db = db` reference prevents GC hazard
8. **`bindings/python/src/quiverdb/constants.py`** — New file; `DataType(IntEnum)` replacing 20+ magic integers in `database.py`

### Critical Pitfalls

1. **Partial rename of `quiver_element_free_string` leaves dangling symbol references** — Julia (`c_api.jl`, `database_query.jl`, `database_read.jl`), Dart (`bindings.dart`, `database_query.dart`, `database_read.dart`), and Python (`_c_api.py`, `database.py`) all must be updated atomically. Any single binding's test suite failing with "symbol not found" is the detection signal. Mitigation: checklist sequence (C header -> C impl -> generators -> Python cdef -> usage sites -> `test-all.bat`).

2. **Python CFFI ABI-mode struct layout mismatch causes silent memory corruption** — If `quiver_database_options_t` layout changes, `_c_api.py` must be updated byte-for-field with no compile-time safety net. The v0.5 fix avoids this risk by leaving `quiver_database_options_t` layout unchanged — only the C++ internal representation changes. Add a Python test that opens a database with `read_only=True` to validate field mapping.

3. **Circular include between C++ and C headers if layer inversion is resolved incorrectly** — `include/quiver/options.h` must not include `include/quiver/c/options.h` and vice versa. Conversion only in `src/c/` implementation files. Caught at compile time, but choosing the wrong include pattern initially can cascade into significant restructuring.

4. **Python LuaRunner database lifetime hazard** — `quiver_lua_runner_t` borrows `quiver_database_t*` but does not own it. Python GC does not know about C-level ownership. Mitigation: `LuaRunner.__init__` must store `self._db = db` (not just `db._ptr`). Test: explicitly close the database then call `run()` — must raise a clean Python exception, not segfault.

5. **Generator overwrite destroys hand-written code if placed in wrong file** — Julia `c_api.jl` and Dart `bindings.dart` are fully generated; never modify by hand. After any C API header change: diff generated files before and after regeneration to catch unexpected removals.

## Implications for Roadmap

Changes are sequenced by scope of impact: narrowest (C++ only) to broadest (all bindings), with generator re-runs batched. This is driven directly by the pitfall analysis: C API surface changes must complete before generator re-runs, and generator re-runs should be batched across phases.

### Phase 1: Fix DatabaseOptions Layer Inversion

**Rationale:** Most architecturally impactful; establishes correct type ownership before any other change. Zero impact on bindings (C API struct layout unchanged). Lowest risk, highest structural value. All subsequent changes build on a correctly-layered foundation.
**Delivers:** C++ core independent of C API headers; `LogLevel` as proper `enum class`; `DatabaseOptions` with member initializers; `static_assert` guards for enum correspondence
**Addresses:** Layer inversion (table stakes), C++ LogLevel enum class (differentiator), member initializers (differentiator)
**Avoids:** Circular include (Pitfall 3); CFFI struct layout mismatch (Pitfall 2) by not touching `quiver_database_options_t`
**Files:** `include/quiver/options.h`, `src/database.cpp`, `src/c/database_options.h`, `src/c/database.cpp`

### Phase 2: Fix Free Function Naming

**Rationale:** Requires C API header changes and generator re-runs across all three binding generators. Should immediately follow Phase 1 so all generator runs happen together. Cross-cutting rename that touches all bindings; must be done before adding new binding features.
**Delivers:** `quiver_database_free_string` in correct entity scope; all 8 cross-binding usage sites updated; generators re-run; C API test for the new function
**Addresses:** Consistent free function naming (table stakes)
**Avoids:** Partial rename dangling symbol hazard (Pitfall 1) via checklist execution; generator overwrite (Pitfall 5)
**Files:** `include/quiver/c/database.h`, `src/c/database_read.cpp`; all binding usage sites; all generators re-run; Python `_c_api.py` updated manually

### Phase 3: Python DataType Constants

**Rationale:** Self-contained Python-only change with zero cross-layer impact. No generator re-runs. No C API changes. Can proceed independently after Phases 1-2 settle. Eliminates 20+ magic integers and enables typed metadata.
**Delivers:** `constants.py` with `DataType(IntEnum)`; all 20+ magic integers in `database.py` replaced; `ScalarMetadata.data_type` type upgraded from `int` to `DataType`; test asserting equality with CFFI lib values
**Addresses:** Python named constants (table stakes), ScalarMetadata typed field (differentiator)
**Avoids:** Dual constant sets divergence (Pitfall from PITFALLS.md) via single `IntEnum` source of truth
**Files:** `bindings/python/src/quiverdb/constants.py` (new), `database.py`, `metadata.py`

### Phase 4: Python LuaRunner Binding

**Rationale:** New feature addition; purely additive; no existing code changes except exports and CFFI declarations. Depends on Phase 2 being complete (stable C API surface, no more pending renames). Most complex of the Python-only changes due to lifetime management.
**Delivers:** `lua_runner.py` with `LuaRunner`/`LuaException`; context manager support; CFFI declarations; Python generator updated to include `lua_runner.h`; Python LuaRunner test suite
**Addresses:** Python LuaRunner binding (table stakes — cross-language feature parity)
**Avoids:** Database lifetime hazard (Pitfall 4) via `self._db = db` reference; double-free via `_closed` flag; wrong error source by using `quiver_lua_runner_get_error` not global error
**Files:** `lua_runner.py` (new), `_c_api.py`, `__init__.py`, `generator.py`, `_declarations.py` (regenerated), `test_lua_runner.py` (new)

### Phase 5: Test Coverage

**Rationale:** All API changes are complete. Adding tests validates the final state rather than a moving target. `is_healthy()`/`path()` tests are trivial per binding; convenience helper audit requires judgment on edge cases.
**Delivers:** `is_healthy()` and `path()` tests in Julia and Dart; Python convenience helper edge case coverage; all five test suites green as final milestone gate
**Addresses:** Cross-binding test coverage (table stakes)
**Avoids:** Dangling pointer exposure in `path()` tests (Pitfall 6) by always reading path before close
**Files:** Julia and Dart lifecycle test additions; Python test audit for edge cases

### Phase Ordering Rationale

- Phase 1 before all others: the only change with outbound architectural dependencies; establishes correct type ownership that Phase 2 builds on
- Phase 2 immediately after Phase 1: both phases trigger generator re-runs; batching them means generators run once, not twice
- Phases 3 and 4 can run in parallel: both are Python-only and have no dependency on each other; Phase 3 has zero risk; Phase 4 is additive only
- Phase 5 last: tests validate the completed API surface; writing tests against an in-progress API creates throwaway work

### Research Flags

No phase requires a `/gsd:research-phase` call. All patterns are confirmed by existing codebase code and verified external sources:

- **Phase 1:** Pattern proven by `CSVOptions` in the same codebase. Template is `src/c/database_options.h`.
- **Phase 2:** Purely mechanical rename with generator re-runs. Checklist-driven.
- **Phase 3:** `IntEnum` is a Python stdlib primitive. No research needed.
- **Phase 4:** Dart `LuaRunner` binding is the reference implementation. Adapt to CFFI conventions.
- **Phase 5:** Adding tests is straightforward. One known trap documented (Pitfall 6: `path()` before close).

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | Existing validated stack; changes are purely internal restructuring with no new dependencies |
| Features | HIGH | All improvements verified by direct codebase inspection; scope defined by PROJECT.md; no speculative features |
| Architecture | HIGH | All changes follow patterns already present in the codebase; `CSVOptions` is the working template |
| Pitfalls | HIGH | All pitfalls sourced from direct code analysis at commit `6dae8d1`; CFFI ABI-mode behavior is well-documented in official docs |

**Overall confidence:** HIGH

### Gaps to Address

- **`quiver_database_path` dangling pointer**: Phase 5's `path()` tests expose an existing API issue — the function returns `const char*` into an internal `std::string`. Decision needed: fix the API (add `strdup_safe` + `quiver_database_free_path`) or document the constraint. This is not in the current v0.5 scope but should be explicitly addressed or deferred during roadmap creation.
- **`_Bool` vs `int` consistency for `in_transaction`**: The C API uses `_Bool*` for `in_transaction` but `int*` for other boolean-returning functions. Should be normalized during Phase 2 when `_c_api.py` is already being modified, or explicitly deferred.
- **`CSVOptions` scope in Phase 1**: Confirm that `include/quiver/options.h` changes leave `CSVOptions` struct definition unchanged. Only `DatabaseOptions` needs restructuring. If `CSVOptions` changes are required, Pitfall 8 (enum labels marshaling across all bindings) activates.

## Sources

### Primary (HIGH confidence)
- Direct codebase inspection at commit `6dae8d1` on branch `rs/more4` — all architecture and pitfall findings
- [Hourglass Interfaces for C++ APIs — CppCon 2014](https://isocpp.org/blog/2015/07/cppcon-2014-hourglass-interfaces-for-cpp-apis-stefanus-dutoit) — authoritative C++/C API layering pattern
- [libgit2 conventions](https://github.com/libgit2/libgit2/blob/main/docs/conventions.md) — entity-scoped free function naming
- [Python enum module](https://docs.python.org/3/library/enum.html) — IntEnum for C enum interoperability
- [CFFI documentation](https://cffi.readthedocs.io/en/latest/ref.html) — ABI-mode struct layout requirements and opaque handle patterns

### Secondary (MEDIUM confidence)
- [Hourglass C-API reference implementation](https://github.com/JarnoRalli/hourglass-c-api) — parallel type definition pattern
- [SQLite C/C++ Interface](https://sqlite.org/capi3ref.html) — entity-scoped free function conventions
- [Python context managers for CFFI](https://kurtraschke.com/2015/10/python-cffi-context-managers/) — lifecycle patterns for opaque handles

---
*Research completed: 2026-02-27*
*Ready for roadmap: yes*
