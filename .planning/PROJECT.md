# Quiver JS Bindings — Deno Compatibility

## What This Is

The `quiverdb` npm package provides JavaScript/TypeScript bindings for the Quiver SQLite wrapper library via native FFI. Currently Bun-only, this milestone makes it work in Deno projects consuming the package via npm.

## Core Value

The JS binding must load and call the Quiver C API correctly in both Bun and Deno runtimes, with no user-facing API differences.

## Current Milestone: v1.0 Deno

**Goal:** Make the JS binding work in both Bun and Deno projects via npm

**Target features:**
- FFI abstraction layer replacing direct `bun:ffi` imports
- Deno FFI adapter using `Deno.dlopen()` / `UnsafePointer` / `UnsafePointerView`
- Package.json exports map with `"bun"` and `"deno"` conditions
- Deno test suite verifying all binding functionality

## Requirements

### Validated

(None yet — ship to validate)

### Active

- [ ] FFI abstraction layer that wraps pointer ops, string encoding, library loading
- [ ] Deno adapter implementing the abstraction via `Deno.dlopen()`
- [ ] Bun adapter preserving current `bun:ffi` behavior
- [ ] All 14 source files migrated off direct `bun:ffi` imports
- [ ] Package.json exports map routing to correct runtime entry point
- [ ] Deno test suite covering core operations
- [ ] Existing Bun tests continue to pass

### Out of Scope

- Node.js support — Node.js N-API is a different FFI model entirely
- Binary subsystem bindings — not yet bound in JS
- Runtime auto-detection fallback — exports map is sufficient

## Context

- All 14 source files in `bindings/js/src/` import directly from `bun:ffi`
- FFI primitives used: `Pointer`, `ptr`, `read`, `CString`, `dlopen`, `FFIType`, `suffix`, `toArrayBuffer`
- Deno has no `CString` or `toArrayBuffer` — must be implemented via `UnsafePointerView`
- Deno pointer is an object (`Deno.UnsafePointer`), Bun pointer is a number — abstraction must hide this
- Deno requires `--allow-ffi` flag

## Constraints

- **Runtime compatibility**: Must work with Bun >= 1.0 and Deno >= 2.0
- **API parity**: Public TypeScript API must be identical across runtimes
- **No runtime detection**: Use package.json exports map conditions, not `typeof Deno`

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Exports map over runtime detection | Cleaner types, no async init, each entry point statically typed | — Pending |
| Opaque handle type for pointers | Bun uses number, Deno uses object — cannot share concrete type | — Pending |

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `/gsd:transition`):
1. Requirements invalidated? → Move to Out of Scope with reason
2. Requirements validated? → Move to Validated with phase reference
3. New requirements emerged? → Add to Active
4. Decisions to log? → Add to Key Decisions
5. "What This Is" still accurate? → Update if drifted

**After each milestone** (via `/gsd:complete-milestone`):
1. Full review of all sections
2. Core Value check — still the right priority?
3. Audit Out of Scope — reasons still valid?
4. Update Context with current state

---
*Last updated: 2026-04-01 after milestone v1.0 initialization*
