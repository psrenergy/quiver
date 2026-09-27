# 73 — `docs/rules.md` and `docs/attributes.md`: schema examples that validate, and the real migration functions

**Batch** 7 · **Severity** medium (user-facing docs teach schemas the validator rejects) · **Breaking** no · **Size** S · **Layers** docs only
**Depends on** 06 (enforces the parent FK on set and time-series tables; this plan's set examples must include it) · **Overlaps with** 06 (06 also fixes the set examples — whichever lands second checks the other's edit is there)

## Why

`docs/rules.md` and `docs/attributes.md` show example schemas that `SchemaValidator`
(`src/schema_validator.cpp`) rejects, or that are not valid SQL:

1. **Configuration without `label`.** `docs/rules.md` ~L33 says "The column `label` is not
   mandatory for a `Configuration` collection", and the example (~L36-39) has no `label` plus a
   trailing comma:
   ```sql
   CREATE TABLE Configuration (
       id INTEGER PRIMARY KEY AUTOINCREMENT,
       value1 REAL NOT NULL DEFAULT 100,
   ) STRICT;
   ```
   `Schema::is_collection` (`src/schema.cpp` ~L100) treats `Configuration` as a collection, and
   `validate_collection` (`schema_validator.cpp` ~L80-81) throws
   `Collection '...' must have a 'label' column`. The root CLAUDE.md lists
   `label TEXT UNIQUE NOT NULL` as required. The trailing comma is also a SQL syntax error.
2. **Self-reference FK with `ON UPDATE SET NULL`.** `rules.md` ~L80 and `attributes.md` ~L46 use
   `FOREIGN KEY(plant_spill_to) REFERENCES Plant(id) ON UPDATE SET NULL ON DELETE CASCADE`. The
   validator (`schema_validator.cpp` ~L326-328) requires `ON UPDATE CASCADE`. The prose right above
   it (`rules.md` ~L69, `attributes.md` ~L34) says "All references should always declare the
   `ON UPDATE CASCADE ON DELETE CASCADE` constraint". That is stricter than the validator, which
   accepts `ON DELETE CASCADE` **or** `ON DELETE SET NULL` (~L325-334); SET NULL requires a
   nullable column (~L309).
3. **Vector-relation examples without the parent FK.** In `rules.md` ~L108-115 and
   `attributes.md` ~L74-81, `HydroPlant_vector_gaugingstations` declares an FK on
   `gaugingstation_id` only. `schema_validator.cpp` ~L152-153 requires
   `must have foreign key to parent collection 'HydroPlant'`. The set examples (`rules.md`
   ~L130-136, `attributes.md` ~L96-102) have the same gap, which plan 06 makes an error.
4. **Missing commas.** `rules.md` ~L52-53 and `attributes.md` ~L14-15 have
   `minimum_generation REAL NOT NULL` followed by another column with no comma.
5. **Julia migration functions that don't exist.** `rules.md` ~L189-196 tells readers to call
   `Quiver.set_migrations_folder`, `Quiver.create_migration` and `Quiver.apply_migrations!`.
   `grep -rn "set_migrations_folder\|create_migration\|apply_migrations"` over the repo finds only
   these lines. The real entry point in every binding is `from_migrations(db_path, migrations_path)`.
   `docs/migrations.md` ~L4 also links `[Quiver](../rules.md)`, which resolves outside `docs/`.

Principle: user-facing docs must match the code.

## Constraints and decisions

- Change the docs to match the validator, never the reverse.
- Reword the FK prose to the rule the validator actually enforces: every relation FK uses
  `ON UPDATE CASCADE`, and `ON DELETE CASCADE` or `ON DELETE SET NULL` (SET NULL needs a nullable
  column). Parent FKs of group tables use `ON DELETE CASCADE ON UPDATE CASCADE`.
- Use `ON UPDATE CASCADE ON DELETE SET NULL` for `plant_spill_to`. That is what a spill-to
  self-relation means: deleting the target should not delete the plant.

## Changes

### `docs/rules.md`

1. Delete the sentence "The column `label` is not mandatory for a `Configuration` collection." (~L33).
2. Replace the Configuration example (~L36-39) with:
   ```sql
   CREATE TABLE Configuration (
       id INTEGER PRIMARY KEY AUTOINCREMENT,
       label TEXT UNIQUE NOT NULL,
       value1 REAL NOT NULL DEFAULT 100
   ) STRICT;
   ```
3. ~L52: `minimum_generation REAL NOT NULL` becomes `minimum_generation REAL NOT NULL,`.
4. ~L69, the relation sentence: "All references should always declare the `ON UPDATE CASCADE ON
   DELETE CASCADE` constraint." becomes "Every reference must declare `ON UPDATE CASCADE`, and
   either `ON DELETE CASCADE` or `ON DELETE SET NULL` (`SET NULL` requires the column to be
   nullable)."
5. ~L80: `FOREIGN KEY(plant_spill_to) REFERENCES Plant(id) ON UPDATE SET NULL ON DELETE CASCADE`
   becomes `FOREIGN KEY(plant_spill_to) REFERENCES Plant(id) ON UPDATE CASCADE ON DELETE SET NULL`.
6. Vector-relation example (~L108-115): add
   `FOREIGN KEY (id) REFERENCES HydroPlant(id) ON DELETE CASCADE ON UPDATE CASCADE,` as the first FK
   line.
7. Set example (~L130-136): the same parent FK line, if plan 06 has not already added it.
8. Replace "### Creating a migration" and "### Running migrations" (~L189-196) with:
   ```markdown
   ### Creating a migration

   There is no scaffolding helper: add a numbered directory `migrations/<N>/` containing `up.sql`
   and `down.sql`, as the tree above shows.

   ### Running migrations

   Open the database with `from_migrations(db_path, migrations_path)` (`Database.fromMigrations` in
   Dart and JS). It applies every pending `up.sql` in version order, each in its own transaction.
   ```
   Keep "### Validating migrations" as it is.

### `docs/attributes.md`

Mirror edits 3–7 at ~L14-15, ~L34, ~L46, ~L74-81 and ~L96-102.

### `docs/migrations.md`

~L4: `[Quiver](../rules.md)` becomes `[Quiver](rules.md)`.

## Tests

Docs have no automated test. Validate each edited example by hand. Paste every full `CREATE TABLE`
block from both files, plus the tables they reference (Configuration, Plant, GaugingStation,
HydroPlant), into one scratch `.sql` file, then run
`build\bin\quiver_cli.exe --schema scratch.sql :memory: tests\cli\smoke.lua`. Plan 65 creates the
smoke script. Any Lua script works, or use a tiny `C++`/Python snippet that calls `from_schema`.
The schema must load without a validation error. Delete the scratch file afterwards.

## Docs and changelog

This plan is the docs change. No CHANGELOG entry.

## Verification

1. `grep -n "not mandatory\|ON UPDATE SET NULL\|set_migrations_folder\|create_migration\|apply_migrations\|\.\./rules.md" docs/*.md`
   should print nothing.
2. Load the scratch schema, as described under Tests.

## Acceptance criteria

- [ ] Every `CREATE TABLE` example in both files is valid SQL and passes `SchemaValidator`.
- [ ] The FK prose matches the validator's rule.
- [ ] The migration section names `from_migrations`, and the link is fixed.

## Pitfalls

- Keep the documents' existing formatting (fenced `sql` blocks, heading levels).

## Out of scope

- Other docs (`docs/introduction.md`, `docs/time_series.md`). Plan 04 edits `time_series.md`.
