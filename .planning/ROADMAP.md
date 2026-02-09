# Roadmap: Quiver Refactoring

## Overview

This roadmap transforms Quiver from a working but inconsistently organized library into a uniformly structured, consistently named, and fully standardized codebase across all five layers (C++, C API, Julia, Dart, Lua). The approach is interleaved: fix one layer at a time starting from the C++ core outward, with each phase producing a verifiable, test-passing state. Every phase builds on the previous one's structural improvements.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [ ] **Phase 1: C++ Impl Header Extraction** - Extract Database::Impl into private internal header to enable file decomposition
- [ ] **Phase 2: C++ Core File Decomposition** - Split monolithic database.cpp into focused modules by operation type
- [ ] **Phase 3: C++ Naming and Error Standardization** - Standardize C++ method names and exception patterns
- [ ] **Phase 4: C API File Decomposition** - Split monolithic C API database.cpp with shared helper extraction
- [ ] **Phase 5: C API Naming and Error Standardization** - Standardize C API function names and error handling patterns
- [ ] **Phase 6: Julia Bindings Standardization** - Standardize Julia binding names and error surfacing
- [ ] **Phase 7: Dart Bindings Standardization** - Standardize Dart binding names and error surfacing
- [ ] **Phase 8: Lua Bindings Standardization** - Standardize Lua binding names and error surfacing
- [ ] **Phase 9: Code Hygiene** - SQL injection mitigation, clang-tidy integration, static analysis cleanup
- [ ] **Phase 10: Cross-Layer Documentation and Final Verification** - Document naming conventions, validate full test suite across all layers

## Phase Details

### Phase 1: C++ Impl Header Extraction
**Goal**: The Database::Impl struct definition lives in a private internal header that multiple .cpp files can include without exposing sqlite3/spdlog in public headers
**Depends on**: Nothing (first phase)
**Requirements**: DECP-02
**Success Criteria** (what must be TRUE):
  1. A file `src/database_impl.h` exists containing the `Database::Impl` struct definition, moved from `src/database.cpp`
  2. `src/database.cpp` includes `src/database_impl.h` and compiles identically to before
  3. No public header in `include/quiver/` has changed
  4. All existing C++ tests pass (`quiver_tests.exe` green)
  5. `src/database_impl.h` is not reachable from any public include path (verified by include structure)
**Plans**: 1 plan

Plans:
- [ ] 01-01-PLAN.md -- Extract Database::Impl struct to src/database_impl.h internal header

### Phase 2: C++ Core File Decomposition
**Goal**: The monolithic database.cpp is split into focused modules where each file handles one category of operations, and developers can navigate directly to the file for any operation type
**Depends on**: Phase 1
**Requirements**: DECP-01, DECP-05
**Success Criteria** (what must be TRUE):
  1. `src/database.cpp` contains only lifecycle operations (constructor, destructor, move, factory methods) and is under 500 lines
  2. Separate implementation files exist for create, read, update, delete, metadata, time series, query, and relations operations
  3. Every split file includes `src/database_impl.h` and compiles as part of the library
  4. All existing C++ tests pass with zero behavior changes (`quiver_tests.exe` green)
  5. No public header in `include/quiver/` has changed
**Plans**: TBD

Plans:
- [ ] 02-01: Split database.cpp into functional modules

### Phase 3: C++ Naming and Error Standardization
**Goal**: All C++ public methods follow a single, documented naming convention and throw exceptions with consistent types and message formats
**Depends on**: Phase 2
**Requirements**: NAME-01, ERRH-01
**Success Criteria** (what must be TRUE):
  1. Every public method on the Database class follows the same naming pattern (verb_noun or verb_adjective_noun)
  2. All exceptions thrown use `std::runtime_error` with a consistent message format that includes the operation name and the reason for failure
  3. No two methods use different naming styles for the same kind of operation (e.g., all reads use `read_`, all updates use `update_`)
  4. All existing C++ tests pass after renaming (updated to match new names)
**Plans**: TBD

Plans:
- [ ] 03-01: Audit and standardize C++ method names
- [ ] 03-02: Standardize C++ exception patterns

### Phase 4: C API File Decomposition
**Goal**: The monolithic C API database.cpp is split into focused modules mirroring C++ structure, with shared helper templates extracted into a common internal header
**Depends on**: Phase 3
**Requirements**: DECP-03, DECP-04, DECP-06
**Success Criteria** (what must be TRUE):
  1. `src/c/database_helpers.h` exists containing marshaling templates, `strdup_safe`, metadata converters, and `QUIVER_REQUIRE` macro
  2. `src/c/database.cpp` contains only lifecycle C API functions and is under 500 lines
  3. Separate C API implementation files exist matching the C++ decomposition structure
  4. Every alloc/free pair is co-located in the same translation unit
  5. All existing C API tests pass with zero behavior changes (`quiver_c_tests.exe` green)
**Plans**: TBD

Plans:
- [ ] 04-01: Extract C API helper templates into shared header
- [ ] 04-02: Split C API database.cpp into functional modules

### Phase 5: C API Naming and Error Standardization
**Goal**: All C API function names follow a single `quiver_{entity}_{operation}` convention and every function uses the try-catch-set_last_error pattern consistently
**Depends on**: Phase 4
**Requirements**: NAME-02, ERRH-02
**Success Criteria** (what must be TRUE):
  1. Every C API function follows `quiver_{entity}_{operation}` naming (e.g., `quiver_database_read_scalar_integers`, not mixed patterns)
  2. Every C API function that can fail uses the `QUIVER_REQUIRE` macro for precondition checks and returns `QUIVER_OK`/`QUIVER_ERROR`
  3. C API header files updated with consistent naming; Julia/Dart generators re-run successfully
  4. All existing C API tests pass after renaming (updated to match new names)
