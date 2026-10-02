# 78 — CHANGELOG: add the `[0.12.0]` link definition and fix the `[0.10.9]` compare range

**Batch** 7 · **Severity** low · **Breaking** no · **Size** S · **Layers** `CHANGELOG.md` only
**Depends on** none · **Overlaps with** every plan that adds a CHANGELOG entry (they edit the `## [0.12.0]` body; this edits the link definitions at the bottom — no conflict)

## Why

`CHANGELOG.md` uses reference-style version headings. `## [0.12.0] — unreleased` (~L8) has **no**
link definition, so `[0.12.0]` renders as literal bracketed text. The definitions at the bottom
(~L657-666) start with:

```
[0.10.9]: https://github.com/psrenergy/quiver/compare/v0.10.8...HEAD
[0.10.8]: https://github.com/psrenergy/quiver/compare/v0.10.7...v0.10.8
```

0.10.9 was released (its heading ~L84 is dated, and `git tag -l v0.10.9` exists), but its link
still compares against `HEAD`, so it keeps growing to include the 0.12.0 changes. The root
AGENTS.md "Versioning" says the changelog carries "`## [x.y.z] — unreleased` plus its compare link".

## Changes — `CHANGELOG.md`

Replace the first definition line with two lines:
```
[0.12.0]: https://github.com/psrenergy/quiver/compare/v0.10.9...HEAD
[0.10.9]: https://github.com/psrenergy/quiver/compare/v0.10.8...v0.10.9
```
Leave the rest of the list unchanged.

Check the tag name first: `git tag -l "v0.10.9"`. If the tag is missing (tags only reach v0.9.9 in
some clones), fetch tags (`git fetch --tags`) before deciding. If `v0.10.9` really does not exist,
keep `[0.10.9]` on `...HEAD` and add only the `[0.12.0]` line with the same base, and say so in the
commit.

## Tests

None.

## Verification

- Every `## [x.y.z]` heading has a matching `[x.y.z]:` definition:
  `grep -o "^## \[[0-9.]*\]" CHANGELOG.md` vs `grep -o "^\[[0-9.]*\]:" CHANGELOG.md`.

## Acceptance criteria

- [x] `[0.12.0]` and `[0.10.9]` definitions are correct.

## Pitfalls

- None.

## Out of scope

- Settling the release ritual for this file (root AGENTS.md leaves it open).

## Implementation notes

Implemented on `rs/plan78` at base HEAD `961287b` (plan 70), which already matched
`origin/master`, so the merge was a no-op. Only `CHANGELOG.md` changed.

**Drift (changed the specifics, not the design; maintainer approved the widened scope):** the plan
was written when the top heading was `## [0.12.0] — unreleased`. Since then v0.11.0 and
v0.12.0–v0.12.8 all shipped (tags and GitHub releases exist; `CMakeLists.txt` is at 0.12.9). The
literal edit, `[0.12.0]` → `v0.10.9...HEAD`, would therefore have been wrong. The definitions
block had moved to ~L1297 (not ~L657), and ten headings (`[0.12.8]`…`[0.11.0]`) had no
definition at all, not one. Also, `[0.12.5]`–`[0.12.8]` were still headed "— unreleased" although
released. Before closing each range at its tag, I checked that every section from 0.10.9 through
0.12.8 is byte-identical to the file as it shipped at that tag (0.12.4 was edited after release,
which doesn't affect its range).

**Done:**
- Replaced `[0.10.9]: …/compare/v0.10.8...HEAD` with eleven closed definitions, newest first:
  `[0.12.8]` (`v0.12.7...v0.12.8`) down to `[0.11.0]` (`v0.10.9...v0.11.0`) and
  `[0.10.9]` (`v0.10.8...v0.10.9`). The rest of the list is unchanged.
- Dated the four released headings with their GitHub release dates (local −03:00, the convention
  the existing dated headings follow; the UTC dates are the same days): `[0.12.8] — 2026-10-01`,
  `[0.12.7] — 2026-10-01`, `[0.12.6] — 2026-09-30`, `[0.12.5] — 2026-09-29`. Without that, a closed
  `v0.12.7...v0.12.8` link would sit under a heading saying "unreleased", which is the
  inconsistency this plan cites for 0.10.9.
- Added no `## [0.12.9] — unreleased` heading and no `...HEAD` link. There are no 0.12.9 entries
  yet, and by convention the first entry creates its heading (plan 53 created `[0.12.8]` that way).

**Results:** the heading-vs-definition `diff` is empty (20 headings, 20 definitions, same order),
and each definition's range is `v<previous heading>...v<own heading>`. `grep -c '\.\.\.HEAD'` is 0,
and no heading says "unreleased". `gh api repos/psrenergy/quiver/compare/<range>` resolves all 11
new ranges (`ahead`, `behind=0`). `scripts/format.bat` exited 0. As in plan 68, Biome rewrote
the 43 JS files from CRLF to LF with no content diff; I restored them with
`git checkout -- bindings/js`.

**For later plans / the next release:** whoever adds the first 0.12.9 entry creates
`## [0.12.9] — unreleased` and adds
`[0.12.9]: https://github.com/psrenergy/quiver/compare/v0.12.8...HEAD` at the top of the
definitions block. When a version ships, date its heading and change `...HEAD` to `...v<version>`.
Until the release ritual covers this, the file will drift again after every release.
