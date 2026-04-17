# Phase 2: Pointer & String Marshaling - Research

**Researched:** 2026-04-17
**Domain:** Deno FFI pointer/string marshaling, native memory allocation
**Confidence:** HIGH

## Summary

Phase 2 rewrites all pointer out-parameter, string marshaling, and native memory allocation helpers in `bindings/js/src/ffi-helpers.ts` from koffi to Deno FFI primitives. The 13 in-scope functions divide into three categories: (1) pointer out-parameter allocation and reading (allocPtrOut, readPtrOut, allocUint64Out, readUint64Out), (2) string encoding/decoding (toCString, decodeStringFromBuf), and (3) native memory allocation for typed values (allocNativeInt64, allocNativeFloat64, allocNativeString, allocNativeStringArray, allocNativePtrTable, nativeAddress, makeDefaultOptions).

The Deno FFI API provides all necessary primitives: `Uint8Array` buffers passed as `"buffer"` parameters, `DataView` for typed field writes, `Deno.UnsafePointer.of()` for buffer-to-pointer conversion, `Deno.UnsafePointer.create()` for BigInt-to-pointer conversion, and `Deno.UnsafePointerView.getCString()` for C string reading. All patterns were verified against Deno 2.7.10 (installed on target machine). The critical design decision (D-03) requires all alloc functions to return `{ ptr, buf }` pairs to prevent GC from collecting buffers while pointers are in use.

**Primary recommendation:** Replace every koffi call in ffi-helpers.ts with the corresponding Deno FFI primitive, using the `{ ptr: Deno.PointerValue; buf: Uint8Array }` return pattern for all allocation functions. Array decoders (5 functions) are Phase 3 scope -- leave them unchanged or stub them.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **D-01:** All native memory allocation uses `Uint8Array` + `Deno.UnsafePointer.of(buffer)` instead of `koffi.alloc`/`koffi.encode`. No C-level malloc/free symbols needed.
- **D-02:** Values are written into Uint8Array backing buffers via `DataView` (setBigInt64, setFloat64, setInt32, etc.) with little-endian byte order.
- **D-03:** All alloc functions return `{ ptr: Deno.PointerValue; buf: Uint8Array }` so callers can hold the buffer reference to prevent GC. This extends the existing `{ table, keepalive }` pattern from `allocNativeStringArray` to all allocation functions.
- **D-04:** `toCString` follows the same pattern -- returns `{ ptr, buf }` instead of a bare Buffer.
- **D-05:** `NativePointer` becomes a type alias for `Deno.PointerValue` (which is `Deno.PointerObject | null`). Simple alias, no branded wrapper. Defined in `types.ts` or `ffi-helpers.ts`.
- **D-06:** Scope-based keepalive -- callers hold buffer references in local variables during C API calls. No centralized allocation tracker.
- **D-07:** `makeDefaultOptions` uses `Uint8Array` + `DataView` for the 8-byte options struct, consistent with all other allocation functions. Returns `{ ptr, buf }`.
- **D-08:** String encoding uses `TextEncoder` to produce UTF-8 bytes, manually appending a null terminator byte. String decoding from C pointers uses `Deno.UnsafePointerView.getCString()`.

### Claude's Discretion
- Exact function signature details for `allocPtrOut`/`readPtrOut` (whether they use Uint8Array + DataView or BigUint64Array)
- Whether `nativeAddress` is still needed or can be replaced by direct pointer operations
- Internal naming of the `Allocation` return type (if a named type is warranted)
- Whether to keep `allocUint64Out`/`readUint64Out` as separate functions or fold into a generic pattern

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| MARSH-01 | Pointer out-parameters work using BigInt (Deno.UnsafePointer) instead of koffi Buffer | Verified: `Uint8Array(8)` as buffer param, `DataView.getBigUint64()` to read, `Deno.UnsafePointer.create()` to convert -- all confirmed in Deno 2.7.10 |
| MARSH-03 | String marshaling uses TextEncoder/TextDecoder for C string encoding/decoding | Verified: `TextEncoder.encode(str + "\0")` produces null-terminated UTF-8 Uint8Array; `Deno.UnsafePointerView.getCString()` reads C strings from pointers |
| MARSH-04 | Native memory allocation (int64, float64, string, pointer tables) works using Deno FFI primitives | Verified: `Uint8Array` + `DataView` + `Deno.UnsafePointer.of()` pattern replaces `koffi.alloc`/`koffi.encode` for all types |
</phase_requirements>

