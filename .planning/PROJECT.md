# Quiver

## What This Is

SQLite wrapper library with a C++20 core, C API for FFI, and language bindings (Julia, Dart, Python, Lua). Provides typed CRUD, queries, transactions, CSV import/export, and a standalone blob file I/O subsystem. Designed for embedding in domain-specific applications that need structured data with multi-language access.

## Core Value

Consistent, typed database access across multiple languages through a single C++ core — logic lives in one place, bindings are thin.

## Current Milestone: v0.5 JavaScript/TypeScript Binding

**Goal:** Add a Bun-native JS/TS binding covering the core API subset, published to npm.

**Target features:**
- Bun-only binding using `bun:ffi`
- Database lifecycle (open, close, factory methods)
- CRUD operations (create, read, update, delete elements)
- Query operations (parameterized and plain)
- Transaction control (begin, commit, rollback)
- TypeScript types for full type safety
- npm package, installable via `bun add`

## Requirements

### Validated

<!-- Shipped and confirmed valuable. Inferred from existing codebase. -->

- ✓ C++20 core with full CRUD, queries, CSV, transactions, schema validation
- ✓ C API layer with opaque handles and binary error codes
- ✓ Julia bindings (full API surface + convenience methods)
- ✓ Dart bindings (full API surface + convenience methods)
- ✓ Python bindings (CFFI ABI-mode, full API surface)
- ✓ Blob subsystem (binary file I/O with TOML metadata)
- ✓ Lua scripting engine via LuaRunner
- ✓ CLI tool (quiver_cli)
- ✓ Comprehensive test suite across all layers

### Active

<!-- Current scope. Building toward these in v0.5. -->

- [ ] JS/TS binding for Bun using bun:ffi
- [ ] Core API subset (lifecycle, CRUD, queries, transactions)
- [ ] TypeScript type definitions
- [ ] npm package publication
- [ ] Test suite for JS/TS binding

### Out of Scope

<!-- Explicit boundaries for v0.5. -->

- Node.js support — Bun only for v0.5, different FFI mechanism
- CSV import/export in JS binding — defer to future milestone
- Blob subsystem in JS binding — defer to future milestone
- Lua scripting in JS binding — defer to future milestone
- Metadata/list operations in JS binding — defer to future milestone

## Context

- Existing bindings follow a consistent pattern: thin wrappers over C API, no logic in binding layer
- Each binding has its own generator script for FFI declarations
- Cross-layer naming conventions are mechanical (documented in CLAUDE.md)
- Bun's `bun:ffi` provides direct C function calling without native addon compilation
- DLL/shared library loading is a solved pattern in existing bindings (Julia ccall, Dart dart:ffi, Python CFFI)

## Constraints

- **Runtime**: Bun only — uses `bun:ffi` which is Bun-specific
- **Architecture**: Thin binding — all logic stays in C++ layer
- **Compatibility**: Must work with existing `libquiver_c` shared library (no C API changes)
- **Naming**: Follow cross-layer naming conventions (snake_case C → camelCase JS/TS)
- **Error handling**: Surface C API error messages, never craft binding-specific errors

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Bun-only (no Node.js) | Simpler FFI, user's target runtime | — Pending |
| Core subset for v0.5 | Manageable scope, add CSV/blob/Lua later | — Pending |
| npm for distribution | Standard JS package registry, Bun uses it natively | — Pending |

---
*Last updated: 2026-03-08 after milestone v0.5 initialization*
