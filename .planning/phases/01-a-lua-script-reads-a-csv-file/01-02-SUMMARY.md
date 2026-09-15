---
phase: 01-a-lua-script-reads-a-csv-file
plan: 02
subsystem: database
tags: [lua, sol2, csv-parser, options-decoding]

requires:
  - phase: 01-01
    provides: "quiver::csv_read::Reader (Options.separator field), db:read_csv and db:read_csv_stream bindings, LuaSandboxTest fixture"
provides:
  - "One shared strict options decoder (read_csv_options_from_lua) wired into both db:read_csv and db:read_csv_stream"
  - "The separator option: { separator = \";\" }, defaulting to \",\", identical behavior on both entry points"
  - "The Phase-1 rejection matrix: options-must-be-a-table, unknown-option-key, separator-must-be-a-string, separator-must-be-a-single-character, each naming the calling entry point"
  - "LUA_DB_API_REFERENCE shows the options table as a literal { separator = \",\" } on both signatures"
affects: ["01-03"]

actuals:
  tokens: 5615
  tasks: 2
  commits: 2

tech-stack:
  added: []
  patterns:
    - "Strict sol::object option decoding (copied from relation_target_from_lua) applied to a second call site, ruling out sol2's silent sol::optional<sol::table> type-mismatch swallow"

key-files:
  created: []
  modified:
    - src/lua_runner.cpp
    - bindings/js/src/lua-api.ts
    - tests/test_lua_runner_read_csv.cpp

key-decisions:
  - "Options are decoded AFTER resolve_sandboxed_path, not before, to match D-22's locked evaluation order (in-memory db / path escape must be checked before the options table). The two bindings originally computed csv_options before calling resolve_sandboxed_path in the same statement; caught and reordered while writing Task 2's rejection-matrix tests, since a combined bad-path + bad-options call would otherwise report the wrong error first."
  - "Collect-then-validate for the options table's for_each walk (per D-17): entries are copied into a std::vector<std::pair<std::string, sol::object>> before any throw, since throwing out of sol2's for_each abandons the traversal mid-stack."
  - "clang-format wrapped db:read_csv_stream's binding onto a single bind.set_function(\"name\", [lambda...]) line differently from db:read_csv's two-line (name, then lambda) wrap -- both are clang-format's own output for the same house style, kept as-is per the project's format-then-accept convention (mirrors the Wave 1 SUMMARY's identical note)."

patterns-established:
  - "A second db: entry-point pair sharing one strict sol::object decoder, extending the relation_target_from_lua precedent beyond its original single call site"

requirements-completed: [PARSE-08, LUA-08, DOC-01]

