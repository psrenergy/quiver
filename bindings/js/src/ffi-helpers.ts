import type { Allocation } from "./types.ts";

const encoder = new TextEncoder();

/**
 * Construct the 8-byte quiver_database_options_t struct as an Allocation.
 * Layout: offset 0 = int32 read_only (default 0), offset 4 = int32 console_level (default 1 = QUIVER_LOG_INFO).
 */
export function makeDefaultOptions(): Allocation {
  const buf = new Uint8Array(8);
  const dv = new DataView(buf.buffer);
  dv.setInt32(0, 0, true); // read_only = false
  dv.setInt32(4, 1, true); // console_level = QUIVER_LOG_INFO
  return { ptr: Deno.UnsafePointer.of(buf)!, buf };
}

/** Allocate an 8-byte buffer for a pointer out-parameter. */
export function allocPtrOut(): Allocation {
  const buf = new Uint8Array(8);
  return { ptr: Deno.UnsafePointer.of(buf)!, buf };
}

/** Read a pointer from an out-parameter buffer. */
export function readPtrOut(alloc: Allocation): Deno.PointerValue {
  const dv = new DataView(alloc.buf.buffer);
  const raw = dv.getBigUint64(0, true);
  return raw === 0n ? null : Deno.UnsafePointer.create(raw);
}

/** Allocate an 8-byte buffer for a uint64 out-parameter. */
export function allocUint64Out(): Allocation {
  const buf = new Uint8Array(8);
  return { ptr: Deno.UnsafePointer.of(buf)!, buf };
}

/** Read a uint64 value from an out-parameter buffer. */
export function readUint64Out(alloc: Allocation): number {
  const dv = new DataView(alloc.buf.buffer);
  return Number(dv.getBigUint64(0, true));
}

/**
 * Read a null-terminated C string from a pointer stored in an out-parameter buffer.
 * Decodes the char* pointer via readPtrOut, then reads the C string with getCString().
 */
export function decodeStringFromBuf(alloc: Allocation): string {
  const ptr = readPtrOut(alloc);
  if (!ptr) return "";
  return new Deno.UnsafePointerView(ptr).getCString();
}

/** Read an array of int64 values from a native pointer, returning JS numbers. */
export function decodeInt64Array(ptr: Deno.PointerValue, count: number): number[] {
  if (!ptr || count === 0) return [];
  const view = new Deno.UnsafePointerView(ptr);
  const ab = view.getArrayBuffer(count * 8);
  const i64 = new BigInt64Array(ab);
  return Array.from(i64, (v) => Number(v));
}

/** Read an array of float64 values from a native pointer. */
export function decodeFloat64Array(ptr: Deno.PointerValue, count: number): number[] {
  if (!ptr || count === 0) return [];
  const view = new Deno.UnsafePointerView(ptr);
  const ab = view.getArrayBuffer(count * 8);
  const f64 = new Float64Array(ab);
  return Array.from(f64);
}

/** Read an array of uint64 values from a native pointer, returning JS numbers. */
export function decodeUint64Array(ptr: Deno.PointerValue, count: number): number[] {
  if (!ptr || count === 0) return [];
  const view = new Deno.UnsafePointerView(ptr);
  const ab = view.getArrayBuffer(count * 8);
  const u64 = new BigUint64Array(ab);
  return Array.from(u64, (v) => Number(v));
}

/** Read an array of pointers from a native pointer using offset-based reading. */
export function decodePtrArray(ptr: Deno.PointerValue, count: number): Deno.PointerValue[] {
  if (!ptr || count === 0) return [];
  const view = new Deno.UnsafePointerView(ptr);
  const result: Deno.PointerValue[] = new Array(count);
  for (let i = 0; i < count; i++) {
    result[i] = view.getPointer(i * 8);
  }
  return result;
}

/** Read an array of C strings from a char** pointer. */
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

/** Encode a JS string as a null-terminated C string buffer. */
export function toCString(str: string): Allocation {
  const encoded = encoder.encode(str + "\0");
  const buf = new Uint8Array(encoded.length);
  buf.set(encoded);
  return { ptr: Deno.UnsafePointer.of(buf)!, buf };
}

/** Allocate native memory for an array of int64 values. */
export function allocNativeInt64(values: number[]): Allocation {
  const buf = new Uint8Array(values.length * 8);
  const dv = new DataView(buf.buffer);
  for (let i = 0; i < values.length; i++) {
    dv.setBigInt64(i * 8, BigInt(values[i]), true);
  }
  return { ptr: Deno.UnsafePointer.of(buf)!, buf };
}

/** Allocate native memory for an array of float64 values. */
export function allocNativeFloat64(values: number[]): Allocation {
  const buf = new Uint8Array(values.length * 8);
  const dv = new DataView(buf.buffer);
  for (let i = 0; i < values.length; i++) {
    dv.setFloat64(i * 8, values[i], true);
  }
  return { ptr: Deno.UnsafePointer.of(buf)!, buf };
}

/**
 * Build a native pointer table from an array of native pointers.
 * The returned allocation contains an array of void* addresses.
 */
export function allocNativePtrTable(ptrs: Deno.PointerValue[]): Allocation {
  const buf = new Uint8Array(ptrs.length * 8);
  const dv = new DataView(buf.buffer);
  for (let i = 0; i < ptrs.length; i++) {
    const addr = ptrs[i] ? Deno.UnsafePointer.value(ptrs[i]!) : 0n;
    dv.setBigUint64(i * 8, addr, true);
  }
  return { ptr: Deno.UnsafePointer.of(buf)!, buf };
}

/** Allocate a native string (null-terminated) and return its allocation. */
export function allocNativeString(str: string): Allocation {
  const buf = encoder.encode(str + "\0");
  return { ptr: Deno.UnsafePointer.of(buf)!, buf };
}

/** Build a native string pointer table (const char* const*) from an array of strings. */
export function allocNativeStringArray(strings: string[]): { table: Allocation; keepalive: Allocation[] } {
  const strAllocs = strings.map((s) => allocNativeString(s));
  const table = allocNativePtrTable(strAllocs.map((a) => a.ptr));
  return { table, keepalive: strAllocs };
}

/** Get the native address of a pointer as BigInt. */
export function nativeAddress(ptr: Deno.PointerValue): bigint {
  return ptr ? Deno.UnsafePointer.value(ptr) : 0n;
}
