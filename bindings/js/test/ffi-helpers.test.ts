import { describe, expect, test } from "bun:test";
// Import from ffi-helpers.ts directly, NOT via src/index.ts -- this test must not drag in the
// FFI loader (mirrors the same constraint enforced by lua-api-sync.test.ts).
import {
  allocPtrOut,
  allocUint64Out,
  GROUP_METADATA_SIZE,
  makeDefaultOptions,
  OPTIONS_OFFSET_CONSOLE_LEVEL,
  OPTIONS_OFFSET_READ_ONLY,
  OPTIONS_OFFSET_UI_CONFIG_DIR,
  OPTIONS_OFFSET_UI_LOCALE,
  OPTIONS_SIZE,
  SCALAR_METADATA_SIZE,
} from "../src/ffi-helpers.ts";

describe("makeDefaultOptions", () => {
  test("named offset/size constants have the values pinned by src/c/options.cpp", () => {
    expect(OPTIONS_OFFSET_READ_ONLY).toBe(0);
    expect(OPTIONS_OFFSET_CONSOLE_LEVEL).toBe(4);
    expect(OPTIONS_OFFSET_UI_CONFIG_DIR).toBe(8);
    expect(OPTIONS_OFFSET_UI_LOCALE).toBe(16);
    expect(OPTIONS_SIZE).toBe(24);
  });

  test("no argument returns a 24-byte buffer with defaults and zeroed pointer slots", () => {
    const [alloc, keepalive] = makeDefaultOptions();
    expect(alloc.buf.length).toBe(24);
    expect(keepalive.length).toBe(0);

    const dv = new DataView(alloc.buf.buffer);
    expect(dv.getInt32(OPTIONS_OFFSET_READ_ONLY, true)).toBe(0);
    expect(dv.getInt32(OPTIONS_OFFSET_CONSOLE_LEVEL, true)).toBe(1);
    expect(dv.getBigUint64(OPTIONS_OFFSET_UI_CONFIG_DIR, true)).toBe(0n);
    expect(dv.getBigUint64(OPTIONS_OFFSET_UI_LOCALE, true)).toBe(0n);
  });

  test("uiConfigDir alone keeps one child allocation alive and writes offset 8 only", () => {
    const [alloc, keepalive] = makeDefaultOptions({ uiConfigDir: "/x" });
    expect(keepalive.length).toBe(1);

    const dv = new DataView(alloc.buf.buffer);
    expect(dv.getBigUint64(OPTIONS_OFFSET_UI_CONFIG_DIR, true)).not.toBe(0n);
    expect(dv.getBigUint64(OPTIONS_OFFSET_UI_LOCALE, true)).toBe(0n);
  });

  test("uiConfigDir and uiLocale together keep two child allocations alive", () => {
    const [alloc, keepalive] = makeDefaultOptions({ uiConfigDir: "/x", uiLocale: "es" });
    expect(keepalive.length).toBe(2);

    const dv = new DataView(alloc.buf.buffer);
    expect(dv.getBigUint64(OPTIONS_OFFSET_UI_CONFIG_DIR, true)).not.toBe(0n);
    expect(dv.getBigUint64(OPTIONS_OFFSET_UI_LOCALE, true)).not.toBe(0n);
  });

  test("an empty string writes NULL and allocates no child string", () => {
    const [alloc, keepalive] = makeDefaultOptions({ uiConfigDir: "" });
    expect(keepalive.length).toBe(0);

    const dv = new DataView(alloc.buf.buffer);
    expect(dv.getBigUint64(OPTIONS_OFFSET_UI_CONFIG_DIR, true)).toBe(0n);
  });
});

// T-02-06: a search-and-replace over "8" in ffi-helpers.ts is itself the bug D-11 exists to
// prevent -- allocPtrOut/allocUint64Out allocate 8 bytes for reasons wholly unrelated to the
// 24-byte options struct (a pointer-out and a u64-out parameter) and must stay 8 forever.
describe("unrelated 8-byte allocators are unaffected by the options struct growth", () => {
  test("allocPtrOut still allocates exactly 8 bytes", () => {
    expect(allocPtrOut().buf.length).toBe(8);
  });

  test("allocUint64Out still allocates exactly 8 bytes", () => {
    expect(allocUint64Out().buf.length).toBe(8);
  });
});

// T-02-07: SCALAR_METADATA_SIZE/GROUP_METADATA_SIZE relocated here from metadata.ts so loader.ts
// can assert them without importing database.ts. Values unchanged.
describe("relocated metadata size constants", () => {
  test("SCALAR_METADATA_SIZE is 56", () => {
    expect(SCALAR_METADATA_SIZE).toBe(56);
  });

  test("GROUP_METADATA_SIZE is 32", () => {
    expect(GROUP_METADATA_SIZE).toBe(32);
  });
});
