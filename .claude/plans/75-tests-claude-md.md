# 75 — `tests/CLAUDE.md`: fix claims that contradict the tests

**Batch** 7 · **Severity** low · **Breaking** no · **Size** S · **Layers** docs only (`tests/CLAUDE.md`)
**Depends on** none · **Overlaps with** 01, 06, 52, 62, 65, 67, 69 (each adds a schema, a test file, a file-list entry or a rule line to `tests/CLAUDE.md` — this plan fixes the pre-existing errors; re-read the file after those land and keep their additions)

## Why

`tests/CLAUDE.md` says things that are false at HEAD.

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

Principle: "Self-Updating" (root CLAUDE.md). The nearest CLAUDE.md must describe the code.

## Changes — `tests/CLAUDE.md`

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
`for f in $(grep -o "test_[a-z_{},]*\.cpp" tests/CLAUDE.md | sort -u); do ...; done`. Expand brace
lists by hand. Fix anything else that is stale in the same pass.

## Tests

None (docs).

## Docs and changelog

This plan is the docs change. No CHANGELOG entry.

## Verification

- `grep -n "omission of elements" tests/CLAUDE.md` prints nothing.
- Every `test_*.cpp` named in `tests/CLAUDE.md` exists in `tests/`, and every file in `tests/`
  matching `test_*.cpp` is mentioned (spot-check with `ls tests/test_*.cpp`).

## Acceptance criteria

- [ ] The four corrections are made, and the file lists match `ls tests`.

## Pitfalls

- Other plans add lines to this file. Merge with them; don't overwrite.

## Out of scope

- Restructuring `tests/CLAUDE.md`.
