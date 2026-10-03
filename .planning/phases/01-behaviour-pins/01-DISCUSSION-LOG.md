# Phase 1: Behaviour Pins - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> The decisions are in CONTEXT.md. This log keeps the alternatives that were considered.

**Date:** 2026-10-02
**Phase:** 01-behaviour-pins
**Areas discussed:** Check-order breadth, Sync-test guard strength, Test placement, Message matching,
Rename scope (raised by the user mid-discussion), Baseline location

---

## Check-order breadth

| Option | Description | Selected |
|--------|-------------|----------|
| Sweep every op | One two-bad-argument test per unpinned op: open_file, read_csv, read_csv_stream, export_csv, import_csv, write_row | ✓ |
| Only the three named | open_file, one CSV op, write_row | |
| Sweep + every multi-arg method | Pitfall 8's wider reading; many sites are Release UB until Phase 4 | |

| Option | Description | Selected |
|--------|-------------|----------|
| Pin both write_row edges | type before closed, and closed before cells | ✓ |
| Type-before-closed only | What PIN-02 names | |

| Option | Description | Selected |
|--------|-------------|----------|
| Non-table, e.g. 5 | Catches a require_table placed at the top of the lambda; adds one write_csv pin | ✓ |
| Bad value inside a table | Same style as the existing write_csv pins | |
| Both forms per op | About 10 CSV pins | |

| Option | Description | Selected |
|--------|-------------|----------|
| Escaping path only | Both failure modes are in one resolve call | ✓ |
| Both variants | Adds :memory: tests | |

**Notes:** while writing the context, read_csv_stream turned out to check `on_row` before containment.
That edge is pinned as part of the sweep.

---

## Sync-test guard strength

| Option | Description | Selected |
|--------|-------------|----------|
| Floors only | >0 per usertype, exactly one open_libraries( | ✓ |
| Floors + exact usertype counts | Catches a single dropped method | |
| Committed snapshot | Full parsed name list; a third place to edit | |

---

## Test placement

| Option | Description | Selected |
|--------|-------------|----------|
| Matching area files | Existing fixtures, tests/AGENTS.md convention | ✓ |
| One dedicated pins file | Named after a milestone, not a behaviour | |

| Option | Description | Selected |
|--------|-------------|----------|
| New test_lua_runner_lifecycle.cpp | Mirrors test_database_lifecycle.cpp | ✓ |
| test_lua_binary.cpp | Next to HandleFromAnEarlierRunIsClosed | |
| test_lua_runner_write_csv.cpp | Next to the writer-flush pins | |

| Option | Description | Selected |
|--------|-------------|----------|
| Two tests | MoveConstructor / MoveAssignment | ✓ |
| One combined test | | |

---

## Message matching

| Option | Description | Selected |
|--------|-------------|----------|
| Full sentence via expect_lua_error | No new helper | ✓ |
| New exact-match helper | Strip the wrapper, then EXPECT_EQ | |

| Option | Description | Selected |
|--------|-------------|----------|
| Full winning sentence | Pins both the operation name and the reason | ✓ |
| Reason substring only | | |

**Notes:** the containment message ends with the resolved root, which depends on the temp
directory. Pins assert up to "escapes the database directory" (Claude's call, matching the
existing tests).

---

## Rename scope (raised by the user)

**User's message:** "lets not change the name from lua_runner to sandbox in this milestone, to keep
it simple"

| Option | Description | Selected |
|--------|-------------|----------|
| src/lua_runner/ | Phase 2 folder matches the class | ✓ |
| src/lua/ | Shorter | |
| Keep src/sandbox/ | Disagrees with the class | |

| Option | Description | Selected |
|--------|-------------|----------|
| Drop the rename tail | No renames at all; keep the "what the sandbox does not limit" doc sentence | ✓ |
| Keep the internal wording changes | resolve_contained_path, "directory containment" | |

| Option | Description | Selected |
|--------|-------------|----------|
| Keep a slim Phase 5 | TEST-01 + docs sweep | ✓ |
| Fold into Phase 4 | Four phases | |

---

## Baseline location

| Option | Description | Selected |
|--------|-------------|----------|
| STATE.md + phase summary | Planning-only | ✓ |
| tests/AGENTS.md | Goes stale | |

---

## Claude's Discretion

- Test and fixture names; the export/import collection strings; how the move tests observe closure;
  the non-table value used in the write_row pin.

## Deferred Ideas

- None. The rename removal is applied to ROADMAP, REQUIREMENTS and PROJECT as a separate step.
