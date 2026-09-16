# Roadmap: Quiver

## Shipped Milestones

- **v1.0 — CSV reading for the Lua runner** (2026-09-16) — 3 phases, 9 plans, 26/26 requirements.
  A Lua script reads a real, dirty CSV off disk instead of having its contents transcribed into the
  script. Full record: [`milestones/v1.0-ROADMAP.md`](milestones/v1.0-ROADMAP.md) ·
  [`milestones/v1.0-REQUIREMENTS.md`](milestones/v1.0-REQUIREMENTS.md) ·
  [`v1.0-MILESTONE-AUDIT.md`](v1.0-MILESTONE-AUDIT.md)

## Current Milestone

None. Run `/gsd-new-milestone` to start the next one — it gathers requirements and writes a fresh
`REQUIREMENTS.md` and phase plan.

Carried forward from v1.0 as deferred: **CSV writing** (`db:write_csv`). Reading was the only
direction in v1.0.

## Phase Numbering

- Integer phases (1, 2, 3): planned milestone work.
- Decimal phases (2.1, 2.2): inserted after the roadmap is written, when work is discovered
  mid-milestone.
