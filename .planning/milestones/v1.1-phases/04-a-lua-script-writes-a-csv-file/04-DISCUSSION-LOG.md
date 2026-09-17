# Phase 4: A Lua script writes a CSV file - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-09-16
**Phase:** 4-a-lua-script-writes-a-csv-file
**Areas discussed:** none — all four presented areas were delegated to Claude

---

## Gray areas presented

Four areas were offered for discussion, chosen as the ones the requirements and the milestone
research genuinely leave open. The user declined all four in one answer.

| Area | Question put to the user | Selected |
|------|--------------------------|----------|
| Float vs integer text | `std::to_chars` writes the float `2014.0` as `2014`, identical to the integer `2014`. Does a float keep a `.0`, or is the distinction dropped? | |
| DOC-05 example size | Phase 3 precedent was the FULL ~35-line example at ~1000 tokens per `claw` session, permanently. Same treatment for the writer, or compact? | |
| Handle name + error voice | Usertype `Writer` or `CsvWriter` (becomes the `lua-api-sync.test.ts` array entry). Do `w:write_row` errors say "Cannot write_row" or "Cannot write_csv"? | |
| Where the code lives | New `src/csv_write.{h,cpp}` mirroring `csv_read`, or ~90 lines inside `lua_runner.cpp`? The Pimpl rationale is gone, but that file is already 1984 lines. | |

**User's choice:** *(free text)* "i dont want to discuss anything, use the best best practices and
good code"

**Notes:** The first presentation of this question received no answer; it was re-presented once per
the workflow's answer-validation rule and answered as above. No area was selected, so no per-area
discussion loop ran.

---

## Claude's Discretion

All four areas, decided in CONTEXT.md as D-34 through D-40. Summary of what was chosen and what was
rejected, so a reviewer can overturn one without re-reading the reasoning:

| Decision | Chosen | Rejected alternative |
|---|---|---|
| D-34 | No synthetic `.0` — `append_number` output verbatim; `2014.0` writes `2014` | Decorating whole floats with `.0` for downstream type inference |
| D-35 | Usertype named `CsvWriter` | Bare `Writer` — too generic in the shared Lua registry, collides with v2 `TOML-02` |
| D-36 | Errors name the method actually called (`write_csv` / `write_row` / `close`) | One fixed `write_csv` voice for every error the feature raises |
| D-37 | `src/csv_write.{h,cpp}`, plain concrete class | Pimpl (no third-party headers to hide); inlining into `lua_runner.cpp` |
| D-38 | `append_number` → `src/utils/number.h` | Duplicating it; templating the writer over a formatter |
| D-39 | Compact ~10-12 line worked example, verified to run | Phase 3's full ~35-line form — its justification (the dirty-file transformation traps) has no write-side equivalent |
| D-40 | State that `nil` and `""` are indistinguishable after a round trip | Any sentinel or option to preserve the difference |

Four smaller calls also settled rather than left to the planner: `header = {}` means no header row;
a ragged row is written as-is in Phase 4 (FMT-07 is Phase 5); tests go in
`tests/test_lua_runner_write_csv.cpp`; the options table is optional and `w:close()` returns nothing.

## Deferred Ideas

Nothing new surfaced — there was no discussion in which scope creep could occur. The deferred list
in CONTEXT.md is carried forward from REQUIREMENTS.md and the phase split, not generated here.