## Project Constraints (from CLAUDE.md)

- **Intelligence in C++ layer:** Bindings remain thin -- ffi-helpers.ts does marshaling only, no business logic.
- **Error messages from C/C++ layer:** Bindings retrieve and surface errors, never craft their own.
- **Ownership explicit:** RAII in C++; JS side must hold buffer references to prevent GC during calls.
- **Homogeneity:** API surface should feel uniform -- all alloc functions returning the same `{ ptr, buf }` shape.
- **Self-Updating:** CLAUDE.md must be updated if binding patterns change.
- **Clean code over defensive code:** No excessive null checks; assume callers obey contracts.

## Standard Stack

### Core
| Library/API | Version | Purpose | Why Standard |
|-------------|---------|---------|--------------|
| `Deno.UnsafePointer` | Deno 2.7.10 (stable) | Pointer creation, comparison, address extraction | Built-in Deno FFI API, no external dependency [VERIFIED: `deno --version` on target machine] |
| `Deno.UnsafePointerView` | Deno 2.7.10 (stable) | Reading typed values and C strings from native pointers | Built-in, provides getCString, getBigInt64, getFloat64 [VERIFIED: `deno eval` test on target] |
| `TextEncoder` | Web standard (always available) | UTF-8 string encoding for C string parameters | Web standard API, zero dependency [VERIFIED: Deno runtime] |
| `DataView` | Web standard (always available) | Typed value writes at byte offsets in Uint8Array buffers | Standard API for little-endian typed access [VERIFIED: Deno runtime test] |
| `Uint8Array` | Web standard (always available) | Buffer allocation for native memory | Deno FFI `"buffer"` parameter type accepts TypedArray [VERIFIED: Deno docs + runtime] |
| `BigUint64Array` | Web standard (always available) | Reading pointer-sized values from out-parameter buffers | 64-bit unsigned integer array for pointer I/O [VERIFIED: Deno runtime test] |

### Supporting
No external libraries needed. All functionality is provided by Deno built-ins and Web standard APIs.

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `Uint8Array` + `DataView` | `BigUint64Array` directly for pointer out-params | BigUint64Array is simpler for pure pointer-sized values but DataView is needed for mixed-type structs; consistency favors one approach |
| `Deno.UnsafePointerView.getCString()` | Manual byte-by-byte reading until null | getCString is a single native call, far more efficient and correct for UTF-8 |

## Architecture Patterns

### Return Type Convention

All allocation functions return the same shape:

```typescript
// Source: CONTEXT.md decision D-03
type Allocation = {
  ptr: Deno.PointerValue;  // pointer for passing to FFI calls
  buf: Uint8Array;          // buffer reference to prevent GC
};
```

This unifies the previous inconsistency where some functions returned `Buffer`, some returned `NativePointer`, and only `allocNativeStringArray` returned `{ table, keepalive }`.

### Pointer Out-Parameter Pattern

The core pattern for C functions that write to a `void**` or `type*` out-parameter:

```typescript
// Source: Verified against Deno 2.7.10 runtime + Denonomicon docs
// Allocate 8-byte buffer for pointer out-param
function allocPtrOut(): Allocation {
  const buf = new Uint8Array(8);
  return { ptr: Deno.UnsafePointer.of(buf), buf };
}

// Read pointer from populated out-param buffer
function readPtrOut(alloc: Allocation): Deno.PointerValue {
  const dv = new DataView(alloc.buf.buffer);
  const raw = dv.getBigUint64(0, true);  // little-endian
  return raw === 0n ? null : Deno.UnsafePointer.create(raw);
}
```

