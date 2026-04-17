# Phase 3: Array Decoding & Domain Helpers - Research

**Researched:** 2026-04-17
**Domain:** Deno FFI array decoding (UnsafePointerView), domain module koffi removal (query.ts, csv.ts, time-series.ts)
**Confidence:** HIGH

## Summary

Phase 3 implements the 5 array decoder stubs left by Phase 2 and removes all remaining `koffi` imports from three domain modules: query.ts, csv.ts, and time-series.ts. The array decoders (decodeInt64Array, decodeFloat64Array, decodeUint64Array, decodeStringArray, decodePtrArray) are the most critical -- they are consumed by read.ts (heavily), time-series.ts, and composites.ts. The Deno FFI API provides `Deno.UnsafePointerView` with `getArrayBuffer()`, `getBigInt64()`, `getFloat64()`, `getPointer()`, and `getCString()` -- all verified on the target machine (Deno 2.7.10). Each decoder creates an `UnsafePointerView` from the native pointer and reads values either via `getArrayBuffer()` + typed array view (bulk, zero-copy) or via offset-based reads.

The three domain modules have distinct koffi dependencies: query.ts uses `koffi` only as an unused import (its `marshalParams` already uses Phase 2 allocation helpers but the keepalive pattern uses `NativePointer` which must become `Allocation`); csv.ts uses `koffi.alloc`/`koffi.encode` for the uint64 entry counts array and `Buffer.alloc`/`Buffer.writeBigUInt64LE` for the 56-byte CSV options struct; time-series.ts uses `koffi.decode` for reading int[] column types and char** nullable string arrays. All three must also update their `NativePointer` type usage to `Allocation` to match Phase 2's Allocation return type.

**Primary recommendation:** Implement all 5 array decoders using `UnsafePointerView.getArrayBuffer()` for numeric arrays and `UnsafePointerView.getPointer()` + `getCString()` for string/pointer arrays. Then update each domain module by replacing koffi calls with equivalent Deno FFI primitives and updating keepalive arrays from `NativePointer[]` to `Allocation[]`.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| MARSH-02 | Integer/float array decoding works using Deno.UnsafePointerView instead of koffi.decode | Verified: `UnsafePointerView.getArrayBuffer(byteLength)` returns ArrayBuffer from which `BigInt64Array`/`Float64Array`/`BigUint64Array` views can be created; `getPointer(offset)` reads pointer values for char** arrays; all tested on Deno 2.7.10 |
</phase_requirements>

## Project Constraints (from CLAUDE.md)

- **Intelligence in C++ layer:** Bindings remain thin -- ffi-helpers.ts and domain modules do marshaling only, no business logic.
- **Error messages from C/C++ layer:** Bindings retrieve and surface errors, never craft their own.
- **Ownership explicit:** JS side must hold buffer references (Allocation.buf) to prevent GC during calls.
- **Homogeneity:** API surface should feel uniform -- all alloc functions use the `Allocation` type.
- **Self-Updating:** CLAUDE.md must be updated if binding patterns change.
- **Clean code over defensive code:** No excessive null checks; assume callers obey contracts.

## Standard Stack

### Core
| Library/API | Version | Purpose | Why Standard |
|-------------|---------|---------|--------------|
| `Deno.UnsafePointerView` | Deno 2.7.10 (stable) | Reading typed values and arrays from native pointers | Built-in Deno FFI API. Provides getArrayBuffer, getBigInt64, getFloat64, getPointer, getCString, getInt32 [VERIFIED: `deno eval --unstable-ffi` tests on target machine] |
| `Deno.UnsafePointer` | Deno 2.7.10 (stable) | Pointer creation, comparison, address extraction | Built-in, used by Phase 2 allocation pattern [VERIFIED: Phase 2 runtime tests] |
| `DataView` | Web standard | Typed value writes at byte offsets in Uint8Array buffers | Standard API for little-endian typed access. Used for CSV options struct building [VERIFIED: Phase 2] |
| `Uint8Array` | Web standard | Buffer allocation for native memory (replaces Node.js Buffer) | Deno FFI `"buffer"` parameter type accepts TypedArray [VERIFIED: Phase 2] |
| `TextEncoder` | Web standard | UTF-8 string encoding for C string parameters | Already used in Phase 2 pattern [VERIFIED: Phase 2] |

