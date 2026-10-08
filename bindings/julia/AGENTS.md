# Julia Binding (Quiver.jl)

Canonical Julia package — the published `psrenergy/Quiver.jl` is a generated mirror of this
directory; never hand-edit the mirror (publishing details in the repo-root `.github/AGENTS.md`). Cross-layer
naming rules (`!` suffix for mutating ops) and the convenience-method parity tables live in the
root `AGENTS.md`.

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

- **Parquet export**: `Binary.bin_to_parquet(path)` forwards directly to
  `quiver_bin_to_parquet`; writers must be closed first. No Arrow dependency is added to Julia.

- **Sandbox array types are handled in C++.** `test/test_sandbox.jl` checks mixed Lua REAL
  arrays on create/update; Julia passes the script through without converting its cells.
- **Regenerate after C API changes**: `generator/generator.bat` rewrites `src/c_api.jl`
  (`prologue.jl`/`epilogue.jl` are spliced around the generated body).
- **Always `GC.@preserve`**: refs produced by `marshal_params` (and any `Ref`s passed as pointers)
  must stay inside a `GC.@preserve refs ...` block spanning the ccall — the GC may otherwise
  collect them mid-call.
- **Free C results in `finally`** when decoding can throw (DateTime parsing, metadata lookups),
  as `read_time_series_group` does — Python's readers follow the same shape.
