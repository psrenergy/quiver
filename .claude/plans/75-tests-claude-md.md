# 75 — `tests/AGENTS.md`: fix claims that contradict the tests

**Batch** 7 · **Severity** low · **Breaking** no · **Size** S · **Layers** docs only (`tests/AGENTS.md`)
**Depends on** none · **Overlaps with** 01, 06, 52, 62, 65, 67, 69 (each adds a schema, a test file, a file-list entry or a rule line to `tests/AGENTS.md` — this plan fixes the pre-existing errors; re-read the file after those land and keep their additions)

## Why

`tests/AGENTS.md` says things that are false at HEAD.

1. **Vector/set date-time coverage** (~L138-140): "vector/set coverage includes empty reads and
   omission of elements without group rows." The core now LEFT JOINs (root design decision "Bulk
   reads of one collection are positionally aligned"), so an element with no group rows is an
   **empty inner list**, not omitted. Every binding's test asserts that:
   `bindings/julia/test/test_database_read_vector.jl` (~L158-164, `[..., []]`),
   `bindings/python/tests/test_database_read_vector.py` (~L159-167) and
   `bindings/dart/test/database_read_vector_test.dart` (~L272-281).
2. **The C++ file list** (~L8-15) omits `test_database_metadata.cpp`, which exists and is registered
   (`tests/CMakeLists.txt` ~L17).
3. **The Lua per-area list** (~L33-35) omits `_describe` (`test_lua_runner_describe.cpp`,
   registered at `tests/CMakeLists.txt` ~L38).
4. **The C++ vs C API comparison** (~L87-89): "the C API adds `test_c_api_database_metadata.cpp`
   and has no errors file". But the C++ core also has a metadata file (point 2). The real
   differences come from `ls tests`: the C API has no `describe`, `errors` or `ui_metadata` database
   file, and its describe / describe_collection / summarize_collection tests live in
   `test_c_api_database_metadata.cpp`. The sentence at ~L92-94 already covers the extra
   `_nulls` file.

Principle: "Self-Updating" (root AGENTS.md). The nearest AGENTS.md must describe the code.

## Changes — `tests/AGENTS.md`

1. ~L140: "vector/set coverage includes empty reads and omission of elements without group rows."
   becomes "vector/set coverage includes empty reads, and an element with no group rows reads back
   as an empty inner list."
2. ~L8-15: add `test_database_metadata.cpp` (group metadata / FK flags) to the Database list, e.g.
   after `test_database_describe.cpp`.
3. ~L33-35: add `_describe` to the Lua per-area list.
4. ~L87-89: replace the parenthetical "the file sets diverge slightly: the C API adds
   `test_c_api_database_metadata.cpp` and has no errors file" with "the file sets diverge
   slightly: the C API has no `describe`, `errors` or `ui_metadata` file (its
   describe/describe_collection/summarize_collection coverage lives in
   `test_c_api_database_metadata.cpp`)". Leave ~L92-94's `_nulls` sentence as it is.

Before editing, confirm each claim against `ls tests` and `tests/CMakeLists.txt`. Also check whether
the file lists any other test file that no longer exists:
`for f in $(grep -o "test_[a-z_{},]*\.cpp" tests/AGENTS.md | sort -u); do ...; done`. Expand brace
lists by hand. Fix anything else that is stale in the same pass.

## Tests

None (docs).

## Docs and changelog

This plan is the docs change. No CHANGELOG entry.

## Verification

- `grep -n "omission of elements" tests/AGENTS.md` prints nothing.
- Every `test_*.cpp` named in `tests/AGENTS.md` exists in `tests/`, and every file in `tests/`
  matching `test_*.cpp` is mentioned (spot-check with `ls tests/test_*.cpp`).

## Acceptance criteria

- [x] The four corrections are made, and the file lists match `ls tests`. Claim 1 was already fixed
  by `61e6236`; this plan made corrections 2-4.

## Pitfalls

- Other plans add lines to this file. Merge with them; don't overwrite.

## Out of scope

- Restructuring `tests/AGENTS.md`.

## Implementation notes

- **Branch base:** merging `origin/master` fast-forwarded `rs/plan75` to plan 74 (`3447a34`). That
  merge did not touch `tests/AGENTS.md`.
- **Drift: claim 1 was already fixed.** Commit `61e6236` ("fix: preserve NULL cells in vector and
  set reads") rewrote the "omission of elements without group rows" sentence before this plan ran.
  The paragraph (now ~L150-164, "The native-DateTime bindings ...") says the vector/set readers
  cover elements without group rows, and that the C++ core and C ABI pin the no-rows / NULL-only-row
  pair. That is correct, so change 1 was skipped. `grep -n "omission of elements" tests/AGENTS.md`
  prints nothing.
- **What changed (only `tests/AGENTS.md`):**
  - **Database list:** added `test_database_metadata.cpp` (group-metadata FK flags,
    `list_{vector,set}_groups`) after `test_database_describe.cpp`. The parenthetical reflects the
    file's actual tests: `GetVectorMetadataForeignKey`, `GetSetMetadataForeignKey`,
    `GetSetMetadataNonForeignKeyColumn`, `ListVectorAndSetGroups`, `ListGroupsCollectionNotFound`.
    The rest of the bullet was reflowed to ~100 columns; the line was 119 before.
  - **Lua per-area list:** added `_describe` after `_query`. The file holds `DescribeReport`,
    `DescribeCollection` and `SummarizeCollection`.
  - **C API section:** the parenthetical now reads "the C API has no `describe`, `errors` or
    `ui_metadata` file (its describe/describe_collection/summarize_collection coverage lives in
    `test_c_api_database_metadata.cpp`)". The `_nulls` sentence is unchanged.
- **Stale sweep: nothing else to fix.** Every checked item exists and matches:
  - Every `test_*.cpp`/`.h` named in the file exists, with brace lists expanded. Every on-disk
    `test_*.cpp` is named literally, through a brace list, through the `test_c_api_database_*.cpp`
    glob, or through the Lua `_suffix` list.
  - The `valid/` and `invalid/` schema lists, `migrations/1-3`, `issues/issue52,issue70` and the
    two `fixtures/` CSVs match disk.
  - Every cited test, fixture and helper name exists.
  - The binding file names are accurate, and neither JS nor Python has a time-series `metadata`
    file.
- **`test_utils.h` is the one `tests/` file the doc never mentions.** It is the shared
  `path_from` / `quiet_options` / `VALID_SCHEMA` header. It was left out, because this plan's check
  covers `test_*.cpp` and adding a description would restructure the doc.
- **Verification:**
  - `scripts/format.bat` exited 0.
  - Biome's CRLF→LF rewrite of 31 JS files was confirmed EOL-only
    (`git diff --ignore-cr-at-eol` is empty) and restored with `git checkout -- bindings/js`.
  - No build or test run: nothing outside docs changed.
- **For later plans:**
  - Plan 82 (the build-all sentence near the end) and plan 85 (the release-preset paragraph in
    "Binding suites") edit other hunks of `tests/AGENTS.md`.
  - The Database bullet is two lines longer and the C API paragraph gains one line, so their
    `~L` hints shift by +3. Their quoted excerpts are untouched.