### Supporting
No external libraries needed.

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `getArrayBuffer()` + typed array view | Per-element `getBigInt64(offset)` loop | getArrayBuffer is zero-copy and creates a single typed array view; per-element loop requires N individual calls and manual array construction. getArrayBuffer is faster for bulk reads |
| `getPointer(offset)` for pointer arrays | `getArrayBuffer()` + `BigUint64Array` + `Deno.UnsafePointer.create()` | getPointer directly returns `PointerValue` with correct null semantics (0n becomes null); the BigUint64Array approach requires manual null checking and create() calls |
| `Uint8Array` + `DataView` for CSV struct | Dedicated struct builder utility | Inline DataView is consistent with Phase 2's makeDefaultOptions pattern (D-07). No utility needed for a single struct type |

## Architecture Patterns

### Array Decoder Pattern (getArrayBuffer approach)

For numeric arrays (int64, float64, uint64), use `getArrayBuffer` to get a zero-copy ArrayBuffer backed by native memory, then create a typed array view:

```typescript
// Source: Verified on Deno 2.7.10 via deno eval --unstable-ffi
export function decodeFloat64Array(ptr: Deno.PointerValue, count: number): number[] {
  if (!ptr || count === 0) return [];
  const view = new Deno.UnsafePointerView(ptr);
  const ab = view.getArrayBuffer(count * 8);
  const f64 = new Float64Array(ab);
  return Array.from(f64);
}

export function decodeInt64Array(ptr: Deno.PointerValue, count: number): number[] {
  if (!ptr || count === 0) return [];
  const view = new Deno.UnsafePointerView(ptr);
  const ab = view.getArrayBuffer(count * 8);
  const i64 = new BigInt64Array(ab);
  return Array.from(i64, (v) => Number(v));
}

export function decodeUint64Array(ptr: Deno.PointerValue, count: number): number[] {
  if (!ptr || count === 0) return [];
  const view = new Deno.UnsafePointerView(ptr);
  const ab = view.getArrayBuffer(count * 8);
  const u64 = new BigUint64Array(ab);
  return Array.from(u64, (v) => Number(v));
}
```

[VERIFIED: `deno eval --unstable-ffi` with Uint8Array-backed pointers confirmed correct BigInt64Array/Float64Array/BigUint64Array reading]

### Pointer Array Decoder Pattern (getPointer approach)

For pointer arrays (void**), use `getPointer(offset)` at each position. This handles null pointers correctly (returns `null` for 0n addresses):

```typescript
// Source: Verified on Deno 2.7.10 via deno eval --unstable-ffi
export function decodePtrArray(ptr: Deno.PointerValue, count: number): Deno.PointerValue[] {
  if (!ptr || count === 0) return [];
  const view = new Deno.UnsafePointerView(ptr);
  const result: Deno.PointerValue[] = new Array(count);
  for (let i = 0; i < count; i++) {
    result[i] = view.getPointer(i * 8);
  }
  return result;
}
```

[VERIFIED: getPointer at offsets correctly returns null for 0n addresses, tested with interleaved null/non-null pointers]

### String Array Decoder Pattern (getPointer + getCString)

For string arrays (char**), compose decodePtrArray with getCString:

```typescript
// Source: Verified on Deno 2.7.10 via deno eval --unstable-ffi
export function decodeStringArray(ptr: Deno.PointerValue, count: number): string[] {
  if (!ptr || count === 0) return [];
  const view = new Deno.UnsafePointerView(ptr);
  const result: string[] = new Array(count);
  for (let i = 0; i < count; i++) {
    const strPtr = view.getPointer(i * 8);
    result[i] = strPtr ? new Deno.UnsafePointerView(strPtr).getCString() : "";
  }
  return result;
}
```

[VERIFIED: Correctly decodes char** with getPointer + getCString, tested with 3 strings]

### Int32 Array Decoding Pattern (for column types)

time-series.ts needs to decode an `int*` array (column types). This replaces `koffi.decode(typesPtr, koffi.array("int", colCount))`:

```typescript
// Source: Verified on Deno 2.7.10 via deno eval --unstable-ffi
function decodeInt32Array(ptr: Deno.PointerValue, count: number): number[] {
  if (!ptr || count === 0) return [];
  const view = new Deno.UnsafePointerView(ptr);
  const ab = view.getArrayBuffer(count * 4);
  const i32 = new Int32Array(ab);
  return Array.from(i32);
}
```

This can be a local function in time-series.ts (not exported from ffi-helpers) since it is only used there. [VERIFIED: Int32Array from getArrayBuffer correctly reads int values]

### Nullable String Array Pattern (for time series files paths)