### String Encoding Pattern

```typescript
// Source: Verified against Deno 2.7.10 runtime + GitHub issue #19200
const encoder = new TextEncoder();

function toCString(str: string): Allocation {
  const encoded = encoder.encode(str + "\0");  // null-terminated UTF-8
  return { ptr: Deno.UnsafePointer.of(encoded), buf: encoded };
}
```

### String Decoding Pattern

```typescript
// Source: Verified against Deno 2.7.10 runtime + official docs
// Read a char* stored inside a char** out-parameter buffer
function decodeStringFromBuf(alloc: Allocation): string {
  const ptr = readPtrOut(alloc);  // get the char* pointer
  if (!ptr) return "";
  return new Deno.UnsafePointerView(ptr).getCString();
}
```

### Native Memory Allocation Pattern

```typescript
// Source: Verified against Deno 2.7.10 runtime
function allocNativeInt64(values: number[]): Allocation {
  const buf = new Uint8Array(values.length * 8);
  const dv = new DataView(buf.buffer);
  for (let i = 0; i < values.length; i++) {
    dv.setBigInt64(i * 8, BigInt(values[i]), true);
  }
  return { ptr: Deno.UnsafePointer.of(buf), buf };
}

function allocNativeFloat64(values: number[]): Allocation {
  const buf = new Uint8Array(values.length * 8);
  const dv = new DataView(buf.buffer);
  for (let i = 0; i < values.length; i++) {
    dv.setFloat64(i * 8, values[i], true);
  }
  return { ptr: Deno.UnsafePointer.of(buf), buf };
}
```

### Pointer Table Allocation Pattern

```typescript
// Source: Verified against Deno 2.7.10 runtime
function allocNativePtrTable(ptrs: Deno.PointerValue[]): Allocation {
  const buf = new Uint8Array(ptrs.length * 8);
  const dv = new DataView(buf.buffer);
  for (let i = 0; i < ptrs.length; i++) {
    const addr = ptrs[i] ? Deno.UnsafePointer.value(ptrs[i]!) : 0n;
    dv.setBigUint64(i * 8, addr, true);
  }
  return { ptr: Deno.UnsafePointer.of(buf), buf };
}
```

### Anti-Patterns to Avoid
- **Losing buffer references:** Never discard the `buf` from an `Allocation` before the FFI call completes. The GC may reclaim the buffer and create a dangling pointer. [CITED: denonomicon.deno.dev/philosophy-of-ffi]
- **Using `Buffer` (Node.js):** Deno FFI works with `Uint8Array`, not Node.js `Buffer`. Even though `Buffer` extends `Uint8Array`, using it introduces a Node.js compatibility layer dependency.
- **Passing BigInt where pointer expected:** Deno FFI `"pointer"` parameters expect `PointerObject | null`, not `bigint`. Use `Deno.UnsafePointer.create()` to convert BigInt to a pointer object.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| C string reading | Manual byte scanning for null terminator | `Deno.UnsafePointerView.getCString()` | Native implementation handles UTF-8 correctly, faster than JS loop |
| Pointer arithmetic | Manual BigInt math on addresses | `Deno.UnsafePointer.offset(ptr, n)` | Type-safe, avoids BigInt overflow errors |
| Pointer comparison | `===` on pointer objects | `Deno.UnsafePointer.equals(a, b)` | Pointer objects are opaque, `===` checks identity not address equality |
| Pointer-to-BigInt | Custom conversion logic | `Deno.UnsafePointer.value(ptr)` | Built-in, handles null correctly (returns 0n) |

## Common Pitfalls

