import { Database } from "./database.ts";
import { check, QuiverError } from "./errors.ts";
import {
  allocNativeFloat64,
  allocNativeInt64,
  allocNativeStringArray,
  allocPtrOut,
  readPtrOut,
  toCString,
} from "./ffi-helpers.ts";
import { type GroupColumns, updateGroupColumns } from "./group-columns.ts";
import { getSymbols, type NativePointer } from "./loader.ts";
import type { ElementData, Value } from "./types.ts";

type Symbols = ReturnType<typeof getSymbols>;

function setElementArray(
  lib: Symbols,
  elemPtr: NativePointer,
  name: string,
  values: unknown[],
): void {
  const nameBuf = toCString(name);

  if (values.length === 0) {
    check(lib.quiver_element_set_array_integer(elemPtr, nameBuf.buf, null, 0, null));
    return;
  }

  const first = values[0];

  if (typeof first === "bigint") {
    const arr = allocNativeInt64(values as bigint[]);
    check(lib.quiver_element_set_array_integer(elemPtr, nameBuf.buf, arr.buf, values.length, null));
    return;
  }

  if (typeof first === "number") {
    const allIntegers = (values as number[]).every((v) => Number.isInteger(v));
    if (allIntegers) {
      const arr = allocNativeInt64(values as number[]);
      check(
        lib.quiver_element_set_array_integer(elemPtr, nameBuf.buf, arr.buf, values.length, null),
      );
    } else {
      const arr = allocNativeFloat64(values as number[]);
      check(lib.quiver_element_set_array_float(elemPtr, nameBuf.buf, arr.buf, values.length, null));
    }
    return;
  }

  if (typeof first === "string") {
    const { table, keepalive: _keepalive } = allocNativeStringArray(values as string[]);
    check(
      lib.quiver_element_set_array_string(elemPtr, nameBuf.buf, table.buf, values.length, null),
    );
    return;
  }

  throw new QuiverError(`Unsupported array element type for '${name}': ${typeof first}`);
}

function setElementField(lib: Symbols, elemPtr: NativePointer, name: string, value: Value): void {
  const nameBuf = toCString(name);

  if (value === null) {
    check(lib.quiver_element_set_null(elemPtr, nameBuf.buf));
    return;
  }

  if (typeof value === "bigint") {
    check(lib.quiver_element_set_integer(elemPtr, nameBuf.buf, value));
    return;
  }

  if (typeof value === "number") {
    if (Number.isInteger(value)) {
      check(lib.quiver_element_set_integer(elemPtr, nameBuf.buf, BigInt(value)));
    } else {
      check(lib.quiver_element_set_float(elemPtr, nameBuf.buf, value));
    }
    return;
  }

  if (typeof value === "string") {
    const valBuf = toCString(value);
    check(lib.quiver_element_set_string(elemPtr, nameBuf.buf, valBuf.buf));
    return;
  }

  if (Array.isArray(value)) {
    setElementArray(lib, elemPtr, name, value);
    return;
  }

  throw new QuiverError(`Unsupported value type for '${name}': ${typeof value}`);
}

Database.prototype.createElement = function (
  this: Database,
  collection: string,
  data: ElementData,
): number {
  const lib = getSymbols();
  const handle = this._handle;

  const outElem = allocPtrOut();
  check(lib.quiver_element_create(outElem.buf));
  const elemPtr = readPtrOut(outElem);

  try {
    for (const [key, value] of Object.entries(data)) {
      if (value === undefined) continue;
      setElementField(lib, elemPtr, key, value);
    }

    const outIdBuf = new Uint8Array(8);
    const collBuf = toCString(collection);
    check(lib.quiver_database_create_element(handle, collBuf.buf, elemPtr, outIdBuf));
    return Number(new DataView(outIdBuf.buffer).getBigInt64(0, true));
  } finally {
    lib.quiver_element_destroy(elemPtr);
  }
};

Database.prototype.updateElement = function (
  this: Database,
  collection: string,
  id: number,
  data: ElementData,
): void {
  const lib = getSymbols();
  const handle = this._handle;

  const outElem = allocPtrOut();
  check(lib.quiver_element_create(outElem.buf));
  const elemPtr = readPtrOut(outElem);

  try {
    for (const [key, value] of Object.entries(data)) {
      if (value === undefined) continue;
      setElementField(lib, elemPtr, key, value);
    }
    const collBuf = toCString(collection);
    check(lib.quiver_database_update_element(handle, collBuf.buf, BigInt(id), elemPtr));
  } finally {
    lib.quiver_element_destroy(elemPtr);
  }
};