time-series.ts `readTimeSeriesFiles` decodes a `char**` where entries can be NULL (representing missing file paths). The C API sets `(*out_paths)[i] = nullptr` for absent paths:

```typescript
// Source: Verified on Deno 2.7.10 via deno eval --unstable-ffi
function decodeNullableStringArray(ptr: Deno.PointerValue, count: number): (string | null)[] {
  if (!ptr || count === 0) return [];
  const view = new Deno.UnsafePointerView(ptr);
  const result: (string | null)[] = new Array(count);
  for (let i = 0; i < count; i++) {
    const strPtr = view.getPointer(i * 8);
    result[i] = strPtr ? new Deno.UnsafePointerView(strPtr).getCString() : null;
  }
  return result;
}
```

[VERIFIED: getPointer returns null for 0n addresses, enabling null detection in char** arrays]

### CSV Options Struct Building Pattern (Uint8Array replaces Buffer)

csv.ts must replace `Buffer.alloc(56)` + `Buffer.writeBigUInt64LE()` with `Uint8Array(56)` + `DataView.setBigUint64()`, matching Phase 2's D-07 pattern:

```typescript
// Source: Verified struct layout against include/quiver/c/options.h
// quiver_csv_options_t: 7 fields, all pointer-sized (8 bytes each) = 56 bytes
// Layout:
//   offset  0: const char* date_time_format       (8 bytes)
//   offset  8: const char* const* enum_attribute_names  (8 bytes)
//   offset 16: const char* const* enum_locale_names     (8 bytes)
//   offset 24: const size_t* enum_entry_counts          (8 bytes)
//   offset 32: const char* const* enum_labels           (8 bytes)
//   offset 40: const int64_t* enum_values               (8 bytes)
//   offset 48: size_t enum_group_count                  (8 bytes)
function buildCsvOptionsBuffer(options?: CsvOptions): [Allocation, Allocation[]] {
  const buf = new Uint8Array(56);
  const dv = new DataView(buf.buffer);
  const keepalive: Allocation[] = [];

  const dtfStr = allocNativeString(options?.dateTimeFormat ?? "");
  keepalive.push(dtfStr);
  dv.setBigUint64(0, nativeAddress(dtfStr.ptr), true);

  // ... enum labels fields at offsets 8-48 ...

  const alloc: Allocation = { ptr: Deno.UnsafePointer.of(buf)!, buf };
  return [alloc, keepalive];
}
```

### Keepalive Pattern Update (NativePointer[] to Allocation[])

Phase 2 changed all allocation functions to return `Allocation` instead of `NativePointer`/`Buffer`. Domain modules that push allocation results into keepalive arrays must update the array type:

**Before (koffi era):**
```typescript
const keepalive: NativePointer[] = [];
const native = allocNativeInt64([p]);
keepalive.push(native);  // native was NativePointer
values[i] = nativeAddress(native);  // nativeAddress(NativePointer)
```

**After (Deno era):**
```typescript
const keepalive: Allocation[] = [];
const native = allocNativeInt64([p]);
keepalive.push(native);  // native is Allocation
values[i] = nativeAddress(native.ptr);  // nativeAddress(Allocation.ptr)
```

### Anti-Patterns to Avoid
- **Using `Buffer` (Node.js):** csv.ts uses `Buffer.alloc(56)`. Must change to `new Uint8Array(56)` with `DataView` for field writes. `Buffer.writeBigUInt64LE()` becomes `DataView.setBigUint64(offset, value, true)`.
- **Using `koffi.alloc`/`koffi.encode`:** csv.ts uses `koffi.alloc("uint64_t", groupCount)` and `koffi.encode(entryCounts, i * 8, "uint64_t", ...)`. Must change to `Uint8Array` + `DataView.setBigUint64()`.
- **Using `koffi.decode` for type arrays:** time-series.ts uses `koffi.decode(typesPtr, koffi.array("int", colCount))`. Must change to `UnsafePointerView.getArrayBuffer()` + `Int32Array`.
- **Mixing NativePointer and Allocation:** After Phase 2, allocation functions return `Allocation`, not `NativePointer`. Keepalive arrays must store `Allocation` objects, and `nativeAddress()` calls must pass `.ptr` not the whole Allocation.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Numeric array reading | Manual per-element DataView reads in a loop | `UnsafePointerView.getArrayBuffer()` + typed array view | Zero-copy, single operation, handles byte alignment automatically |
| C string reading | Manual byte scanning for null terminator | `UnsafePointerView.getCString()` | Native implementation handles UTF-8 correctly, faster than JS loop |
| Pointer array reading | getArrayBuffer + BigUint64Array + manual create() | `UnsafePointerView.getPointer(offset)` | Returns PointerValue directly with correct null semantics |
| Struct field writes | Manual byte manipulation | `DataView.setBigUint64(offset, value, true)` | Type-safe, handles endianness correctly |