### Pitfall 1: GC Collecting Buffers Before FFI Call Completes
**What goes wrong:** After calling `Deno.UnsafePointer.of(buf)`, the returned pointer does NOT prevent `buf` from being GC'd. If the caller discards `buf`, the pointer becomes dangling.
**Why it happens:** Deno uses WeakMap internally for pointer-to-buffer tracking since 1.31, but this only helps within a single FFI call frame. Between JS statements, V8 may GC unreferenced buffers.
**How to avoid:** Always return and hold `{ ptr, buf }` from allocation functions. Callers must keep `buf` in scope until after the C function returns.
**Warning signs:** Sporadic segfaults or corrupted data that appear non-deterministically.

### Pitfall 2: Forgetting Null Terminator on C Strings
**What goes wrong:** `TextEncoder.encode("hello")` produces `[104, 101, 108, 108, 111]` -- no null terminator. Passing this to a C function expecting `const char*` reads past the buffer.
**Why it happens:** Unlike Node.js `Buffer.from("str\0")`, TextEncoder does not automatically append a null byte.
**How to avoid:** Always append `"\0"` before encoding: `encoder.encode(str + "\0")`.
**Warning signs:** Random characters at end of strings, buffer overreads, potential crashes.

### Pitfall 3: Endianness Assumption
**What goes wrong:** Writing multi-byte values with wrong endianness produces garbage data when read by C code.
**Why it happens:** DataView methods default to big-endian. C code on x86/ARM expects little-endian.
**How to avoid:** Always pass `true` as the littleEndian parameter to DataView methods: `dv.setInt32(offset, value, true)`.
**Warning signs:** Values appear byte-swapped (e.g., 1 appears as 16777216 on 32-bit int).

### Pitfall 4: Null Pointer from create(0n) vs JavaScript null
**What goes wrong:** `Deno.UnsafePointer.create(0n)` returns `null`, not a zero-address pointer object. But reading a pointer from a buffer that was never written to gives `0n`, which must be handled.
**Why it happens:** Deno maps the zero address to JavaScript `null` for null pointer semantics.
**How to avoid:** In `readPtrOut`, check for `0n` before calling `create()`: `raw === 0n ? null : Deno.UnsafePointer.create(raw)`.
**Warning signs:** `getCString(null)` throws TypeError.

### Pitfall 5: Signature Change Cascading to Domain Modules
**What goes wrong:** Changing return types from `Buffer`/`NativePointer` to `{ ptr, buf }` breaks all callers.
**Why it happens:** Phase 2 changes the contract of ffi-helpers functions. Domain modules (database.ts, query.ts, read.ts, etc.) call these functions extensively.
**How to avoid:** Phase 2 scope is ffi-helpers.ts only. Domain modules will be updated in Phase 3-4. During Phase 2, function signatures change but callers are NOT updated yet -- code will not compile until Phase 3.
**Warning signs:** TypeScript compilation errors in domain modules (expected -- deferred to Phase 3).

### Pitfall 6: nativeAddress May Become Unnecessary
**What goes wrong:** `nativeAddress(ptr)` currently wraps `koffi.address(ptr)`. With Deno, pointers are already representable as BigInt via `Deno.UnsafePointer.value()`.
**Why it happens:** koffi used opaque pointer objects that needed explicit address extraction. Deno provides this directly.
**How to avoid:** Consider whether `nativeAddress` adds value as a thin wrapper around `Deno.UnsafePointer.value()`, or whether callers should use `Deno.UnsafePointer.value()` directly. Since csv.ts and query.ts use `nativeAddress` to write pointer addresses into BigUint64 slots, keeping it as a convenience wrapper is reasonable for uniformity.
**Warning signs:** None -- both approaches work. Decision is ergonomic, not correctness.

## Code Examples

