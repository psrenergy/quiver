import { getSymbols } from "./loader.ts";

export class QuiverError extends Error {
  constructor(message: string) {
    super(message);
    this.name = "QuiverError";
  }
}

export function check(err: number): void {
  if (err !== 0) {
    const lib = getSymbols();
    const ptr = lib.quiver_get_last_error();
    if (ptr) {
      const detail = new Deno.UnsafePointerView(ptr).getCString();
      throw new QuiverError(detail);
    }
    throw new QuiverError("Unknown error");
  }
}