## Common Pitfalls

### Pitfall 1: getArrayBuffer Returns a View of Native Memory, Not a Copy
**What goes wrong:** The ArrayBuffer from `getArrayBuffer()` is backed by the same native memory. If the C API frees that memory (via a `free_*` function), the ArrayBuffer becomes invalid (use-after-free).
**Why it happens:** `getArrayBuffer` is zero-copy for performance.
**How to avoid:** Always copy data out of the typed array view (via `Array.from()`) before calling the corresponding `free_*` function. The current pattern already does this: `const result = decodeInt64Array(ptr, count); lib.quiver_database_free_integer_array(ptr);` -- the decode happens before free.
**Warning signs:** Corrupted array values that appear after a free call.

### Pitfall 2: BigInt-to-Number Precision Loss for Large int64 Values
**What goes wrong:** `Number(bigint)` loses precision for values outside the safe integer range (above 2^53 - 1 or below -(2^53 - 1)).
**Why it happens:** JavaScript `number` is a 64-bit IEEE 754 float with only 53 bits of integer precision.
**How to avoid:** For this codebase, `Number()` conversion is acceptable because: (a) element IDs are sequential integers starting from 1, (b) vector indices are small, (c) count values are bounded by practical data sizes. The existing code already makes this conversion. If BigInt precision is ever needed, the public API (types.ts) already has `bigint` in its `ScalarValue` union.
**Warning signs:** ID values that differ slightly from expected (e.g., 9007199254740993 becomes 9007199254740992).

### Pitfall 3: nativeAddress Receives Allocation Instead of PointerValue
**What goes wrong:** After Phase 2, `allocNativeInt64([p])` returns `Allocation`, not `NativePointer`. Code like `nativeAddress(native)` (where `native` is now an Allocation) passes an object where a pointer is expected.
**Why it happens:** Phase 2 changed return types but domain modules were not updated.
**How to avoid:** In every domain module, update `nativeAddress(native)` to `nativeAddress(native.ptr)`. Update keepalive array type from `NativePointer[]` to `Allocation[]`.
**Warning signs:** TypeScript type errors: "Argument of type 'Allocation' is not assignable to parameter of type 'PointerValue'".

### Pitfall 4: Buffer.alloc vs Uint8Array for Struct Building
**What goes wrong:** csv.ts uses `Buffer.alloc(56)` which is a Node.js API. In Deno, `Buffer` is available via `node:buffer` compatibility layer but should not be used.
**Why it happens:** Original code was written for Node.js + koffi.
**How to avoid:** Replace `Buffer.alloc(N)` with `new Uint8Array(N)`. Replace `buf.writeBigUInt64LE(value, offset)` with `new DataView(buf.buffer).setBigUint64(offset, value, true)`. This matches Phase 2's `makeDefaultOptions` pattern (D-07).
**Warning signs:** `Buffer is not defined` error in Deno (if `node:buffer` not imported), or unnecessary Node.js compat layer dependency.

### Pitfall 5: koffi.alloc for uint64_t Array in CSV Options
**What goes wrong:** csv.ts line 62-64 uses `koffi.alloc("uint64_t", groupCount)` and `koffi.encode(entryCounts, i * 8, "uint64_t", BigInt(...))` to create a native uint64 array.
**Why it happens:** koffi provided type-string-based allocation that Deno FFI does not have.
**How to avoid:** Use the same `Uint8Array` + `DataView.setBigUint64()` pattern from Phase 2:
```typescript
const entryCountsBuf = new Uint8Array(groupCount * 8);
const entryCountsDv = new DataView(entryCountsBuf.buffer);
for (let i = 0; i < groupCount; i++) {
  entryCountsDv.setBigUint64(i * 8, BigInt(groupEntryCounts[i]), true);
}
const entryCounts: Allocation = {
  ptr: Deno.UnsafePointer.of(entryCountsBuf)!,
  buf: entryCountsBuf,
};
```

