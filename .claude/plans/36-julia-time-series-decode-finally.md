# 36 — Julia `read_time_series_group`: free the C buffers in `finally`

**Batch** 4 · **Severity** low · **Breaking** no · **Size** S · **Layers** Julia binding only
**Depends on** none · **Overlaps with** 17 (edits `read_time_series_row`, the next function in the same file), 18 (adds a vector/set group decoder in the same file and explicitly leaves `read_time_series_group` alone), 41/42 (comment/rename edits elsewhere in `database_read.jl`), 22 (Julia `query_string` signature — the test below calls `query_string(db, sql)`, valid before and after 22)

## Why

`bindings/julia/src/database_read.jl`, `read_time_series_group` (currently ~L627-697). The C
result is allocated by `C.quiver_database_read_time_series_group(...)` and freed only at the end
of the happy path:

```julia
    col_count = out_col_count[]
    row_count = out_row_count[]

    if col_count == 0 || row_count == 0
        return Dict{String, Vector}()
    end

    # Get dimension column name for DateTime parsing
    metadata = get_time_series_metadata(db, collection, group)
    dim_col = metadata.dimension_column
    ...
            if col_name == dim_col
                result[col_name] =
                    DateTime[string_to_date_time(unsafe_string(p), collection, col_name) for p in str_ptrs]
    ...
        else
            throw(ArgumentError("Unsupported data type $(col_type) for column '$col_name'"))
        end
    end

    # Free C-allocated memory
    C.quiver_database_free_time_series_data(
        out_col_names[], out_col_types[], out_col_data[], out_col_has_value[],
        Csize_t(col_count), Csize_t(row_count),
    )

    return result
end
```

Three calls between the allocation and the free can throw:
- `get_time_series_metadata(db, collection, group)`;
- `string_to_date_time(...)`, which throws `ArgumentError` on a dimension cell that is not a valid
  `YYYY-MM-DD[THH:MM:SS]` (`src/date_time.jl`);
- the `throw(ArgumentError("Unsupported data type ..."))` branch.

Any of them leaks every name, type, data and mask array. A malformed dimension value can reach the
reader from a database written before the DATE_TIME write gate existed, by another tool, or by raw
SQL. Python already wraps the same decode in `try: ... finally:
lib.quiver_database_free_time_series_data(...)` (`bindings/python/src/quiverdb/database.py`,
`read_time_series_group`).

Principle: Ownership — "RAII used strictly. Ownership of pointers/resources must be explicit and
unambiguous" (root CLAUDE.md).

## Constraints and decisions

- **Maintainer notes (binding):** local fix only; **keep** the `get_time_series_metadata` call.
  Do not add a shared helper: nothing else in Julia decodes this columnar result today, and plan
  18's vector/set decoder deliberately leaves `read_time_series_group` alone.
- The early return for an empty result stays **before** the `try`: when `col_count == 0 ||
  row_count == 0` the C API allocates nothing, so there is nothing to free on that path.
- Everything the decoder keeps is copied out before the free: `unsafe_string` copies, and the
  comprehensions build fresh `Vector`s from the `unsafe_wrap`ped views, so freeing in `finally`
  after building `result` is safe.

## Changes

### `bindings/julia/src/database_read.jl` — `read_time_series_group`

Replace everything from the metadata lookup to the end of the function. Current (from
`# Get dimension column name for DateTime parsing` through the final `end`):

```julia
    # Get dimension column name for DateTime parsing
    metadata = get_time_series_metadata(db, collection, group)
    dim_col = metadata.dimension_column

    # Unmarshal column names, types, data, and per-cell NULL masks
    name_ptrs = unsafe_wrap(Array, out_col_names[], col_count)
    ...
    # Free C-allocated memory
    C.quiver_database_free_time_series_data(
        out_col_names[], out_col_types[], out_col_data[], out_col_has_value[],
        Csize_t(col_count), Csize_t(row_count),
    )

    return result
end
```

New:

```julia
    # Free in `finally`: the metadata lookup, the DateTime parse of a malformed dimension cell and
    # the unsupported-type branch can all throw while the C buffers are held. The decode copies
    # everything out (unsafe_string, fresh Vectors) before the free runs.
    try
        # Get dimension column name for DateTime parsing
        metadata = get_time_series_metadata(db, collection, group)
        dim_col = metadata.dimension_column

        # Unmarshal column names, types, data, and per-cell NULL masks
        name_ptrs = unsafe_wrap(Array, out_col_names[], col_count)
        type_vals = unsafe_wrap(Array, out_col_types[], col_count)
        data_ptrs = unsafe_wrap(Array, out_col_data[], col_count)
        mask_ptrs = unsafe_wrap(Array, out_col_has_value[], col_count)

        # Value columns are typed Optional{T}: mask[r] == 0 surfaces as `nothing`. The
        # dimension column's mask is always all 1, so it stays a dense Vector{DateTime}.
        result = Dict{String, Vector}()
        for i in 1:col_count
            col_name = unsafe_string(name_ptrs[i])
            col_type = type_vals[i]
            mask = unsafe_wrap(Array, mask_ptrs[i], row_count)

            if col_type == Cint(C.QUIVER_DATA_TYPE_INTEGER)
                int_arr = unsafe_wrap(Array, reinterpret(Ptr{Int64}, data_ptrs[i]), row_count)
                result[col_name] = Optional{Int64}[mask[r] != 0 ? int_arr[r] : nothing for r in 1:row_count]
            elseif col_type == Cint(C.QUIVER_DATA_TYPE_FLOAT)
                float_arr = unsafe_wrap(Array, reinterpret(Ptr{Float64}, data_ptrs[i]), row_count)
                result[col_name] = Optional{Float64}[mask[r] != 0 ? float_arr[r] : nothing for r in 1:row_count]
            elseif col_type == Cint(C.QUIVER_DATA_TYPE_STRING) || col_type == Cint(C.QUIVER_DATA_TYPE_DATE_TIME)
                str_ptr_ptr = reinterpret(Ptr{Ptr{Cchar}}, data_ptrs[i])
                str_ptrs = unsafe_wrap(Array, str_ptr_ptr, row_count)
                if col_name == dim_col
                    result[col_name] =
                        DateTime[string_to_date_time(unsafe_string(p), collection, col_name) for p in str_ptrs]
                else
                    # Never unsafe_string a masked-out (NULL) pointer.
                    result[col_name] =
                        Optional{String}[mask[r] != 0 ? unsafe_string(str_ptrs[r]) : nothing for r in 1:row_count]
                end
            else
                throw(ArgumentError("Unsupported data type $(col_type) for column '$col_name'"))
            end
        end
        return result
    finally
        C.quiver_database_free_time_series_data(
            out_col_names[], out_col_types[], out_col_data[], out_col_has_value[],
            Csize_t(col_count), Csize_t(row_count),
        )
    end
end
```

The body inside `try` is the current code verbatim, indented one level; only the free moves into
`finally` and `return result` moves inside `try`. If plans 02/17/18 have touched this function in
the meantime, apply the same shape to the current body: wrap from the metadata lookup to the end
of the decode loop, move the free into `finally`.

## Tests

### `bindings/julia/test/test_database_time_series_group.jl`

Add inside `@testset "Time Series Group" begin ... end`:

```julia
    @testset "Read With Malformed Dimension Throws And Frees" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        id = Quiver.create_element!(db, "Collection"; label = "Item 1")
        Quiver.update_time_series_group!(db, "Collection", "data", id;
            date_time = ["2024-01-01T10:00:00"],
            value = [1.5],
        )
        # Bypass the DATE_TIME write gate with raw SQL, as a pre-gate database or another tool would.
        Quiver.query_string(db, "UPDATE Collection_time_series_data SET date_time = '2024-1-5' WHERE id = ?", [id])

        @test_throws ArgumentError Quiver.read_time_series_group(db, "Collection", "data", id)
        # The handle is still healthy afterwards: a second read fails the same way, no crash.
        @test_throws ArgumentError Quiver.read_time_series_group(db, "Collection", "data", id)

        Quiver.close!(db)
    end
```

What it proves: the error path runs through `finally` without double-free or crash (the second
call re-allocates and re-frees). The leak itself is not observable from Julia; the code review of
the `try/finally` shape is the check. Confirm first that `query_string` executes a DML statement
(it prepares and steps the statement through `Database::execute`; an UPDATE returns no row, so it
returns `nothing`). If `query_string` with a parameter vector has a different signature after
plan 22, use the current signature (`grep -n "function query_string" bindings/julia/src/database_query.jl`).

Before the change this test also passes (the throw happens either way) — it is a behaviour pin for
the error path, not a failing-first test. Say so in the commit message.

## Docs and changelog

- `bindings/julia/CLAUDE.md`: no rule currently describes free placement for this reader; add one
  line to the "Always `GC.@preserve`" area:
  > - **Free C results in `finally`** when decoding can throw (DateTime parsing, metadata lookups),
  >   as `read_time_series_group` does — Python's readers follow the same shape.
- `CHANGELOG.md`, `## [0.11.0] — unreleased` → `### Fixed`:
  ```markdown
  - **Julia: `read_time_series_group` no longer leaks when decoding fails.** A dimension value that
    is not a valid date (possible in a database written before the DATE_TIME write gate, or by raw
    SQL) raised before the C result was freed.
  ```

## Verification

From the repo root:
1. `cmake --build build --config Debug`
2. `bindings/julia/test/test.bat test_database_time_series_group.jl`
3. `bindings/julia/test/test.bat`
4. `scripts/format.bat`

## Acceptance criteria

- [ ] The free is in a `finally` that covers the metadata lookup and the decode loop.
- [ ] The empty-result early return stays before the `try`.
- [ ] New testset passes; full suite green.

## Pitfalls

- Do not move the empty-result early return inside `try` — the `finally` would then free NULL
  arrays with counts 0 (harmless, but it changes nothing and reads wrong).
- `return result` must be inside `try`, otherwise `result` is out of scope.

## Out of scope

- The vector/set group readers (plan 18) and `read_time_series_row` (plan 17).
- Dropping the `get_time_series_metadata` call (column 0 is the dimension per
  `include/quiver/c/database.h`); maintainer asked to keep it.
