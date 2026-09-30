import { CString, type Pointer, ptr, read, toArrayBuffer } from "bun:ffi";
import { check, QuiverError } from "./errors.ts";
import {
  allocNativeFloat64,
  allocNativeInt64,
  allocNativePtrTable,
  allocNativeStringArray,
  allocPtrOut,
  allocUint64Out,
  decodeFloat64Array,
  decodeInt64Array,
  decodePtrArray,
  decodeStringArray,
  readPtrOut,
  readUint64Out,
  toCString,
} from "./ffi-helpers.ts";
import { getSymbols, type NativePointer } from "./loader.ts";
import {
  type Allocation,
  DATA_TYPE_DATE_TIME,
  DATA_TYPE_FLOAT,
  DATA_TYPE_INTEGER,
  DATA_TYPE_STRING,
} from "./types.ts";

/**
 * Column-oriented group payload: one array of cells per column name, `null` for SQL NULL. A
 * `bigint` cell is written as an exact int64 (like createElement); a `boolean` as INTEGER 1/0.
 */
export type GroupColumns = Record<string, (number | bigint | string | boolean | null)[]>;

/**
 * Column-oriented group read result: one array of cells per column name, `null` for SQL NULL.
 * The read-side twin of GroupColumns; no reader produces a boolean or a bigint, so it admits
 * neither.
 */
export type TimeSeriesData = Record<string, (number | string | null)[]>;

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
 * Check and normalize the cells of a numeric column: one whose first non-null cell is a number, a
 * bigint or a boolean. Shared by updateGroupColumns and setElementArray (create.ts) so the group
 * writers and the element arrays agree on every cell. String columns are never passed here.
 *
 * - A boolean becomes INTEGER 1/0, since SQLite has no boolean type. Per cell, because mapping the
 *   whole column from a leading boolean truthiness-mapped the rest and rewrote [true, 5] as
 *   [1, 1]. Before the INTEGER/FLOAT choice, because Number.isInteger(true) is false.
 * - A bigint stays a bigint, so an INTEGER column keeps it exact; the caller maps it through
 *   Number() only if the column turns out FLOAT.
 * - Any other non-null cell throws, naming the column and the cell. It would fail
 *   Number.isInteger, tag the column FLOAT and reach DataView.setFloat64, which converts it with no
 *   error: "abc" to NaN, which SQLite stores as NULL, and "2" to 2.
 */
