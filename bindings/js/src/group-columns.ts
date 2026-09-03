import { type Pointer, ptr } from "bun:ffi";
import { check, QuiverError } from "./errors.ts";
import {
  allocNativeFloat64,
  allocNativeInt64,
  allocNativePtrTable,
  allocNativeStringArray,
  toCString,
} from "./ffi-helpers.ts";
import type { NativePointer } from "./loader.ts";
import { DATA_TYPE_FLOAT, DATA_TYPE_INTEGER, DATA_TYPE_STRING, type Allocation } from "./types.ts";

/** Column-oriented group payload: one array of cells per column name, `null` for SQL NULL. */
export type GroupColumns = Record<string, (number | string | boolean | null)[]>;

/**
 * The parallel-array signature every columnar group update C function shares
 * (quiver_database_update_{time_series,vector,set}_group and their _by_label forms). The 4th
 * argument addresses the element: an id for the by-id forms, a NUL-terminated label otherwise.
 */
type ColumnUpdateFn = (
  db: NativePointer,
  collection: Uint8Array,
  group: Uint8Array,
  key: bigint | Uint8Array,
  names: Uint8Array | null,
  types: Uint8Array | null,
  data: Uint8Array | null,
  masks: Uint8Array | null,
  columnCount: bigint,
  rowCount: bigint,
) => number;

/**
 * Marshal a column-oriented payload and forward it to one of the columnar group update C
 * functions. Shared by updateTimeSeriesGroup / updateVectorGroup / updateSetGroup and their
 * _by_label counterparts: they differ only in which C entry point they call.
 *
 * Pass `{}` (no columns) to clear the group.
 */
export function updateGroupColumns(
  handle: NativePointer,
  caller: string,
  update: ColumnUpdateFn,
  collection: string,
  group: string,
  key: number | string,
  data: GroupColumns,
): void {
  const collBuf = toCString(collection);
  const grpBuf = toCString(group);
  const keyArg = typeof key === "string" ? toCString(key).buf : BigInt(key);
  const entries = Object.entries(data);

  if (entries.length === 0) {
    check(update(handle, collBuf.buf, grpBuf.buf, keyArg, null, null, null, null, 0n, 0n));
    return;
  }

  const columnCount = entries.length;
  const rowCount = entries[0][1].length;

  // Validate before marshalling: a jagged column would desync the parallel arrays the C API
  // reads against row_count, and a zero-length column (with columns present) would otherwise
  // marshal a null data pointer. Named-but-empty columns are a caller mistake -- pass {} to
  // clear the group instead. The C API rejects both too; failing here names the column.
  for (const [name, values] of entries) {
    if (values.length !== rowCount) {
      throw new QuiverError(
        `Cannot ${caller}: column '${name}' has length ${values.length} but expected ${rowCount}`,
      );
    }
  }
  if (rowCount === 0) {
    const names = entries.map(([name]) => name).join(", ");
    throw new QuiverError(
      `Cannot ${caller}: columns [${names}] contain no rows; pass {} to clear the group`,
    );
  }

  const keepalive: Allocation[] = [];

  // Build column names as native string array
  const colNames = entries.map(([name]) => name);
  const { table: namesTable, keepalive: namesPtrs } = allocNativeStringArray(colNames);
  keepalive.push(namesTable, ...namesPtrs);

  // Build column types, data, and per-cell NULL masks. A null cell becomes mask 0 + a
  // placeholder in the data array (the C API never reads it). An all-null column is tagged
  // FLOAT with zeroed data — the type tag is ignored for masked-out cells.
  const typesBuf = new Uint8Array(columnCount * 4);
  const typesDv = new DataView(typesBuf.buffer);
  const dataPtrs: (Pointer | null)[] = [];
  const maskPtrs: (Pointer | null)[] = [];

  for (let c = 0; c < columnCount; c++) {
    const [colName, values] = entries[c];
    const first = values.find((v) => v !== null);

    // Mask via direct indexing — never a DataView, to avoid the documented
    // .buffer-materialization pitfall between ptr() and the FFI call.
    const maskBuf = new Uint8Array(rowCount);
    for (let r = 0; r < rowCount; r++) maskBuf[r] = values[r] === null ? 0 : 1;
    const maskAlloc: Allocation = { ptr: ptr(maskBuf), buf: maskBuf };
    keepalive.push(maskAlloc);
    maskPtrs.push(maskAlloc.ptr);

    if (first === undefined) {
      // All-null column
      typesDv.setInt32(c * 4, DATA_TYPE_FLOAT, true);
      const p = allocNativeFloat64(new Array(rowCount).fill(0));
      keepalive.push(p);
      dataPtrs.push(p.ptr);
    } else if (typeof first === "string") {
      typesDv.setInt32(c * 4, DATA_TYPE_STRING, true);
      const { table, keepalive: strPtrs } = allocNativeStringArray(
        values.map((v) => (v === null ? null : (v as string))),
      );
      keepalive.push(table, ...strPtrs);
      dataPtrs.push(table.ptr);
    } else if (typeof first === "boolean") {
      // SQLite has no boolean type: a boolean is INTEGER 1/0, as in setElementField and
      // marshalParams. A null cell keeps its 0 placeholder and is masked out above.
      typesDv.setInt32(c * 4, DATA_TYPE_INTEGER, true);
      const p = allocNativeInt64(values.map((v) => (v ? 1 : 0)));
      keepalive.push(p);
      dataPtrs.push(p.ptr);
    } else if (typeof first === "number") {
      const nonNull = values.filter((v) => v !== null) as number[];
      const sanitized = values.map((v) => (v === null ? 0 : (v as number)));
      if (nonNull.every((v) => Number.isInteger(v))) {
        typesDv.setInt32(c * 4, DATA_TYPE_INTEGER, true);
        const p = allocNativeInt64(sanitized);
        keepalive.push(p);
        dataPtrs.push(p.ptr);
      } else {
        typesDv.setInt32(c * 4, DATA_TYPE_FLOAT, true);
        const p = allocNativeFloat64(sanitized);
        keepalive.push(p);
        dataPtrs.push(p.ptr);
      }
    } else {
      throw new QuiverError(
        `Cannot ${caller}: column '${colName}' has unsupported value type ${typeof first}`,
      );
    }
  }

  const typesAlloc: Allocation = { ptr: ptr(typesBuf), buf: typesBuf };
  keepalive.push(typesAlloc);
  const dataTable = allocNativePtrTable(dataPtrs);
  keepalive.push(dataTable);
  const maskTable = allocNativePtrTable(maskPtrs);
  keepalive.push(maskTable);

  check(
    update(
      handle,
      collBuf.buf,
      grpBuf.buf,
      keyArg,
      namesTable.buf,
      typesAlloc.buf,
      dataTable.buf,
      maskTable.buf,
      BigInt(columnCount),
      BigInt(rowCount),
    ),
  );
}