### Complete allocPtrOut / readPtrOut Pair
```typescript
// Source: Verified against Deno 2.7.10 runtime
type Allocation = { ptr: Deno.PointerValue; buf: Uint8Array };

function allocPtrOut(): Allocation {
  const buf = new Uint8Array(8);
  return { ptr: Deno.UnsafePointer.of(buf), buf };
}

function readPtrOut(alloc: Allocation): Deno.PointerValue {
  const dv = new DataView(alloc.buf.buffer);
  const raw = dv.getBigUint64(0, true);
  return raw === 0n ? null : Deno.UnsafePointer.create(raw);
}

// Usage in database.ts (Phase 3):
const outDb = allocPtrOut();
check(lib.quiver_database_from_schema(dbPath, schemaPath, options.buf, outDb.buf));
const dbPtr = readPtrOut(outDb);
```

### Complete allocUint64Out / readUint64Out Pair
```typescript
// Source: Verified against Deno 2.7.10 runtime
function allocUint64Out(): Allocation {
  const buf = new Uint8Array(8);
  return { ptr: Deno.UnsafePointer.of(buf), buf };
}

function readUint64Out(alloc: Allocation): number {
  const dv = new DataView(alloc.buf.buffer);
  return Number(dv.getBigUint64(0, true));
}
```

### Complete makeDefaultOptions
```typescript
// Source: Verified struct layout against include/quiver/c/options.h
// quiver_database_options_t: { int read_only; quiver_log_level_t console_level; }
// Total size: 8 bytes (two 4-byte int32 fields)
function makeDefaultOptions(): Allocation {
  const buf = new Uint8Array(8);
  const dv = new DataView(buf.buffer);
  dv.setInt32(0, 0, true);  // read_only = false
  dv.setInt32(4, 1, true);  // console_level = QUIVER_LOG_INFO
  return { ptr: Deno.UnsafePointer.of(buf), buf };
}
```

### Complete allocNativeString
```typescript
// Source: Verified against Deno 2.7.10 runtime
const encoder = new TextEncoder();

function allocNativeString(str: string): Allocation {
  const buf = encoder.encode(str + "\0");
  return { ptr: Deno.UnsafePointer.of(buf), buf };
}
```

### Complete allocNativeStringArray
```typescript
// Source: Verified against Deno 2.7.10 runtime
function allocNativeStringArray(strings: string[]): { table: Allocation; keepalive: Allocation[] } {
  const strAllocs = strings.map((s) => allocNativeString(s));
  const table = allocNativePtrTable(strAllocs.map((a) => a.ptr));
  return { table, keepalive: strAllocs };
}
```

### Complete nativeAddress
```typescript
// Source: Verified against Deno 2.7.10 runtime
function nativeAddress(ptr: Deno.PointerValue): bigint {
  return ptr ? Deno.UnsafePointer.value(ptr) : 0n;
}
```

### Complete decodeStringFromBuf
```typescript
// Source: Verified against Deno 2.7.10 runtime
function decodeStringFromBuf(alloc: Allocation): string {
  const ptr = readPtrOut(alloc);
  if (!ptr) return "";
  return new Deno.UnsafePointerView(ptr).getCString();
}
```

## State of the Art