export function numericCells(
  caller: string,
  name: string,
  values: readonly unknown[],
): (number | bigint | null)[] {
  return values.map((v, r) => {
    if (v === null || typeof v === "number" || typeof v === "bigint") return v;
    if (typeof v === "boolean") return v ? 1 : 0;
    throw new QuiverError(
      `Cannot ${caller}: numeric column '${name}' has unsupported value type ${typeof v} in cell ${r}`,
    );
  });
}

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
      // Not checked by numericCells: a mixed ['a', true] column still writes what it always wrote.
      typesDv.setInt32(c * 4, DATA_TYPE_STRING, true);
      const { table, keepalive: strPtrs } = allocNativeStringArray(
        values.map((v) => (v === null ? null : (v as string))),
      );
      keepalive.push(table, ...strPtrs);
      dataPtrs.push(table.ptr);
    } else if (
      typeof first === "number" ||
      typeof first === "boolean" ||
      typeof first === "bigint"
    ) {
      const cells = numericCells(caller, colName, values);
      if (cells.every((v) => v === null || typeof v === "bigint" || Number.isInteger(v))) {
        typesDv.setInt32(c * 4, DATA_TYPE_INTEGER, true);
        const p = allocNativeInt64(cells.map((v) => v ?? 0));
        keepalive.push(p);
        dataPtrs.push(p.ptr);
      } else {
        // A fractional cell makes the column FLOAT; a bigint cell then follows the int-for-REAL
        // rule through Number(), exact up to 2^53 like any JS number.
        typesDv.setInt32(c * 4, DATA_TYPE_FLOAT, true);
        const p = allocNativeFloat64(cells.map((v) => (v === null ? 0 : Number(v))));
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

/**
 * The out-parameter signature every columnar group read C function shares
 * (quiver_database_read_time_series_group, quiver_database_read_{vector,set}_group_by_id).
 */
type ColumnReadFn = (
  db: NativePointer,
  collection: Uint8Array,
  group: Uint8Array,
  id: bigint,
  names: Uint8Array,
  types: Uint8Array,
  data: Uint8Array,
  masks: Uint8Array,
  columnCount: Uint8Array,
  rowCount: Uint8Array,
) => number;

/**
 * Call one of the columnar group read C functions, decode its typed arrays + per-cell mask into
 * columns, and free the C result. The read-side twin of updateGroupColumns, shared by
 * readTimeSeriesGroup (which returns the columns) and readVectorGroupById / readSetGroupById
 * (which transpose them into rows). A masked cell is `null`; DATE_TIME cells stay ISO 8601 strings.
 */
export function readGroupColumns(
  handle: NativePointer,
  readGroup: ColumnReadFn,
  collection: string,
  group: string,
  id: number,
): TimeSeriesData {
  const collBuf = toCString(collection);
  const grpBuf = toCString(group);
  const outNames = allocPtrOut();
  const outTypes = allocPtrOut();
  const outData = allocPtrOut();
  const outHasValue = allocPtrOut();
  const outColCount = allocUint64Out();
  const outRowCount = allocUint64Out();

  check(
    readGroup(
      handle,
      collBuf.buf,
      grpBuf.buf,
      BigInt(id),
      outNames.buf,
      outTypes.buf,
      outData.buf,
      outHasValue.buf,
      outColCount.buf,
      outRowCount.buf,
    ),
  );

  const colCount = readUint64Out(outColCount);
  const rowCount = readUint64Out(outRowCount);
  if (colCount === 0) return {};

  const namesPtr = readPtrOut(outNames);
  const typesPtr = readPtrOut(outTypes);
  const dataPtr = readPtrOut(outData);
  const hasValuePtr = readPtrOut(outHasValue);

  try {
    const colNames = decodeStringArray(namesPtr, colCount);
    const typesAb = toArrayBuffer(typesPtr as Pointer, 0, colCount * 4);
    const types = Array.from(new Int32Array(typesAb));
    const dataPtrs = decodePtrArray(dataPtr, colCount);
    const maskPtrs = decodePtrArray(hasValuePtr, colCount);

    // Per-cell NULL mask: mask[r] === 0 means SQL NULL, surfaced as JS null. A time series'
    // dimension column's mask is always all 1, so it stays dense.
    const result: TimeSeriesData = {};
    for (let c = 0; c < colCount; c++) {
      const colName = colNames[c];
      const maskPtr = maskPtrs[c];
      const mask = maskPtr ? new Uint8Array(toArrayBuffer(maskPtr as Pointer, 0, rowCount)) : null;
      switch (types[c]) {
        case DATA_TYPE_INTEGER: {
          const vals = decodeInt64Array(dataPtrs[c], rowCount);
          result[colName] = mask ? vals.map((v, r) => (mask[r] ? v : null)) : vals;
          break;
        }
        case DATA_TYPE_FLOAT: {
          const vals = decodeFloat64Array(dataPtrs[c], rowCount);
          result[colName] = mask ? vals.map((v, r) => (mask[r] ? v : null)) : vals;
          break;
        }
        case DATA_TYPE_STRING:
        case DATA_TYPE_DATE_TIME: {
          // Read pointer-by-pointer, not with decodeStringArray, which turns a NULL char* into "":
          // a masked-out or NULL cell must come back as null.
          const base = dataPtrs[c];
          const col: (string | null)[] = new Array(rowCount);
          for (let r = 0; r < rowCount; r++) {
            if (mask && !mask[r]) {
              col[r] = null;
              continue;
            }
            const strPtr = base ? read.ptr(base as Pointer, r * 8) : 0;
            col[r] = strPtr === 0 ? null : new CString(strPtr as Pointer).toString();
          }
          result[colName] = col;
          break;
        }
      }
    }
    return result;
  } finally {
    getSymbols().quiver_database_free_time_series_data(
      namesPtr,
      typesPtr,
      dataPtr,
      hasValuePtr,
      BigInt(colCount),
      BigInt(rowCount),
    );
  }
}
