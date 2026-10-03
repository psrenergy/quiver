# Phase 5: Path-Policy Test and Docs - Research

**Researched:** 2026-10-03 (at `0d10f17`)
**Domain:** C++20 / CMake test linkage of an internal symbol across a hidden-visibility shared library; repo-wide comment sweep; docs and CHANGELOG
**Confidence:** HIGH (every claim about this repo was read or run this session; two platform claims are cited, one is assumed and guarded)

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

#### TEST-01: path-policy unit test
- **D-01:** The suite is named `SandboxedPathTest`. That name sits outside the `Lua*` filter, so the Phase 4 baseline still holds exactly: `quiver_tests --gtest_filter=Lua*` = 477 / 12 suites on Windows (Linux 475 + 1 root skip), and C API `LuaRunnerCApiTest` = 27. The roadmap's "428 at `bdf9087`" numbers are stale; STATE.md's Phase 5 baseline (recorded at `e6aa5c1`) supersedes them.
- **D-02:** Coverage: containment of a relative path, a subdirectory, escape rejection (`../`, absolute outside, a symlink- or `..`-normalised escape where the platform allows), the root itself, `:memory:` rejection, and the Windows device-name prefix (the `_WIN32`-only case, like the existing Lua-level tests). Every message is asserted with the exact Pattern 1 text the function throws today.
- **D-03:** The test reaches `resolve_sandboxed_path` through a sol2-free header (for example `src/lua_runner/path_policy.h`). `internal.h` includes it instead of declaring the function itself, so the test TU never parses sol2. The link strategy is Claude's discretion and should be settled by research. The roadmap lists three options: a header-inline definition, an exported symbol, or compiling `path_policy.cpp` into `quiver_tests`. Preference: no new exported symbol on the public ABI (the function is internal). Behaviour and messages must be byte-identical, and the golden harness proves it.

#### DOC-01: planning-ID sweep
- **D-04:** Every planning-ID comment outside `.planning/` and `CHANGELOG.md` is replaced with its one-line reason, or with the name of the test that pins it. The ID is never just deleted when it was carrying meaning. Scope is the roadmap's full regex: legacy `D-xx`, `LUA-xx`, `WRITE-xx`, `FMT-xx`, `TEST-xx`, `NN-NN-PLAN.md` references, plus this milestone's `C1`-`C8`, `M1`-`M16`, `PIN-NN`, `SPLIT-NN`, `DEDUP-NN`, `SAFE-NN`, `FIX-NN`, `DOC-NN`, `TEST-NN`. The gate is the roadmap's `git grep` returning 0. Hits that are not planning IDs (e.g. `.gitattributes`, a test value, a date) are verified one by one and the regex is adjusted, not the text.
- **D-05:** Test names and assertion strings never change during the sweep. Only comments and docs change, so test counts and expectations stay identical.

#### DOC-02..04: docs
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

