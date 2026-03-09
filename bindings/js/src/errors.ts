import { CString } from "bun:ffi";
import { getSymbols } from "./loader";

export class QuiverError extends Error {
  constructor(message: string) {
    super(message);
    this.name = "QuiverError";
  }
}

export function check(err: number): void {
  if (err !== 0) {
    const lib = getSymbols();
    const errPtr = lib.quiver_get_last_error();
    if (errPtr) {
      const detail = new CString(errPtr).toString();
      if (detail) {
        throw new QuiverError(detail);
      }
    }
    console.warn("check: C API returned error but quiver_get_last_error() is empty");
    throw new QuiverError("Unknown error");
  }
}
