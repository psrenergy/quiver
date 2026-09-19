import { describe, expect, test } from "bun:test";
import { QuiverError } from "../src/errors.ts";
import { GROUP_METADATA_SIZE, OPTIONS_SIZE, SCALAR_METADATA_SIZE } from "../src/ffi-helpers.ts";
// This test intentionally DOES drag in the FFI loader (unlike ffi-helpers.test.ts) -- it is
// proving the load-time gate itself, which only exists inside loader.ts.
import { checkStructSize, getSymbols, loadLibrary } from "../src/loader.ts";

describe("native struct sizes (happy path)", () => {
  test("loading the library does not throw", () => {
    expect(() => loadLibrary()).not.toThrow();
  });

  // Every USIZE FFI return arrives as a bigint (Bun, probe-verified); Number(...) around each
  // call is what turns `24n === 24` into `24 === 24`.
  test("quiver_database_options_sizeof reports 24, matching OPTIONS_SIZE", () => {
    const lib = getSymbols();
    expect(Number(lib.quiver_database_options_sizeof())).toBe(OPTIONS_SIZE);
    expect(Number(lib.quiver_database_options_sizeof())).toBe(24);
  });

  test("quiver_scalar_metadata_sizeof reports 56, matching SCALAR_METADATA_SIZE", () => {
    const lib = getSymbols();
    expect(Number(lib.quiver_scalar_metadata_sizeof())).toBe(SCALAR_METADATA_SIZE);
    expect(Number(lib.quiver_scalar_metadata_sizeof())).toBe(56);
  });

  test("quiver_group_metadata_sizeof reports 32, matching GROUP_METADATA_SIZE", () => {
    const lib = getSymbols();
    expect(Number(lib.quiver_group_metadata_sizeof())).toBe(GROUP_METADATA_SIZE);
    expect(Number(lib.quiver_group_metadata_sizeof())).toBe(32);
  });
});

// CONTEXT.md criterion 5: asserting only the happy path proves nothing -- no binding has any
// test today that a wrong layout would actually fail. These three assertions inject a
// deliberately wrong expected value per struct and observe the throw.
describe("native struct sizes (failure path is actually exercised)", () => {
  test("checkStructSize passes when expected equals native", () => {
    expect(() => checkStructSize("quiver_database_options_t", 24, 24)).not.toThrow();
  });

  test("checkStructSize throws naming the struct and both numbers when options size disagrees", () => {
    expect(() => checkStructSize("quiver_database_options_t", 24, 8)).toThrow(QuiverError);
    try {
      checkStructSize("quiver_database_options_t", 24, 8);
    } catch (e) {
      expect((e as Error).message).toContain("quiver_database_options_t");
      expect((e as Error).message).toContain("24");
      expect((e as Error).message).toContain("8");
    }
  });

  test("checkStructSize throws naming the struct and both numbers when scalar metadata size disagrees", () => {
    expect(() => checkStructSize("quiver_scalar_metadata_t", 56, 48)).toThrow(QuiverError);
    try {
      checkStructSize("quiver_scalar_metadata_t", 56, 48);
    } catch (e) {
      expect((e as Error).message).toContain("quiver_scalar_metadata_t");
      expect((e as Error).message).toContain("56");
      expect((e as Error).message).toContain("48");
    }
  });

  test("checkStructSize throws naming the struct and both numbers when group metadata size disagrees", () => {
    expect(() => checkStructSize("quiver_group_metadata_t", 32, 16)).toThrow(QuiverError);
    try {
      checkStructSize("quiver_group_metadata_t", 32, 16);
    } catch (e) {
      expect((e as Error).message).toContain("quiver_group_metadata_t");
      expect((e as Error).message).toContain("32");
      expect((e as Error).message).toContain("16");
    }
  });

  // Adjacency: a size off by one in either direction throws, not just wildly wrong values.
  test("checkStructSize throws when native is off by exactly one byte", () => {
    expect(() => checkStructSize("quiver_database_options_t", 24, 23)).toThrow(QuiverError);
    expect(() => checkStructSize("quiver_database_options_t", 24, 25)).toThrow(QuiverError);
  });
});