### Deferred Ideas (OUT OF SCOPE)
- Adding the `got <type>` suffix to the remaining type errors (04-REVIEW IN-01) and to the converter messages. That is a behaviour change, out of a docs phase.
- The `[0.12.9]` CHANGELOG backfill (maintainer's call).
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| TEST-01 | `resolve_sandboxed_path` is unit-tested directly through a sol2-free header (containment, escape rejection, the root itself, `:memory:`, device-name prefix), outside the `Lua*` filter | Link strategy settled: compile `path_policy.cpp` into `quiver_tests` (§Architecture Patterns 1). Header, CMake and the 11 test cases with exact messages drafted (§Code Examples) |
| DOC-01 | Planning-ID comments replaced repo-wide with their reason or the pinning test | Full inventory: 256 real hit lines in 19 files under the corrected gate, 11 false-positive lines + one svg classified; 19 of the hits are diagnostic strings, not comments (§DOC-01 Inventory). Final gate command given |
| DOC-02 | Eight AGENTS.md files describe the layout, the C7 rule, text-only `load`, the safety flags | Per-file gap matrix (§DOC-02 Matrix) |
| DOC-03 | CHANGELOG `[0.13.0]` complete | Audited against roadmap criterion 4: complete except the IN-03 sentence; 0 planning IDs in the section; version 0.13.0 in all five manifests (§DOC-03 Audit) |
| DOC-04 | `LUA_DB_API_REFERENCE` states the empty-array rule and what the sandbox does not limit; sync test green | Empty-array rule already present (`lua-api.ts:309-315`); the "does not limit" bullet is missing; sync-test constraints listed (§DOC-04) |
</phase_requirements>

## Project Constraints (from AGENTS.md)

There is no `CLAUDE.md`; the root and nested `AGENTS.md` files are the project instructions. Directives that bind this phase:

- **Self-Updating:** keep the AGENTS.md nearest each change current (root, `src/`, `src/c/`, `tests/`, four bindings).
- **Changelog:** user-visible changes get an entry under the current unreleased section (`[0.13.0] — unreleased`); BREAKING entries say what a caller must do. No version bump this phase (all five manifests stay 0.13.0).
- **Error messages** live in C++; Pattern 1 is `Cannot {operation}: {reason}`. The test must assert the existing text, never invent new text.
- **Do Not Fix:** do not relocate `LUA_DB_API_REFERENCE` (only its text changes); do not drive-by fix lint debt in untouched JS files; do not delete `tests/sandbox`.
- **clang-format pinned to 22.1.8** (`uvx --from clang-format==22.1.8 clang-format`).
- **Python via `uv run`**, never bare `python`.
- `src/lua_runner/` conventions (src/AGENTS.md "Layout"): every TU uses the `#include "lua_runner/..."` spelling; comments must not spell the Database usertype call or the stdlib-opening call (greps count them); no file over about 450 lines; `internal.h` holds only templates, `inline` functions and declarations.
- `.gitattributes` forces LF; `.pre-commit-config.yaml` checks trailing whitespace and EOF.
- Memory note (user): binding test filters break through `cmd //c` with quotes; call `pytest`/`dart`/`bun` directly when filtering.

## Summary

TEST-01 is a small, mechanical refactor plus one new test file. `resolve_sandboxed_path` uses only `Database::path()`, and `Database` is a `QUIVER_API` class (`include/quiver/database.h:17`), so the test executable can compile its own copy of `path_policy.cpp` and link it against the exported `Database` without exporting anything new. The function body does not change; only its declaration moves from `internal.h:302` into a new sol2-free `src/lua_runner/path_policy.h`, which `internal.h` includes. That is the shortest diff of the three options, keeps the Phase 2 file layout and every doc citation of `path_policy.cpp` valid, and adds nothing to the ABI.

DOC-01 is wider than CONTEXT.md estimated. The roadmap's literal regex hits 214 lines in 14 files, but 11 of those lines are false positives (Unicode "C0/C1 control" prose) plus `assets/logo.svg` base64, and it misses 54 real planning references that sit in the same comment blocks (`PARSE-xx`, `READ-xx`, `D2-xx`, `T-01-03`, `Task 1-01-01`, `plan 02-01`, `CONTEXT.md`, `ROADMAP criterion`, `RESEARCH.md Q1`) plus older-milestone section labels (`CSV-01`, `OPT-01`, `CAPI-11`, `QUERY-01`, `JSCSV-01`). With a corrected regex the sweep is 256 lines in 19 files, all real. `.gitattributes:4` is a real ID (`02-03-PLAN.md, D-24`), not a false positive as CONTEXT.md says. 19 of the hits are not comments: they are failure-diagnostic strings (Lua `assert` messages and `FAIL() <<` text) that D-05's "assertion strings never change" can be read to protect. This needs one decision (Open Question 1).

The docs are mostly done. Phase 4 already wrote the C7 rule, text-only `load`/`run()` and the safety flags into `src/AGENTS.md`, the empty-array rule into `lua-api.ts`, and a complete `[0.13.0]` CHANGELOG section. What remains is: a "what the sandbox does not limit" bullet in `lua-api.ts`; the IN-03 CHANGELOG sentence; the IN-01 note and the new header in `src/AGENTS.md`; the stack-check nit, the C7 pin and the new suite in `tests/AGENTS.md`; the C7 rule and the line-88 wrap in root `AGENTS.md`; and one line on text-only `run` in `src/c/` and each binding AGENTS.md.

**Primary recommendation:** Three sequential plans: (1) TEST-01 by compiling `path_policy.cpp` into `quiver_tests`, in a test file named `tests/test_sandboxed_path.cpp`; (2) the DOC-01 sweep under the corrected PCRE gate below; (3) DOC-02..04 plus the full phase gate.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Path containment (`resolve_sandboxed_path`) | C++ core, `src/lua_runner/path_policy.cpp` | — | The single gate; LuaRunner policy only, unchanged this phase |
| Declaration visible to tests | Internal header `src/lua_runner/path_policy.h` | `internal.h` re-includes it | Must not pull sol2 into the test TU |
| Unit test | `quiver_tests` (gtest), own compiled copy of `path_policy.cpp` | Lua-level tests keep covering the shipped library copy | The symbol is hidden in libquiver, so tests cannot link to it |
| Planning-ID removal | Comments in `src/`, `tests/`, binding tests, AGENTS.md, `.gitattributes` | — | Text only; no expected value or test name changes |
| Agent-facing Lua contract | `bindings/js/src/lua-api.ts` (`LUA_DB_API_REFERENCE`) | sync test guards names | Shipped on npm as prompt payload |
| Release notes | `CHANGELOG.md` `[0.13.0]` | — | Hand-edited |

## Standard Stack

No new libraries. Everything used is already in the build:

| Tool | Version (verified this session) | Purpose |
|------|--------------------------------|---------|
| GoogleTest | v1.17.0 (`AGENTS.md` Build System) | `SandboxedPathTest` |
| `std::filesystem` | C++20 stdlib | temp dirs, `create_directory_symlink`, `weakly_canonical` in the expectations |
| MSVC | 14.51.36231 (`build/CMakeCache.txt`, Ninja, Debug) | local build |
| clang-format | 22.1.8 (`uvx`) | formatting gate |
| clang-tidy / run-clang-tidy | LLVM 22.1.3 (VS-bundled) | tidy baseline (14) |
| Bun | 1.3.14 | sync test, biome |
| Docker | 29.6.2 | Linux GCC 13 / Clang 18 runs (`build/fixes-check/linux.sh`) |

## Package Legitimacy Audit

Not applicable: this phase installs no external packages.

**Packages removed due to [SLOP] verdict:** none
**Packages flagged as suspicious [SUS]:** none

## Architecture Patterns

### System Architecture Diagram

```
 Lua script ──> db:read_csv / db:write_csv / db:open_file / expr:save / ... (10 callers, src/lua_runner/*.cpp)
                      │  (all include internal.h ──includes──> path_policy.h)
                      ▼
            resolve_sandboxed_path(db, op, path)      [libquiver, hidden symbol]
              1. db.path() == ":memory:"?  ──yes──> throw "Cannot <op>: database is in-memory, ..."
              2. root = weakly_canonical(parent_path(db) or cwd)
                 candidate = weakly_canonical(root / path)      ──filesystem_error──> throw "Cannot <op>: cannot resolve path '<p>': <os reason>"
              3. rel = candidate.lexically_relative(root)
                 rel empty / "." / starts with ".." ──> throw "Cannot <op>: path '<p>' escapes the database directory '<root>'"
              4. return candidate.string()

 SandboxedPathTest (quiver_tests) ──includes──> path_policy.h (no sol2)
        └── calls its OWN compiled copy of path_policy.cpp ──uses──> quiver::Database::path()  [exported from libquiver]
```

### Recommended Project Structure (delta only)

```
src/lua_runner/
├── path_policy.h      # NEW: sol2-free declaration of resolve_sandboxed_path
├── path_policy.cpp    # include line changes from internal.h to path_policy.h; body untouched
└── internal.h         # declaration at :302 removed; includes lua_runner/path_policy.h
tests/
├── test_sandboxed_path.cpp   # NEW: SandboxedPathTest
└── CMakeLists.txt            # + test file, + ../src/lua_runner/path_policy.cpp, + src include dir
src/CMakeLists.txt            # + lua_runner/path_policy.h next to lua_runner/internal.h (listing only)
```

### Pattern 1: Compile the internal TU into the test target (recommended link strategy)

**What:** `quiver_tests` lists `${CMAKE_SOURCE_DIR}/src/lua_runner/path_policy.cpp` as one of its own sources and adds `${CMAKE_SOURCE_DIR}/src` as a PRIVATE include directory.

**Why it links on every toolchain:**
- The only external symbol `path_policy.cpp` needs is `quiver::Database::path() const` [VERIFIED: path_policy.cpp:16 `const std::string& db_path = db.path();`], and `Database` is declared `class QUIVER_API Database` [VERIFIED: include/quiver/database.h:17], which is `__declspec(dllimport)` in the test TU on Windows and `visibility("default")` elsewhere [VERIFIED: include/quiver/export.h, `#define QUIVER_API __declspec(dllimport)` / `__attribute__((visibility("default")))`]. So the test copy resolves against the exported class.
- libquiver's own copy is not exported: `QUIVER_API` is absent on the function, and `cmake/Platform.cmake:41-42` sets `set(CMAKE_C_VISIBILITY_PRESET hidden)` / `set(CMAKE_CXX_VISIBILITY_PRESET hidden)` [VERIFIED: Read]. Platform.cmake is included before `add_subdirectory(src)` (root `CMakeLists.txt:45,53`). MSVC/MinGW DLLs export only `dllexport` symbols (and MinGW's auto-export is off once any explicit `dllexport` exists [ASSUMED]). So the executable's definition and the DLL's never meet at link time.
- **ODR:** two identical definitions in two link units (exe and shared library) are outside the standard's single-program ODR, and the code is byte-for-byte the same source. On ELF, even with default visibility the exe definition would interpose with identical code.
- **Static build (`QUIVER_BUILD_SHARED=OFF`):** the exe's own `path_policy.obj` defines the symbol first, so the archive member `path_policy.o` in `libquiver.a` is never pulled to satisfy the `lua_runner/*.o` references, and nothing is defined twice. This holds only while `path_policy.cpp` defines nothing else that the library needs. If a second function is ever added there, a static link pulls the archive member and reports a duplicate definition. Put that constraint in a CMake comment. No CI job builds static (`git grep QUIVER_BUILD_SHARED` shows only the option, `src/CMakeLists.txt:54` and the Dart hook's `'ON'`).
- **The test does not parse sol2:** `path_policy.cpp` currently includes `lua_runner/internal.h` (sol2) only for the declaration [VERIFIED: path_policy.cpp:1]. After the include swap the test TU sees `path_policy.h` + `quiver/database.h` + `<filesystem>`. The test target has no sol2 include path, so the build fails loudly if sol2 ever creeps back in, which acts as a free guard.

**Rejected alternatives:**
| Option | Why not |
|--------|---------|
| Exported symbol (`QUIVER_API` on the function) | Adds an internal function to the public DLL/so ABI; CONTEXT D-03 prefers against it |
| Header-inline definition in `path_policy.h` | Works on every toolchain with no link caveat, but deletes `path_policy.cpp` (a SPLIT-01 file) and invalidates its citations (`src/AGENTS.md:50,168`, root `AGENTS.md:92`, `src/csv/csv_write.cpp:50`); puts `<filesystem>` and a 50-line body in every `src/lua_runner/` TU via `internal.h`. Larger diff for the same result. Use it only if a static-build duplicate ever appears |

