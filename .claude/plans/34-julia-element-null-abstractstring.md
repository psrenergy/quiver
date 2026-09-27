# 34 — Julia `Element`: `setindex!` for `nothing` and for any `AbstractString` scalar

**Batch** 4 · **Severity** medium · **Breaking** no (additive methods) · **Size** S · **Layers** Julia binding only (+ bindings/julia/AGENTS.md, CHANGELOG)
**Depends on** none · **Overlaps with** 21 (deletes `quiver_element_has_scalars`/counters from the C API and regenerates `c_api.jl`; this plan only calls the existing `quiver_element_set_null`, which 21 keeps), 42 (Julia reader renames in other files)

## Why

Julia's `Element` (`bindings/julia/src/element.jl`) has `setindex!` methods only for
`value::Integer`, `::Real`, `::String`, `::DateTime`, `::Vector{DateTime}`, `::Vector{<:Integer}`,
`::Vector{<:Real}` and `::Vector{<:AbstractString}`:

```julia
function Base.setindex!(el::Element, value::String, name::String)
    cname = Base.cconvert(Cstring, name)
    cvalue = Base.cconvert(Cstring, value)
    check(C.quiver_element_set_string(el.ptr, cname, cvalue))
    return nothing
end
```

Two consequences, both reachable through the keyword forms, which all do `e[String(k)] = v`
(`src/database_create.jl` `create_element!(db, collection; kwargs...)`,
`src/database_update.jl` `update_element!(db, collection, id; kwargs...)` and
`update_element_by_label!(db, collection, label; kwargs...)`):

1. **No way to write SQL NULL to a scalar.**
   `Quiver.update_element!(db, "Configuration", 1; float_attribute = nothing)` throws
   `MethodError: no method matching setindex!(::Quiver.Element, ::Nothing, ::String)`.
   The C API function exists and is already generated — `src/c_api.jl`:
   ```julia
   function quiver_element_set_null(element, name)
       @ccall libquiver_c.quiver_element_set_null(element::Ptr{quiver_element_t}, name::Ptr{Cchar})::quiver_error_t
   end
   ```
   but nothing in `bindings/julia/src` calls it (`grep -rn set_null bindings/julia/src` → only
   `c_api.jl`). Every other FFI binding maps its null to it: Python `element.py`
   (`if value is None: self._set_null(name)`), Dart `element.dart` (`case null: setNull(name)`),
   JS `create.ts` (`if (value === null) check(lib.quiver_element_set_null(...))`, tested by
   `bindings/js/test/database-update.test.ts` "updates with null value"). In Julia the only way to
   clear a non-FK scalar today is raw SQL (`update_relation!` covers FK columns only).

2. **`SubString` (any non-`String` `AbstractString`) scalars are rejected** while arrays of them
   work: `create_element!(db, "Configuration"; label = split("a,b", ",")[1])` throws a
   `MethodError` on `SubString{String}`, but `el["tag"] = split("a,b", ",")` works via
   `Vector{<:AbstractString}`.

Principles: Homogeneity (every binding can write NULL), thin binding (the C API already does the
work).

## Constraints and decisions

- **Maintainer decision (binding):** do **not** extend this to nullable `Vector` cells. The root
  design decision "Element arrays accept NULL cells" says "Julia/Python/JS pass a dense (NULL) mask
  and keep their non-null surfaces".
- Match the sibling methods' style: `function ... end`, `check(...)`, `return nothing`.
- No C API change and no regeneration: `quiver_element_set_null` is already in `c_api.jl`.
- `Bool <: Integer`, so booleans keep going through the `Integer` method (root boolean write
  policy) — unaffected.
- `Base.cconvert(Cstring, s)` accepts any `AbstractString` (it converts to `String`, Base
  `strings/cstring.jl`), exactly as the existing `Vector{<:AbstractString}` method relies on, so
  widening the signature needs no body change.

## Changes

### 1. `bindings/julia/src/element.jl` — widen the scalar string setter

Current (currently ~L30-35):
```julia
function Base.setindex!(el::Element, value::String, name::String)
    cname = Base.cconvert(Cstring, name)
    cvalue = Base.cconvert(Cstring, value)
    check(C.quiver_element_set_string(el.ptr, cname, cvalue))
    return nothing
end
```
New (signature only):
```julia
function Base.setindex!(el::Element, value::AbstractString, name::String)
    cname = Base.cconvert(Cstring, name)
    cvalue = Base.cconvert(Cstring, value)
    check(C.quiver_element_set_string(el.ptr, cname, cvalue))
    return nothing
end
```
The `DateTime` method (`el[name] = date_time_to_string(value)`) calls it with a `String`, which
still dispatches here. No ambiguity: no other method takes an `AbstractString` scalar.

### 2. `bindings/julia/src/element.jl` — add a `Nothing` method

Insert right after the string method:
```julia
# `nothing` writes SQL NULL (quiver_element_set_null). Scalars only: Julia's array surface stays
# non-null (root design decision "Element arrays accept NULL cells").
function Base.setindex!(el::Element, ::Nothing, name::String)
    cname = Base.cconvert(Cstring, name)
    check(C.quiver_element_set_null(el.ptr, cname))
    return nothing
end
```

