# Quiver

## What This Is

Quiver is a SQLite wrapper library with a C++ core, C API for FFI, and language bindings (Julia, Dart, Python, JS, Lua). It provides structured access to SQLite databases with typed scalars, vectors, sets, time series, metadata, queries, CSV import/export, and transactions.

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
- Julia, Dart, Python, JS (Deno FFI), JS-WASM bindings
- JS binding migrated from koffi to Deno.dlopen (v1.0 milestone)
- JS binding published to JSR as @psrenergy/quiver (v1.1 milestone)
- Deno test job and JSR publish workflow in CI (v1.1 milestone)

### Active

- [ ] Bundle pre-built native libraries (Linux x64, Windows x64) in JSR package
- [ ] Update publish workflow to build natives before JSR publish
- [ ] Verify loader Tier 1 search works with JSR-installed packages

### Out of Scope

- Multi-runtime JS support (Node.js, Bun) -- dropping in favor of Deno-only
- New API methods -- public JS API surface stays identical
- WASM binding changes -- separate package, not affected

## Current Milestone: v1.2 Native Library Bundling

**Goal:** Bundle pre-built native libraries in the JSR package so consumers don't need to build C++ themselves

**Target features:**
- Build libquiver + libquiver_c for Linux x64 and Windows x64 in CI
- Bundle binaries in JSR package via publish.include (libs/{os}-{arch}/)
- Update publish workflow to build natives before npx jsr publish
- Verify loader Tier 1 search (libs/{os}-{arch}/) works with JSR-installed packages

## Context

- C++ core compiles to shared libraries (libquiver.dll/so/dylib, libquiver_c.dll/so/dylib)
- JS binding uses Deno.dlopen for FFI (migrated from koffi in v1.0)
- Deno.dlopen enables path-scoped --allow-ffi permissions
- The JS binding covers: database lifecycle, CRUD, scalar/vector/set reads, queries, metadata, time series, CSV, transactions, composites, LuaRunner
- JS binding source: bindings/js/src/ (16 TypeScript files)
- JSR package: @psrenergy/quiver (jsr.io)
- CI has Deno test job and JSR publish with OIDC (v1.1)
- JSR package currently publishes TypeScript source only — no native binaries bundled
- Loader has 3-tier search: bundled libs/{os}-{arch}/ → dev build/bin/ walk-up → system PATH

## Constraints

- **Public API**: Keep identical -- consumers shouldn't need changes
- **Runtime**: Deno-only (dropping multi-runtime support)
- **Security**: Do NOT expose UnsafePointer, UnsafeCallback, or UnsafeFnPointer through the public API
- **Symbols**: Keep symbol list minimal -- only what quiver actually needs

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Deno.dlopen over koffi | Path-scoped --allow-ffi permissions, native Deno support | Validated (v1.0 Phase 1) |
| Drop multi-runtime support | Simplifies FFI layer, Deno is target runtime | Validated (v1.0 Phase 5) |
| Preserve public API | Consumer code shouldn't need changes | Validated (v1.0 Phase 6) |
| JSR over npm | Deno-native registry, no build step needed, OIDC auth | Validated (v1.1 Phase 8) |
| Bundle natives in JSR | Consumers need pre-built libs, can't expect C++ toolchain | Pending |

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
*Last updated: 2026-04-18 — Milestone v1.2 started*
