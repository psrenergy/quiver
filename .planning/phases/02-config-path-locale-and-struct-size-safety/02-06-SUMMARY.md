---
phase: 02-config-path-locale-and-struct-size-safety
plan: 06
status: complete
completed: 2026-09-19
requirements: [OPT-01, OPT-02, OPT-03, OPT-04]
commits:
  - 3c11653 feat(02-06): db:has_ui_config() and --ui-config-dir/--ui-locale on quiver_cli
---

# Plan 02-06 Summary — Lua host and Lua binding

## What shipped

**`db:has_ui_config()`** is bound on the `Database` usertype in `src/lua_runner.cpp:273-274`,
beside `is_healthy`, and mirrored into the agent-facing Lua reference
(`bindings/js/src/lua-api.ts:149`) in the same edit — the `lua-api-sync.test.ts` guard reads
`lua_runner.cpp` from disk and fails on drift, so the two cannot separate.

**`--ui-config-dir` and `--ui-locale`** were added to `quiver_cli` (`src/cli/main.cpp:60`, `:62`,
consumed at `:102` and `:105`). `LuaRunner` is deliberately **unchanged**: it takes an
already-configured `Database&` and has no options channel of its own (D-14), so the host is where
the flags belong. Both are read via `program.present<std::string>(...)` before the three-mode
`Database` construction, so an unsupplied flag leaves the field at its default.

Neither flag is validated by the CLI. That is deliberate and consistent: D-05 already degrades an
absent or malformed explicit directory with a `warn` log and no throw, and every other option the
CLI forwards is passed through unvalidated the same way.

## The cwd trap, and how it was closed

The adversarial plan review caught that a CLI shell-out test would depend on the working directory:
`gtest_discover_tests` sets `WORKING_DIRECTORY` to `$<TARGET_FILE_DIR:quiver_tests>`
(`tests/CMakeLists.txt:64-66`), but the plan's own verify command runs
`./build/bin/quiver_tests.exe` from the repo root — where `quiver_cli` is not on PATH. The three
`Cli*` cases would have failed under the exact command the plan prescribed and passed only under
`ctest`.

Closed as planned: `tests/CMakeLists.txt:71` now carries
`target_compile_definitions(quiver_tests PRIVATE QUIVER_CLI_PATH="$<TARGET_FILE:quiver_cli>")`, and
`tests/test_lua_runner_ui_options.cpp` invokes that absolute path. The only occurrence of the string
`"quiver_cli"` in the test sources is a comment explaining why a bare name must never be used.

## Verification

| Suite | Result |
|-------|--------|
| `quiver_tests.exe` (full) | **1175 / 1175** |
| `quiver_c_tests.exe` (full) | **567 / 567** |
| `LuaRunnerUiOptions.*` + `LuaRunnerDescribe.*` + `DatabaseUi*` | 65 / 65 |

`LuaRunnerUiOptions` covers five cases, including `CliUiLocaleFlagRendersSpanishLabel` — the proof
that a caller-supplied locale crosses the CLI boundary and renders a Spanish label. That case is
exactly what would have failed had Phase 2 not threaded `locale` into `parse_enum_content`
(the second hardcoded-`"en"` site the adversarial review found); `foresight_like`'s only
locale-varying data is `ui/enum.toml`, because `economic_driver.toml`'s labels are bare strings.

`CliDefaultLocaleRendersEnglishLabel` and `CliMissingUiConfigDirStillSucceeds` pin the default and
the D-05 degrade path respectively.

## Deviations

**Plan completion was finished by the orchestrator.** The executor agent committed the
implementation (`3c11653`) and the `src/CLAUDE.md` update, then stopped while waiting on a
background `scripts/build-all.bat` that never reported back. The orchestrator verified the build had
in fact completed (binaries newer than sources), ran the full C++ and C API suites directly, checked
every acceptance criterion by hand, and wrote this SUMMARY. No implementation work was changed.

## Constraints honoured

- `LuaRunner` unchanged — no options channel added.
- `bindings/js/src/lua-api.ts` updated in the same edit as the binding; reference **not** relocated
  (root CLAUDE.md "Do Not Fix").
- No Lua boolean readers, no whole-group readers, no widened sandbox or stdlib set.
- No field added to `ScalarMetadata` / `GroupMetadata`; no CSV source touched.
- `CHANGELOG.md` heading untouched — that is 02-07's.
- No file under `bindings/{julia,dart,python}/` touched; `bindings/js/` limited to `lua-api.ts`.
