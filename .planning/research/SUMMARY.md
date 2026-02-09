# Project Research Summary

**Project:** Quiver Library Refactoring
**Domain:** C++20 library refactoring with C FFI and multi-language bindings (Julia, Dart, Lua)
**Researched:** 2026-02-09
**Confidence:** HIGH

## Executive Summary

Quiver is a SQLite wrapper library with a C++ core, C API for FFI, and bindings to Julia, Dart, and Lua. The research reveals that the primary issue is monolithic file organization: `src/database.cpp` (1934 lines) and `src/c/database.cpp` (1612 lines) mix unrelated concerns, making navigation, review, and concurrent development difficult. The recommended approach is to decompose these files by functional responsibility (create, read, update, metadata, time series, query, relations) while preserving the clean layer architecture. This is a well-understood refactoring pattern with established solutions.

The key risks are: (1) breaking Pimpl encapsulation by exposing internal headers publicly, (2) desynchronizing bindings after C API changes, (3) creating memory ownership mismatches in the C API alloc/free pairs, and (4) SQL injection through unvalidated identifier interpolation. All four risks have clear mitigation strategies: use private internal headers only, regenerate bindings after every C API change, co-locate alloc/free pairs, and validate all identifiers against the schema.

The refactoring is low-risk because it operates entirely on file organization without changing public API contracts. The multi-language binding architecture is Quiver's competitive advantage - no comparable SQLite wrapper offers C++ + C API + Julia + Dart + Lua from a single codebase. The refactoring strengthens this advantage by making the architecture more maintainable and adding complete API parity across all layers.

## Key Findings

### Recommended Stack

The existing stack is optimal and requires no changes. All dependencies (C++20, CMake 3.21+, SQLite 3.50.2, spdlog 1.17.0, sol2 3.5.0, GoogleTest 1.17.0) are already in place and appropriate. The key addition is clang-tidy for automated refactoring support - it enforces naming conventions, catches bugs, and can auto-fix many issues via the `-fix` flag. The project already generates `compile_commands.json`, so clang-tidy integration is straightforward.

**Core technologies:**
- C++20 standard — already in use, provides sufficient features for this codebase (no need for C++23)
- CMake 3.21+ with FetchContent — build system with dependency management
- SQLite 3.50.2 — core database dependency
- spdlog 1.17.0 — logging infrastructure
- sol2 3.5.0 — Lua C++ bindings for scripting layer
- GoogleTest 1.17.0 — C++ testing framework

**Refactoring tools:**
- clang-tidy — static analysis and automated refactoring (add `.clang-tidy` config)
- clang-format — code formatting (already in use)
- include-what-you-use — one-shot header cleanup (do not integrate into CI)

**Binding generation:**
- Dart ffigen — generates `bindings.dart` from C headers (already in use)
- Julia Clang.jl — generates `c_api.jl` from C headers (already in use)
- `scripts/generator.bat` — orchestrates both generators (run after every C API change)

### Expected Features

The refactoring focuses on structural improvements rather than new features. The critical work is achieving complete API parity across all four layers (C++, C API, Julia, Dart, Lua) and decomposing monolithic files by functional responsibility.

**Must have (table stakes):**
- Complete API parity across C++/C/Julia/Dart/Lua layers — every C++ method must be callable from all bindings
- Source file decomposition by responsibility — no file exceeds 500 lines
- Uniform naming conventions per layer — C++ uses `snake_case`, C uses `quiver_` prefix, Dart uses `camelCase`, Julia uses `snake_case`
- Explicit resource lifecycle — every C API allocation has a matching free function
- Test coverage for every public API function — tests are the contract

**Should have (competitive advantages):**
- Canonical API mapping table — single document mapping each operation across all five layers
- Mechanical binding generation — Julia/Dart bindings auto-generated from C headers
- Consistent file decomposition mirroring across layers — same logical split at every layer
- Version query at all layers — runtime compatibility checks
- Unified GroupMetadata across vector/set/time_series — reduces type proliferation

**Defer (v2+):**
- Blob module completion — implement binary data support (stub exists)
- Transaction control through C API — explicit begin/commit/rollback if scripting needs it
- Batch operations — bulk create/update for performance-critical cases
- Automated parity gap detection — script that validates binding completeness

### Architecture Approach

The current layer architecture is clean and correct. The problem is not architectural but organizational: monolithic files within layers. The solution is to split implementation files while preserving the Pimpl pattern and public API contracts. The `Database::Impl` struct definition moves to a private internal header (`src/database_impl.h`) that all implementation files share. This allows distribution of method bodies across multiple `.cpp` files without breaking encapsulation or exposing private dependencies (sqlite3, spdlog) in public headers.

