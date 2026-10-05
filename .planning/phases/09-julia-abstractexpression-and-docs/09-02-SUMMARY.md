---
phase: 09-julia-abstractexpression-and-docs
plan: 02
subsystem: docs
status: complete
tags: [docs, agents-md, changelog, expression, gate]
requires:
  - "09-01: Julia AbstractExpression (Binary.File and Expression subtypes)"
provides:
  - "root AGENTS.md Design Decision: a binary file is an expression"
  - "binary cross-layer rows for get_metadata, is-an-expression, save, aggregate, select/rename agents"
  - "accurate src/AGENTS.md ExpressionFile bullet; src/c/AGENTS.md from_file bridge note"
  - "CHANGELOG [0.13.0] completeness proven by build/julia-check/changelog_check.sh"
affects: []
tech-stack:
  added: []
  patterns: []
key-files:
  created:
    - build/julia-check/changelog_check.sh (gitignored)
    - build/julia-check/test-all.txt (gitignored)
  modified:
    - AGENTS.md
    - src/AGENTS.md
    - src/c/AGENTS.md
    - .planning/STATE.md
decisions:
  - "No CHANGELOG edit: the [0.13.0] section already held all four BREAKING lines with caller actions and both Added entries"
metrics:
  duration: "21 min"
  completed: 2026-10-05
  tasks: 3
  files: 4
actuals:
  tokens: 2800
  tasks: 3
  commits: 3
---

# Phase 9 Plan 2: AbstractExpression docs and milestone gate Summary

The root, `src/` and `src/c/` AGENTS.md files now say a binary file is an expression in every layer that
exposes expressions, with `quiver_expression_from_file` documented as the one C API bridge. The stale
"caches an open BinaryFile" bullet is gone. A script proves the CHANGELOG `[0.13.0]` section complete, and all
six suites pass on the final code commit.

## Doc edits and the code each was checked against

| Edit | Checked against |
|------|-----------------|
| AGENTS.md Design Decision `- **A binary file is an expression.**` (line 78, between the "exposed in Julia and Lua only" and "Lua file operations are db-scoped" bullets) | `include/quiver/expression/abstract_expression.h` (one pure virtual `node()`, virtual `get_metadata()`, protected copy/move, non-virtual save/aggregate*/\*_agents); `include/quiver/expression/expression.h` (`final`, `explicit Expression(const AbstractExpression&)`, `const AbstractExpression&` operator/unary/ifelse operands); `src/expression/expression_file.cpp` (`BinaryFile::node()` makes a path-based `ExpressionFile`); `src/lua_runner/internal.h` (the three `SOL_*_CLASSES` lines); `src/lua_runner/expression.cpp` (`operand_error` text, too-many-arguments text); `bindings/julia/src/Quiver.jl`, `binary/file.jl`, `expression.jl`; REQUIREMENTS.md Out of Scope (rejected alternatives) |
| AGENTS.md sandbox list: `save` on a file or an expression | `src/lua_runner/expression.cpp:136-138,194` (one sandboxed `save` lambda bound on both usertypes) |
| AGENTS.md Binary & Expression Subsystems sentence | same as the decision |
| AGENTS.md binary table: `Get metadata` updated; `Is an expression`, `Save`, `Aggregate`, `Select / rename agents` added; `x` note under the table | C API names grepped in `include/quiver/c/expression/expression.h`; Julia `save`/`aggregate`/`aggregate_agents`/`select_agents`/`rename_agents` on `AbstractExpression` in `expression.jl` |
| AGENTS.md Lua expression paragraph (methods on `x`, arity rule, save sandbox) | `src/lua_runner/expression.cpp` `operand_error`; `lua_type_error` format in `internal.h`; `tests/test_lua_expression.cpp:561,635` |
| src/AGENTS.md `ExpressionFile` bullet | `expression_file.cpp` (TOML read in constructor, private `file_(path)`, `dim_map_` mutable map, `collect_input_files`); `src/expression/expression.cpp` `AbstractExpression::save` (opens inputs `'r'`, `CloseOnExit`) |
| src/AGENTS.md Expression Subsystem intro: `save` on a file or an expression | as the sandbox list |
| src/AGENTS.md `Expression subsystem:` line names `final` and `AbstractExpression` | `abstract_expression.h`, `expression.h`, `binary_file.h` (`class BinaryFile : public AbstractExpression`) |
| src/c/AGENTS.md file-map comment, `**File to expression.**` paragraph, `src/lua_runner/` clause on `lua_runner.cpp` | `src/c/expression/expression.cpp:99-112` (`new quiver_expression(quiver::Expression(file->binary_file))`, nothing retained from the handle) |