/** Label-addressed counterpart of updateElement. */
Database.prototype.updateElementByLabel = function (
  this: Database,
  collection: string,
  label: string,
  data: ElementData,
): void {
  const lib = getSymbols();
  const handle = this._handle;

  const outElem = allocPtrOut();
  check(lib.quiver_element_create(outElem.buf));
  const elemPtr = readPtrOut(outElem);

  try {
    for (const [key, value] of Object.entries(data)) {
      if (value === undefined) continue;
      setElementField(lib, elemPtr, key, value);
    }
    const collBuf = toCString(collection);
    check(
      lib.quiver_database_update_element_by_label(
        handle,
        collBuf.buf,
        toCString(label).buf,
        elemPtr,
      ),
    );
  } finally {
    lib.quiver_element_destroy(elemPtr);
  }
};

Database.prototype.deleteElement = function (this: Database, collection: string, id: number): void {
  const lib = getSymbols();
  const collBuf = toCString(collection);
  check(lib.quiver_database_delete_element(this._handle, collBuf.buf, BigInt(id)));
};

/** Label-addressed counterpart of deleteElement. */
Database.prototype.deleteElementByLabel = function (
  this: Database,
  collection: string,
  label: string,
): void {
  const lib = getSymbols();
  const collBuf = toCString(collection);
  check(
    lib.quiver_database_delete_element_by_label(this._handle, collBuf.buf, toCString(label).buf),
  );
};

/**
 * Set or clear one scalar foreign-key column on an existing element.
 *
 * The column is derived from `(collectionTo, relationType)` as
 * `lowercase(collectionTo) + "_" + relationType` and must be a foreign key to `collectionTo`.
 * `targetLabel` is always a label, never an id; pass `null` to clear the relation.
 * For a relation in a vector or set group, use updateVectorGroup / updateSetGroup.
 */
Database.prototype.updateRelation = function (
  this: Database,
  collectionFrom: string,
  collectionTo: string,
  relationType: string,
  id: number,
  targetLabel: string | null,
): void {
  const lib = getSymbols();
  check(
    lib.quiver_database_update_relation(
      this._handle,
      toCString(collectionFrom).buf,
      toCString(collectionTo).buf,
      toCString(relationType).buf,
      BigInt(id),
      targetLabel === null ? null : toCString(targetLabel).buf,
    ),
  );
};

/** Label-addressed counterpart of updateRelation. */
Database.prototype.updateRelationByLabel = function (
  this: Database,
  collectionFrom: string,
  collectionTo: string,
  relationType: string,
  label: string,
  targetLabel: string | null,
): void {
  const lib = getSymbols();
  check(
    lib.quiver_database_update_relation_by_label(
      this._handle,
      toCString(collectionFrom).buf,
      toCString(collectionTo).buf,
      toCString(relationType).buf,
      toCString(label).buf,
      targetLabel === null ? null : toCString(targetLabel).buf,
    ),
  );
};

/**
 * Replace all of an element's rows in one *named* vector group, from column arrays keyed by name.
 *
 * Pass `{}` to clear the group. Prefer this over routing the group's columns through
 * updateElement when a column name is shared by two groups of the collection (legal for foreign
 * keys): (collection, group) names exactly one table, a column name alone does not, and
 * updateElement writes an ambiguous column to every match.
 */
Database.prototype.updateVectorGroup = function (
  this: Database,
  collection: string,
  group: string,
  id: number,
  data: GroupColumns,
): void {
  updateGroupColumns(
    this._handle,
    "updateVectorGroup",
    getSymbols().quiver_database_update_vector_group,
    collection,
    group,
    BigInt(id),
    data,
  );
};

/** Label-addressed counterpart of updateVectorGroup. */
Database.prototype.updateVectorGroupByLabel = function (
  this: Database,
  collection: string,
  group: string,
  label: string,
  data: GroupColumns,
): void {
  updateGroupColumns(
    this._handle,
    "updateVectorGroupByLabel",
    getSymbols().quiver_database_update_vector_group_by_label,
    collection,
    group,
    toCString(label).buf,
    data,
  );
};

/** Set-group counterpart of updateVectorGroup. */
Database.prototype.updateSetGroup = function (
  this: Database,
  collection: string,
  group: string,
  id: number,
  data: GroupColumns,
): void {
  updateGroupColumns(
    this._handle,
    "updateSetGroup",
    getSymbols().quiver_database_update_set_group,
    collection,
    group,
    BigInt(id),
    data,
  );
};

/** Label-addressed counterpart of updateSetGroup. */
Database.prototype.updateSetGroupByLabel = function (
  this: Database,
  collection: string,
  group: string,
  label: string,
  data: GroupColumns,
): void {
  updateGroupColumns(
    this._handle,
    "updateSetGroupByLabel",
    getSymbols().quiver_database_update_set_group_by_label,
    collection,
    group,
    toCString(label).buf,
    data,
  );
};
