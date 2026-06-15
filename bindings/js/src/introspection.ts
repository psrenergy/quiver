import { Database } from "./database.ts";
import { check } from "./errors.ts";
import { allocPtrOut, decodeStringFromBuf, readPtrOut, toCString } from "./ffi-helpers.ts";
import { getSymbols } from "./loader.ts";

Database.prototype.isHealthy = function (this: Database): boolean {
  const lib = getSymbols();
  const outBuf = new Uint8Array(4);
  check(lib.quiver_database_is_healthy(this._handle, outBuf));
  return new DataView(outBuf.buffer).getInt32(0, true) !== 0;
};

Database.prototype.currentVersion = function (this: Database): number {
  const lib = getSymbols();
  const outBuf = new Uint8Array(8);
  check(lib.quiver_database_current_version(this._handle, outBuf));
  return Number(new DataView(outBuf.buffer).getBigInt64(0, true));
};

Database.prototype.path = function (this: Database): string {
  const lib = getSymbols();
  const outPath = allocPtrOut();
  check(lib.quiver_database_path(this._handle, outPath.buf));
  return decodeStringFromBuf(outPath);
};

Database.prototype.describe = function (this: Database): string {
  const lib = getSymbols();
  const out = allocPtrOut();
  check(lib.quiver_database_describe(this._handle, out.buf));
  const result = decodeStringFromBuf(out);
  lib.quiver_database_free_string(readPtrOut(out));
  return result;
};

Database.prototype.describeCollection = function (this: Database, collection: string): string {
  const lib = getSymbols();
  const collBuf = toCString(collection);
  const out = allocPtrOut();
  check(lib.quiver_database_describe_collection(this._handle, collBuf.buf, out.buf));
  const result = decodeStringFromBuf(out);
  lib.quiver_database_free_string(readPtrOut(out));
  return result;
};

Database.prototype.summarizeCollection = function (this: Database, collection: string): string {
  const lib = getSymbols();
  const collBuf = toCString(collection);
  const out = allocPtrOut();
  check(lib.quiver_database_summarize_collection(this._handle, collBuf.buf, out.buf));
  const result = decodeStringFromBuf(out);
  lib.quiver_database_free_string(readPtrOut(out));
  return result;
};

Database.prototype.getModelName = function (this: Database): string {
  const lib = getSymbols();
  const out = allocPtrOut();
  check(lib.quiver_database_get_model_name(this._handle, out.buf));
  const result = decodeStringFromBuf(out);
  lib.quiver_database_free_string(readPtrOut(out));
  return result;
};

Database.prototype.getAttributeUnit = function (
  this: Database,
  collection: string,
  attribute: string,
): string {
  const lib = getSymbols();
  const collBuf = toCString(collection);
  const attrBuf = toCString(attribute);
  const out = allocPtrOut();
  check(lib.quiver_database_get_attribute_unit(this._handle, collBuf.buf, attrBuf.buf, out.buf));
  const result = decodeStringFromBuf(out);
  lib.quiver_database_free_string(readPtrOut(out));
  return result;
};
