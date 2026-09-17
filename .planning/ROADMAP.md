# Roadmap: Quiver

## Shipped Milestones

- **v1.0 — CSV reading for the Lua runner** (2026-09-16) — 3 phases, 9 plans, 26/26 requirements.
  A Lua script reads a real, dirty CSV off disk instead of having its contents transcribed into the
  script. Full record: [`milestones/v1.0-ROADMAP.md`](milestones/v1.0-ROADMAP.md) ·
  [`milestones/v1.0-REQUIREMENTS.md`](milestones/v1.0-REQUIREMENTS.md) ·
  [`v1.0-MILESTONE-AUDIT.md`](v1.0-MILESTONE-AUDIT.md)

- **v1.1 — CSV writing for the Lua runner** (2026-09-17) — 2 phases, 6 plans, 29/29 requirements.
  A Lua script writes a CSV file to disk — and a file written by an imperfect script, with a ragged
  row or a forgotten `close()`, still reads back complete and aligned. Full record:
  [`milestones/v1.1-ROADMAP.md`](milestones/v1.1-ROADMAP.md) ·
  [`milestones/v1.1-REQUIREMENTS.md`](milestones/v1.1-REQUIREMENTS.md) ·
  [`v1.1-MILESTONE-AUDIT.md`](v1.1-MILESTONE-AUDIT.md)

## Current Milestone

None. Run `/gsd-new-milestone` to define the next one — it gathers requirements fresh, so
`.planning/REQUIREMENTS.md` is intentionally absent until then.

Deferred to a future milestone (carried from v1.1's requirements):

| Category | Item |
|----------|------|
| TOML | `TOML-01`/`TOML-02` — a Lua TOML reader and writer (toml++ already vendored) |
| Core unification | `UNIFY-01..03` — `import_csv` onto the shared parser, `separator` in `CSVOptions`, import-side quoting tests |
| Write throughput | `PERF-01`/`PERF-02` — one prepared statement per group insert, a bounded-memory append path |

## Phase Numbering

- Integer phases (1, 2, 3): planned milestone work.
- Decimal phases (2.1, 2.2): inserted after the roadmap is written, when work is discovered
  mid-milestone.
- Numbering continues across milestones: v1.0 ended at Phase 3, v1.1 ended at Phase 5.

---
*v1.1 archived: 2026-09-17*
