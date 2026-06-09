import { describe, expect, test } from "bun:test";
import { join } from "node:path";

const __dirname = import.meta.dir;

import { Database, QuiverError } from "../src/index.ts";

const SCHEMA_PATH = join(__dirname, "..", "..", "..", "tests", "schemas", "valid", "all_types.sql");

// The happy path (a real exit code from CarbSteeler) is not testable hermetically because the
// model directory is a hardcoded constant in the native layer; it is verified manually. The
// in-memory precondition fires before the model directory is consulted, so it is deterministic.
describe("run model", () => {
  test("runModel is bound and throws on an in-memory database", () => {
    const db = Database.fromSchema(":memory:", SCHEMA_PATH);
    try {
      expect(typeof db.runModel).toEqual("function");
      expect(() => db.runModel()).toThrow(QuiverError);
    } finally {
      db.close();
    }
  });
});