### Pattern 2: Reuse the existing sandbox fixture

`LuaSandboxTest` (`tests/test_lua_runner.h`) already creates a per-test temp dir named from the suite and test name, exposes `sandbox` and `db_path()`, and removes the dir in `TearDown`. `class SandboxedPathTest : public LuaSandboxTest {};` gives the suite its own name (outside `Lua*`) with zero new fixture code. `test_lua_runner.h` includes only public headers (`quiver/database.h`, `quiver/element.h`, `quiver/lua_runner.h`), so sol2 stays out.

Open the database directly with the existing test idiom `quiver::Database db(path, {.read_only = false, .console_level = quiver::LogLevel::Off});` [VERIFIED: tests/test_database_errors.cpp:12]; no schema is needed because schema loading is lazy and the gate only reads `db.path()`.

### Anti-Patterns to Avoid
- **Expecting `sandbox.string()` as the root in a message.** The gate prints `weakly_canonical(root)`. On macOS the temp dir is under the `/var` → `/private/var` symlink, and on Windows `%TEMP%` can contain 8.3 short names that canonicalisation expands. Always build expectations from `fs::weakly_canonical(sandbox)`.
- **Asserting the OS reason text of the device-name error.** It is localised (`"The parameter is incorrect."` in English). Assert the exact prefix and that a non-empty reason follows, as the Lua-level tests do (`tests/test_lua_binary.cpp:423`).
- **Naming the test file `test_lua_*.cpp`.** The phase harness (`build/fixes-check/gate.sh`) derives the expected `Lua*` count from `git diff ... -- 'tests/test_lua*.cpp' | grep -c '^+TEST(_F)?\('`, so a `test_lua_runner_path_policy.cpp` would inflate the expected `Lua*` count and fail the gate. Use `tests/test_sandboxed_path.cpp`, which matches the suite name.
- **Spelling `.set_function(`, `new_usertype<`, or `open_libraries(` in a `path_policy.h` comment.** `lua-api-sync.test.ts` reads every `.cpp`/`.h` under `src/lua_runner/` and counts those tokens.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Per-test temp dir + cleanup | New fixture | `LuaSandboxTest` (`tests/test_lua_runner.h`) | Already unique per suite/test, already cleans up |
| Expected canonical root | String surgery on temp paths | `std::filesystem::weakly_canonical(sandbox)` | Same function the gate uses; handles `/private/var`, 8.3 names |
| Behaviour-neutral proof | New probes | Existing golden harness `build/fixes-check/golden.sh debug|release` + the 23 Lua-level `escapes the database directory` assertions (plus the in-memory and device-name ones) | Already captures path errors across ops |
| Linux runs | New Docker script | `build/fixes-check/linux.sh gcc|clang` + one extra `--gtest_filter='SandboxedPathTest.*'` line | Same image and flags as Phase 4 |
| Planning-ID detection | Ad-hoc eyeballing | The PCRE gate below | Reproducible 0/non-0 |

## DOC-01 Inventory

### The roadmap regex, run as written

```bash
RE='\b(D|LUA|WRITE|FMT|TEST|PIN|SPLIT|DEDUP|SAFE|FIX|DOC)-[0-9]+\b|[0-9]{2}-[A-Z0-9-]+\.md|RESEARCH\.md|Phase [0-9]|Pitfall [0-9]|Group [0-9]+|\b[CM][0-9]{1,2}\b'
git grep -nE "$RE" -- . ':!.planning' ':!CHANGELOG.md'
```
At `0d10f17`: 214 lines in 14 files [VERIFIED: run this session]. It has two flaws:
1. **False positives it can never clear (12):** `assets/logo.svg:72` (base64 `C0`, `M2`, `C48`, `C4`) and 11 lines of Unicode "C0/C1 control" prose: `src/AGENTS.md:225,226,227`, `src/database_describe.cpp:56,57,59,76`, `tests/AGENTS.md:23`, `tests/test_database_ui_metadata.cpp:511,532,533`. `C1` there is the U+0080..U+009F control block, not a planning ID. `\b[CM][0-9]{1,2}\b` is also wider than the milestone's `C1`-`C8`/`M1`-`M16`.
2. **Real IDs it misses (54 lines),** mostly inside the same comment blocks: `PARSE-02..09`, `READ-01..04`, `RENDER-03`, `D2-01..12`, `T-01-03`/`T-01-07`/`T-04-02` (threat IDs), `Task 1-01-01`, `plan 01/02`, `Plan 02-01`, `CONTEXT.md`, `01-PATTERNS.md` (caught), `ROADMAP criterion N`, `RESEARCH.md Q1/Q4`, `Finding 2`, `this phase`/`this milestone`, and older-milestone section labels `CSV-01..04`, `OPT-01..04` (`tests/test_c_api_database_csv_export.cpp`), `CAPI-11..13`, `CORE-11..14`, `BUG-01`, `QUERY-01/02` (Python), `JSCSV-01/02` (JS).

### Recommended gate (PCRE; `git grep -P` works in Git for Windows 2.53 [VERIFIED: run])

```bash
G='\b(D|D2|LUA|WRITE|FMT|TEST|PIN|SPLIT|DEDUP|SAFE|FIX|DOC|PARSE|READ|RENDER|CSV|OPT|QUERY|JSCSV|CORE|CAPI|BUG|T)-[0-9]+(-[0-9]+)?\b|[0-9]{2}-[A-Z0-9-]+\.md|\b(RESEARCH|CONTEXT|PATTERNS|ROADMAP|REVIEW|SUMMARY|PITFALLS)(\.md)?\b|\bPhase [0-9]|\bPitfall [0-9]|\bGroup [0-9]+|\b[Pp]lan 0[0-9]|\bTask [0-9]|\bFinding [0-9]|\bQ[0-9]\b|\bthis (phase|milestone)\b|\b(C[1-8]|M(1[0-6]|[1-9]))\b(?! (block|half|code point|gate))(?<!C0/C1)(?<!not C1)'
test -z "$(git grep -nP "$G" -- . ':!.planning' ':!CHANGELOG.md' ':!*.svg')"
```
- It is a superset of the roadmap regex on every real ID, and it excludes only the 12 verified false positives: `*.svg` by pathspec (binary image data, no comments), and the Unicode `C1` by lookarounds that match only the four phrasings in the tree (`C1 block`, `C1 half`, `C1 code point`, `C1 gate`, `C0/C1`, `not C1`). A real `C1` (the milestone bug) elsewhere on such a line would still match.
- Today it reports **256 lines in 19 files, all real** [VERIFIED: run]. `comm` against the roadmap regex shows the only lines the roadmap regex catches and this one does not are the 11 Unicode lines [VERIFIED: run].
- Piping into GNU `grep -P` on Windows needs `LC_ALL=C.UTF-8` (`grep: -P supports only unibyte and UTF-8 locales` was seen this session); `git grep -P` itself does not.
- ERE fallback if PCRE is unavailable: run the regex without the last alternative, then `git grep -nE '\b(C[1-8]|M(1[0-6]|[1-9]))\b' -- . ':!.planning' ':!CHANGELOG.md' ':!*.svg' | grep -vE 'C1 (block|half|code point|gate)|C0/C1|not C1'` and require both to be empty.

### Per-file hits (gate `G`, at `0d10f17`)

