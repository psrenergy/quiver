# Phase 4: Fixes and Release Type Safety - Context

**Gathered:** 2026-10-03
**Status:** Ready for planning
**Mode:** Smart discuss (autonomous). The user accepted the recommended answer in all 4 grey areas.

<domain>
## Phase Boundary

In Release builds, a wrong-type argument from an untrusted script raises a Pattern 1 error instead of undefined behaviour. The C4/C6/C7/C8 bugs are fixed, `load` is text-only, and each fix lands red-then-green with its own test and CHANGELOG line.

Requirements: SAFE-01..07, FIX-01..03. The CHANGELOG `[0.13.0] — unreleased` section and its compare link open here. No version bump: every manifest is already at 0.13.0.

Out of scope: the `resolve_sandboxed_path` unit test (TEST-01), the repo-wide planning-ID sweep (DOC-01), and the "what the sandbox does not limit" sentence (DOC-04 part). All three are Phase 5. Also out: the sparse-extent cap (C3), which the user decided against, and M6.

</domain>

<decisions>
## Implementation Decisions

### Type-error message shape (SAFE-01/02/03/05)
- **D-01:** Template: `Cannot <op>: <arg> must be a table, got <lua type>`, which extends the existing "options must be a table" style. Key and optional-argument checks follow the same shape: `Cannot <op>: <what> must be <expected>, got <lua type>`.
- **D-02:** `<lua type>` is Lua's own `type()` name (`number`, `string`, `boolean`, `userdata`, `function`, `nil`, `table`). It is the same in every build. A usertype prints as `userdata`.
- **D-03:** The 9 existing hand-written table checks go through `require_table` and gain `, got <type>`. Existing tests match by substring (`expect_lua_error`), so they stay green without editing their expectations.
- **D-04:** The existing "has unsupported Lua type" converter messages (`lua_to_value`, `lua_cell_as`) stay unchanged. They are value-converter errors, not SAFE-05's checks.

### Text-only load (SAFE-07)
- **D-05:** The global `load` is replaced by a wrapper that always calls the original `load` with mode `"t"`, whatever mode the caller passed. It is installed in the `Impl` constructor next to the `dofile`/`loadfile` nil-out, keeping the constructor order (open_libraries → sandbox edits → `quiver` table → binders → `db`).
- **D-06:** The failure surface keeps `load`'s contract: a bytecode chunk makes `load` return `nil` plus Lua's own message, "attempt to load a binary chunk (mode is 't')". There is no new Pattern 1 text. Tests check both the binary rejection and that string-form `load` (with and without an explicit mode/env) still works.
- **D-07:** `string.dump` stays. Its output is just a string once `load` refuses binary chunks.
- The root AGENTS.md sandbox decision gains "`load` accepts text chunks only".

### Safety flags and measurement (SAFE-06)
- **D-08:** Perf protocol: Release `quiver_cli` wall time, median of 5 runs, taken before and after the flag commit. Workloads: a 100k-element `read_scalar_floats` bulk read and a 1M-cell `file:read` loop. Run by hand and reported in the PR and SUMMARY, with no committed perf script. The budget is 5%. If it is exceeded, add `SOL_SAFE_GETTER=0` and `SOL_SAFE_STACK_CHECK=0` and re-measure. `SOL_SAFE_FUNCTION_CALLS` and `SOL_SAFE_USERTYPE` are never disabled.
- **D-09:** The stderr-silence test uses gtest `CaptureStderr` around two scripts: one whose error is caught by `pcall` inside the script, and one whose error propagates out of `run()`. Both must leave stderr empty with `SOL_PRINT_ERRORS=0`.
- **D-10:** A Release dot-call (`db.commit()`, which exits 139 today) is covered by the `SOL_ALL_SAFETIES_ON` backstop's raw sol2 text (`sol: received nil for 'self' argument…`), as the roadmap says. A test asserts that a dot-call throws, not crashes, in both builds. There is no explicit Pattern 1 self-check.
- The flags land as the phase's LAST commit, after every explicit check (SAFE-01..05, SAFE-07, FIX-01..03). The no-op `SOL_SAFE_FUNCTION=1` define and its AGENTS.md claim are deleted in the same commit.

### Commit discipline and CHANGELOG
- **D-11:** Red-then-green: each fix is one commit holding both the new test(s) and the fix. "Red" is shown by running the new test against the pre-fix tree before applying the fix, with the failing output recorded in the plan SUMMARY. Every commit stays green, so the series bisects.
- **D-12:** The C7 BREAKING entry also warns that a `read_vectors_by_id` → `update_element` round trip of an empty or all-NULL column now clears that group instead of skipping it, alongside the clear-on-`{col = {}}` and the typo'd-empty-column throw.
- **D-13:** New tests go in the existing per-domain files (`tests/test_lua_runner_*.cpp`, `tests/test_lua_binary.cpp`, `tests/test_lua_expression.cpp`). No new test file.
- CHANGELOG (roadmap criterion 5): `## [0.13.0] — unreleased` plus the compare link `[0.13.0]: https://github.com/psrenergy/quiver/compare/v0.12.9...v0.13.0` in the existing link block. BREAKING entries (each saying what a script author must change): wrong-type arguments now throw (C1/C5), the empty-array change (C7, with D-12), text-only `load`, and the Release dot-call going from UB to an error with sol2's raw text. `### Fixed`: C2, C4, C6, C8. Behaviour wording only, no planning IDs.
- At the phase end, record the new `Lua*` and C API gtest counts as the baseline Phase 5 must reproduce.

