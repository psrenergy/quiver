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
- **Scalar bulk NULLs (nullability-aware element type)**: `read_scalar_{integers,floats,strings}`
  first read `get_scalar_metadata(db, collection, attribute).not_null`, then return a **concrete
  `Vector{T}`** for `NOT NULL` columns and a **`Vector{Optional{T}}`** for nullable columns — for
  both the empty and the populated path, so the element type is decided by the schema, not by the
  data in a given read. Nullable decoding is unchanged: `read_scalar_integers`/`read_scalar_floats`
  turn a parallel `Ptr{UInt8}` mask (0 → `nothing`) into the `Union`; `read_scalar_strings`
  null-guards `C_NULL`. The `NOT NULL` branch skips the mask entirely. `c_api.jl` carries the mask
  arg + `quiver_database_free_mask` (regenerate, don't hand-edit long-term). This makes the public
  reader's *inferred* type a 2-way `Union{Vector{T}, Vector{Optional{T}}}` (the choice is a runtime
  metadata lookup) — intended: accurate per-column types over `@inferred` purity; assert with `isa`
  on the result, not `@inferred` on the reader. Julia-only; the `_by_id`/`query_*`/time-series
  readers are not yet converted — see `type_stability_followup.md`. An `INTEGER PRIMARY KEY` (e.g.
  `id`) is a rowid alias and is reported `not_null` by the C++ core (`scalar_metadata_from_column`),
  so `read_scalar_integers(db, c, "id")` is a concrete `Vector{Int64}`.
- **`run!` owns its result**: `quiver_lua_runner_run` takes an `out_result::Ptr{Ptr{Cchar}}` and the
  JSON string must be freed with `quiver_lua_runner_free_string` — *not*
  `quiver_database_free_string`. `check` throws before the `unsafe_string`, and the C API leaves
  `out_result` NULL on failure.
- **Time-series group NULLs**: `read_time_series_group` returns value columns as
  `Vector{Union{T, Nothing}}` **always** (type-stable, like the `Optional{String}` precedent in
  `read_time_series_row`) — a NULL cell is `nothing`; the dimension column stays a dense
  `Vector{DateTime}`. `update_time_series_group!` accepts `nothing` cells, dispatching on
  `Base.nonnothingtype(eltype(v))` with the all-`nothing` branch (`Union{}`) first; it always passes
  a per-column `UInt8` mask (added to the `GC.@preserve` set). An all-`nothing` column marshals as a
  FLOAT tag + zeroed placeholder.
- **One marshaller for all three group writers**: `_update_group_columns(db, update, ...)`
  (`src/database_update.jl`) takes the C entry point as an argument, so `update_time_series_group!`,
  `update_vector_group!` and `update_set_group!` are one-line wrappers over it, including their
  label-addressed methods (multiple dispatch on `element::Union{Int64, String}`, no `_by_label`
  names). `upsert_time_series_row!` shares the same shape via `_upsert_time_series_row`. Don't copy
  the `GC.@preserve` body per writer.
- **Library loader** (`src/c_api.jl`, emitted from `generator/prologue.jl`) is **relocatable** —
  this matters for downstream apps compiled with PackageCompiler (`create_app`), where a baked
  absolute path would freeze the build machine's depot and fail on the target. Split design:
  - at **precompile** time it resolves only the content-addressed artifact **hash**
    (`const _quiver_artifact_hash = find_artifacts_toml(@__DIR__)` → `artifact_hash("quiver", …)`;
    `nothing` in the monorepo, so precompilation still succeeds — this is why `@artifact_str` is
    avoided). A `SHA1` is a value, not a path, so it survives baking + relocation.
  - at **runtime** (`__init__`) `quiver_lib_dir()` picks the directory in three tiers:
    `QUIVER_LIB_DIR` env → `artifact_path(_quiver_artifact_hash)` located against the *runtime*
    `DEPOT_PATH` (published mirror, or the dir bundled into a compiled app) → in-tree `build/`
    (monorepo dev, via `@__DIR__`). It then sets the typed global `libquiver_c::String` (used
    verbatim by every `@ccall`) and pre-`dlopen`s the `libquiver` dependency from the same dir.
  - **Never** move the directory/`libquiver_c` resolution back to module top level (a `const`
    path) — that reintroduces the PackageCompiler relocation bug.
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