| Old Approach (koffi) | Current Approach (Deno FFI) | When Changed | Impact |
|----------------------|---------------------------|--------------|--------|
| `koffi.alloc(type, count)` | `new Uint8Array(count * size)` | Deno FFI stable since Deno 1.30+ | No external dependency for memory allocation |
| `koffi.encode(ptr, offset, type, value)` | `DataView.setBigInt64/setFloat64(offset, value, true)` | Web standard | Type-safe, no string-based type dispatch |
| `koffi.decode(ptr, type)` | `Deno.UnsafePointerView` methods | Deno FFI stable | Built-in pointer view API |
| `koffi.address(ptr)` | `Deno.UnsafePointer.value(ptr)` | Deno FFI stable | Native BigInt return |
| `Buffer.from(str + "\0")` | `TextEncoder.encode(str + "\0")` | Web standard | No Node.js Buffer dependency |
| `koffi.decode(buf, "void *")` | `DataView.getBigUint64() + UnsafePointer.create()` | Deno FFI stable | Two-step but fully type-safe |

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | `allocPtrOut` should use `Uint8Array(8)` + `DataView` rather than `BigUint64Array(1)` for consistency with other allocation functions | Architecture Patterns | Low -- both work identically; BigUint64Array would be 2 lines shorter but inconsistent with the struct-building pattern |
| A2 | `nativeAddress` should be kept as a wrapper around `Deno.UnsafePointer.value()` rather than removed | Pitfall 6 | Low -- removing it just means callers call Deno API directly; wrapper adds uniformity |
| A3 | `allocUint64Out`/`readUint64Out` should remain as separate functions rather than being folded into allocPtrOut/readPtrOut | Architecture Patterns | Low -- they serve the same byte-level purpose but communicate different intent (pointer vs count) |
| A4 | Phase 2 changes will cause TypeScript compilation errors in domain modules (database.ts, query.ts, etc.) that are deferred to Phase 3 | Pitfall 5 | Medium -- if CI runs on every commit, Phase 2 alone will fail type checks. Plan should note this explicitly |

## Open Questions

1. **Should `allocPtrOut` return `{ ptr, buf }` or just `Uint8Array`?**
   - What we know: Decision D-03 says "all alloc functions return `{ ptr, buf }`". The callers currently pass `Buffer` directly to FFI calls (e.g., `lib.quiver_database_from_schema(..., outDb)`), but with Deno FFI `"buffer"` params, we pass `Uint8Array` (the buf).
   - What's unclear: Whether callers should pass `alloc.buf` (the Uint8Array directly, since Deno accepts TypedArray for `"buffer"` params) or `alloc.ptr` (the pointer, for `"pointer"` params). Looking at loader.ts, out-parameter slots use `P` (pointer) type, not `BUF` (buffer).
   - Recommendation: Since out-parameters are declared as `P` (pointer) in loader.ts, callers need the `ptr` from `allocPtrOut`. But since `Deno.UnsafePointer.of()` on a Uint8Array gives the pointer, and passing the Uint8Array as `"buffer"` type also works, it depends on the symbol definition. The current loader.ts uses `P` for out-params, so callers pass `alloc.buf` as the buffer argument. **Resolution:** Deno FFI accepts both `TypedArray` and `PointerObject` for `"pointer"` parameters (the docs say buffer type "accepts TypedArrays"). So passing `alloc.buf` to a `P`-typed parameter works. Callers should pass `alloc.buf` for simplicity and GC safety. This means `allocPtrOut` technically only needs to return `Uint8Array`, but returning `{ ptr, buf }` maintains uniformity with D-03.

2. **Compilation breakage during Phase 2**
   - What we know: ffi-helpers.ts return types change (Buffer -> Allocation, NativePointer -> Allocation). All callers in domain modules will have type errors.
   - What's unclear: Whether the plan should include stub/shim changes to domain modules to maintain compilability.
   - Recommendation: Accept compilation breakage. Phase 2 is ffi-helpers.ts only. Document that TypeScript will not compile until Phase 3 updates the callers. This matches the phase boundary defined in CONTEXT.md.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| Deno runtime | All FFI operations | Yes | 2.7.10 | -- |
| Deno.UnsafePointer | Pointer manipulation | Yes | Stable API | -- |
| Deno.UnsafePointerView | String/value reading from pointers | Yes | Stable API | -- |
| TextEncoder | String encoding | Yes | Web standard | -- |
| DataView | Typed value writes | Yes | Web standard | -- |

**Missing dependencies:** None.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Deno.test (Deno 2.7.10 built-in) |
| Config file | None -- Deno.test requires no config file |
| Quick run command | `deno test bindings/js/test/ --allow-ffi --allow-read --allow-write --allow-env` |
| Full suite command | `deno test bindings/js/test/ --allow-ffi --allow-read --allow-write --allow-env` |