**Major components (after decomposition):**

1. **database.cpp** — Lifecycle (constructor, destructor, move), execute core, factory methods, schema/migration application (~300 lines)
2. **database_create.cpp** — Element creation, update, deletion (~400 lines)
3. **database_read.cpp** — All scalar/vector/set reads (~200 lines)
4. **database_update.cpp** — All scalar/vector/set updates (~200 lines)
5. **database_metadata.cpp** — Metadata retrieval, listing, describe (~250 lines)
6. **database_time_series.cpp** — Time series operations including files (~200 lines)
7. **database_relations.cpp** — Relation operations (~80 lines)
8. **database_query.cpp** — Parameterized query methods (~30 lines)
9. **database_csv.cpp** — CSV export/import stubs (~10 lines)

The C API follows the same decomposition pattern with matching file names, maintaining a 1:1 correspondence between C++ and C API files.

### Critical Pitfalls

1. **SQL injection via identifier interpolation** — Collection and attribute names are concatenated directly into SQL strings throughout the codebase. Since SQLite parameterized queries only work for values, not identifiers, every identifier must be validated against the loaded schema before use. Add a centralized `validate_identifier()` function that all SQL-building methods call.

2. **C API memory ownership ambiguity** — Complex allocation/free patterns with 13+ different free functions. When splitting `src/c/database.cpp`, alloc/free pairs must move together to the same file. A mismatch between allocation strategy and deallocation strategy produces silent corruption. Document each pair as a formal contract and co-locate them.

3. **Binding desynchronization after C API changes** — Julia, Dart, and Lua bindings independently declare C API signatures. A parameter type change or signature modification is invisible until runtime. After every C API header change, regenerate Julia/Dart bindings via `scripts/generator.bat` and manually update Lua bindings before running tests.

4. **Pimpl encapsulation broken by file decomposition** — Moving `Database::Impl` to a shared header risks exposing sqlite3/spdlog dependencies publicly. The internal header (`src/database_impl.h`) must stay in `src/`, never in `include/`. Public headers remain unchanged.

5. **TransactionGuard rollback failures swallowed silently** — The destructor calls `rollback()` which logs errors but does not throw. A failed rollback leaves the connection in an indeterminate state but `is_healthy()` still returns true. Add a connection health flag that marks the database as unhealthy after a failed rollback.

## Implications for Roadmap

Based on research, suggested phase structure:

### Phase 1: C++ Core File Decomposition
**Rationale:** Foundation phase. All other refactoring depends on establishing clean file boundaries and internal headers. Must be first because C API decomposition mirrors C++ structure.

**Delivers:**
- Private internal header (`src/database_impl.h`) defining `Database::Impl` struct
- Split `database.cpp` (1934 lines) into 9 focused files by functional responsibility
- Each file <500 lines, single responsibility
- All existing tests pass with zero behavior change
- SQL injection mitigation via `validate_identifier()` function
- Blob type handling (either implement or add SchemaValidator rejection)
- TransactionGuard health tracking for rollback failures

**Addresses:**
- Source file decomposition (table stakes)
- Schema naming fragility (critical pitfall)
- SQL injection risk (critical pitfall)

**Avoids:**
- Breaking Pimpl encapsulation (keep internal header in `src/`)
- Exposing sqlite3/spdlog in public headers
- Monolithic files that cause merge conflicts

### Phase 2: C API File Decomposition
**Rationale:** Mirrors Phase 1 structure. C API file organization follows C++ file organization, creating predictable navigation. Memory ownership pairs co-located.

**Delivers:**
- Private C API helper header (`src/c/database_helpers.h`) with template helpers and conversion functions
- Split `src/c/database.cpp` (1612 lines) into 6 focused files matching C++ decomposition
- All alloc/free pairs co-located in same translation unit
- Memory ownership documented as formal contracts
- All C API tests pass

**Addresses:**
- Complete C API decomposition
- Memory ownership clarity (critical pitfall)

**Avoids:**
- Alloc/free pair separation causing memory corruption
- Template helper duplication across C API files

### Phase 3: Complete API Parity
**Rationale:** With file structure clean, fill gaps in binding coverage. Many C++ methods exist but lack C API wrappers or binding methods.

**Delivers:**
- Every C++ method has C API function
- Every C API function has Julia wrapper method
- Every C API function has Dart wrapper method
- All CSV operations exposed in Lua
- `is_healthy`, `path`, `current_version` exposed in all binding layers
- Automated binding regeneration verified (generators run, tests pass)