- **Vector/set NULL cells are nullability-aware too.** All twelve vector/set readers consult
  `list_{vector,set}_groups(...)` for the value column's `not_null` (`_group_value_not_null`,
  `database_read.jl`) and return a concrete `Vector{Vector{Int64}}` / `Vector{Int64}` for a
  `NOT NULL` column, `Optional{...}` otherwise — the scalar rule extended per cell. The lookup runs
  **after** the C read, so an unknown collection reports the reader, not `list_vector_groups`.
  Every cell goes through the C mask (`_masked_cells`; nested `Ptr{Ptr{UInt8}}` in bulk, freed by
  `quiver_database_free_masks`, flat by id, freed by `quiver_database_free_mask`) and every string
  through a `C_NULL` check (`_string_cells`), the concrete path included: a masked cell in a
  `NOT NULL` column (a reader of the wrong type, or a NULL in a non-STRICT composite key, which the
  core reports `not_null`) raises instead of passing the C placeholder `0`/`0.0` off as data, and
  the C arrays are freed in `finally`. The public by-id readers wrap `_read_*_by_id(..., not_null)`
  kernels: `read_{vectors,sets}_by_id` pass the answer from the groups they already listed, so no
  composite pays a `list_*_groups` round-trip per column. The boolean/datetime
  wrappers recover nullability from the delegate's container type (`values isa
  Vector{Vector{Int64}}`), so there is no second metadata hop. `Element` accepts the
  `Vector{Union{Nothing, T}}` a nullable read returns, `Optional{Bool}` from the boolean wrappers
  included (narrowed; a real `nothing` cell raises `ArgumentError` — NULL cells are written with
  `update_vector_group!` / `update_set_group!` / `update_time_series_group!`). The union is an
  explicit list on purpose: a `where T` form would also match `Vector{Any}`, which must keep
  raising `MethodError`.
- **`Element` scalars**: `el[name] = nothing` writes SQL NULL via `quiver_element_set_null`
  (so `create_element!`/`update_element!(...; x = nothing)` clears a column), and any
  `AbstractString` is accepted. Arrays stay non-null (root design decision).
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
  on the result, not `@inferred` on the reader. `read_scalar_booleans` keeps the same
  concrete-vs-optional shape but does **not** re-read the metadata: it recovers the schema's
  nullability from `read_scalar_integers`' container type (`values isa Vector{Int64}`), so it adds
  no FFI round-trip — if that delegate's container type ever changes, this branch changes with it.
  `read_scalar_date_times` is the second wrapper on that mechanism (it branches on
  `values isa Vector{String}`), so both are deliberately **outside** the brace list above.
  Julia-only; the `_by_id`/`query_*`/time-series
  readers are not yet converted — see `type_stability_followup.md`. An `INTEGER PRIMARY KEY` (e.g.
  `id`) is a rowid alias and is reported `not_null` by the C++ core (`scalar_metadata_from_column`),
  so `read_scalar_integers(db, c, "id")` is a concrete `Vector{Int64}`.
- **One date-time grammar, gated in `string_to_date_time`** (`src/date_time.jl`). Julia's
  `dateformat` treats field widths as **maxima** and fills missing trailing components, so it used
  to fabricate a date from truncated input: `"2024"` and `"2024-01"` both read as 2024-01-01 and
  `"20240115"` as **year 20240115**, all without an error, where Python and Dart reject or read
  those differently. `QUIVER_DATE_TIME_PATTERN` now gates the shape to the core's own band
  (`datetime::is_valid_iso8601`) before parsing, and an out-of-range field that clears the regex
  (`"2024-02-31"`) falls through to the same rejection, so the message always names the column.
  Keep the three parsers (here, `_parse_datetime` in Python, `stringToDateTime` in Dart) accepting
  exactly the same set — that intersection is the whole point of the core's write gate. Also note
  `replace(s, ' ' => 'T'; count = 1)`: replacing *every* space turned `"Config 1"` into
  `"ConfigT1"` and quoted that in the error. `string_to_date_time(::Nothing)` returns `nothing`
  (the `_integer_to_boolean` precedent), which is why no caller hand-rolls a null guard.
- **`run!` owns its result**: `quiver_sandbox_run` takes an `out_result::Ptr{Ptr{Cchar}}` and the
  JSON string must be freed with `quiver_sandbox_free_string` — *not*
  `quiver_database_free_string`. `check` throws before the `unsafe_string`, and the C API leaves
  `out_result` NULL on failure. The script must be Lua source text: the core loads it in text mode,
  so a precompiled (bytecode) chunk is rejected with `Failed to run Lua script: ...` and surfaces
  like any other script error.
- **Time-series group NULLs**: `read_time_series_group` returns value columns as
  `Vector{Union{T, Nothing}}` **always** (type-stable, like `read_time_series_row`) — a NULL cell
  is `nothing`; the dimension column stays a dense `Vector{DateTime}`. `update_time_series_group!` accepts `nothing` cells, dispatching on
  `Base.nonnothingtype(eltype(v))` with the all-`nothing` branch (`Union{}`) first; it always passes
  a per-column `UInt8` mask (added to the `GC.@preserve` set). An all-`nothing` column marshals as a
  FLOAT tag + zeroed placeholder.
- **`read_time_series_row` always returns `Vector{Optional{T}}`**, `T` keyed on the returned
  `data_type` (`Int64` / `Float64` / `String` for STRING and DATE_TIME), on the empty path too. Its
  `nothing` means "no data at or before `date_time`", so the optional is inherent — never narrow it
  by `not_null`. It decodes the C API's `out_mask` (returned for every data type, freed with
  `quiver_database_free_mask`) and never `unsafe_string`s a masked-out pointer.
- **One marshaller for every group writer**: `_update_group_columns(db, update, ...)`
  (`src/database_update.jl`) takes the C entry point as an argument, so `update_time_series_group!`,
  `update_vector_group!`, `update_set_group!` and their `_by_label!` forms are one-line
  wrappers over it. Its `key` is a `Union{Int64, String}` — the `@ccall` in `c_api.jl` annotates
  that argument per function, so an id and a label both marshal correctly. Don't copy the
  `GC.@preserve` body per writer.
- **One marshaller for every row upsert**: `_upsert_row_columns(db, upsert, ...)`
  (`src/database_update.jl`) is its row-shaped sibling — every kwarg is a scalar wrapped in a
  1-element typed array — so `upsert_time_series_row!` and `upsert_time_series_row_by_label!` are
  one-line wrappers over it, with the same `key::Union{Int64, String}` note. Kept separate from
  `_update_group_columns` because the row-upsert C signature carries no per-cell NULL mask, and
  that helper writes a zeroed placeholder for a masked cell.
- **The whole-group readers call the native C readers**: `read_vector_group_by_id` /
  `read_set_group_by_id` are one-line wrappers over `_read_group_rows(db, read_group, ...)`
  (`src/database_read.jl`), which takes the C entry point as `_update_group_columns` does, decodes
  the columnar typed arrays + per-cell mask into `Vector{Dict{String, Any}}` rows (masked cell →
  `nothing`, DATE_TIME column → `DateTime` via `string_to_date_time`, never `unsafe_string` on a
  masked-out pointer) and frees with `quiver_database_free_time_series_data` in a `finally`. They
  used to zip one per-column `_by_id` read per column, and a per-column read resolves the column
  *name*: a column another group of the same kind shares came from that group's table (wrong rows,
  or a `BoundsError` from the row count of the last column), and the N reads were N snapshots.
  `read_time_series_group` keeps its own decode on purpose: it returns columns and parses only the
  dimension column, by name.
- **A nullable scalar string argument passes `Ptr{Cchar}(C_NULL)`, never `""`**
  (`update_relation!`/`update_relation_by_label!`) — the C API reads NULL as "clear the relation"
  and an empty string as a label to look up. The `GC.@preserve` rule above does not apply: there
  are no `Ref`s, and `@ccall` pins a `String` argument itself for the duration of the call.
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
- **AbstractExpression**: `abstract type AbstractExpression end` is declared in `src/Quiver.jl`
  before the Binary include, and `Binary.jl` subtypes it through `using ..Quiver: AbstractExpression`
  (subtyping needs no import; extending a function does). `Binary.File` and `Expression` are its
  subtypes, and every expression operation is defined once on it: each converts its operands with
  the private `_expression` (identity for an `Expression`, `Expression(file)` through
  `quiver_expression_from_file` for a file). The C expression copies the file's path, so it
  outlives a later `close!` of its file — but a closed `Binary.File` is itself not an operand:
  `close!` frees the C handle, so it raises `Null argument` (unlike Lua, where a closed file is
  still read by path). The file is passed to `GC.@preserve` across `quiver_expression_from_file`,
  and every converted handle across its operation's ccall. `get_metadata` is one generic owned by
  `Binary` and imported into `Quiver` before `include("expression.jl")` (an import after the
  definition is a load error), so a file answers with its handle's metadata, never through the
  conversion: an unopened `Binary.File(path)` reports its handle's empty metadata, while an
  expression built from it reads the file's metadata from disk. There is no public
  `Expression(::Expression)`: closing the result would close the argument. Do not re-add
  per-type methods for the file type — that was 97 forwarders.
- **Scoped resource factories**: `open`, `from_schema`, `from_migrations`, and
  `Binary.open_file` have callback-first overloads for Julia `do` syntax. They return the
  callback result and call the existing idempotent `close!` from `finally`, so both normal and
  exceptional exits release the resource. The callback is typed **`fn::Function`** — with it
  untyped, an arity slip (`from_schema("a.db", "b.db", "schema.sql")`) dispatches here and the
  factory *runs* before the `MethodError`, and `from_schema` starts with `fs::remove(db_path)`
  while a plain `open` creates the file. The overloads forward `kwargs...` rather than restating
  the base method's keywords, so a keyword added later reaches the `do` form too. Two caveats a
  caller has to know: a `Sandbox` borrows a raw `Database&` (`src/sandbox/sandbox.cpp`), so one built
  inside the block dangles after it (`.ptr` stays non-NULL — no error, just freed memory; the real
  guard belongs in the C API, since Python's `with` has the same hole), and an uncommitted
  transaction open at the block's exit is rolled back by the close — nest
  `transaction(db) do db ... end`.
- **No schemas live in this binding**: `test/fixture.jl` resolves the schema directory at
  runtime, preferring repo-root `tests/schemas/`; the publish workflow copies those schemas
  into the mirror's `test/schemas/`.
- **`Project.toml` is published verbatim**: it shares the published Quiver.jl UUID, and its
  `version` must match the `CMakeLists.txt` version (checked by `scripts/assert_version.py`).