Acceptance greps: `expr:save` 0 in AGENTS.md and src/AGENTS.md; the four new table rows = 4; every required
term present in the decision block (`abstract_expression.h` 1, `node()` 3, `get_metadata()` 2, `SOL_BASE_CLASSES`
1, `Binary.File <: AbstractExpression` 1, `quiver_expression_from_file` 2, `ownership` 1, `AbstractArray` 1,
`open decision` 1); `grep -niE 'caches an open .?BinaryFile' src/AGENTS.md` and the ROADMAP literal both empty;
`quiver_expression_from_file` 2 lines in src/c/AGENTS.md.

## Gate results

- `bash build/julia-check/changelog_check.sh`: `CHANGELOG OK` (no entry forced). Mutation-checked: removing the
  Julia Added heading text, the `Drop the extra argument.` action, or the `### Fixed` heading each makes it fail.
- `uv run python scripts/assert_version.py`: `All project files at 0.13.0` (CMakeLists, pyproject, package.json,
  pubspec, Project.toml), exit 0.
- `scripts/test-all.bat` (Debug, Dart hook cache deleted first), log `build/julia-check/test-all.txt`:
  C++ PASS (1476), C API PASS (543), Julia PASS (1675), Dart PASS (444), JavaScript PASS (241), Python PASS (350),
  `All tests PASSED`.
- `bun test test/lua-api-sync.test.ts`: 6 pass, 0 fail.
- `git diff --quiet BASE -- bindings/julia/src/c_api.jl include/quiver/c`: exit 0; `Binary.File` lines in
  `expression.jl`: 1.
- `END=1 bash build/julia-check/gate.sh` on `e43374f`: `JULIA GATE PASS expression=287 ambiguities=0 binary_file_lines=1`.
- STATE.md: `[lua-2] Phase 9 final counts (e43374f, Windows Debug)` bullet with the counts above.

## Commits

| Task | Commit | Message |
|------|--------|---------|
| 1 | `cf07244` | docs(09-02): root AGENTS.md AbstractExpression decision and binary cross-layer rows |
| 2 | `e43374f` | docs(09-02): src and src/c AGENTS.md for the abstract expression base |
| 3 | `be2f984` | docs(09-02): record the final Julia and suite counts |

## Deviations from Plan

1. **[Wording] src/c/AGENTS.md paragraph names the function twice.** The plan's acceptance grep wants
   `quiver_expression_from_file` on at least 2 lines, but its file-map comment spells only `from_file`. The
   paragraph's last sentence now ends "Julia converts every file operand through `quiver_expression_from_file`"
   instead of "through this one call".
2. **[Harness] changelog_check.sh pipefail.** The first version exited silently when the Julia entry was missing
   (`grep | cut` under `pipefail`). Wrapped those greps in `{ ... || true; }`; the check now names the failure.
   Gitignored harness only.

Otherwise the plan ran as written.

## Known Stubs

None.

## Self-Check: PASSED

- FOUND: AGENTS.md, src/AGENTS.md, src/c/AGENTS.md, .planning/STATE.md, build/julia-check/changelog_check.sh,
  build/julia-check/test-all.txt
- FOUND commits: cf07244, e43374f, be2f984