**Plans**: TBD

Plans:
- [ ] 05-01: Audit and standardize C API function names
- [ ] 05-02: Standardize C API error handling patterns

### Phase 6: Julia Bindings Standardization
**Goal**: Julia bindings use idiomatic Julia naming conventions while mapping predictably to C API, and surface all C API errors uniformly without crafting custom messages
**Depends on**: Phase 5
**Requirements**: NAME-03, ERRH-03
**Success Criteria** (what must be TRUE):
  1. Every Julia wrapper function uses `snake_case` naming following Julia conventions
  2. Julia function names map predictably from C API names (e.g., `quiver_database_read_scalar_integers` -> `read_scalar_integers`)
  3. All Julia error handling retrieves and surfaces C API error messages -- no custom error strings defined in Julia code
  4. Julia test suite passes (`bindings/julia/test/test.bat` green)
**Plans**: TBD

Plans:
- [ ] 06-01: Standardize Julia binding names and error handling

### Phase 7: Dart Bindings Standardization
**Goal**: Dart bindings use idiomatic Dart naming conventions while mapping predictably to C API, and surface all C API errors uniformly without crafting custom messages
**Depends on**: Phase 5
**Requirements**: NAME-04, ERRH-04
**Success Criteria** (what must be TRUE):
  1. Every Dart wrapper method uses `camelCase` naming following Dart conventions
  2. Dart method names map predictably from C API names (e.g., `quiver_database_read_scalar_integers` -> `readScalarIntegers`)
  3. All Dart error handling retrieves and surfaces C API error messages -- no custom error strings defined in Dart code
  4. Dart test suite passes (`bindings/dart/test/test.bat` green)
**Plans**: TBD

Plans:
- [ ] 07-01: Standardize Dart binding names and error handling

### Phase 8: Lua Bindings Standardization
**Goal**: Lua bindings use consistent naming matching C++ method names and surface errors uniformly through pcall/error patterns
**Depends on**: Phase 3
**Requirements**: NAME-05, ERRH-05
**Success Criteria** (what must be TRUE):
  1. Every Lua binding method name matches the corresponding C++ method name
  2. All Lua error handling uses pcall/error patterns that surface C++ exception messages
  3. No custom error messages are crafted in Lua code -- all errors originate from C++
  4. Lua scripting tests pass through LuaRunner
**Plans**: TBD

Plans:
- [ ] 08-01: Standardize Lua binding names and error handling

### Phase 9: Code Hygiene
**Goal**: SQL string concatenation is eliminated from schema queries, clang-tidy is integrated, and the codebase passes static analysis checks
**Depends on**: Phase 5
**Requirements**: HYGN-01, HYGN-02, HYGN-03
**Success Criteria** (what must be TRUE):
  1. No SQL query in the codebase constructs table/column names via string concatenation without validation -- all identifiers are validated against the loaded schema or use parameterized equivalents
  2. A `.clang-tidy` configuration file exists with `readability-identifier-naming`, `bugprone-*`, `modernize-*`, `performance-*` checks enabled
  3. Running clang-tidy against the codebase produces zero errors (suppressions are documented for intentional exceptions)
  4. All test suites continue to pass after hygiene changes
**Plans**: TBD

Plans:
- [ ] 09-01: Replace SQL string concatenation with validated identifiers
- [ ] 09-02: Add clang-tidy configuration and fix violations

### Phase 10: Cross-Layer Documentation and Final Verification
**Goal**: Naming conventions are documented with cross-layer mapping examples in CLAUDE.md, and the full test suite across all layers passes as a final gate
**Depends on**: Phases 6, 7, 8, 9
**Requirements**: NAME-06, TEST-01, TEST-02, TEST-03, TEST-04, TEST-05
**Success Criteria** (what must be TRUE):
  1. CLAUDE.md contains a naming convention section with cross-layer mapping examples (C++ method -> C API function -> Julia method -> Dart method -> Lua method)
  2. `scripts/build-all.bat` succeeds end-to-end (builds all targets, runs all tests)
  3. C++ test suite passes (`quiver_tests.exe` green)
  4. C API test suite passes (`quiver_c_tests.exe` green)
  5. Julia test suite passes (`bindings/julia/test/test.bat` green)
  6. Dart test suite passes (`bindings/dart/test/test.bat` green)
**Plans**: TBD

Plans:
- [ ] 10-01: Document cross-layer naming conventions in CLAUDE.md
- [ ] 10-02: Final full-stack verification

## Progress

**Execution Order:**
Phases execute in numeric order: 1 -> 2 -> 3 -> 4 -> 5 -> 6 -> 7 -> 8 -> 9 -> 10
Note: Phases 6, 7 both depend on Phase 5. Phase 8 depends on Phase 3. Phase 10 depends on 6, 7, 8, 9.

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. C++ Impl Header Extraction | 0/1 | Not started | - |
| 2. C++ Core File Decomposition | 0/1 | Not started | - |
| 3. C++ Naming and Error Standardization | 0/2 | Not started | - |
| 4. C API File Decomposition | 0/2 | Not started | - |
| 5. C API Naming and Error Standardization | 0/2 | Not started | - |
| 6. Julia Bindings Standardization | 0/1 | Not started | - |
| 7. Dart Bindings Standardization | 0/1 | Not started | - |
| 8. Lua Bindings Standardization | 0/1 | Not started | - |
| 9. Code Hygiene | 0/2 | Not started | - |
| 10. Cross-Layer Documentation and Final Verification | 0/2 | Not started | - |
