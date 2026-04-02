import { Database } from "./database.js";
import { check, QuiverError } from "./errors.js";
import { allocPtrOut, readPtrOut } from "./ffi-helpers.js";
import { getSymbols } from "./loader.js";
import type { ElementData, Value } from "./types.js";

declare module "./database.js" {
  interface Database {
    createElement(collection: string, data: ElementData): number;
    updateElement(collection: string, id: number, data: ElementData): void;
    deleteElement(collection: string, id: number): void;
  }
}

type Symbols = ReturnType<typeof getSymbols>;

function setElementArray(lib: Symbols, elemPtr: unknown, name: string, values: unknown[]): void {
  if (values.length === 0) {
    check(lib.quiver_element_set_array_integer(elemPtr, name, null, 0));
    return;
  }

  const first = values[0];

  if (typeof first === "bigint") {
    const arr = BigInt64Array.from(values as bigint[], (v) => BigInt(v));
    check(lib.quiver_element_set_array_integer(elemPtr, name, arr, values.length));
    return;
  }

  if (typeof first === "number") {
    const allIntegers = (values as number[]).every((v) => Number.isInteger(v));
    if (allIntegers) {
      const arr = BigInt64Array.from(values as number[], (v) => BigInt(v));
      check(lib.quiver_element_set_array_integer(elemPtr, name, arr, values.length));
    } else {
      const arr = Float64Array.from(values as number[]);
      check(lib.quiver_element_set_array_float(elemPtr, name, arr, values.length));
    }
    return;
  }

  if (typeof first === "string") {
    check(lib.quiver_element_set_array_string(elemPtr, name, values as string[], values.length));
    return;
  }

  throw new QuiverError(`Unsupported array element type for '${name}': ${typeof first}`);
}

function setElementField(lib: Symbols, elemPtr: unknown, name: string, value: Value): void {
  if (value === null) {
    check(lib.quiver_element_set_null(elemPtr, name));
    return;
  }

  if (typeof value === "bigint") {
    check(lib.quiver_element_set_integer(elemPtr, name, value));
    return;
  }

  if (typeof value === "number") {
    if (Number.isInteger(value)) {
      check(lib.quiver_element_set_integer(elemPtr, name, value));
    } else {
      check(lib.quiver_element_set_float(elemPtr, name, value));
    }
    return;
  }

  if (typeof value === "string") {
    check(lib.quiver_element_set_string(elemPtr, name, value));
    return;
  }

  if (Array.isArray(value)) {
    setElementArray(lib, elemPtr, name, value);
    return;
  }

  throw new QuiverError(`Unsupported value type for '${name}': ${typeof value}`);
}

Database.prototype.createElement = function (this: Database, collection: string, data: ElementData): number {
  const lib = getSymbols();
  const handle = this._handle;

  const outElem = allocPtrOut();
  check(lib.quiver_element_create(outElem));
  const elemPtr = readPtrOut(outElem);

  try {
    for (const [key, value] of Object.entries(data)) {
      if (value === undefined) continue;
      setElementField(lib, elemPtr, key, value);
    }

    const outId = new BigInt64Array(1);
    check(lib.quiver_database_create_element(handle, collection, elemPtr, outId));
    return Number(outId[0]);
  } finally {
    lib.quiver_element_destroy(elemPtr);
  }
};

Database.prototype.updateElement = function (this: Database, collection: string, id: number, data: ElementData): void {
  const lib = getSymbols();
  const handle = this._handle;

  const outElem = allocPtrOut();
  check(lib.quiver_element_create(outElem));
  const elemPtr = readPtrOut(outElem);

  try {
    for (const [key, value] of Object.entries(data)) {
      if (value === undefined) continue;
      setElementField(lib, elemPtr, key, value);
    }
    check(lib.quiver_database_update_element(handle, collection, id, elemPtr));
  } finally {
    lib.quiver_element_destroy(elemPtr);
  }
};

Database.prototype.deleteElement = function (this: Database, collection: string, id: number): void {
  const lib = getSymbols();
  check(lib.quiver_database_delete_element(this._handle, collection, id));
};