coverage:
  - id: D1
    description: "A file using a non-comma separator reads correctly via { separator = \";\" }; the separator defaults to \",\" when the options table is absent"
    requirement: "PARSE-08"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_read_csv.cpp#LuaRunner_ReadCsv.SemicolonSeparatorReadsCorrectly"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_read_csv.cpp#LuaRunner_ReadCsv.DefaultSeparatorMatchesEmptyOptionsTable"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_read_csv.cpp#LuaRunner_ReadCsv.TabSeparatorProvesOptionIsNotSpecialCased"
        status: pass
    human_judgment: false
  - id: D2
    description: "Both entry points take the options table as the same trailing optional parameter, decoded by one shared decoder, so they cannot diverge on any option"
    requirement: "LUA-08"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_read_csv.cpp#LuaRunner_ReadCsv.StreamSemicolonSeparatorYieldsSameRowsAsWholeFileRead"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_read_csv.cpp#LuaRunner_ReadCsv.BothFormsAgreeOnAValidTableAndNeitherLeavesTheFileOpen"
        status: pass
    human_judgment: false
  - id: D3
    description: "A separator passed positionally (db:read_csv(\"f.csv\", \";\")) raises 'options must be a table' instead of silently parsing with the default separator; same for a number and a boolean in that slot"
    requirement: "LUA-08"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_read_csv.cpp#LuaRunner_ReadCsv.PositionalSeparatorStringThrowsOptionsMustBeATable"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_read_csv.cpp#LuaRunner_ReadCsv.NumberInOptionsSlotThrowsOptionsMustBeATable"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_read_csv.cpp#LuaRunner_ReadCsv.BooleanInOptionsSlotThrowsOptionsMustBeATable"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_read_csv.cpp#LuaRunner_ReadCsv.StreamPositionalSeparatorStringThrowsOptionsMustBeATable"
        status: pass
    human_judgment: false
  - id: D4
    description: "An unknown option key throws naming the key, even alongside a valid sibling key or a future (Phase 2) header key"
    requirement: "LUA-08"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_read_csv.cpp#LuaRunner_ReadCsv.UnknownOptionKeyThrowsNamingTheKey"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_read_csv.cpp#LuaRunner_ReadCsv.FutureHeaderKeyIsAnUnknownOptionToday"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_read_csv.cpp#LuaRunner_ReadCsv.ValidKeyDoesNotExcuseAnInvalidSibling"
        status: pass
    human_judgment: false
  - id: D5
    description: "A non-string separator and a separator that is not exactly one character each throw a crafted Pattern 1 message, never a raw sol2 message"
    requirement: "LUA-08"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_read_csv.cpp#LuaRunner_ReadCsv.SeparatorAsNumberThrowsMustBeAString"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_read_csv.cpp#LuaRunner_ReadCsv.SeparatorAsBooleanThrowsMustBeAString"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_read_csv.cpp#LuaRunner_ReadCsv.EmptySeparatorThrowsMustBeASingleCharacter"
        status: pass
      - kind: unit
        ref: "tests/test_lua_runner_read_csv.cpp#LuaRunner_ReadCsv.TwoCharacterSeparatorThrowsMustBeASingleCharacter"
        status: pass
    human_judgment: false
  - id: D6
    description: "Option errors name the entry point the script actually called (read_csv vs read_csv_stream) for the same bad table"
    requirement: "LUA-08"
    verification:
      - kind: unit
        ref: "tests/test_lua_runner_read_csv.cpp#LuaRunner_ReadCsv.StreamUnknownOptionKeyNamesTheStreamEntryPoint"
        status: pass
    human_judgment: false
  - id: D7
    description: "LUA_DB_API_REFERENCE shows the options table as a literal { separator = \",\" } in both entry points' signatures, not an abstract opts placeholder"
    requirement: "DOC-01"
    verification:
      - kind: unit
        ref: "bindings/js/test/lua-api-sync.test.ts (bun test)"
        status: pass
    human_judgment: false

duration: 25min
completed: 2026-09-15
status: complete
---

# Phase 01 Plan 02: A file with a non-comma separator reads correctly Summary

**One shared `sol::object`-typed options decoder gives `db:read_csv` and `db:read_csv_stream` a `{ separator = "," }` option, and rejects every wrong type, unknown key, and malformed separator value with a Pattern 1 error naming the entry point that was actually called.**

## Performance

- **Duration:** ~25 min
- **Started:** 2026-09-15T12:10:00-03:00 (approx.)
- **Completed:** 2026-09-15T12:19:38-03:00
- **Tasks:** 2
- **Files modified:** 3

## Accomplishments

- A Lua script reads a semicolon-, tab-, or any single-character-separated file by passing
  `{ separator = ";" }` to either `db:read_csv` or `db:read_csv_stream`; the option defaults to
  `,` when the table is absent, and an empty options table `{}` behaves identically to omitting it.
- One shared file-scope decoder (`read_csv_options_from_lua`) serves both entry points, so they
  cannot diverge on any option — verified directly by a test that reads the same semicolon file
  through both forms and compares every cell.
- The decoder takes the options parameter as `sol::object`, never `sol::optional<sol::table>`,
  closing the same silent-corruption path already documented for `db:export_csv`/`db:import_csv`:
  a separator passed positionally (`db:read_csv("f.csv", ";")`) now throws
  `Cannot read_csv: options must be a table` instead of silently parsing with a comma and filling
  the database with one-wide-column garbage.
- Every wrong option type (string/number/boolean in the options slot), every unknown key
  (including a plausible abbreviation and a key Phase 2 will legitimately add), and every
  malformed separator (non-string, empty, multi-character) throws a Pattern 1 message naming the
  calling entry point — 13 new tests in the rejection matrix, all pinning the full message
  substring from D-22 rather than a loose keyword.
- `LUA_DB_API_REFERENCE` now shows the options table as a literal `{ separator = "," }` on both
  signatures instead of an abstract `opts` placeholder, and documents that an unknown key or bad
  value throws.

## Task Commits

Each task was committed atomically:

1. **Task 1: A file with a non-comma separator reads correctly — one strict decoder, both forms** — `f3d208b` (feat)
2. **Task 2: Every bad option throws — the rejection matrix** — `bab29e5` (test)

## Files Created/Modified

