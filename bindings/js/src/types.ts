export type ScalarValue = number | bigint | string | null;
export type ArrayValue = number[] | bigint[] | string[];
export type Value = ScalarValue | ArrayValue;
export type ElementData = Record<string, Value | undefined>;
export type QueryParam = number | string | null;

/** Native memory allocation result. Callers MUST hold `buf` in scope to prevent GC. */
export type Allocation = {
  ptr: Deno.PointerValue;
  buf: Uint8Array;
};
