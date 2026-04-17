# Phase 1: FFI Foundation & Library Loading - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md -- this log preserves the alternatives considered.

**Date:** 2026-04-17
**Phase:** 01-ffi-foundation-library-loading
**Areas discussed:** Library search paths, Windows DLL chain, Symbol organization, Type mapping

---

## Library Search Paths

| Option | Description | Selected |
|--------|-------------|----------|
| Keep all 3 tiers | Bundled libs/, dev build/bin/ walk-up, system PATH. Replace node APIs with Deno equivalents. Most compatible. | ✓ |
| Dev + PATH only | Drop bundled tier. Deno modules aren't distributed via npm. Simpler, 2 tiers. | |
| You decide | Let Claude pick based on Deno module distribution patterns. | |

**User's choice:** Keep all 3 tiers (Recommended)
**Notes:** None

### Follow-up: Path Utilities

| Option | Description | Selected |
|--------|-------------|----------|
| Deno std @std/path | Import join/dirname from jsr:@std/path. Official, well-tested, cross-platform. | ✓ |
| Inline helpers | Write minimal join/dirname using string ops. Zero deps. | |
| You decide | Let Claude pick based on Deno FFI project conventions. | |

**User's choice:** Deno std @std/path (Recommended)
**Notes:** None

---

## Windows DLL Chain

| Option | Description | Selected |
|--------|-------------|----------|
| Explicit pre-load | Call Deno.dlopen(libquiver.dll, {}) first with empty symbols, then load libquiver_c.dll. Guaranteed to work. | ✓ |
| Rely on OS search order | Don't pre-load. Simpler but fragile if DLLs are in different locations. | |
| You decide | Let Claude determine safest approach. | |

**User's choice:** Explicit pre-load (Recommended)
**Notes:** None

---

## Symbol Organization

| Option | Description | Selected |
|--------|-------------|----------|
| Grouped by domain | Split into logical groups (lifecycle, element, read, query, etc.) as separate objects, spread into combined symbols. Mirrors C API file structure. | ✓ |
| One flat object | Single symbols object with all ~75 entries. Simple but long (~200 lines). | |
| You decide | Let Claude pick for maintainability. | |

**User's choice:** Grouped by domain (Recommended)
**Notes:** None

---

## Type Mapping

| Option | Description | Selected |
|--------|-------------|----------|
| Named constants | Define P, I32, I64, U64, F64, V constants at top. Mirrors existing koffi shorthand. | ✓ |
| Inline Deno types | Use 'pointer', 'i32', etc. directly. More verbose, no indirection. | |
| You decide | Let Claude balance readability and correctness. | |

**User's choice:** Named constants (Recommended)
**Notes:** None

---

## Claude's Discretion

- Error message format for library-not-found
- Exact domain grouping boundaries for symbols
- Whether to keep singleton caching pattern

## Deferred Ideas

None -- discussion stayed within phase scope.
