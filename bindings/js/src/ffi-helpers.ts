import { CString, type Pointer, ptr, read, toArrayBuffer } from "bun:ffi";
import { type Allocation, type DatabaseOptions, LOG_LEVEL_INFO } from "./types.ts";

const encoder = new TextEncoder();

/**
 * Construct the 8-byte quiver_database_options_t struct as an Allocation.
 * Layout: offset 0 = int32 read_only (default 0), offset 4 = int32 console_level (default 1 = QUIVER_LOG_INFO).
 */
export function makeDefaultOptions(options?: DatabaseOptions): Allocation {
  const buf = new Uint8Array(8);
  const dv = new DataView(buf.buffer);
  dv.setInt32(0, options?.readOnly ? 1 : 0, true);
  dv.setInt32(4, options?.consoleLevel ?? LOG_LEVEL_INFO, true);
  return { ptr: ptr(buf), buf };
}

/** Allocate an 8-byte buffer for a pointer out-parameter. */
export function allocPtrOut(): Allocation {
  const buf = new Uint8Array(8);
  return { ptr: ptr(buf), buf };
}

/** Read a pointer from an out-parameter buffer. */
export function readPtrOut(alloc: Allocation): Pointer | null {
  const dv = new DataView(alloc.buf.buffer);
  const raw = dv.getBigUint64(0, true);
  return raw === 0n ? null : (Number(raw) as Pointer);
}

/** Allocate an 8-byte buffer for a uint64 out-parameter. */
export function allocUint64Out(): Allocation {
  const buf = new Uint8Array(8);
  return { ptr: ptr(buf), buf };
}

/** Read a uint64 value from an out-parameter buffer. */
export function readUint64Out(alloc: Allocation): number {
  const dv = new DataView(alloc.buf.buffer);
  return Number(dv.getBigUint64(0, true));
}

/**
 * Read a null-terminated C string from a pointer stored in an out-parameter buffer.
 * Decodes the char* pointer via readPtrOut, then reads the C string with CString.
 */
export function decodeStringFromBuf(alloc: Allocation): string {
  const p = readPtrOut(alloc);
  if (!p) return "";
  return new CString(p).toString();
}

/** Read an array of int64 values from a native pointer, returning JS numbers. */
export function decodeInt64Array(p: Pointer | null, count: number): number[] {
  if (!p || count === 0) return [];
  const i64 = new BigInt64Array(toArrayBuffer(p, 0, count * 8));
  return Array.from(i64, (v) => Number(v));
}

/** Read an array of float64 values from a native pointer. */
export function decodeFloat64Array(p: Pointer | null, count: number): number[] {
  if (!p || count === 0) return [];
  const f64 = new Float64Array(toArrayBuffer(p, 0, count * 8));
  return Array.from(f64);
}

/** Read an array of uint64 values from a native pointer, returning JS numbers. */
export function decodeUint64Array(p: Pointer | null, count: number): number[] {
  if (!p || count === 0) return [];
  const u64 = new BigUint64Array(toArrayBuffer(p, 0, count * 8));
  return Array.from(u64, (v) => Number(v));
}

/** Read an array of pointers from a native pointer using offset-based reading. */
export function decodePtrArray(p: Pointer | null, count: number): (Pointer | null)[] {
  if (!p || count === 0) return [];
  const result: (Pointer | null)[] = new Array(count);
  for (let i = 0; i < count; i++) {
    const v = read.ptr(p, i * 8);
    result[i] = v === 0 ? null : (v as Pointer);
  }
  return result;
}

/** Read an array of C strings from a char** pointer. */
export function decodeStringArray(p: Pointer | null, count: number): string[] {
  if (!p || count === 0) return [];
  const result: string[] = new Array(count);
  for (let i = 0; i < count; i++) {
    const strPtr = read.ptr(p, i * 8);
    result[i] = strPtr === 0 ? "" : new CString(strPtr as Pointer).toString();
  }
  return result;
}

/** Encode a JS string as a null-terminated C string buffer. */
export function toCString(str: string): Allocation {
  const encoded = encoder.encode(str + "\0");
  const buf = new Uint8Array(encoded.length);
  buf.set(encoded);
  return { ptr: ptr(buf), buf };
}

/** Allocate native memory for an array of int64 values. */
export function allocNativeInt64(values: (number | bigint)[]): Allocation {
  const buf = new Uint8Array(values.length * 8);
  const dv = new DataView(buf.buffer);
  for (let i = 0; i < values.length; i++) {
    dv.setBigInt64(i * 8, BigInt(values[i]), true);
  }
  return { ptr: ptr(buf), buf };
}

/** Allocate native memory for an array of float64 values. */
export function allocNativeFloat64(values: number[]): Allocation {
  const buf = new Uint8Array(values.length * 8);
  const dv = new DataView(buf.buffer);
  for (let i = 0; i < values.length; i++) {
    dv.setFloat64(i * 8, values[i], true);
  }
  return { ptr: ptr(buf), buf };
}

/**
 * Build a native pointer table from an array of native pointers.
 * The returned allocation contains an array of void* addresses.
 */
export function allocNativePtrTable(ptrs: (Pointer | null)[]): Allocation {
  const buf = new Uint8Array(ptrs.length * 8);
  const dv = new DataView(buf.buffer);
  for (let i = 0; i < ptrs.length; i++) {
    const addr = ptrs[i] ? BigInt(ptrs[i] as number) : 0n;
    dv.setBigUint64(i * 8, addr, true);
  }
  return { ptr: ptr(buf), buf };
}

/** Allocate a native string (null-terminated) and return its allocation. */
export function allocNativeString(str: string): Allocation {
  const buf = encoder.encode(str + "\0");
  return { ptr: ptr(buf), buf };
}

/**
 * Build a native string pointer table (const char* const*) from an array of strings.
 * A `null` entry becomes a NULL pointer in the table (used for SQL-NULL string cells).
 */
export function allocNativeStringArray(strings: (string | null)[]): {
  table: Allocation;
  keepalive: Allocation[];
} {
  const strAllocs = strings.map((s) => (s === null ? null : allocNativeString(s)));
  const table = allocNativePtrTable(strAllocs.map((a) => (a ? a.ptr : null)));
  return { table, keepalive: strAllocs.filter((a): a is Allocation => a !== null) };
}

/** Get the native address of a pointer as BigInt. */
export function nativeAddress(p: Pointer | null): bigint {
  return p ? BigInt(p as number) : 0n;
}
