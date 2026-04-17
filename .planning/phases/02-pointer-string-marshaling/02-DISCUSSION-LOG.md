# Phase 2: Pointer & String Marshaling - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md -- this log preserves the alternatives considered.

**Date:** 2026-04-17
**Phase:** 02-pointer-string-marshaling
**Areas discussed:** Native memory allocation, Pointer type representation, GC & lifetime safety, Struct buffer building

---

## Native Memory Allocation

| Option | Description | Selected |
|--------|-------------|----------|
| Uint8Array + UnsafePointer.of | Create Uint8Array, write values via DataView, pass to C via Deno.UnsafePointer.of(buffer). JS-managed GC. Must keep buffer reference alive. | ✓ |
| C malloc/free via dlopen | Add malloc/free symbols to Deno.dlopen, allocate native-heap memory, write via UnsafePointerView. Explicit lifecycle, no GC concerns, but requires manual free() calls. | |
| You decide | Claude picks the best approach based on existing patterns and Deno FFI best practices. | |

**User's choice:** Uint8Array + UnsafePointer.of (Recommended)
**Notes:** User explicitly selected the recommended option. No follow-up discussion needed.

---

## Return Types (follow-up from allocation decision)

| Option | Description | Selected |
|--------|-------------|----------|
| Always return { ptr, buf } | Every alloc function returns an object with the pointer and the backing buffer. Callers destructure. Uniform pattern. | |
| Return pointer only, track buffers internally | Functions return just the pointer. Module-level Set keeps buffers alive. Cleaner caller API but hidden state. | |
| You decide | Claude picks based on how domain modules currently use these functions. | ✓ |

**User's choice:** You decide
**Notes:** Claude will use { ptr, buf } pattern -- consistent with existing allocNativeStringArray { table, keepalive } precedent.

---

## Pointer Type Representation

| Option | Description | Selected |
|--------|-------------|----------|
| Deno.PointerValue directly | Type alias: `type NativePointer = Deno.PointerValue`. Simple, standard, no wrapper. | |
| Branded wrapper type | Newtype wrapper for extra type safety. More ceremony. | |
| You decide | Claude picks the simplest approach that preserves type safety. | ✓ |

**User's choice:** You decide
**Notes:** Claude will use Deno.PointerValue alias -- simple and standard.

---

## GC & Lifetime Safety

| Option | Description | Selected |
|--------|-------------|----------|
| Scope-based keepalive | Alloc functions return both pointer and buffer. Callers hold buffer in local scope. Simple, explicit. | |
| Centralized allocation tracker | Module-level Set tracks all live allocations. Explicit release after each C API call. | |
| You decide | Claude picks based on call patterns in the domain modules. | ✓ |

**User's choice:** You decide
**Notes:** Claude will use scope-based keepalive -- matches existing patterns in domain modules.

---

## Struct Buffer Building

| Option | Description | Selected |
|--------|-------------|----------|
| Uint8Array + DataView | Same pattern as alloc functions. Consistent with everything else in the file. | |
| Dedicated struct helper | Small struct-builder utility. Reusable for CSVOptions in Phase 3. | |
| You decide | Claude picks the simplest approach for this phase. | ✓ |

**User's choice:** You decide
**Notes:** Claude will use Uint8Array + DataView -- consistent with allocation pattern, no premature abstraction.

---

## Claude's Discretion

- Return type pattern for alloc functions (chose { ptr, buf })
- NativePointer type representation (chose Deno.PointerValue alias)
- GC lifetime management strategy (chose scope-based keepalive)
- Struct building approach (chose Uint8Array + DataView)

## Deferred Ideas

None -- discussion stayed within phase scope.
