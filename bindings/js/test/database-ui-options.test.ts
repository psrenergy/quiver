import { describe, expect, test } from "bun:test";
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
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
const MALFORMED_UI_DIR = join(
  __dirname,
  "..",
  "..",
  "..",
  "tests",
  "schemas",
  "ui",
  "malformed",
  "ui",
);

// A fresh scratch directory with no `ui/` sibling of its own -- what makes the explicit
// uiConfigDir proof real (the convention path <db_dir>/ui/ cannot resolve here).
function makeScratchDbDir(): string {
  return mkdtempSync(join(tmpdir(), "quiver_js_ui_options_"));
}

// Gap 7 (02-VERIFICATION.md): fromMigrations() needs a migrations directory that creates the
// EconomicDriver schema the foresight_like sidecar labels -- no such fixture is checked in (the
// plan's files_modified list is the six test files only), so this writes one at runtime,
// mirroring foresight_like/schema.sql's own DDL. Nothing here is committed.
function makeScratchMigrationsDir(): string {
  const dir = makeScratchDbDir();
  const upDir = join(dir, "1");
  mkdirSync(upDir, { recursive: true });
  writeFileSync(join(upDir, "up.sql"), readFileSync(FORESIGHT_SCHEMA, "utf8"));
  writeFileSync(join(upDir, "down.sql"), "DROP TABLE EconomicDriver;\nDROP TABLE Configuration;\n");
  return dir;
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

  // Gap 5 (02-VERIFICATION.md): malformed polarity, previously C++-only.
  test("malformedSidecarReportsFalseWithoutThrowing", () => {
    const dir = makeScratchDbDir();
    try {
      const db = Database.fromSchema(join(dir, "test.sqlite"), FORESIGHT_SCHEMA, {
        uiConfigDir: MALFORMED_UI_DIR,
      });
      try {
        expect(db.hasUiConfig()).toBe(false);
      } finally {
        db.close();
      }
    } finally {
      rmSync(dir, { recursive: true, force: true });
    }
  });

  // Gap 5 (02-VERIFICATION.md): the D-01 :memory: distinction, previously C++-only.
  test("memoryDatabaseDistinction", () => {
    const withDir = Database.fromSchema(":memory:", FORESIGHT_SCHEMA, {
      uiConfigDir: FORESIGHT_UI_DIR,
    });
    try {
      expect(withDir.hasUiConfig()).toBe(true);
    } finally {
      withDir.close();
    }

    const withoutDir = Database.fromSchema(":memory:", FORESIGHT_SCHEMA);
    try {
      expect(withoutDir.hasUiConfig()).toBe(false);
    } finally {
      withoutDir.close();
    }
  });

  // Gap 7 (02-VERIFICATION.md): open() with uiConfigDir + uiLocale, previously proven only
  // through fromSchema() by a committed test.
  test("openThreadsUiConfigDirAndLocale", () => {
    const dir = makeScratchDbDir();
    try {
      const dbPath = join(dir, "test.sqlite");
      Database.fromSchema(dbPath, FORESIGHT_SCHEMA).close();

      const db = Database.open(dbPath, { uiConfigDir: FORESIGHT_UI_DIR, uiLocale: "es" });
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

  test("fromMigrationsThreadsUiConfigDirAndLocale", () => {
    const migrationsDir = makeScratchMigrationsDir();
    const dbDir = makeScratchDbDir();
    try {
      const db = Database.fromMigrations(join(dbDir, "test.sqlite"), migrationsDir, {
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
      rmSync(migrationsDir, { recursive: true, force: true });
      rmSync(dbDir, { recursive: true, force: true });
    }
  });

  // Gap 7 (02-VERIFICATION.md): a locale label through whole-database describe(), not only
  // describeCollection().
  test("describeCarriesLocaleSpecificLabel", () => {
    const dir = makeScratchDbDir();
    try {
      const db = Database.fromSchema(join(dir, "test.sqlite"), FORESIGHT_SCHEMA, {
        uiConfigDir: FORESIGHT_UI_DIR,
        uiLocale: "es",
      });
      try {
        const report = db.describe();
        expect(report).toContain("Ingenuo Estacional");
        expect(report).not.toContain("Seasonal Naïve");
      } finally {
        db.close();
      }
    } finally {
      rmSync(dir, { recursive: true, force: true });
    }
  });

  // OPT-03/empty (02-09 edge lift): an empty-string uiLocale is unset (D-03), not a locale named
  // "".
  test("emptyStringUiLocaleMatchesUnsetOutput", () => {
    const dir = makeScratchDbDir();
    try {
      const db = Database.fromSchema(join(dir, "test.sqlite"), FORESIGHT_SCHEMA, {
        uiConfigDir: FORESIGHT_UI_DIR,
        uiLocale: "",
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
});
