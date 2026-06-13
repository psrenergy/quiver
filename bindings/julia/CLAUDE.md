# Julia Binding (Quiver.jl)

Canonical Julia package — the published `psrenergy/Quiver.jl` is a generated mirror of this
directory; never hand-edit the mirror (publishing details in the repo-root `.github/CLAUDE.md`). Cross-layer
naming rules (`!` suffix for mutating ops) and the convenience-method parity tables live in the
root `CLAUDE.md`.

## Layout

```
src/              # Hand-written wrappers (database_*.jl, element.jl, helper_maps.jl, ...)
src/c_api.jl      # GENERATED low-level FFI module (do not hand-edit; regenerate)
src/binary/       # Binary subsystem wrappers (Metadata, File, csv_converter)
generator/        # Clang.jl-based generator: generator.bat runs generator.jl (own Project.toml;
                  # config in generator.toml; prologue.jl/epilogue.jl spliced around the output)
format/           # JuliaFormatter runner (called by scripts/format.bat)
revise/           # Revise.jl hot-reload helper for development
test/             # Test suite (runtests.jl + test_*.jl per area)
example2.jl       # Binary/expression subsystem demo (.qvr arithmetic)
Project.toml      # Deps: Artifacts, CEnum, Dates, Libdl; julia 1.11 compat
```

## Rules and gotchas

- **Regenerate after C API changes**: `generator/generator.bat` rewrites `src/c_api.jl`
  (`prologue.jl`/`epilogue.jl` are spliced around the generated body).
- **Always `GC.@preserve`**: refs produced by `marshal_params` (and any `Ref`s passed as pointers)
  must stay inside a `GC.@preserve refs ...` block spanning the ccall — the GC may otherwise
  collect them mid-call.
- **Time-series group NULLs**: `read_time_series_group` returns value columns as
  `Vector{Union{T, Nothing}}` **always** (type-stable, like the `Optional{String}` precedent in
  `read_time_series_row`) — a NULL cell is `nothing`; the dimension column stays a dense
  `Vector{DateTime}`. `update_time_series_group!` accepts `nothing` cells, dispatching on
  `Base.nonnothingtype(eltype(v))` with the all-`nothing` branch (`Union{}`) first; it always passes
  a per-column `UInt8` mask (added to the `GC.@preserve` set). An all-`nothing` column marshals as a
  FLOAT tag + zeroed placeholder.
- **Library loader** (`src/c_api.jl`, emitted from `generator/prologue.jl`) resolves `libquiver_c`
  in three tiers: `QUIVER_LIB_DIR` env → S3 artifact (when an `Artifacts.toml` is present, i.e. the
  published mirror) → in-tree `build/` (monorepo dev, the default here). Uses the runtime Artifacts
  functional API (`find_artifacts_toml`/`artifact_hash`/`artifact_path`), NOT `@artifact_str`, so the
  monorepo (which has no `Artifacts.toml`) still precompiles.
- **Manifest conflicts**: delete `bindings/julia/Manifest.toml`, then
  `julia --project=bindings/julia -e "using Pkg; Pkg.instantiate()"`.
- **Julia-only surfaces**: the binary/expression wrappers (`src/binary/`, expression functions)
  and the relation-map helpers (`src/helper_maps.jl`: `scalar_relation_map`/`set_relation_map`,
  FK column derived from the naming convention, mapping each element to the positional index of
  its related element) exist only in this binding — documented exceptions in the root design
  decisions.
- **No schemas live in this binding**: `test/fixture.jl` resolves the schema directory at
  runtime, preferring repo-root `tests/schemas/`; the publish workflow copies those schemas
  into the mirror's `test/schemas/`.
- **`Project.toml` is published verbatim**: it shares the published Quiver.jl UUID, and its
  `version` must match the `CMakeLists.txt` version (checked by `scripts/assert_version.py`).
