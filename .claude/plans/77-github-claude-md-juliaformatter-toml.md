# 77 — `.github/AGENTS.md`: drop the deleted `.JuliaFormatter.toml` from the mirror file list

**Batch** 7 · **Severity** low · **Breaking** no · **Size** S · **Layers** docs only (`.github/AGENTS.md`)
**Depends on** none · **Overlaps with** 81, 88 (other `.github/AGENTS.md` edits)

## Why

`.github/AGENTS.md` (~L108-112) describes what `publish-julia.yml` copies into the `Quiver.jl`
mirror, and lists `.JuliaFormatter.toml`:

> ... `README.md`, `.gitignore`, `.gitattributes`, `.JuliaFormatter.toml`, `LICENSE`, and the
> verbatim `Project.toml` ...

Commit 79ac8ba ("feat: add Style.jl to julia binding (#274)") deleted
`bindings/julia/.JuliaFormatter.toml`. `git ls-files | grep JuliaFormatter.toml` finds nothing.
`publish-julia.yml` copies the whole `bindings/julia/` tree (`cp -a bindings/julia/. quiver-jl/`,
~L74), so the listed file cannot be there.

## Constraints and decisions

- **Only this line.** A review claim that "JuliaFormatter" is stale in root AGENTS.md ~L430 and
  `bindings/julia/AGENTS.md` ~L16 was **refuted**: `bindings/julia/format/format.jl` runs
  `Style.format(...)`, and psrenergy/Style.jl wraps JuliaFormatter with PSR's fixed options. So
  "JuliaFormatter" is still the formatter. Leave those two mentions alone.

## Changes — `.github/AGENTS.md`

In that sentence, delete `` `.JuliaFormatter.toml`, `` so the list reads
"... `README.md`, `.gitignore`, `.gitattributes`, `LICENSE`, and the verbatim `Project.toml` ...".

## Tests

None.

## Verification

- `grep -n "JuliaFormatter.toml" .github/AGENTS.md` prints nothing.

## Acceptance criteria

- [ ] The stale file name is removed. No other edit.

## Pitfalls

- None.

## Out of scope

- Root AGENTS.md and `bindings/julia/AGENTS.md` JuliaFormatter mentions (correct as written).

## Implementation notes

- **Done as planned.** `` `.JuliaFormatter.toml`, `` was removed from the Quiver.jl mirror file
  list in `.github/AGENTS.md`, a one-line diff. The paragraph was not reflowed.
- **Claim re-checked before editing.** `git ls-files` finds no `.JuliaFormatter.toml`, and
  `bindings/julia/` holds none. `publish-julia.yml` L75 copies the tree with
  `cp -a bindings/julia/. quiver-jl/`.
- **Drift fixed:** the sentence was at L121, not ~L108-112. The quoted excerpt matched exactly.
  The two out-of-scope JuliaFormatter mentions are at root `AGENTS.md` L492 and
  `bindings/julia/AGENTS.md` L16, and were left alone.
- **Master:** `origin/master` (`961287b`, plan 70) was already in the branch, so
  `git merge origin/master` reported "Already up to date".
- **Verification:** `grep -n "JuliaFormatter.toml" .github/AGENTS.md` prints nothing.
  `scripts/format.bat` exited 0. clang-format, Julia, Dart and ruff changed nothing. Biome again
  rewrote 43 JS files from CRLF to LF with no content change (the whitespace-insensitive diff was
  empty), so `git checkout -- bindings/js` restored them.
- **No CHANGELOG entry.** This is internal agent docs, not a user-visible change.
- **For 81/88:** neither touches this line. 81 adds a line to the child-publish description, and
  88 edits ~L13 and ~L142, so they should apply cleanly on top of this.