### Post-research resolutions (orchestrator decisions, flagged to the user)
- **D-14:** In `table_to_element`, a userdata cell gets its own check before dispatch. The roadmap's criterion 1 explicitly names "`table_to_element` cells". The message is `Cannot <op>: attribute '<name>' must be a value or a table, got userdata`, carrying the SAFE-05 suffix. The converter messages in `lua_to_value`/`lua_cell_as` stay unchanged (D-04 holds), so this resolves the criterion-1 vs D-04 conflict the research raised.
- **D-15:** `metadata_from_element`'s argument is named `element_table` in messages. The reference's `tbl` reads badly in an error.
- **D-16:** The second "must be an array of values" message in `collect_group_columns` reports a bad cell key, not an argument type, so it does not gain the `got <type>` suffix.
- **D-17:** Expression errors name Lua's event names for operators (`add`, `sub`, `mul`, `div`, `unm`, `band`, `bor`, `bnot`) and the function name for helpers (`abs`, `gt`, `ifelse`, …), following Pattern 1's `{operation}` rule.
- **D-18:** Golden-probe outputs that change on purpose (about 40: the new suffix, the `transaction`/`dry_run` Debug text, the expression wording) are re-baselined per fix commit and listed in the plan SUMMARY. Every other probe stays byte-identical.

### Claude's Discretion
- The exact helper signatures (`require_table(obj, operation, what)`, `lua_string_key(key, operation, what)`, `optional_from_lua<T>(obj, operation, what)` with `luaL_opt` semantics and `is<BinaryMetadata>()` for the usertype) and the argument names used in messages. Prefer the parameter names in `LUA_DB_API_REFERENCE`.
- Plan and wave grouping by file overlap. Phase 3's single-helper layout means each fix lands in one place.

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- `build/dedupe-check/` (gitignored) is the golden harness from Phase 3: `quiver_cli` probes in Debug and Release diffed against the base, plus `gate.sh`/`wave_gate.sh`. Reuse it to show that untouched paths stay byte-identical, and promote baselines where a fix changes output on purpose.
- Phase 3 helpers are now the single place for each fix:
  - `run_in_scope` (`db_core.cpp`): C4 and C6.
  - `option_entries`/`option_table` (`internal.h`): table checks.
  - `columns_to_cpp_rows`/`collect_group_columns`: group decoders.
  - `binop<Op>`/`to_expression` (`binary.cpp`): C8.
  - `lua_to_value`/`lua_cell_as`.
- Existing guarded-key patterns to generalize into `lua_string_key`: `string_key` and the guarded loops noted in LUA-RUNNER-MAP C2. Find them by symbol, not by pre-split line numbers.
- `expect_lua_error(lua, script, substring)` (`tests/test_lua_runner.h:48`) matches by substring.

### Established Patterns
- The `on_row` check (`sol::object` plus an explicit `sol::type::function` check) is the model for C6.
- Check order decides which error a call reports. Each new `require_table` must sit where today's implicit check would fire, never at the top of a lambda ahead of `resolve_sandboxed_path` (Pitfall 8). The Phase 1 order pins guard this.
- sol2 settings stay PRIVATE on `quiver` (`SOL_SAFE_NUMERICS`, `SOL_NO_NIL`). Write `sol::lua_nil`, never `sol::nil`.

### Integration Points
- `bindings/js/src/lua-api.ts` `LUA_DB_API_REFERENCE`: the empty-array text (FIX-02) and the error-shape mentions. The sync test must stay green.
- `src/CMakeLists.txt`: the sol2 compile definitions (SAFE-06).
- Root `AGENTS.md`: the sandbox decision (`load`), the boolean/typing notes if affected, and the `SOL_SAFE_FUNCTION` claim removal. `src/AGENTS.md`: the helpers and the "Three guards" section.
- Phase 3 carry-overs (STATE.md):
  - two duplicated converter messages, untouched by D-04;
  - the `apply_binop` dead branch, already gone, so FIX-03 has only the D1 nil branch and `lua_data_type_name`'s `default:` left;
  - IN-07 (`lua_table_to_dim_map` keys), which is SAFE-02.

</code_context>

<specifics>
## Specific Ideas

- C1's worst case must be pinned red-first: `db:update_vector_group("C","g",id, some_file_userdata)` currently clears the group silently.
- The C7 rule: `create_element` still skips an empty array (the core does), while `update_element` passes it through and clears. A typo'd empty column throws "does not match any vector, set, or time series table".
- Dart is the only binding suite that runs Release locally. Delete `bindings/dart/.dart_tool/hooks_runner/` and `.dart_tool/lib/` before `test-all.bat`.

</specifics>

<deferred>
## Deferred Ideas

- Adding the `got <type>` suffix to the "has unsupported Lua type" converter messages (D-04). It could be done later, editing both throw sites of each.
- An explicit Pattern 1 dot-call self-check in place of sol2's raw backstop text (D-10 alternative).

</deferred>
