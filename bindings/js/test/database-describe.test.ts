import { describe, expect, test } from "bun:test";

const __dirname = import.meta.dir;

import { join } from "node:path";
import { Database } from "../src/index.ts";

const SCHEMA_PATH = join(
  __dirname,
  "..",
  "..",
  "..",
  "tests",
  "schemas",
  "valid",
  "collections.sql",
);

function openDb(): Database {
  return Database.fromSchema(":memory:", SCHEMA_PATH);
}

// Shared tests/schemas/ui/ fixture corpus (CORPUS-03) -- never copied into this binding.
function uiFixture(name: string): string {
  return join(__dirname, "..", "..", "..", "tests", "schemas", "ui", name);
}

function openUiFixture(name: string, stem: string): Database {
  const dir = uiFixture(name);
  return Database.fromSchema(
    join(dir, `js_${stem}.sqlite`),
    join(dir, "schema.sql"),
  );
}

describe("describe", () => {
  test("returns a string", () => {
    const db = openDb();
    try {
      expect(typeof db.describe()).toBe("string");
    } finally {
      db.close();
    }
  });
});

describe("describeCollection", () => {
  test("returns a string", () => {
    const db = openDb();
    try {
      expect(typeof db.describeCollection("Collection")).toBe("string");
    } finally {
      db.close();
    }
  });
});

describe("summarizeCollection", () => {
  test("returns a string", () => {
    const db = openDb();
    try {
      expect(typeof db.summarizeCollection("Collection")).toBe("string");
    } finally {
      db.close();
    }
  });
});

// DESC-07: exact-string enum rendering against the shared tests/schemas/ui/ fixtures -- a
// "returns a string" assertion alone cannot catch a per-binding decoding bug (D-31).

describe("enum vocabulary declared with zero elements", () => {
  test("renders the declared vocabulary", () => {
    const db = openUiFixture("enum_basic", "declared");
    try {
      const report = db.describeCollection("Storage");
      expect(report).toContain("enum bool {0: Disabled, 1: Enabled}");
    } finally {
      db.close();
    }
  });
});

describe("enum histogram over twelve elements", () => {
  test("renders the value histogram", () => {
    const db = openUiFixture("enum_basic", "histogram");
    try {
      for (let i = 0; i < 8; i++) {
        db.createElement("Storage", { label: `Disabled ${i}`, has_commitment: 0 });
      }
      for (let i = 0; i < 4; i++) {
        db.createElement("Storage", { label: `Enabled ${i}`, has_commitment: 1 });
      }
      const report = db.summarizeCollection("Storage");
      expect(report).toContain("values {0: 8 (Disabled), 1: 4 (Enabled)}");
    } finally {
      db.close();
    }
  });
});

describe("UI config header line", () => {
  test("names the sidecar path and locale", () => {
    const db = openUiFixture("enum_basic", "header");
    try {
      const report = db.describe();
      expect(report).toContain("UI config: ");
      expect(report).toContain(" (locale: en)");
    } finally {
      db.close();
    }
  });
});

describe("unit and hidden decoration", () => {
  test("renders unit and hidden markers", () => {
    const db = openUiFixture("enum_basic", "unit_hidden");
    try {
      const report = db.describeCollection("Storage");
      expect(report).toContain("[MW]");
      expect(report).toContain("[hidden]");
    } finally {
      db.close();
    }
  });
});

describe("accented enum label renders byte-for-byte", () => {
  test("renders Seasonal Naïve", () => {
    const db = openUiFixture("foresight_like", "accented");
    try {
      const report = db.describeCollection("EconomicDriver");
      expect(report).toContain("Seasonal Naïve");
    } finally {
      db.close();
    }
  });
});

describe("no ui/ directory leaves the header off", () => {
  test("renders no UI config header", () => {
    const db = openUiFixture("no_ui_dir", "no_sidecar");
    try {
      const report = db.describe();
      expect(report).not.toContain("UI config: ");
    } finally {
      db.close();
    }
  });
});
