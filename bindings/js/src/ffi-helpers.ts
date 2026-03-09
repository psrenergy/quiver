import { type Pointer, ptr, read } from "bun:ffi";

/**
 * Construct the 8-byte quiver_database_options_t struct manually.
 * Layout: offset 0 = int32 read_only (default 0), offset 4 = int32 console_level (default 1 = QUIVER_LOG_INFO).
 * DO NOT call quiver_database_options_default() -- it returns struct by value which bun:ffi cannot handle.
 */
export function makeDefaultOptions(): Uint8Array {
  const buf = new ArrayBuffer(8);
  const view = new Int32Array(buf);
  view[0] = 0; // read_only = false
  view[1] = 1; // console_level = QUIVER_LOG_INFO
  return new Uint8Array(buf);
}

/**
 * Allocate a 1-element BigInt64Array (8 bytes) to receive a quiver_database_t** out-parameter.
 * Caller passes ptr(result) to the FFI function.
 */
export function allocPointerOut(): BigInt64Array {
  return new BigInt64Array(1);
}

/**
 * Read the pointer value from an out-parameter buffer.
 * Dereferences the pointer stored at offset 0 of the typed array.
 * Returns a branded Pointer suitable for passing to FFI functions.
 * read.ptr() returns number at runtime; the cast to Pointer satisfies bun:ffi type signatures.
 */
export function readPointerOut(buf: BigInt64Array): Pointer {
  return read.ptr(ptr(buf), 0) as unknown as Pointer;
}

/**
 * Read a pointer at the given byte offset from a base pointer.
 * Wraps read.ptr() with a cast to the branded Pointer type.
 */
export function readPtrAt(base: Pointer, byteOffset: number): Pointer {
  return read.ptr(base, byteOffset) as unknown as Pointer;
}

/**
 * Encode a JS string as null-terminated UTF-8 for the C API.
 * Returns a Buffer suitable for passing as FFIType.ptr.
 */
export function toCString(str: string): Buffer {
  return Buffer.from(`${str}\0`);
}
