# 76 — `src/CLAUDE.md`: where `query_int_rows` lives, and the "every source has a public header" claim

**Batch** 7 · **Severity** low · **Breaking** no · **Size** S · **Layers** docs only (`src/CLAUDE.md`)
**Depends on** none · **Overlaps with** 53 (deletes `query_int_rows` and rewrites the same `describe*` sentence — **if 53 has landed, step 1 is already done; skip it**), 55 (adds three more no-public-header `.cpp` files and edits the same "first `.cpp` in `src/`" paragraph)

## Why

Two statements in `src/CLAUDE.md` are false at HEAD.

1. **~L418**: the `describe*` bullet says they "run their own read-only SQL via an anon-namespace
   `query_int_rows` helper that prepares/steps directly on `impl_->db` (the `current_version() const`
   pattern — `execute()` is non-const)". `query_int_rows` is an `inline` free function in
   `namespace quiver` in `src/database_impl.h` (~L26-27), not in an anonymous namespace. It is also
   shared: `number_of_elements` calls it (`src/database_read.cpp` ~L226). `current_version` does not
   use it; it prepares its own statement.
2. **~L90-93**: "`csv/csv_read.h`/`.cpp` is the first `.cpp` in `src/` with no `include/quiver/`
   public counterpart — ... every other `QUIVER_SOURCES` entry implements a public header." But
   `src/CMakeLists.txt`'s `QUIVER_SOURCES` also lists `csv/csv_write.cpp` and `ui_metadata.cpp`,
   neither of which has a public header. The same file later says so (~L128 "Same
   no-`include/quiver/`-header ... posture as `csv_read`", ~L141).

## Changes — `src/CLAUDE.md`

1. **Only if plan 53 has not landed.** In the ~L418 bullet, replace "via an anon-namespace
   `query_int_rows` helper that prepares/steps directly on `impl_->db`" with "via `query_int_rows`
   (an inline helper in `database_impl.h`, also used by `number_of_elements`), which prepares and
   steps directly on `impl_->db`". Keep the "(the `current_version() const` pattern — `execute()` is
   non-const)" parenthetical; `current_version` does its own prepare. If 53 **has** landed, the
   sentence should already say they use the const `Impl::execute`. Check that it does.
2. ~L92-93: replace "and every other `QUIVER_SOURCES` entry implements a public header" with "and
   `csv/csv_write.cpp` and `ui_metadata.cpp` (below) share the same no-public-header posture". If
   plan 55 has landed, also list `schema.cpp`, `schema_validator.cpp` and `type_validator.cpp` there,
   or better, say "several internal `.cpp` files (`csv/csv_write.cpp`, `ui_metadata.cpp`,
   `schema*.cpp`, `type_validator.cpp`) share that posture". Change "is the first `.cpp`" to "was the
   first `.cpp`".

Confirm the `QUIVER_SOURCES` list before editing: `grep -n "QUIVER_SOURCES" -A40 src/CMakeLists.txt`.

## Tests

None.

## Verification

- Re-read both paragraphs. Every file name they mention must exist, and every claim must match
  `src/CMakeLists.txt`.

## Acceptance criteria

- [ ] Both statements are accurate.

## Pitfalls

- Coordinate with 53/55, which edit the same paragraphs.

## Out of scope

- Other `src/CLAUDE.md` edits (owned by the plans that change the code).