### Pitfall 6: Unused koffi Import in query.ts
**What goes wrong:** query.ts line 1 has `import koffi from "koffi"` but none of its code actually calls any koffi function. The import is vestigial.
**Why it happens:** The marshalParams function was rewritten to use Phase 2 helpers, but the import was not removed.
**How to avoid:** Simply remove the `import koffi from "koffi"` line. No functional changes needed beyond the NativePointer -> Allocation type update.
**Warning signs:** `Cannot find module "koffi"` runtime error in Deno.

## Code Examples

### Complete decodeInt64Array
```typescript
// Source: Verified on Deno 2.7.10 -- BigInt64Array from getArrayBuffer reads correct values
export function decodeInt64Array(ptr: Deno.PointerValue, count: number): number[] {
  if (!ptr || count === 0) return [];
  const view = new Deno.UnsafePointerView(ptr);
  const ab = view.getArrayBuffer(count * 8);
  const i64 = new BigInt64Array(ab);
  return Array.from(i64, (v) => Number(v));
}
```

### Complete decodeFloat64Array
```typescript
// Source: Verified on Deno 2.7.10 -- Float64Array from getArrayBuffer reads correct values
export function decodeFloat64Array(ptr: Deno.PointerValue, count: number): number[] {
  if (!ptr || count === 0) return [];
  const view = new Deno.UnsafePointerView(ptr);
  const ab = view.getArrayBuffer(count * 8);
  const f64 = new Float64Array(ab);
  return Array.from(f64);
}
```

### Complete decodeUint64Array
```typescript
// Source: Verified on Deno 2.7.10 -- BigUint64Array from getArrayBuffer reads correct values
export function decodeUint64Array(ptr: Deno.PointerValue, count: number): number[] {
  if (!ptr || count === 0) return [];
  const view = new Deno.UnsafePointerView(ptr);
  const ab = view.getArrayBuffer(count * 8);
  const u64 = new BigUint64Array(ab);
  return Array.from(u64, (v) => Number(v));
}
```

### Complete decodePtrArray
```typescript
// Source: Verified on Deno 2.7.10 -- getPointer at offsets returns correct PointerValue or null
export function decodePtrArray(ptr: Deno.PointerValue, count: number): Deno.PointerValue[] {
  if (!ptr || count === 0) return [];
  const view = new Deno.UnsafePointerView(ptr);
  const result: Deno.PointerValue[] = new Array(count);
  for (let i = 0; i < count; i++) {
    result[i] = view.getPointer(i * 8);
  }
  return result;
}
```

### Complete decodeStringArray
```typescript
// Source: Verified on Deno 2.7.10 -- getPointer + getCString decodes char** correctly
export function decodeStringArray(ptr: Deno.PointerValue, count: number): string[] {
  if (!ptr || count === 0) return [];
  const view = new Deno.UnsafePointerView(ptr);
  const result: string[] = new Array(count);
  for (let i = 0; i < count; i++) {
    const strPtr = view.getPointer(i * 8);
    result[i] = strPtr ? new Deno.UnsafePointerView(strPtr).getCString() : "";
  }
  return result;
}
```

### query.ts marshalParams Update
```typescript
// Current (broken -- NativePointer type, keepalive stores Allocation as NativePointer):
function marshalParams(params: QueryParam[]): { types: Int32Array; values: BigInt64Array; _keepalive: NativePointer[] } {
  const keepalive: NativePointer[] = [];
  // ...
  const native = allocNativeInt64([p]);
  keepalive.push(native);  // ERROR: native is Allocation, not NativePointer
  values[i] = nativeAddress(native);  // ERROR: native is Allocation, not PointerValue
}

// Fixed (Allocation type, .ptr access):
function marshalParams(params: QueryParam[]): { types: Int32Array; values: BigInt64Array; _keepalive: Allocation[] } {
  const keepalive: Allocation[] = [];
  // ...
  const native = allocNativeInt64([p]);
  keepalive.push(native);
  values[i] = nativeAddress(native.ptr);
}
```