| File | Lines | What is there | Replacement source |
|------|-------|---------------|--------------------|
| `.gitattributes` | 1 (`:4`) | `(02-03-PLAN.md, D-24)`, **a real ID**, not the false positive CONTEXT.md lists | Pinning test: `LuaRunner_ReadCsv.EnergiaRegressionJunkRowAboveUnitsRowBelowHeader` (reads `fixtures/ma_energia_residencial.csv`, `tests/test_lua_runner_read_csv.cpp:598,606`) |
| `AGENTS.md` | 1 (`:156`) | `PITFALLS.md, Pitfall 4` inside a `git show f92af8d:` citation | Keep the commit pointer; replace `Pitfall 4` with its heading, "SAVEPOINT Complexity Leaking Into the Design" [VERIFIED: `git show f92af8d:.planning/research/PITFALLS.md` line 121] |
| `src/AGENTS.md` | 12 (`:118,133,142,143,151,221,239,240,299,710,856,913`) | `D-20/34/37/38/40/05/09/06`, `FMT-07`, `WRITE-08`, `LUA-08`, `Phase 1/2 of ... milestone`, `RESEARCH.md Q1` | The prose already states each reason; drop the tag. `LUA-08` → "the rule that no standard-library, csv-parser or sol2 message reaches a script without a Pattern 1 prefix". `RESEARCH.md Q1` → the pinning tests `LuaRunner_WriteCsv.UnclosedWriterIsFlushedWhenRunReturns` / `ScriptErrorMidWriteStillLeavesEarlierRowsReadable`. `D-09 (deliberate divergence from D-06)` → "Deliberate divergence from the `enum {}` clause" |
| `src/csv/csv_read.cpp` | 12 | `D-12/13/20/22/08`, `LUA-08`, `PARSE-09`, `this milestone` | `D-22` = the constructor's numbered Pattern 1 catalogue in evaluation order; `D-20` = `header_row` (1-based, 0 = no header, default 1); `PARSE-09` = bounded read window; the rest are explained in place |
| `src/csv/csv_read.h` | 2 | `D-20`, `LUA-03` (read_csv and read_csv_stream must not diverge) | Reason in place, or `LuaRunner_ReadCsv` tests named in `test_lua_runner_read_csv.cpp:473-521` |
| `src/csv/csv_write.cpp` | 10 | Message catalogue tagged `TEST-12`, `D-36`, `WRITE-05/07/08`, `FMT-02/03/05/07`, `LUA-10`, `T-04-02` | Each catalogue tag → the test asserting that message (grep the message text in `tests/test_lua_runner_write_csv.cpp`); header line → "matched exactly by the catalogue tests in `tests/test_lua_runner_write_csv.cpp`" |
| `src/csv/csv_write.h` | 8 | `D-37/40/36`, `TEST-12`, `FMT-01/02`, `WRITE-08`, `T-04-02` | Reasons are in place; drop tags |
| `src/database_describe.cpp` | 11 | `D-01..D-09`, `T-01-03`, `D2-06`, `D2-12` (UI-metadata milestone decisions) | Reasons are in place (the same rules are prose in `src/AGENTS.md`'s render paragraph); drop tags |
| `src/ui_metadata.cpp` | 8 | `READ-02/04`, `D-17/18/19`, `SAFE-01`, `RESEARCH.md Pattern 3/4`, `CONTEXT.md` | Reasons in place; `SAFE-01` → "the no-`ui/` baseline (`DatabaseUiMetadataTest` SAFE baseline test)" |
| `tests/AGENTS.md` | 6 (`:26,68,73,88,150,151`) | `SAFE-01`, `Phase 2, TEST-02`, `D-24`, `WRITE-08`, `Phase 2 ... (TEST-05): 291/291` | Rewrite `:150-151` as a present-tense instruction (the historic count is meaningless now) |
| `tests/test_database_ui_metadata.cpp` | 42 | `D-0x`, `D2-0x`, `READ-0x`, `SAFE-01/02`, `RENDER-03`, `T-01-03/07`, `Task 1-0x-0x`, `plan 01/02`, `Plan 02-01`, `CONTEXT.md`, `01-PATTERNS.md` | Section-header comments: keep the description after the colon, drop the ID |
| `tests/test_lua_runner_read_csv.cpp` | 67 | `D-xx`, `LUA-03/05/06/07/08`, `TEST-01..04`, `PARSE-02..07`, `02-CONTEXT.md`, `02-RESEARCH.md Finding 4`, `plan 02-01`, `Phase 1/2` | Comments: reason in place. **17 non-comment lines:** see Open Question 1 |
| `tests/test_lua_runner_write_csv.cpp` | 61 | `TEST-06..12`, `FMT-02..08`, `WRITE-03/05/06/07/08`, `LUA-09/10`, `D-14/20/34/39/40/42/44/47/49`, `DOC-05`, `04-01 task 3`, `ROADMAP criterion N`, `RESEARCH.md Q4`, `Pitfall 2`, `Phase 5's` | Several comments are **stale, not just tagged**: `:1121`, `:1532`, `:1554` say `close()` is "this phase's only flush (WRITE-06 is Phase 5's)", but the close-at-exit flush exists now. The true reason the `w:close()` stays: the run-exit flush happens after the `db:read_csv` in the same script. **2 non-comment lines** (`:1605`, `:1689` `FAIL() <<` text) |
| `tests/test_c_api_database_csv_export.cpp` | 8 | `CSV-01..04`, `OPT-01..04` section headers | Drop the ID, keep the description |
| `tests/test_c_api_database_time_series_row.cpp` | 1 | `(CAPI-11..13)` | Drop |
| `tests/test_database_time_series_row.cpp` | 1 | `(CORE-11..14)` | Drop |
| `tests/test_database_update.cpp` | 1 | `(BUG-01)` | Drop |
| `bindings/python/tests/test_database_query.py` | 2 | `(QUERY-01/02)` | Drop |
| `bindings/js/test/database-csv.test.ts` | 2 | `(JSCSV-01/02)` | Drop |

**Non-comment hits (29 lines):**
- 19 diagnostic strings: `test_lua_runner_read_csv.cpp:162-167,198,240,242,248,249` (`"PARSE-0x: ..."`), `:571,573,574,577,580,585` (`"LUA-06: ..."`), `test_lua_runner_write_csv.cpp:1605,1689` (`FAIL() << "... (WRITE-06 not yet implemented ..."`). Each is shown only when its assertion already failed; none is an expected value, a test name, or a compared string.
- 10 Lua `--` comments inside embedded script raw strings (`read_csv.cpp:622,630,654,660,661,667`, `write_csv.cpp:1361,1554,1586,1670`). They are comments; edit them, but keep the script's line count identical (Lua error messages carry `[string "..."]:N:` line numbers; no test asserts one today [VERIFIED: `git grep -nE '\]:[0-9]+:' -- tests` is empty], and keeping the count makes that irrelevant).

**Effort:** about 70 lines in the src/docs half (10 files) and about 186 in the tests half (9 files). Of those, about 170 are in the three big test files. Comment edits are mechanical; the only judgement calls are the three stale `close()` comments, the catalogue pins in `csv_write.cpp`, and Open Question 1.

## DOC-02 Matrix (what each AGENTS.md has vs needs)

Checked by grep and by reading the relevant sections this session.

