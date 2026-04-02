# Roadmap: Quiver JS Bindings — Deno Compatibility

## Overview

This milestone adds Deno runtime support to the JS bindings by introducing an FFI abstraction layer between the 14 source files and runtime-specific FFI APIs. The work progresses from defining abstract types and interfaces, through implementing concrete adapters for Bun and Deno, migrating all source files, and finally packaging and validating both runtimes.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3, 4): Planned milestone work
- Decimal phases (e.g., 2.1): Urgent insertions (marked with INSERTED)

- [ ] **Phase 1: FFI Type System & Library Loader** - Define opaque handle type and abstract library loading interface
- [ ] **Phase 2: FFI Operation Abstractions** - Abstract pointer, string, and buffer operations
- [ ] **Phase 3: Runtime Adapters & Migration** - Implement Bun/Deno adapters and migrate all source files
- [ ] **Phase 4: Packaging & Validation** - Exports map, Deno test suite, Bun regression check

## Phase Details

### Phase 1: FFI Type System & Library Loader
**Goal**: A runtime-agnostic type system and library loading interface exist that source files can import instead of bun:ffi
**Depends on**: Nothing (first phase)
**Requirements**: ABST-01, ABST-02
**Success Criteria** (what must be TRUE):
  1. An opaque handle type is defined that can represent both Bun's number pointers and Deno's UnsafePointer objects
  2. An abstract library loader interface exists that accepts symbol definitions and returns a symbols object
  3. Importing the abstraction module produces no runtime errors in either Bun or Deno
**Plans**: TBD

### Phase 2: FFI Operation Abstractions
**Goal**: All low-level FFI operations (pointer manipulation, string encoding/decoding, buffer access) are abstracted behind runtime-agnostic interfaces
**Depends on**: Phase 1
**Requirements**: ABST-03, ABST-04, ABST-05
**Success Criteria** (what must be TRUE):
  1. Pointer operations (alloc out-param, read pointer, read at offset) work through abstract functions without referencing bun:ffi or Deno directly
  2. String encoding (to C string) and decoding (from C string) work through abstract functions
  3. Buffer operations (get raw pointer, convert to array buffer) work through abstract functions
**Plans**: TBD

### Phase 3: Runtime Adapters & Migration
**Goal**: Concrete Bun and Deno adapters implement the abstraction, and all 14 source files use the abstraction instead of direct bun:ffi imports
**Depends on**: Phase 2
**Requirements**: RTAD-01, RTAD-02, RTAD-03
**Success Criteria** (what must be TRUE):
  1. Bun adapter wraps bun:ffi and passes existing Bun tests with no behavioral changes
  2. Deno adapter wraps Deno.dlopen/UnsafePointer/UnsafePointerView and can load the Quiver C library
  3. Zero source files in bindings/js/src/ contain direct imports from bun:ffi -- all FFI access goes through the abstraction
  4. The Quiver Database can be opened, queried, and closed using the Deno adapter
**Plans**: TBD

### Phase 4: Packaging & Validation
**Goal**: The npm package works out of the box in both Bun and Deno projects, validated by test suites
**Depends on**: Phase 3
**Requirements**: PACK-01, PACK-02, PACK-03
**Success Criteria** (what must be TRUE):
  1. package.json exports map routes Bun consumers to the Bun adapter and Deno consumers to the Deno adapter via conditions
  2. Deno test suite passes covering lifecycle, CRUD, read, query, and CSV operations
  3. Existing Bun test suite passes with no modifications
**Plans**: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1 -> 2 -> 3 -> 4

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. FFI Type System & Library Loader | 0/? | Not started | - |
| 2. FFI Operation Abstractions | 0/? | Not started | - |
| 3. Runtime Adapters & Migration | 0/? | Not started | - |
| 4. Packaging & Validation | 0/? | Not started | - |
