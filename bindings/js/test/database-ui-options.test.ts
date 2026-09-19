import { describe, expect, test } from "bun:test";
import { mkdtempSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { Database } from "../src/index.ts";

const __dirname = import.meta.dir;

// Shared tests/schemas/ui/ fixture corpus (CORPUS-03) -- never copied into this binding.
// DatabaseUiCorpus.FixturesAreNeverCopiedIntoABinding (C++ suite) fails if a main.toml/enum.toml
// lands under a directory named "ui" anywhere in bindings/.
const FORESIGHT_DIR = join(__dirname, "..", "..", "..", "tests", "schemas", "ui", "foresight_like");
const FORESIGHT_SCHEMA = join(FORESIGHT_DIR, "schema.sql");
const FORESIGHT_UI_DIR = join(FORESIGHT_DIR, "ui");

// A fresh scratch directory with no `ui/` sibling of its own -- what makes the explicit
// uiConfigDir proof real (the convention path <db_dir>/ui/ cannot resolve here).
function makeScratchDbDir(): string {
  return mkdtempSync(join(tmpdir(), "quiver_js_ui_options_"));
}

describe("Database UI options (uiConfigDir / uiLocale / hasUiConfig)", () => {
  test("explicitUiConfigDirLoadsAConfigNotBesideTheDatabase", () => {
    const dir = makeScratchDbDir();
    try {
      const db = Database.fromSchema(join(dir, "test.sqlite"), FORESIGHT_SCHEMA, {
        uiConfigDir: FORESIGHT_UI_DIR,
      });
      try {
        expect(db.hasUiConfig()).toBe(true);
      } finally {
        db.close();
      }
    } finally {
      rmSync(dir, { recursive: true, force: true });
    }
  });

  test("spanishLocaleRendersSpanishLabels", () => {
    const dir = makeScratchDbDir();
    try {
      const db = Database.fromSchema(join(dir, "test.sqlite"), FORESIGHT_SCHEMA, {
        uiConfigDir: FORESIGHT_UI_DIR,
        uiLocale: "es",
      });
      try {
        const report = db.describeCollection("EconomicDriver");
        expect(report).toContain("Ingenuo Estacional");
        expect(report).toContain("Tendencia Lineal Local");
        expect(report).not.toContain("Seasonal Naïve");
      } finally {
        db.close();
      }
    } finally {
      rmSync(dir, { recursive: true, force: true });
    }
  });

  test("defaultLocaleRendersEnglishLabels", () => {
    const dir = makeScratchDbDir();
    try {
      const db = Database.fromSchema(join(dir, "test.sqlite"), FORESIGHT_SCHEMA, {
        uiConfigDir: FORESIGHT_UI_DIR,
      });
      try {
        const report = db.describeCollection("EconomicDriver");
        expect(report).toContain("Seasonal Naïve");
        expect(report).not.toContain("Ingenuo Estacional");
      } finally {
        db.close();
      }
    } finally {
      rmSync(dir, { recursive: true, force: true });
    }
  });

  test("missingUiConfigDirDegradesWithoutThrowing", () => {
    const dir = makeScratchDbDir();
    try {
      expect(() => {
        const db = Database.fromSchema(join(dir, "test.sqlite"), FORESIGHT_SCHEMA, {
          uiConfigDir: join(dir, "nonexistent-ui-dir"),
        });
        try {
          expect(db.hasUiConfig()).toBe(false);
        } finally {
          db.close();
        }
      }).not.toThrow();
    } finally {
      rmSync(dir, { recursive: true, force: true });
    }
  });
});
