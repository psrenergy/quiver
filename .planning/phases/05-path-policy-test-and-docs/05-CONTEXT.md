# Phase 5: Path-Policy Test and Docs - Context

**Gathered:** 2026-10-03
**Status:** Ready for planning
**Mode:** Smart discuss, no grey areas put to the user. The roadmap explicitly delegates the TEST-01 link strategy to the plan, DOC-01's replacement rule is written in the requirement, and DOC-02..04 list what the docs must say. The decisions below are carried forward from upstream documents and Phase 4's results.

<domain>
## Phase Boundary

The path-containment gate `resolve_sandboxed_path` has its own unit test, no planning-ID comment remains in the repo, and the docs, the shipped Lua reference and the `[0.13.0]` CHANGELOG fully describe the finished milestone.

Requirements: TEST-01, DOC-01, DOC-02, DOC-03, DOC-04. This is the last phase of the milestone. No behaviour change except what TEST-01 needs in order to link: a refactor of where `resolve_sandboxed_path` is declared/defined, with the same logic and messages. No version bump; manifests stay 0.13.0.

</domain>

<decisions>
## Implementation Decisions

### TEST-01: path-policy unit test
- **D-01:** The suite is named `SandboxedPathTest`. That name sits outside the `Lua*` filter, so the Phase 4 baseline still holds exactly: `quiver_tests --gtest_filter=Lua*` = 477 / 12 suites on Windows (Linux 475 + 1 root skip), and C API `LuaRunnerCApiTest` = 27. The roadmap's "428 at `bdf9087`" numbers are stale; STATE.md's Phase 5 baseline (recorded at `e6aa5c1`) supersedes them.
- **D-02:** Coverage: containment of a relative path, a subdirectory, escape rejection (`../`, absolute outside, a symlink- or `..`-normalised escape where the platform allows), the root itself, `:memory:` rejection, and the Windows device-name prefix (the `_WIN32`-only case, like the existing Lua-level tests). Every message is asserted with the exact Pattern 1 text the function throws today.
- **D-03:** The test reaches `resolve_sandboxed_path` through a sol2-free header (for example `src/lua_runner/path_policy.h`). `internal.h` includes it instead of declaring the function itself, so the test TU never parses sol2. The link strategy is Claude's discretion and should be settled by research. The roadmap lists three options: a header-inline definition, an exported symbol, or compiling `path_policy.cpp` into `quiver_tests`. Preference: no new exported symbol on the public ABI (the function is internal). Behaviour and messages must be byte-identical, and the golden harness proves it.

### DOC-01: planning-ID sweep
- **D-04:** Every planning-ID comment outside `.planning/` and `CHANGELOG.md` is replaced with its one-line reason, or with the name of the test that pins it. The ID is never just deleted when it was carrying meaning. Scope is the roadmap's full regex: legacy `D-xx`, `LUA-xx`, `WRITE-xx`, `FMT-xx`, `TEST-xx`, `NN-NN-PLAN.md` references, plus this milestone's `C1`-`C8`, `M1`-`M16`, `PIN-NN`, `SPLIT-NN`, `DEDUP-NN`, `SAFE-NN`, `FIX-NN`, `DOC-NN`, `TEST-NN`. The gate is the roadmap's `git grep` returning 0. Hits that are not planning IDs (e.g. `.gitattributes`, a test value, a date) are verified one by one and the regex is adjusted, not the text.
- **D-05:** Test names and assertion strings never change during the sweep. Only comments and docs change, so test counts and expectations stay identical.

### DOC-02..04: docs
- **D-06:** AGENTS.md files (root, `src/`, `src/c/`, `tests/`, four bindings) describe the `src/lua_runner/` layout, the C7 rule, text-only `load` and `run()`, and the safety flags including the getter/stack-check fallback and its Debug side effect. Each file gets only what is true of its area.
- **D-07:** `LUA_DB_API_REFERENCE` states the empty-array rule and what the sandbox does not limit: instructions, memory, wall time, and globals persisting across `run()`. The sync test must stay green.
- **D-08:** The CHANGELOG `[0.13.0]` section is checked for completeness against roadmap criterion 4, and the Phase 4 doc nits are closed:
  - add an entry for `quiver.metadata_from_element`'s empty-array error-text change (04-REVIEW IN-03);
  - `tests/AGENTS.md:146` must also say the stack check is off;
  - fix `AGENTS.md:88`'s wrap width.
  - IN-01 (type errors that do not end in `got <type>`) is documented where the error-shape rule is described in `src/AGENTS.md`, not changed in code (D-04/D-16 of Phase 4).
- **D-09:** Out of scope, by STATE.md's standing note: backfilling the missing `[0.12.9]` CHANGELOG section is the maintainer's call. The `[0.13.0]` compare link stays based on the existing `v0.12.9` tag.

### Claude's Discretion
- The TEST-01 link mechanism (D-03), the exact header name, and the test fixture shape (temp directory plus a file-backed `Database`).
- Plan granularity. The suggested split is TEST-01, then the DOC-01 sweep, then DOC-02..04 plus the phase gate. The sweep is wide (about 13 files, several hundred lines), so splitting it by area is fine.

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- `src/lua_runner/path_policy.cpp`: `quiver::lua_internal::resolve_sandboxed_path(const Database&, const std::string& operation, const std::string& path)`, declared in `internal.h:302` (sol2 header).
- Lua-level coverage today: the escape, in-memory and device-name tests in `tests/test_lua_runner_*.cpp` and `tests/test_lua_binary.cpp` (`DeviceNamePathIsReportedWithPrefix`, `_WIN32`-only).
- The golden harness `build/fixes-check/` (gitignored) proves byte-identical behaviour across the TEST-01 refactor.

### Established Patterns
- `quiver` is a shared library with hidden visibility (`cmake/Platform.cmake`). Tests link the public API only, and nothing in `tests/` includes `src/` internals today, so TEST-01 is the first test to do so. The header must not drag in sol2 or csv-parser.
- Pattern 1 messages: `Cannot <op>: ...` for containment and in-memory errors.

### Integration Points
- `tests/CMakeLists.txt` adds the new test source, and possibly `path_policy.cpp` or an include directory, depending on D-03.
- Planning-ID footprint at `2ecd5d9`: `src/AGENTS.md` 10, `src/csv/csv_read.cpp` 10, `csv_read.h` 2, `csv_write.cpp` 10, `csv_write.h` 7, `src/database_describe.cpp` 10, `src/ui_metadata.cpp` 4, `tests/AGENTS.md` 5, `tests/test_database_ui_metadata.cpp` 26, `tests/test_lua_runner_read_csv.cpp` 44, `tests/test_lua_runner_write_csv.cpp` 55, plus the `.gitattributes` false positive. That count is for the narrow regex only; the plan must run the roadmap's full regex.

</code_context>

<specifics>
## Specific Ideas

- The roadmap's success-criterion-1 numbers (428 at `bdf9087`) and its "about 224 lines in 13 files at `0a32506`" are historical. Use the live `git grep` and STATE.md's baseline.
- At the phase end, run the six suites with both Dart hook caches deleted, the Linux GCC/Clang Docker pass, and the sync test.

</specifics>

<deferred>
## Deferred Ideas

- Adding the `got <type>` suffix to the remaining type errors (04-REVIEW IN-01) and to the converter messages. That is a behaviour change, out of a docs phase.
- The `[0.12.9]` CHANGELOG backfill (maintainer's call).

</deferred>