| File | Layout | C7 rule | Text-only `load`/`run()` | Safety flags | To add / fix |
|------|--------|---------|--------------------------|--------------|--------------|
| `AGENTS.md` (root) | ✓ `:738-739` points to `src/AGENTS.md` | ✗ | ✓ sandbox decision `:85-88` | — (cross-cutting build bullet lists sol2 only) | Add the C7 rule once, cross-layer (e.g. in the "Element arrays accept NULL cells" decision or the CRUD bullet): an empty array to `update_element` clears its group and `create_element` skips it, in every layer, Lua included since 0.13.0. Re-wrap `:85-90` (line 88 is 126 chars; that paragraph wraps at about 100). Optional: one clause on the sol2 build defines in the `sol2 v3.5.0` dependency bullet (`:471`), pointing to `src/AGENTS.md` |
| `src/AGENTS.md` | ✓ file map `:46-57` | ✓ (`lua_table_to_vector` bullet: "an empty array reaches the core ... `create_element` skips and `update_element` turns into a clear") | ✓ (`dofile`/`loadfile` bullet) | ✓ incl. "unchecked in every build, Debug included" | Add `path_policy.h` to the file map; name `SandboxedPathTest` in the "Filesystem sandbox" bullet's coverage sentence; **IN-01**: next to the `lua_type_error` sentence, list the type errors that predate the `got <type>` shape and keep their pinned texts (see below); DOC-01 tags (12 lines) |
| `src/c/AGENTS.md` | n/a (C API TU listing `:38`) | n/a | ✗ | n/a | One clause on `lua_runner.cpp`: `run` passes the script through; the core accepts text chunks only, so bytecode fails with `Failed to run Lua script: ...` |
| `tests/AGENTS.md` | ✓ sync-test paragraph | ✗ | ✓ (`_errors` pins text-only `load`) | ✗ nit `:146` | `:146` "every other sol2 safety is on" → "every other sol2 safety except the stack check is on"; add that `_update` pins the C7 rule (`UpdateElementEmptyArrayClearsGroup`, `...Errors`, `...ClearsEveryGroupSharingTheColumn`) and `_create` the skip (`CreateElementSkipsEmptyArray`); describe `test_sandboxed_path.cpp` (the first test that includes a `src/` header and compiles a `src/` TU, why, and that its suite is outside `Lua*`); DOC-01 tags (6) |
| `bindings/julia/AGENTS.md` | ✓ `:158` | n/a (the binding already cleared) | ✗ | n/a | One clause in the `run!` bullet (`:87`): source only, not bytecode |
| `bindings/dart/AGENTS.md` | ✓ `:65` | n/a | ✗ | stale-cache note `:79-85` already covers config/define changes | One clause in the `LuaRunner.run` bullet (`:121`) |
| `bindings/python/AGENTS.md` | n/a | n/a | ✗ | n/a | One clause in the `LuaRunner.run` bullet (`:109`) |
| `bindings/js/AGENTS.md` | ✓ `:37` | n/a | ✗ | n/a | One clause in the `LuaRunner.run` bullet (`:114`); optionally note that `LUA_DB_API_REFERENCE` now states the sandbox limits |

**IN-01 list (type errors without the `got <type>` suffix), verified in source this session:**
`Cannot read_csv_stream: on_row must be a function` (`src/lua_runner/csv.cpp:351`); `option 'separator' must be a string` (`csv.cpp:173`); `option 'header' entry must be a string` (`csv.cpp:159`); `option 'header_row' must be an integer` (`csv.cpp:241`); `option 'date_time_format' must be a string` (`db_core.cpp:39`); `keys of option '<what>' must be strings` (`db_core.cpp:21`); `option key must be a string` (`internal.h:270`); `<what> has unsupported Lua type` (`internal.h:130,154`; cells, values, `target_label` at `db_write.cpp:189`). The shape everything else uses is `internal.h:185-195`: `"Cannot " + operation + ": " + what + " must be " + expected + ", got " + lua_type_name(got)`. `src/AGENTS.md` already says "the older guarded key checks (`option_entries`, `collect_group_columns`, `string_key`) keep their own pinned texts"; extend that sentence to the value checks above.

## DOC-03 Audit (CHANGELOG `[0.13.0]`)

Against roadmap criterion 4 and Phase 4 criterion 5 [VERIFIED: read `CHANGELOG.md:8-78`]:

| Required | Present |
|----------|---------|
| `## [0.13.0] — unreleased` + compare link `[0.13.0]: https://github.com/psrenergy/quiver/compare/v0.12.9...v0.13.0` | ✓ `:8`, `:1369` |
| BREAKING C1 (table args) and C5 (optional args), each saying what to change | ✓ two entries |
| BREAKING C7 (empty array) | ✓ |
| BREAKING text-only `load` | ✓ |
| Release dot-call UB → error, with sol2's raw text `sol: received nil for 'self' argument…` | ✓ |
| `### Fixed` C2, C4, C6, C8 | ✓ four entries |
| No rename entry | ✓ |
| No planning IDs | ✓ (gate `G` over the section: 0 hits) |
| Version still 0.13.0 | ✓ `uv run python scripts/assert_version.py` → `All project files at 0.13.0` |
| **IN-03** `quiver.metadata_from_element` empty-array text | ✗ **missing** |

