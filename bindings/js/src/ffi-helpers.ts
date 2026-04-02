import koffi from "koffi";
import type { NativePointer } from "./loader.js";

/**
 * Construct the 8-byte quiver_database_options_t struct as a Buffer.
 * Layout: offset 0 = int32 read_only (default 0), offset 4 = int32 console_level (default 1 = QUIVER_LOG_INFO).
 */
export function makeDefaultOptions(): Buffer {
  const buf = Buffer.alloc(8);
  buf.writeInt32LE(0, 0); // read_only = false
  buf.writeInt32LE(1, 4); // console_level = QUIVER_LOG_INFO
  return buf;
}

/** Allocate an 8-byte buffer for a pointer out-parameter. */
export function allocPtrOut(): Buffer {
  return Buffer.alloc(8);
}

/** Read a pointer from an out-parameter buffer. */
export function readPtrOut(buf: Buffer): NativePointer {
  return koffi.decode(buf, "void *") as NativePointer;
}

/** Allocate and read a uint64 out-parameter. */
export function allocUint64Out(): Buffer {
  return Buffer.alloc(8);
}

/** Read a uint64 value from an out-parameter buffer. */
export function readUint64Out(buf: Buffer): number {
  return Number(buf.readBigUInt64LE());
}

/**
 * Read a null-terminated C string from a pointer stored in a Buffer.
 * koffi.decode(externalPtr, "str") segfaults with external pointers,
 * so we decode from the original Buffer that holds the char* pointer.
 */
export function decodeStringFromBuf(buf: Buffer): string {
  return (koffi.decode(buf, koffi.array("str", 1)) as string[])[0];
}

/** Read an array of int64 values from a native pointer, returning JS numbers. */
export function decodeInt64Array(ptr: NativePointer, count: number): number[] {
  if (count === 0) return [];
  const bigInts = koffi.decode(ptr, koffi.array("int64_t", count)) as bigint[];
  const result: number[] = new Array(count);
  for (let i = 0; i < count; i++) {
    result[i] = Number(bigInts[i]);
  }
  return result;
}

/** Read an array of float64 values from a native pointer. */
export function decodeFloat64Array(ptr: NativePointer, count: number): number[] {
  if (count === 0) return [];
  return Array.from(koffi.decode(ptr, koffi.array("double", count)) as Float64Array);
}

/** Read an array of uint64 values from a native pointer, returning JS numbers. */
export function decodeUint64Array(ptr: NativePointer, count: number): number[] {
  if (count === 0) return [];
  const bigInts = koffi.decode(ptr, koffi.array("uint64_t", count)) as bigint[];
  const result: number[] = new Array(count);
  for (let i = 0; i < count; i++) {
    result[i] = Number(bigInts[i]);
  }
  return result;
}

/** Read an array of pointers from a native pointer using offset-based reading. */
export function decodePtrArray(ptr: NativePointer, count: number): NativePointer[] {
  if (count === 0) return [];
  const result: NativePointer[] = new Array(count);
  for (let i = 0; i < count; i++) {
    result[i] = koffi.decode(ptr, i * 8, "void *") as NativePointer;
  }
  return result;
}

/** Read an array of C strings from a char** pointer. */
export function decodeStringArray(ptr: NativePointer, count: number): string[] {
  if (count === 0) return [];
  return koffi.decode(ptr, koffi.array("str", count)) as string[];
}

/** Encode a JS string as a null-terminated C string buffer. */
export function toCString(str: string): Buffer {
  return Buffer.from(`${str}\0`);
}

/**
 * Allocate native memory for a typed array and write values into it.
 * Returns [nativePtr, address] where nativePtr must be kept alive to prevent GC.
 */
export function allocNativeInt64(values: number[]): NativePointer {
  const p = koffi.alloc("int64_t", values.length);
  for (let i = 0; i < values.length; i++) {
    koffi.encode(p, i * 8, "int64_t", BigInt(values[i]));
  }
  return p;
}

export function allocNativeFloat64(values: number[]): NativePointer {
  const p = koffi.alloc("double", values.length);
  for (let i = 0; i < values.length; i++) {
    koffi.encode(p, i * 8, "double", values[i]);
  }
  return p;
}

/**
 * Build a native pointer table from an array of native pointers.
 * The returned pointer contains an array of void* addresses.
 */
export function allocNativePtrTable(ptrs: NativePointer[]): NativePointer {
  const p = koffi.alloc("void *", ptrs.length);
  for (let i = 0; i < ptrs.length; i++) {
    koffi.encode(p, i * 8, "void *", ptrs[i]);
  }
  return p;
}

/**
 * Allocate a native string (null-terminated) and return its external pointer.
 */
export function allocNativeString(str: string): NativePointer {
  const buf = Buffer.from(`${str}\0`);
  const p = koffi.alloc("char", buf.length);
  for (let i = 0; i < buf.length; i++) {
    koffi.encode(p, i, "char", buf[i]);
  }
  return p;
}

/**
 * Build a native string pointer table (const char* const*) from an array of strings.
 */
export function allocNativeStringArray(strings: string[]): { table: NativePointer; keepalive: NativePointer[] } {
  const strPtrs: NativePointer[] = strings.map((s) => allocNativeString(s));
  const table = allocNativePtrTable(strPtrs);
  return { table, keepalive: strPtrs };
}

/** Get the native address of an external pointer as BigInt. */
export function nativeAddress(ptr: NativePointer): bigint {
  return koffi.address(ptr) as bigint;
}