- `src/lua_runner.cpp` — `read_csv_options_from_lua` (the shared decoder, placed alongside
  `parse_csv_options`); `db:read_csv` and `db:read_csv_stream` bindings extended with the trailing
  `sol::object options` parameter, each threading its own operation name into the decoder;
  `resolve_sandboxed_path` now runs before options decoding in both bindings (D-22 ordering fix)
- `bindings/js/src/lua-api.ts` — both `db:read_csv`/`db:read_csv_stream` signatures now show
  `{ separator = "," }` literally; one new sentence stating `separator` is the only key and that a
  bad key or value throws
- `tests/test_lua_runner_read_csv.cpp` — 4 happy-path tests (Task 1: semicolon read, stream/whole
  agreement, empty-vs-absent options, tab separator) and 13 rejection-matrix tests (Task 2:
  three wrong-type cases, three unknown-key cases, four bad-separator-value cases, two
  per-entry-point-naming cases, one final stream/whole agreement + file-not-left-open check)

## Decisions Made

- **D-22 evaluation-order fix (Rule 1 — bug):** Both bindings originally computed the decoded
  `Options` before calling `resolve_sandboxed_path` (in the same constructor-argument-list
  statement), which meant a bad options table would report before an escaping path — the reverse
  of D-22's locked order (`in-memory db` → `path escape` → `options must be a table` → ...).
  Caught while writing Task 2's rejection-matrix tests (none of which exercise the interaction, but
  the ordering is a locked decision independent of what this plan's own tests probe) and reordered
  in both `db:read_csv` and `db:read_csv_stream` so `resolve_sandboxed_path` runs first.
- Collect-then-validate for the options table walk (D-17): `for_each` entries are copied into a
  `std::vector<std::pair<std::string, sol::object>>` before any validation, since throwing inside
  sol2's `for_each` callback abandons the table traversal mid-stack.
- clang-format wrapped `read_csv_stream`'s binding call differently from `read_csv`'s (name and
  lambda both on separate lines for `read_csv`; name inlined with the lambda opening for
  `read_csv_stream`) purely due to line-length differences in each lambda's parameter list — both
  are the formatter's own output for the same `-style=file` config and were kept as-is, matching
  the identical situation and resolution recorded in the Wave 1 (01-01) SUMMARY.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] D-22 evaluation order violated: options decoded before sandbox path check**
- **Found during:** Task 2, while writing the rejection-matrix tests
- **Issue:** Both `db:read_csv` and `db:read_csv_stream` computed the decoded `csv_read::Options`
  in a statement preceding `resolve_sandboxed_path`, so a call with both an escaping path and a
  malformed options table would report the options error first — contrary to D-22's locked
  evaluation order, which requires the in-memory/path-escape checks (entries 1-2) before the
  options-table checks (entries 3-6).
- **Fix:** Reordered both bindings to call `resolve_sandboxed_path` first, storing the resolved
  path, then decode the options table, then construct the `Reader`.
- **Files modified:** `src/lua_runner.cpp`
- **Verification:** Full `LuaRunner_ReadCsv` suite (32/32) and full `quiver_tests` (1142/1142)
  still pass after the reorder; no test in this plan specifically exercises the combined
  bad-path+bad-options interaction (that would be a TEST-03 concern for plan 03), but the fix
  restores the locked contract regardless.
- **Committed in:** `bab29e5` (Task 2 commit)

---

**Total deviations:** 1 auto-fixed (1 bug fix — evaluation-order correctness, no scope creep)
**Impact on plan:** The fix is a two-line reorder inside the two bindings already being modified by
this plan; no new files, no behavior change visible to any test in this plan's own matrix (all of
which use a path that stays inside the sandbox), and it brings the implementation into agreement
with a locked decision (D-22) that this plan's own Task 1 had temporarily violated.

## Issues Encountered

None beyond the deviation above.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- The options decoder (`read_csv_options_from_lua`) and its `separator`-only key are in place for
  Plan 03 (or a later Phase 2) to extend with header-row keys — D-14 through D-22 all constrain
  that extension: the same decoder, one more branch in the unknown-key check, no change to the
  `sol::object` parameter shape.
- Plan 03's TEST-03 sandbox-negative and full-error-catalogue coverage can now assume both
  `db:read_csv` and `db:read_csv_stream` observe D-22's full evaluation order (path checks before
  options checks before file-existence checks), since this plan corrected the one place it had
  drifted.
- No blockers.

## Self-Check: PASSED

All modified files verified present on disk; both task commit hashes (`f3d208b`, `bab29e5`)
verified present in `git log --oneline --all`.

---
*Phase: 01-a-lua-script-reads-a-csv-file*
*Completed: 2026-09-15*