No other file changes. The kwargs forms in `database_create.jl`/`database_update.jl` pick the new
methods up automatically.

## Tests

### `bindings/julia/test/test_element.jl`

Add two testsets inside `@testset "Element" begin ... end`, next to "Set String" (currently ~L26),
in the file's existing style:

```julia
    @testset "Set Nothing" begin
        el = Quiver.Element()
        el["label"] = "Test"
        el["value"] = nothing
        @test occursin("value: null", string(el))
    end

    @testset "Set SubString" begin
        el = Quiver.Element()
        el["label"] = split("a,b", ",")[1]
        @test occursin("label: \"a\"", string(el))
    end
```
`Base.show(io, ::Element)` prints `quiver_element_to_string`, i.e. C++ `Element::to_string`
(`src/element.cpp`), which renders each scalar as `    <name>: <value>` with `null` for a NULL value
and double-quoted strings. That is why the assertions check `value: null` and `label: "a"`.

### `bindings/julia/test/test_database_update.jl`

Add inside `@testset "Update" begin ... end`:

```julia
    @testset "Element Scalar Set To Nothing" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Config 1", float_attribute = 1.5)
        @test Quiver.read_scalar_float_by_id(db, "Configuration", "float_attribute", 1) == 1.5

        Quiver.update_element!(db, "Configuration", 1; float_attribute = nothing)
        @test isnothing(Quiver.read_scalar_float_by_id(db, "Configuration", "float_attribute", 1))

        Quiver.update_element_by_label!(db, "Configuration", "Config 1"; float_attribute = 2.5)
        Quiver.update_element_by_label!(db, "Configuration", "Config 1"; float_attribute = nothing)
        @test isnothing(Quiver.read_scalar_float_by_id(db, "Configuration", "float_attribute", 1))

        Quiver.close!(db)
    end
```
(`basic.sql`: `float_attribute REAL` is nullable; `Configuration` is `id INTEGER PRIMARY KEY`, so
the first element is id 1.)

### `bindings/julia/test/test_database_create.jl`

Add inside its top-level `@testset`:

```julia
    @testset "Scalar Nothing And SubString" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        label = split("a,b", ",")[1]            # SubString{String}
        Quiver.create_element!(db, "Configuration"; label = label, string_attribute = nothing)

        @test Quiver.read_scalar_string_by_id(db, "Configuration", "label", 1) == "a"
        @test isnothing(Quiver.read_scalar_string_by_id(db, "Configuration", "string_attribute", 1))

        Quiver.close!(db)
    end
```
(`test_database_create.jl`'s top-level testset is `@testset "Create" begin`. Databases are closed with
`Quiver.close!(db)`.)

Before the change all three new DB tests throw `MethodError` at the `e[String(k)] = v` line.

## Docs and changelog

- `bindings/julia/AGENTS.md`: add one bullet under "Rules and gotchas":
  > - **`Element` scalars**: `el[name] = nothing` writes SQL NULL via `quiver_element_set_null`
  >   (so `create_element!`/`update_element!(...; x = nothing)` clears a column), and any
  >   `AbstractString` is accepted. Arrays stay non-null (root design decision).
- `CHANGELOG.md`, `## [0.11.0] — unreleased` → `### Added`:
  ```markdown
  - **Julia: `Element` accepts `nothing` and any `AbstractString` scalar.** `update_element!(db, c,
    id; attr = nothing)` (and the `create_element!` / `update_element_by_label!` keyword forms) now
    write SQL NULL, as every other binding already could; previously it raised a `MethodError`. A
    `SubString` scalar is accepted too, not only `String`.
  ```

## Verification

From the repo root:
1. `cmake --build build --config Debug` (the Julia tests load the built C library).
2. `bindings/julia/test/test.bat test_element.jl`
3. `bindings/julia/test/test.bat test_database_update.jl`
4. `bindings/julia/test/test.bat test_database_create.jl`
5. `bindings/julia/test/test.bat` — full Julia suite.
6. `scripts/format.bat` (runs the Julia formatter via `bindings/julia/format`).

## Acceptance criteria

- [ ] `setindex!(::Element, ::Nothing, ::String)` exists and calls `quiver_element_set_null`.
- [ ] The scalar string method takes `AbstractString`.
- [ ] New tests pass; full Julia suite green.
- [ ] AGENTS.md bullet and CHANGELOG entry added.

## Pitfalls

- Do not add a `Vector{Union{Nothing, T}}` method — that contradicts the recorded decision.
- `update_relation!` already treats `nothing` as "clear the relation" through its own path; it is
  unaffected.
- `test.bat` takes the test file name relative to `bindings/julia/test/` (see `runtests.jl`:
  `include(joinpath(@__DIR__, ARGS[1]))`).

## Out of scope

- Nullable vector/set cells in Julia `Element`.
- Any C API change.
