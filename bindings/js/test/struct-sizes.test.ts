import { describe, expect, test } from "bun:test";
import { QuiverError } from "../src/errors.ts";
import {
  CSV_OPTIONS_SIZE,
  GROUP_METADATA_SIZE,
  OPTIONS_SIZE,
  SCALAR_METADATA_SIZE,
} from "../src/ffi-helpers.ts";
// This test intentionally DOES drag in the FFI loader (unlike ffi-helpers.test.ts) -- it is
// proving the load-time gate itself, which only exists inside loader.ts.
import {
  checkedStructNames,
  checkStructSize,
  getSymbols,
  loadLibrary,
  resolveLibrary,
} from "../src/loader.ts";

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

  test("quiver_csv_options_sizeof reports 56, matching CSV_OPTIONS_SIZE", () => {
    const lib = getSymbols();
    expect(Number(lib.quiver_csv_options_sizeof())).toBe(CSV_OPTIONS_SIZE);
    expect(Number(lib.quiver_csv_options_sizeof())).toBe(56);
  });
});

// D-12: a struct-size test must never be able to pass while its gate is unwired. Five tests
// above (and the failure-path tests below) drive only checkStructSize with fabricated numbers --
// exactly why replacing assertNativeStructSizes's body with `void lib;` left this suite green
// before this plan. checkedStructNames() records the struct names the gate actually walked
// through on the memoized loadLibrary() path, so this test fails if the gate call is ever
// deleted or its body gutted. It must NOT call assertNativeStructSizes itself -- doing so would
// repopulate the record and recreate the exact vacuous pass this test exists to prevent.
describe("the load-time gate cannot be silently unwired", () => {
  test("checkedStructNames records all four structs in the fixed check order", () => {
    getSymbols();
    expect(checkedStructNames()).toEqual([
      "quiver_database_options_t",
      "quiver_scalar_metadata_t",
      "quiver_group_metadata_t",
      "quiver_csv_options_t",
    ]);
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
    expect(() => checkStructSize("quiver_csv_options_t", 56, 55)).toThrow(QuiverError);
    expect(() => checkStructSize("quiver_csv_options_t", 56, 57)).toThrow(QuiverError);
  });
});

// Gap 3: a native library predating this milestone has the full Phase 1 surface but none of the
// *_sizeof exports Phase 2 introduces -- the only version skew a published native can actually
// produce today. This drives resolveLibrary() with a symbol map containing one symbol that
// certainly does not exist, taking the exact real-dlopen code path a stale native takes (not a
// stubbed loader), and asserts the diagnosis names all four accessors instead of surfacing the
// generic "Cannot load native library" text.
describe("resolveLibrary diagnoses a native that loads but lacks the size accessors", () => {
  test("a symbol map with a nonexistent symbol produces a version-skew diagnosis, not a not-found", () => {
    const staleSymbols = {
      quiver_symbol_that_does_not_exist: { args: [], returns: "void" },
    } as const;

    let thrown: unknown;
    try {
      resolveLibrary(staleSymbols);
    } catch (e) {
      thrown = e;
    }

    expect(thrown).toBeInstanceOf(QuiverError);
    const message = (thrown as Error).message;
    expect(message).toContain("quiver_database_options_sizeof");
    expect(message).toContain("quiver_scalar_metadata_sizeof");
    expect(message).toContain("quiver_group_metadata_sizeof");
    expect(message).toContain("quiver_csv_options_sizeof");
    expect(message).not.toContain("Searched:");
  });

  // The genuine not-found path (no native loadable at all, so the probe also fails and the
  // original "Cannot load native library" error is rethrown unchanged) cannot be driven from
  // this test file without deleting the real native library out from under the rest of the
  // suite -- there is no loadable-directory parameter to redirect resolveLibrary() at a truly
  // empty search tree. That path is a direct code-read: resolveLibrary's inner catch rethrows
  // `e` (the original caught error), not a new QuiverError, only when the PROBE_SYMBOLS
  // resolution also throws -- see loader.ts.
});