### csv.ts buildCsvOptionsBuffer Update
```typescript
// Current (koffi + Buffer):
function buildCsvOptionsBuffer(options?: CsvOptions): [Buffer, NativePointer[]] {
  const buf = Buffer.alloc(56);
  const keepalive: NativePointer[] = [];
  const dtfStr = allocNativeString(options?.dateTimeFormat ?? "");
  keepalive.push(dtfStr);
  buf.writeBigUInt64LE(nativeAddress(dtfStr), 0);
  // ...
  const entryCounts = koffi.alloc("uint64_t", groupCount);
  koffi.encode(entryCounts, i * 8, "uint64_t", BigInt(groupEntryCounts[i]));
  // ...
  return [buf, keepalive];
}

// Fixed (Uint8Array + DataView + Allocation):
function buildCsvOptionsBuffer(options?: CsvOptions): [Allocation, Allocation[]] {
  const buf = new Uint8Array(56);
  const dv = new DataView(buf.buffer);
  const keepalive: Allocation[] = [];
  const dtfStr = allocNativeString(options?.dateTimeFormat ?? "");
  keepalive.push(dtfStr);
  dv.setBigUint64(0, nativeAddress(dtfStr.ptr), true);
  // ...
  const entryCountsBuf = new Uint8Array(groupCount * 8);
  const entryCountsDv = new DataView(entryCountsBuf.buffer);
  for (let i = 0; i < groupCount; i++) {
    entryCountsDv.setBigUint64(i * 8, BigInt(groupEntryCounts[i]), true);
  }
  const entryCounts: Allocation = { ptr: Deno.UnsafePointer.of(entryCountsBuf)!, buf: entryCountsBuf };
  keepalive.push(entryCounts);
  // ...
  const alloc: Allocation = { ptr: Deno.UnsafePointer.of(buf)!, buf };
  return [alloc, keepalive];
}
```

### time-series.ts Column Types Decoding Update
```typescript
// Current (koffi):
const types = koffi.decode(typesPtr, koffi.array("int", colCount)) as number[];

// Fixed (UnsafePointerView):
const typesView = new Deno.UnsafePointerView(typesPtr!);
const typesAb = typesView.getArrayBuffer(colCount * 4);
const types = Array.from(new Int32Array(typesAb));
```

### time-series.ts Nullable Paths Decoding Update
```typescript
// Current (koffi):
const paths = koffi.decode(pathsPtr, koffi.array("str", count)) as (string | null)[];

// Fixed (UnsafePointerView):
const pathsView = new Deno.UnsafePointerView(pathsPtr!);
const paths: (string | null)[] = new Array(count);
for (let i = 0; i < count; i++) {
  const strPtr = pathsView.getPointer(i * 8);
  paths[i] = strPtr ? new Deno.UnsafePointerView(strPtr).getCString() : null;
}
```

## Detailed Koffi Usage Inventory

Exact inventory of every koffi-related line in the three target files that must change:

### query.ts (3 changes)
| Line | Current Code | Replacement | Category |
|------|-------------|-------------|----------|
| 1 | `import koffi from "koffi"` | Remove entirely | Unused import |
| 5 | `import type { NativePointer } from "./loader.js"` | `import type { Allocation } from "./types.ts"` | Type import |
| 22 | `_keepalive: NativePointer[]` | `_keepalive: Allocation[]` | Return type |
| 26 | `const keepalive: NativePointer[] = []` | `const keepalive: Allocation[] = []` | Variable type |
| 37 | `keepalive.push(native); values[i] = nativeAddress(native)` | `keepalive.push(native); values[i] = nativeAddress(native.ptr)` | Allocation.ptr access |
| 42 | `keepalive.push(native); values[i] = nativeAddress(native)` | `keepalive.push(native); values[i] = nativeAddress(native.ptr)` | Allocation.ptr access |
| 48 | `keepalive.push(native); values[i] = nativeAddress(native)` | `keepalive.push(native); values[i] = nativeAddress(native.ptr)` | Allocation.ptr access |

### csv.ts (14 changes)
| Line | Current Code | Replacement | Category |
|------|-------------|-------------|----------|
| 1 | `import koffi from "koffi"` | Remove entirely | koffi removal |
| 5 | `import type { NativePointer } from "./loader.js"` | `import type { Allocation } from "./types.ts"` | Type import |
| 24 | `function buildCsvOptionsBuffer(options?: CsvOptions): [Buffer, NativePointer[]]` | `: [Allocation, Allocation[]]` | Return type |
| 25 | `const buf = Buffer.alloc(56)` | `const buf = new Uint8Array(56); const dv = new DataView(buf.buffer)` | Buffer -> Uint8Array |
| 26 | `const keepalive: NativePointer[] = []` | `const keepalive: Allocation[] = []` | Variable type |
| 29-30 | `keepalive.push(dtfStr); buf.writeBigUInt64LE(nativeAddress(dtfStr), 0)` | `keepalive.push(dtfStr); dv.setBigUint64(0, nativeAddress(dtfStr.ptr), true)` | Buffer.write -> DataView + .ptr |
| 57 | `keepalive.push(attrTable, ...attrPtrs)` | `keepalive.push(attrTable, ...attrPtrs)` | Already correct (allocNativeStringArray returns { table: Allocation, keepalive: Allocation[] }) |
| 62-65 | `koffi.alloc("uint64_t", groupCount)` + `koffi.encode(...)` | `Uint8Array(groupCount * 8)` + `DataView.setBigUint64()` | koffi.alloc -> Uint8Array+DataView |
| 74-79 | `buf.writeBigUInt64LE(nativeAddress(...), offset)` x6 | `dv.setBigUint64(offset, nativeAddress(...ptr), true)` x6 | Buffer.write -> DataView + .ptr |
| 81 | `return [buf, keepalive]` | `return [{ ptr: Deno.UnsafePointer.of(buf)!, buf }, keepalive]` | Return Allocation |