### Phase Requirements to Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| MARSH-01 | allocPtrOut/readPtrOut produce valid pointer via BigUint64/DataView | unit | Cannot run standalone -- requires C library loaded | No test file yet (Phase 6 scope) |
| MARSH-03 | toCString produces null-terminated UTF-8; decodeStringFromBuf reads C string | unit | Cannot run standalone -- decodeStringFromBuf requires C-allocated pointer | No test file yet |
| MARSH-04 | allocNativeInt64/Float64/String/StringArray/PtrTable produce valid native memory | unit | Cannot run standalone -- requires FFI call to validate | No test file yet |

### Sampling Rate
- **Per task commit:** Manual verification that ffi-helpers.ts has zero koffi imports
- **Per wave merge:** N/A (single-file phase)
- **Phase gate:** Full JS test suite green after Phase 3-4 (ffi-helpers alone cannot be tested without domain modules calling through to C)

### Wave 0 Gaps
- No unit tests exist for ffi-helpers.ts functions in isolation (they are integration-tested through domain modules)
- Standalone unit tests for marshaling functions would require a mock native library or direct Deno FFI calls -- deferred to Phase 6 (TEST-01, TEST-02)

## Security Domain

### Applicable ASVS Categories

| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | No | N/A |
| V3 Session Management | No | N/A |
| V4 Access Control | No | N/A -- FFI requires --allow-ffi permission flag |
| V5 Input Validation | Yes (string encoding) | TextEncoder handles UTF-8; null terminator manually appended |
| V6 Cryptography | No | N/A |

### Known Threat Patterns for FFI Marshaling

| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Buffer overread from missing null terminator | Information Disclosure | Always append `"\0"` to encoded strings before FFI call |
| Dangling pointer from GC'd buffer | Denial of Service / Elevation | Return `{ ptr, buf }` from all alloc functions; callers hold buf reference |
| Use-after-free from premature buffer collection | Denial of Service | Scope-based keepalive -- buffers survive function scope via local variables |

## Sources

### Primary (HIGH confidence)
- [Deno 2.7.10 runtime] - `deno --version` confirmed on target machine; all API patterns verified with `deno eval`
- [Deno.UnsafePointerView API](https://docs.deno.com/api/deno/~/Deno.UnsafePointerView) - Official API reference for getCString, getBigInt64, getPointer, etc.
- [Deno.UnsafePointer API](https://docs.deno.com/api/deno/~/Deno.UnsafePointer) - Official API reference for of(), create(), value(), equals(), offset()
- [Denonomicon Pointers](https://denonomicon.deno.dev/types/pointers) - Comprehensive pointer type reference with examples
- [Denonomicon Philosophy of FFI](https://denonomicon.deno.dev/philosophy-of-ffi) - Memory ownership model, GC implications, buffer lifecycle
- [include/quiver/c/options.h] - Struct layout for quiver_database_options_t (2 x int32, 8 bytes total)
- [include/quiver/c/common.h] - quiver_get_last_error returns const char*, quiver_error_t enum

### Secondary (MEDIUM confidence)
- [Deno FFI Fundamentals](https://docs.deno.com/runtime/fundamentals/ffi/) - buffer vs pointer parameter semantics
- [GitHub Issue #19200](https://github.com/denoland/deno/issues/19200) - char** out-parameter pattern with BigUint64Array + UnsafePointer.create
- [Ian Bull: Struct Packing with Deno FFI](https://ianbull.com/posts/deno-ffi-struct-packing/) - DataView struct field extraction patterns

### Tertiary (LOW confidence)
None -- all claims verified against runtime or official documentation.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - All APIs are Deno built-ins or Web standards, verified on target machine
- Architecture: HIGH - Patterns verified with `deno eval` on target Deno 2.7.10; struct layout verified against C headers
- Pitfalls: HIGH - GC/dangling pointer risks documented in official Denonomicon; endianness verified empirically

**Research date:** 2026-04-17
**Valid until:** 2026-07-17 (stable Deno APIs, unlikely to change)
