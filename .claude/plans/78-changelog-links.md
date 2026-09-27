# 78 — CHANGELOG: add the `[0.11.0]` link definition and fix the `[0.10.9]` compare range

**Batch** 7 · **Severity** low · **Breaking** no · **Size** S · **Layers** `CHANGELOG.md` only
**Depends on** none · **Overlaps with** every plan that adds a CHANGELOG entry (they edit the `## [0.11.0]` body; this edits the link definitions at the bottom — no conflict)

## Why

`CHANGELOG.md` uses reference-style version headings. `## [0.11.0] — unreleased` (~L8) has **no**
link definition, so `[0.11.0]` renders as literal bracketed text. The definitions at the bottom
(~L657-666) start with:

```
[0.10.9]: https://github.com/psrenergy/quiver/compare/v0.10.8...HEAD
[0.10.8]: https://github.com/psrenergy/quiver/compare/v0.10.7...v0.10.8
```

0.10.9 was released (its heading ~L84 is dated, and `git tag -l v0.10.9` exists), but its link
still compares against `HEAD`, so it keeps growing to include the 0.11.0 changes. The root
AGENTS.md "Versioning" says the changelog carries "`## [x.y.z] — unreleased` plus its compare link".

## Changes — `CHANGELOG.md`

Replace the first definition line with two lines:
```
[0.11.0]: https://github.com/psrenergy/quiver/compare/v0.10.9...HEAD
[0.10.9]: https://github.com/psrenergy/quiver/compare/v0.10.8...v0.10.9
```
Leave the rest of the list unchanged.

Check the tag name first: `git tag -l "v0.10.9"`. If the tag is missing (tags only reach v0.9.9 in
some clones), fetch tags (`git fetch --tags`) before deciding. If `v0.10.9` really does not exist,
keep `[0.10.9]` on `...HEAD` and add only the `[0.11.0]` line with the same base, and say so in the
commit.

## Tests

None.

## Verification

- Every `## [x.y.z]` heading has a matching `[x.y.z]:` definition:
  `grep -o "^## \[[0-9.]*\]" CHANGELOG.md` vs `grep -o "^\[[0-9.]*\]:" CHANGELOG.md`.

## Acceptance criteria

- [ ] `[0.11.0]` and `[0.10.9]` definitions are correct.

## Pitfalls

- None.

## Out of scope

- Settling the release ritual for this file (root AGENTS.md leaves it open).
