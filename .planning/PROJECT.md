# Quiver

## What This Is

Quiver is a SQLite wrapper library with a C++ core, C API for FFI, and language bindings (Julia, Dart, Python, JS, JS-WASM, Lua). It provides structured access to SQLite databases with typed scalars, vectors, sets, time series, metadata, queries, CSV import/export, and transactions.

## Core Value

Uniform, type-safe database access across multiple languages through a single C++ implementation with thin FFI bindings.

## Requirements

### Validated

- Create/read/update/delete elements with typed scalar, vector, set attributes
- Time series read/write with multi-column support
- Parameterized SQL queries (string, integer, float)
- CSV export/import with enum and date formatting options
- Metadata introspection (scalar, vector, set, time series)
- Transaction control (begin, commit, rollback)
- Lua scripting with database access
- Binary file I/O subsystem (.qvr files with TOML metadata)
- Julia, Dart, Python, JS (koffi), JS-WASM bindings

### Active

- [x] Replace koffi FFI with Deno.dlopen in JS binding (Validated in Phase 1: FFI Foundation & Library Loading)
- [x] Rewrite pointer/buffer handling for Deno (UnsafePointer, Uint8Array) (Validated in Phase 2: Pointer & String Marshaling)
- [x] Rewrite string marshaling with TextEncoder/TextDecoder (Validated in Phase 2: Pointer & String Marshaling)
- [x] Rewrite array decoders with UnsafePointerView and convert domain modules (query, csv, time-series) (Validated in Phase 3: Array Decoding & Domain Helpers)
- [x] Update package config (deno.json, remove koffi dep) (Validated in Phase 5: Configuration & Packaging)
- [x] Create MIGRATION.md documenting the transition (Validated in Phase 7: Documentation)
- [x] Validate existing tests under Deno runtime (Validated in Phase 6: Test Migration & Validation)

### Out of Scope

- Multi-runtime JS support (Node.js, Bun) -- dropping in favor of Deno-only
- New API methods -- public JS API surface stays identical
- WASM binding changes -- separate package, not affected

## Current Milestone: v1.0 Deno FFI Migration

**Goal:** Replace koffi FFI with Deno.dlopen in the JS binding while preserving the public API surface.

**Target features:**
- Replace all koffi.load/koffi.func calls with Deno.dlopen symbol definitions
- Rewrite pointer/buffer handling to use Deno.UnsafePointer and Uint8Array
- Rewrite string marshaling with TextEncoder/TextDecoder
- Update package config (deno.json, remove koffi dep)
- Update README with --allow-ffi permission instructions
- Create MIGRATION.md documenting the transition
- Validate all existing tests pass under Deno

## Context

- C++ core compiles to shared libraries (libquiver.dll/so/dylib, libquiver_c.dll/so/dylib)
- JS binding currently uses koffi for FFI (Node.js-specific)
- Switching to Deno.dlopen enables path-scoped --allow-ffi permissions
- The JS binding covers: database lifecycle, CRUD, scalar/vector/set reads, queries, metadata, time series, CSV, transactions, composites, LuaRunner
- Existing JS binding source: bindings/js/src/ (16 TypeScript files)

## Constraints

- **Public API**: Keep identical -- consumers shouldn't need changes
- **Runtime**: Deno-only (dropping multi-runtime support)
- **Security**: Do NOT expose UnsafePointer, UnsafeCallback, or UnsafeFnPointer through the public API
- **Symbols**: Keep symbol list minimal -- only what quiver actually needs

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Deno.dlopen over koffi | Path-scoped --allow-ffi permissions, native Deno support | Validated (Phase 1) |
| Drop multi-runtime support | Simplifies FFI layer, Deno is target runtime | Validated (Phase 5) |
| Preserve public API | Consumer code shouldn't need changes | Validated (Phase 6) |

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `/gsd-transition`):
1. Requirements invalidated? -> Move to Out of Scope with reason
2. Requirements validated? -> Move to Validated with phase reference
3. New requirements emerged? -> Add to Active
4. Decisions to log? -> Add to Key Decisions
5. "What This Is" still accurate? -> Update if drifted

**After each milestone** (via `/gsd-complete-milestone`):
1. Full review of all sections
2. Core Value check -- still the right priority?
3. Audit Out of Scope -- reasons still valid?
4. Update Context with current state

---
*Last updated: 2026-04-18 after Phase 7 completion (final phase — milestone complete)*