**Addresses:**
- Complete API parity (table stakes)
- Binding desynchronization prevention (critical pitfall)

**Avoids:**
- Partial binding coverage forcing users to drop to lower layers
- Generator not run after C API changes

### Phase 4: Naming Standardization and Documentation
**Rationale:** With structure and parity complete, standardize names across layers and document the mapping.

**Delivers:**
- Canonical API mapping table in CLAUDE.md (C++ method → C function → Julia method → Dart method → Lua method)
- Naming consistency audit across all five layers
- Any inconsistent names flagged and corrected
- Lua bindings updated to match C API

**Addresses:**
- Uniform naming conventions (table stakes)
- Canonical API mapping table (competitive advantage)
- Naming inconsistency cascade prevention (critical pitfall)

**Avoids:**
- Cross-layer naming drift
- Manual binding mismatches

### Phase Ordering Rationale

- **C++ first, C API second:** The C API wraps C++ methods. Split C++ first to establish the correct functional boundaries, then mirror those boundaries in the C API split.
- **Parity after structure:** Filling API gaps is easier with clean file organization. Adding a new C API function to `database_read.cpp` is straightforward when that file already exists and is focused.
- **Naming last:** Renaming across all five layers requires the structure to be stable. Doing it last avoids re-renaming during file moves.
- **Each phase maintains green tests:** The refactoring is incremental. Every phase completes with all tests passing, making it safe to stop or pivot at any boundary.

### Research Flags

**Phases needing deeper research during planning:**
- None — this is a refactoring project within a well-understood codebase. All patterns are established (Pimpl decomposition, C FFI layer, FFI binding generation).

**Phases with standard patterns (skip research-phase):**
- All phases — file decomposition, API parity completion, and naming standardization are well-documented patterns with clear implementation paths.

**Notes:**
- Each phase should use clang-tidy for automated naming and modernization checks
- Generator scripts (`generator.bat`) must run after every C API change
- Full test suite (`scripts/test-all.bat`) must pass at every phase boundary

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | All dependencies already in place and appropriate. Clang-tidy configuration verified against LLVM docs. |
| Features | HIGH | Based on direct codebase analysis and established FFI design principles (Botan, CppCon hourglass pattern). |
| Architecture | HIGH | Pimpl decomposition and C API wrapper patterns are standard practice. LLVM uses same approach for large classes. |
| Pitfalls | HIGH | Direct inspection of 1934-line and 1612-line monoliths revealed specific issues with concrete solutions. |

**Overall confidence:** HIGH

The refactoring is low-risk because:
1. No public API changes (headers remain unchanged)
2. No binding regeneration needed (C API signatures unchanged)
3. Tests verify behavior at every step
4. File decomposition is a solved problem with established patterns
5. The project has comprehensive test coverage across all layers

### Gaps to Address

The research is comprehensive with no major gaps. Minor validation points during execution:

- **Windows TLS behavior for thread-local error storage:** Verify `thread_local` works correctly with the DLL build. Current implementation appears correct but should be tested under concurrent load.
- **SQLite STRICT mode limitations:** Confirm that STRICT mode provides sufficient protection against type confusion to make the SQL injection risk primarily about identifiers, not values.
- **Generator reliability:** Verify that Dart ffigen and Julia Clang.jl produce consistent output across different versions. Current YAML/TOML configs should be stable, but test regeneration produces no diffs.

These are verification tasks, not research gaps. The approach is clear.

## Sources

### Primary (HIGH confidence)
- Quiver codebase direct analysis (all source files, tests, schemas)
- Clang-Tidy documentation (LLVM) — check categories, configuration format
- C++ Pimpl pattern (Herb Sutter GotW #100, Scott Meyers Effective Modern C++ Item 22)
- Botan FFI C Binding Documentation — authoritative FFI design reference
- CppCon 2014: Hourglass Interfaces for C++ APIs (Stefanus DuToit) — foundational pattern
- Dart C interop official guide — FFI binding patterns
- Julia Clang.jl documentation — binding generator
- LLVM source (Sema decomposition pattern) — split implementation pattern reference

### Secondary (MEDIUM confidence)
- Binding a C++ Library to 10 Languages (Ash Vardanian) — practitioner experience
- Layered Library Design (Big Book of Rust Interop) — pattern applies universally
- Principles of FFI API Design (Edward Z. Yang) — principled guidance

### Tertiary (LOW confidence)
- None — all findings verified against primary sources

---
*Research completed: 2026-02-09*
*Ready for roadmap: yes*