### time-series.ts (10 changes)
| Line | Current Code | Replacement | Category |
|------|-------------|-------------|----------|
| 1 | `import koffi from "koffi"` | Remove entirely | koffi removal |
| 19 | `import type { NativePointer } from "./loader.js"` | `import type { Allocation } from "./types.ts"` | Type import |
| 61 | `koffi.decode(typesPtr, koffi.array("int", colCount))` | `UnsafePointerView.getArrayBuffer(colCount * 4)` + `Int32Array` | koffi.decode -> UnsafePointerView |
| 96 | `const keepalive: NativePointer[] = []` | `const keepalive: Allocation[] = []` | Variable type |
| 100-101 | `keepalive.push(namesTable, ...namesPtrs)` | Same (already Allocation from allocNativeStringArray) | Type alignment |
| 105 | `const dataPtrs: NativePointer[] = []` | `const dataPtrs: Deno.PointerValue[] = []` | Type (already was PointerValue-compatible) |
| 117 | `dataPtrs.push(table)` | `dataPtrs.push(table.ptr)` | Allocation.ptr access |
| 124/129 | `keepalive.push(p); dataPtrs.push(p)` | `keepalive.push(p); dataPtrs.push(p.ptr)` | Allocation.ptr access |
| 134-135 | `const dataTable = allocNativePtrTable(dataPtrs); keepalive.push(dataTable)` | Same but dataPtrs is now PointerValue[] | Already correct |
| 173 | `koffi.decode(pathsPtr, koffi.array("str", count))` | `UnsafePointerView` + `getPointer` + `getCString` with null handling | koffi.decode -> UnsafePointerView |
| 186-200 | Multiple keepalive/dataPtrs pushes with NativePointer | Update to Allocation | Type alignment |

## State of the Art

| Old Approach (koffi) | Current Approach (Deno FFI) | When Changed | Impact |
|----------------------|---------------------------|--------------|--------|
| `koffi.decode(ptr, koffi.array("int64_t", n))` | `UnsafePointerView.getArrayBuffer(n * 8)` + `BigInt64Array` | Deno FFI stable | Zero-copy read, no external dependency |
| `koffi.decode(ptr, koffi.array("double", n))` | `UnsafePointerView.getArrayBuffer(n * 8)` + `Float64Array` | Deno FFI stable | Zero-copy read |
| `koffi.decode(ptr, "void *")` at offset | `UnsafePointerView.getPointer(offset)` | Deno FFI stable | Direct PointerValue return with null semantics |
| `koffi.decode(ptr, koffi.array("str", n))` | `UnsafePointerView.getPointer(i*8)` + `getCString()` | Deno FFI stable | Per-element decode, handles null entries |
| `koffi.decode(ptr, koffi.array("int", n))` | `UnsafePointerView.getArrayBuffer(n * 4)` + `Int32Array` | Deno FFI stable | Zero-copy read |
| `koffi.alloc("uint64_t", n)` + `koffi.encode(...)` | `new Uint8Array(n * 8)` + `DataView.setBigUint64()` | Phase 2 pattern | Consistent with all other allocations |
| `Buffer.alloc(56)` + `Buffer.writeBigUInt64LE(...)` | `new Uint8Array(56)` + `DataView.setBigUint64(offset, value, true)` | Phase 2 pattern | No Node.js Buffer dependency |

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | `decodeInt32Array` should be a local function in time-series.ts rather than exported from ffi-helpers.ts | Architecture Patterns | Low -- only time-series.ts needs int32 array decoding. If another module needs it later, it can be promoted to ffi-helpers.ts |
| A2 | `decodeStringArray` should return empty string `""` for null char* entries, while time-series.ts `readTimeSeriesFiles` needs a separate nullable variant returning `null` | Architecture Patterns | Low -- the existing `decodeStringArray` in read.ts callers never encounters null entries (C API always provides valid strings for scalar/vector/set reads). The nullable case is specific to file path reads |
| A3 | The CSV options struct is 56 bytes on the target platform (7 pointer-sized fields at 8 bytes each) | Code Examples | Medium -- struct padding could differ on non-64-bit platforms. However, this binding targets Deno on x86-64 only and the existing code already assumes 56 bytes |