**IN-03 facts:** `quiver.metadata_from_element` decodes through `table_to_element("metadata_from_element", t)` [VERIFIED: `src/lua_runner/binary.cpp:272-274`], which now passes an empty array through as `element.set(k, std::vector<int64_t>{})` [VERIFIED: `db_write.cpp:65-68`]. `BinaryMetadata::from_element` requires `dimensions`, `dimension_sizes` and `labels` (`get_string_array`/`get_int_array` throw `"Cannot from_element: missing array '" + name + "'"` at `binary_metadata.cpp:197,213`), while `time_dimensions`/`frequencies` are optional (`get_string_array_opt`, so empty there means the same as absent, with no change). So an empty required array used to report `Cannot from_element: missing array '<name>'` and now reaches validation, e.g. `"Number of labels must be positive, got 0"` (`binary_metadata.cpp:353`) or `"Number of dimensions must be positive, got 0"` (`:348`). An empty `dimension_sizes` next to non-empty `dimensions` most likely reports a count mismatch from `build_metadata` (`:82-88`) [ASSUMED: exact text not probed]. **Probe each case once through `quiver_cli` before writing the sentence.** No Lua test pins it, and adding one would change the `Lua*` count, which D-01 forbids. Add the sentence to the C7 BREAKING entry (IN-03's suggested fix).

The `### Fixed`/`### Changed` structure needs no other change. `SandboxedPathTest`, the sweep and AGENTS.md edits are not user-visible and get no entry. The `lua-api.ts` limits bullet is shipped text; an entry is optional (see Open Question 3).

## DOC-04 (`LUA_DB_API_REFERENCE`)

**Already true:** the empty-array rule (`bindings/js/src/lua-api.ts:309-315`: "**An empty array clears on update.** ... `create_element` skips an empty array"); text-only `load` (`:105-106`); the filesystem sandbox (`:111-117`).

**Missing:** the statement of what the sandbox does not limit. Facts behind it:
- No instruction hook, allocator cap or deadline exists: `git grep -n "set_hook\|lua_sethook\|instruction\|setallocf\|deadline" -- src` is empty [VERIFIED].
- Globals persist across `run()`: one `sol::state` lives in `LuaRunner::Impl` and `run()` calls `impl.lua.safe_script(...)` on it (`src/lua_runner/lua_runner.cpp:132-160`); pinned by `LuaRunner_Lifecycle.MoveConstructor` and its siblings, which set `origin = NAME` in one run and `assert(origin == 'first', ...)` in the next (`tests/test_lua_runner_lifecycle.cpp:27,63`).
- HARD-01..03 (limits, `__close`, fresh `_ENV`) are v2 requirements, so this is a documented limit, not a bug.

**Suggested bullet (Critical rules, right after "Filesystem sandbox"):**
> **What the sandbox does not limit.** The sandbox controls which files a script can touch and which standard libraries exist. It does not bound how much work a script does: there is no instruction-count limit, no memory cap and no wall-clock timeout (`while true do end` runs until the host stops it). Globals persist across `run()` calls on the same runner, so a global one script sets is visible to the next; use `local`. A host that runs untrusted scripts has to impose those limits outside the library.

**Sync-test constraints on any edit** (`bindings/js/test/lua-api-sync.test.ts`):
- Every `db:<name>` / `quiver.<name>` token in the text must be a bound name ("no documented db:/quiver. name has been removed"), so do not write `db:` or `quiver.` before a word that is not a method.
- The sentence `Loaded standard libraries: base, string, table, math, coroutine, utf8.` must stay verbatim (`DOC_FLAT` regex).
- The text is a TS template literal: escape every backtick as `` \` `` and never write `${`.
- The `## CSV file writing` worked example is executed by `LuaRunner_WriteCsv.ReferenceWorkedExampleRunsAndRoundTripsItsOwnData` (`tests/test_lua_runner_write_csv.cpp:1319+`); do not touch that section.
- Run `bunx biome check src/lua-api.ts` (format/lint) as well as the sync test.

**Optional accuracy fixes found while reading:** `:723` quotes `Cannot read_csv: options must be a table`; the code now appends `, got string` (`require_table` → `lua_type_error`, `internal.h:200-209,264`). That is a prefix, so it is not wrong, but writing the full text is cheap. The maintainer header `:14-17` lists only five file-touching operations; the body (`:111-114`) lists all ten. The reference never states the C1/C5 argument-error shape (`Cannot <op>: <what> must be <expected>, got <type>`); one sentence under "Unsupported types throw" would match the BREAKING entries.

## Common Pitfalls

### Pitfall 1: Expected root built from the raw temp path
**What goes wrong:** `escapes the database directory '<root>'` mismatches on macOS (`/private/var/...`) or on Windows with 8.3 `%TEMP%`.
**How to avoid:** `const auto root = fs::weakly_canonical(sandbox);` in the test, and use `root.string()` and `(root / "x").string()` in expectations. The result is in normal form with preferred separators [CITED: en.cppreference.com/w/cpp/filesystem/canonical].
**Warning signs:** passes on Windows/Linux, fails only on the macOS CI job.

### Pitfall 2: Symlink creation not permitted
**What goes wrong:** `create_directory_symlink` fails on Windows without Developer Mode or admin rights.
**How to avoid:** use the `std::error_code` overload and `GTEST_SKIP()` on failure. Locally it works (Developer Mode on: `AllowDevelopmentWithoutDevLicense = 0x1`, and `mklink /D` succeeded this session). GitHub's Windows runners run elevated [ASSUMED]. Linux Docker runs as root.
**Warning signs:** a skipped test in the `SandboxedPathTest` count; record skips alongside the count.

### Pitfall 3: Symlink target left behind
**How to avoid:** create the outside target as a sibling (`fs::path(sandbox.string() + "_outside")`), `remove_all` it before and after, and use `EXPECT_*` (not `ASSERT_*`) between them. `remove_all(sandbox)` in `TearDown` removes the link, not its target.

### Pitfall 4: Diagnostic strings in the sweep
**What goes wrong:** the gate cannot reach 0 without touching 19 failure-message strings, which D-05's wording ("assertion strings never change") seems to forbid.
**How to avoid:** decide Open Question 1 before executing the sweep. Either way, verify with a test-list diff (names unchanged) and a full run (pass/fail unchanged).

### Pitfall 5: Stale comments read as tag-only
**What goes wrong:** removing `WRITE-06 is Phase 5's` from `write_csv.cpp:1121,1532,1554` leaves "close() is this phase's only flush", which is false now.
**How to avoid:** rewrite these to the current reason: the explicit `w:close()` is needed because the close-at-exit flush runs when `run()` returns, after the `db:read_csv` in the same script.

### Pitfall 6: Dart hook cache after a source-list change
**What goes wrong:** listing `lua_runner/path_policy.h` in `QUIVER_SOURCES` changes the source list, which the hook's cache does not notice (`bindings/dart/AGENTS.md:79-85`).
**How to avoid:** delete `bindings/dart/.dart_tool/hooks_runner/` and `bindings/dart/.dart_tool/lib/` before the six-suite run (the roadmap requires this anyway).

### Pitfall 7: The golden baseline's provenance
`build/fixes-check/BASE` is `b39fe78` and `golden.sh` diffs against `baseline/debug`. 04-04 recorded "Debug golden unchanged" after the flag commit. Run `bash build/fixes-check/golden.sh debug` at `0d10f17` **before** the TEST-01 change, so a pre-existing drift is not blamed on the refactor.

### Pitfall 8: Coverage job compiles `path_policy.cpp` twice
The C++ Coverage CI job (`--coverage`) produces coverage data for `path_policy.cpp` from both targets. lcov/gcovr merge by source path, so this is harmless, but the per-line numbers will include the unit test's calls. No action needed; do not be surprised.

## Code Examples

### `src/lua_runner/path_policy.h` (new)
```cpp
#ifndef QUIVER_SRC_LUA_RUNNER_PATH_POLICY_H
#define QUIVER_SRC_LUA_RUNNER_PATH_POLICY_H

#include "quiver/database.h"

#include <string>

namespace quiver::lua_internal {

// The one filesystem gate every file-touching Lua operation routes through. Resolves a
// script-supplied path against the database file's directory and requires the result to stay
// strictly inside it; returns the resolved absolute path. No sol2 here: SandboxedPathTest
// includes this header and compiles path_policy.cpp into quiver_tests.
std::string resolve_sandboxed_path(const Database& db, const std::string& operation, const std::string& path);

}  // namespace quiver::lua_internal

#endif  // QUIVER_SRC_LUA_RUNNER_PATH_POLICY_H
```
`internal.h`: delete the declaration at `:302` and add `#include "lua_runner/path_policy.h"` to its include block. `path_policy.cpp:1`: `#include "lua_runner/internal.h"` → `#include "lua_runner/path_policy.h"`; the comment at `:10-12` can move to the header. Let clang-format 22.1.8 order the includes.

### `tests/CMakeLists.txt` (delta)
```cmake
add_executable(quiver_tests
    ...
    test_row_result.cpp
    test_sandboxed_path.cpp
    test_schema_validator.cpp
    # resolve_sandboxed_path is internal to quiver (hidden, not exported), so SandboxedPathTest
    # compiles its own copy. Keep path_policy.cpp to that one function: anything else the library
    # also uses would be defined twice in a static (QUIVER_BUILD_SHARED=OFF) link.
    ${CMAKE_SOURCE_DIR}/src/lua_runner/path_policy.cpp
)

# path_policy.cpp and the test include "lua_runner/path_policy.h".
target_include_directories(quiver_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
```
(`CMAKE_SOURCE_DIR` is the spelling `src/CMakeLists.txt:65` already uses.)

### `tests/test_sandboxed_path.cpp` (draft; messages verbatim from `path_policy.cpp:18,47,55-57`)
```cpp
#include "lua_runner/path_policy.h"
#include "test_lua_runner.h"

#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>

namespace fs = std::filesystem;
using quiver::lua_internal::resolve_sandboxed_path;

// Calls the gate directly, without Lua. The suite name stays outside the Lua* filter.
class SandboxedPathTest : public LuaSandboxTest {
protected:
    // What the gate prints: on macOS the temp dir sits behind the /var symlink, on Windows it can
    // be an 8.3 short name.
    fs::path root() const { return fs::weakly_canonical(sandbox); }

    std::string escapes(const std::string& operation, const std::string& path) const {
        return "Cannot " + operation + ": path '" + path + "' escapes the database directory '" + root().string() +
               "'";
    }
};

namespace {

quiver::DatabaseOptions quiet() {
    return {.read_only = false, .console_level = quiver::LogLevel::Off};
}

std::string error_of(const quiver::Database& db, const std::string& operation, const std::string& path) {
    try {
        resolve_sandboxed_path(db, operation, path);
    } catch (const std::runtime_error& e) {
        return e.what();
    }
    return "<no throw>";
}

}  // namespace

TEST_F(SandboxedPathTest, RelativePathResolvesInsideTheDatabaseDirectory) {
    quiver::Database db(db_path(), quiet());
    EXPECT_EQ(resolve_sandboxed_path(db, "read_csv", "data.csv"), (root() / "data.csv").string());
}

TEST_F(SandboxedPathTest, SubdirectoryIsAllowed) {
    quiver::Database db(db_path(), quiet());
    EXPECT_EQ(resolve_sandboxed_path(db, "write_csv", "sub/data.csv"), (root() / "sub" / "data.csv").string());
}

TEST_F(SandboxedPathTest, AbsolutePathInsideIsAllowed) {
    quiver::Database db(db_path(), quiet());
    const auto inside = (sandbox / "abs.csv").string();  // un-canonical on purpose
    EXPECT_EQ(resolve_sandboxed_path(db, "open_file", inside), (root() / "abs.csv").string());
}

TEST_F(SandboxedPathTest, DotDotThatStaysInsideIsAllowed) {
    quiver::Database db(db_path(), quiet());
    EXPECT_EQ(resolve_sandboxed_path(db, "read_csv", "sub/../data.csv"), (root() / "data.csv").string());
}

TEST_F(SandboxedPathTest, DotDotEscapeIsRejected) {
    quiver::Database db(db_path(), quiet());
    EXPECT_EQ(error_of(db, "read_csv", "../outside.csv"), escapes("read_csv", "../outside.csv"));
}

TEST_F(SandboxedPathTest, NormalisedEscapeIsRejected) {
    // "sub" does not exist; weakly_canonical still normalises the ".." lexically.
    quiver::Database db(db_path(), quiet());
    EXPECT_EQ(error_of(db, "export_csv", "sub/../../outside.csv"), escapes("export_csv", "sub/../../outside.csv"));
}

TEST_F(SandboxedPathTest, AbsolutePathOutsideIsRejected) {
    quiver::Database db(db_path(), quiet());
    const auto outside = (sandbox.parent_path() / "quiver_sandboxed_path_outside.csv").string();
    EXPECT_EQ(error_of(db, "import_csv", outside), escapes("import_csv", outside));
}

TEST_F(SandboxedPathTest, RootItselfIsRejected) {
    // The binary subsystem appends ".qvr" by concatenation, so the root would write "<root>.qvr" outside.
    quiver::Database db(db_path(), quiet());
    for (const std::string path : {std::string("."), std::string("sub/.."), sandbox.string()}) {
        EXPECT_EQ(error_of(db, "open_file", path), escapes("open_file", path));
    }
}

TEST_F(SandboxedPathTest, SymlinkPointingOutsideIsRejected) {
    const fs::path outside(sandbox.string() + "_outside");
    fs::remove_all(outside);
    fs::create_directories(outside);
    std::error_code ec;
    fs::create_directory_symlink(outside, sandbox / "link", ec);
    if (ec) {
        fs::remove_all(outside);
        GTEST_SKIP() << "cannot create a directory symlink here: " << ec.message();
    }
    {
        quiver::Database db(db_path(), quiet());
        EXPECT_EQ(error_of(db, "write_csv", "link/x.csv"), escapes("write_csv", "link/x.csv"));
    }
    fs::remove_all(outside);
}

TEST_F(SandboxedPathTest, InMemoryDatabaseIsRejectedBeforeContainment) {
    quiver::Database db(":memory:", quiet());
    const std::string in_memory = "Cannot save: database is in-memory, file operations are unavailable";
    EXPECT_EQ(error_of(db, "save", "out"), in_memory);
    EXPECT_EQ(error_of(db, "save", "../out"), in_memory);
}

#ifdef _WIN32
// A device name makes weakly_canonical throw; the OS reason after the prefix is localized.
TEST_F(SandboxedPathTest, DeviceNameIsReportedWithPrefix) {
    quiver::Database db(db_path(), quiet());
    for (const std::string device : {std::string("NUL"), std::string("nul")}) {
        const auto prefix = "Cannot open_file: cannot resolve path '" + device + "': ";
        const auto message = error_of(db, "open_file", device);
        EXPECT_EQ(message.rfind(prefix, 0), 0U) << message;
        EXPECT_GT(message.size(), prefix.size()) << message;
    }
}
#endif
```
Expected count: **11 on Windows, 10 on Linux/macOS** (the device test is `_WIN32`-only). The symlink test may report SKIPPED where symlinks are not allowed. Each operation string is different on purpose, to show the operation is threaded into the message. If `RootItselfIsRejected`'s absolute `sandbox.string()` case trips over a trailing-separator difference, drop that one element; `.` and `sub/..` are the required cases.

**Mutation check (Phase 1 lesson: pins must be shown to fail):** temporarily change `rel.begin()->string() == ".."` to `false` → `DotDotEscapeIsRejected`, `NormalisedEscapeIsRejected`, `AbsolutePathOutsideIsRejected` and `SymlinkPointingOutsideIsRejected` must fail; temporarily drop `rel == "."` → `RootItselfIsRejected` must fail. Revert both.

## State of the Art

| Old | Current | When | Impact |
|-----|---------|------|--------|
| `resolve_sandboxed_path` declared in sol2 `internal.h` | Declared in sol2-free `path_policy.h`, included by `internal.h` | this phase | First `src/` header a test includes |
| Planning IDs as comment tags | Reason text or pinning test name | this phase | Gate `G` returns 0 |

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | MinGW does not auto-export the hidden function from `libquiver.dll` once explicit `dllexport`s exist | Pattern 1 | Low: even if exported, the exe uses its own definition; no CI MinGW job |
| A2 | GitHub Windows runners may create directory symlinks | Pitfall 2 | Low: the test skips instead of failing |
| A3 | Empty `dimension_sizes` beside non-empty `dimensions` reports a `build_metadata` count-mismatch message | DOC-03 | CHANGELOG text wrong; mitigated by the probe step |
| A4 | Static-build reasoning (archive member not pulled) | Pattern 1 | Low: no static CI job; documented in a CMake comment |

## Open Questions

1. **May the 19 failure-diagnostic strings lose their IDs?** (`"PARSE-05: ..."`, `"LUA-06: ..."`, `FAIL() << "... (WRITE-06 not yet implemented ..."`)
   - What we know: none is a compared value or a test name. They appear only after the assertion has already failed. The gate cannot reach 0 without changing them, short of a line-level exclusion that would also hide real IDs.
   - What's unclear: D-05 says "assertion strings never change".
   - Recommendation: read D-05 as "expected values and test names never change" and rewrite the prefixes (e.g. `"PARSE-05: BOM leaked..."` → `"BOM leaked..."`; the `FAIL()` text → "the close-at-exit flush did not reach disk"). Verify with an unchanged `--gtest_list_tests` diff and identical pass counts. Record this interpretation in the plan.
2. **Does the gate cover older-milestone IDs (`CSV-01`, `OPT-01`, `CAPI-11`, `CORE-11`, `BUG-01`, `QUERY-01`, `JSCSV-01`, `PARSE-xx`, `READ-xx`, `D2-xx`, `T-xx-xx`)?**
   - Recommendation: yes. The phase goal says "no planning-ID comment remains in the repo", the cost is about 54 extra lines (most in blocks already being edited), and D-04's "adjust the regex" applies to false positives, not to narrowing.
3. **CHANGELOG entry for the new `lua-api.ts` bullet?** It is shipped text, but it documents existing behaviour. Recommendation: no entry. If the maintainer wants one, a single `### Changed` line saying the Lua reference now states the sandbox's limits.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| MSVC + Ninja | build | ✓ | 14.51 | — |
| `build/release` preset tree | Release gate | ✓ (`build/release` exists) | — | `cmake --preset release` |
| uvx clang-format | format gate | ✓ | 22.1.8 | — |
| run-clang-tidy | tidy baseline | ✓ | LLVM 22.1.3 | — |
| Bun | sync test, biome, JS suite | ✓ | 1.3.14 | — |
| Julia | Julia suite | ✓ | 1.11.9 | — |
| Dart | Dart suite | ✓ | 3.13.4 | — |
| uv | Python suite, assert_version | ✓ | 0.12.3 | — |
| Docker | Linux GCC/Clang runs | ✓ | 29.6.2 | — |
| Symlink permission | symlink test | ✓ (Developer Mode) | — | GTEST_SKIP |
| Golden harness `build/fixes-check/` | behaviour-neutral proof | ✓ (gitignored, local) | BASE `b39fe78` | the 23 Lua-level `escapes the database directory` assertions (plus the in-memory and device-name ones) |

**Missing dependencies:** none.

## Phase Gate Recipe

Counts at `0d10f17` [VERIFIED: run]: `quiver_tests --gtest_filter=Lua*` lists **477 tests in 12 suites**; `quiver_c_tests --gtest_filter=LuaRunnerCApiTest.*` lists **27**; all of `quiver_tests` lists **1443**.

1. Build: `cmake --build build --config Debug` and `cmake --build --preset release`.
2. Counts, Debug and Release: `Lua*` = 477 / 12 suites; `SandboxedPathTest.*` = 11 (Windows), all pass (or the symlink case SKIPPED with a recorded reason); C API `LuaRunnerCApiTest.*` = 27; full `quiver_tests` = 1443 + 11. Test-name invariance: `quiver_tests --gtest_list_tests` before and after differs only by the added `SandboxedPathTest.` block.
3. Golden (cheap, keeps D-03's promise): `bash build/fixes-check/golden.sh debug` and `... release` print `GOLDEN ... OK`, before and after TEST-01. The refactor changes no function body, so this is confirmation, not discovery. It is no longer the only proof: `git diff` of `path_policy.cpp` must show only the include line.
4. Six suites: `rm -rf bindings/dart/.dart_tool/hooks_runner bindings/dart/.dart_tool/lib`, then `cmd //c scripts\\test-all.bat`.
5. Sync + lint: `(cd bindings/js && bun test test/lua-api-sync.test.ts && bunx biome check src/lua-api.ts)`.
6. Format: `uvx --from clang-format==22.1.8 clang-format --dry-run --Werror src/lua_runner/*.cpp src/lua_runner/*.h tests/test_sandboxed_path.cpp` plus every swept `.cpp/.h`.
7. Tidy: `cmd //c scripts\\tidy.bat`; no warning beyond the 14-warning baseline (`build/fixes-check/tidy.txt`). `run-clang-tidy` dedups compile entries by file (`files = {...}` set, line 677), so the second compile of `path_policy.cpp` is not double-reported.
8. Linux: commit first (`linux.sh` uses `git archive HEAD`), add one line to `build/fixes-check/linux.sh`, `./build/bin/quiver_tests --gtest_filter='SandboxedPathTest.*' --gtest_brief=1 ...`, then run `bash build/fixes-check/linux.sh gcc` and `clang`. Expect `Lua*` 475 run, 474 pass + 1 skip; `SandboxedPathTest` 10; C API 27.
9. Planning IDs: gate `G` above returns empty.
10. Comment-only proof for the sweep: `git diff -U0 <phase-base> -- src tests bindings | grep -E '^[-+][^-+]' | grep -vE '^[-+]\s*(//|#|--|\*)'` lists only the TEST-01 files, the approved diagnostic strings (Open Question 1) and nothing else.
11. Version: `uv run python scripts/assert_version.py` → 0.13.0; `git diff --quiet <phase-base> -- CMakeLists.txt bindings/python/pyproject.toml bindings/js/package.json bindings/dart/pubspec.yaml bindings/julia/Project.toml`.

Do **not** reuse `build/fixes-check/gate.sh` unchanged: it hard-codes Phase 4 math (444 + added `test_lua*` TESTs since `b39fe78`), `.set_function(` = 86 and the golden. A small Phase 5 gate (fixed 477/12/27/11 plus `G`) is clearer.

## Security Domain

`security_enforcement: true`, ASVS level 1.

| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | no | — |
| V3 Session Management | no | — |
| V4 Access Control | no | — |
| V5 Input Validation | yes (script-supplied paths) | `resolve_sandboxed_path`, unchanged; TEST-01 adds direct verification |
| V6 Cryptography | no | — |
| V12 Files and Resources (path traversal) | yes | strict containment via `weakly_canonical` + `lexically_relative` |
| V14 Configuration / documentation | yes (DOC-04) | the reference states the sandbox's limits so hosts do not assume a CPU/memory/time bound |

| Threat | STRIDE | Mitigation (existing; now unit-tested) |
|--------|--------|-----------------------------------------|
| `../` or absolute-path traversal | Tampering / Info disclosure | lexical containment after canonicalisation |
| Symlink inside the db dir pointing out | Tampering | `weakly_canonical` resolves the existing prefix, symlinks included [CITED: cppreference canonical] |
| Root-as-target (`<root>.qvr` written outside) | Tampering | `rel == "."` rejected |
| Windows device name leaking a raw std message | Info disclosure | `filesystem_error` wrapped as Pattern 1 |
| Unbounded script (CPU, memory, time) | DoS | **Not mitigated by design** (HARD-01 deferred); DOC-04 makes it explicit |

The refactor must not change the gate: the `git diff` of `path_policy.cpp` shows only the include line, and the golden run plus the Lua-level tests prove behaviour is unchanged. Known limit, unchanged: a TOCTOU race between the check and the later open (a symlink swapped in afterwards) is outside this function's reach.

## Sources

### Primary (HIGH confidence; read or run this session)
- `src/lua_runner/path_policy.cpp` (whole file), `src/lua_runner/internal.h:100-315`, `src/CMakeLists.txt`, `tests/CMakeLists.txt`, `cmake/Platform.cmake`, `include/quiver/export.h`, `include/quiver/database.h:15-40`
- `tests/test_lua_runner.h`, `tests/test_lua_binary.cpp:370-445`, `tests/test_lua_runner_read_csv.cpp` (hit regions), `tests/test_lua_runner_write_csv.cpp` (hit regions), `tests/test_lua_runner_lifecycle.cpp`
- `bindings/js/src/lua-api.ts`, `bindings/js/test/lua-api-sync.test.ts`
- `CHANGELOG.md:8-78,1369`; `.planning/phases/04-fixes-and-release-type-safety/04-REVIEW.md`, `04-04-SUMMARY.md`
- All eight AGENTS.md files
- `build/fixes-check/{golden.sh,gate.sh,linux.sh,BASE}`
- Commands: the roadmap regex and gate `G` over `git grep`; `--gtest_list_tests` counts; `assert_version.py`; `mklink /D` probe; tool `--version`s

### Secondary (MEDIUM confidence)
- cppreference, `std::filesystem::weakly_canonical` / `canonical`: existing prefix canonicalised (symlinks, `.`, `..` resolved), remainder appended, result in normal form. https://en.cppreference.com/w/cpp/filesystem/canonical

### Tertiary (LOW confidence)
- A1-A4 in the Assumptions Log

## Metadata

**Confidence breakdown:**
- TEST-01 link strategy: HIGH. Every symbol and visibility fact was read; static-build and MinGW notes are reasoned, not run.
- DOC-01 inventory: HIGH. Both regexes were run and the difference was computed line by line.
- Docs gaps: HIGH. Each file was grepped and read.
- Pitfalls: HIGH for path and canonicalisation; MEDIUM for CI symlink permissions.

**Research date:** 2026-10-03
**Valid until:** the next commit that touches `src/lua_runner/`, the swept files, or `lua-api.ts` (line numbers are at `0d10f17`)
