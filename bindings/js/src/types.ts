import type { Pointer } from "bun:ffi";

// Mirrors quiver_data_type_t in the C API
export const DATA_TYPE_INTEGER = 0;
export const DATA_TYPE_FLOAT = 1;
export const DATA_TYPE_STRING = 2;
export const DATA_TYPE_DATE_TIME = 3;
export const DATA_TYPE_NULL = 4;

// Mirrors quiver_log_level_t in the C API
export const LOG_LEVEL_DEBUG = 0;
export const LOG_LEVEL_INFO = 1;
export const LOG_LEVEL_WARN = 2;
export const LOG_LEVEL_ERROR = 3;
export const LOG_LEVEL_OFF = 4;

/** Options for opening or creating a database. */
export type DatabaseOptions = {
  readOnly?: boolean;
  /** A LOG_LEVEL_* constant; defaults to LOG_LEVEL_INFO. */
  consoleLevel?: number;
};

export type ScalarValue = number | bigint | string | null;
export type ArrayValue = number[] | bigint[] | string[];
export type Value = ScalarValue | ArrayValue;
export type ElementData = Record<string, Value | undefined>;
export type QueryParam = number | string | null;

/** Native memory allocation result. Callers MUST hold `buf` in scope to prevent GC. */
export type Allocation = {
  ptr: Pointer;
  buf: Uint8Array<ArrayBuffer>;
};