## Open Questions (RESOLVED)

1. **Should `decodeInt32Array` be in ffi-helpers.ts or local to time-series.ts?**
   - What we know: Only time-series.ts needs to decode an `int*` array (column types). read.ts does not decode int32 arrays.
   - What's unclear: Whether future domain modules might need int32 array decoding.
   - Recommendation: Keep it local to time-series.ts for now. It can be promoted to ffi-helpers.ts if a second consumer appears. This avoids bloating the shared helper surface.
   - **RESOLVED:** Local to time-series.ts. Single consumer, inline decode.

2. **Should `allocNativeUint64` be added to ffi-helpers.ts?**
   - What we know: csv.ts needs to allocate a `uint64_t[]` array for enum entry counts. Currently uses `koffi.alloc("uint64_t", n)`. Phase 2 added `allocNativeInt64` (signed) and `allocNativeFloat64` but not uint64.
   - What's unclear: Whether to inline the uint64 allocation in csv.ts or add a dedicated function.
   - Recommendation: Inline it in csv.ts since it is the only consumer. The pattern is identical to `allocNativeInt64` but uses `setBigUint64` instead of `setBigInt64`. If more consumers appear, extract to ffi-helpers.ts.
   - **RESOLVED:** Inline in csv.ts. Single consumer, uses `setBigUint64` pattern.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| Deno runtime | All FFI operations | Yes | 2.7.10 | -- |
| Deno.UnsafePointerView | Array decoders, struct reading | Yes | Stable API | -- |
| Deno.UnsafePointerView.getArrayBuffer | Numeric array bulk reading | Yes | Stable API | Per-element offset reads |
| Deno.UnsafePointerView.getPointer | Pointer/string array reading | Yes | Stable API | -- |
| TextEncoder | String encoding | Yes | Web standard | -- |
| DataView | Struct field writes | Yes | Web standard | -- |

**Missing dependencies:** None.

## Sources

### Primary (HIGH confidence)
- [Deno.UnsafePointerView API](https://docs.deno.com/api/deno/~/Deno.UnsafePointerView) - Official API reference: getArrayBuffer, getPointer, getCString, getInt32, getBigInt64, getFloat64 methods and signatures [VERIFIED: runtime tests on Deno 2.7.10]
- [Denonomicon Pointers](https://denonomicon.deno.dev/types/pointers) - getArrayBuffer returns zero-copy ArrayBuffer backed by native memory; pointer reading at offsets [VERIFIED: runtime tests]
- [Deno 2.7.10 runtime] - `deno --version` confirmed on target machine; all array decoder patterns verified with `deno eval --unstable-ffi`
- [include/quiver/c/options.h] - quiver_csv_options_t struct layout (7 fields, 56 bytes on 64-bit)
- [src/c/database_time_series.cpp] - read_time_series_group returns `char***` names, `int**` types, `void***` data; read_time_series_files returns `char***` with nullable path entries
- [bindings/js/src/ffi-helpers.ts] - Phase 2 output with 5 array decoder stubs
- [Phase 2 RESEARCH.md] - Allocation pattern, DataView conventions, little-endian requirement

### Secondary (MEDIUM confidence)
- [Deno.UnsafePointerView.getArrayBuffer](https://docs.deno.com/api/deno/~/Deno.UnsafePointerView.prototype.getArrayBuffer) - Signature: `getArrayBuffer(byteLength: number, offset?: number): ArrayBuffer`

### Tertiary (LOW confidence)
None -- all claims verified against runtime or official documentation.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - All APIs are Deno built-ins verified on target machine
- Architecture: HIGH - Array decoder patterns verified with `deno eval --unstable-ffi`; struct layout verified against C headers; koffi usage inventory verified by grep
- Pitfalls: HIGH - Zero-copy semantics verified empirically; type change cascade identified by static analysis of source files

**Research date:** 2026-04-17
**Valid until:** 2026-07-17 (stable Deno APIs, unlikely to change)
