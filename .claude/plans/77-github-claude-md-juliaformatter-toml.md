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
