# Phase 12: Julia Binding Migration - Research

**Researched:** 2026-02-19
**Domain:** Julia FFI binding migration for multi-column time series C API
**Confidence:** HIGH

## Summary

Phase 12 migrates Julia's `update_time_series_group!` and `read_time_series_group` from the old single-value-column C API to the new multi-column typed C API delivered in Phase 11. The old C API signature (`date_times, values, row_count`) has been replaced with a columnar interface (`column_names[], column_types[], column_data[], column_count, row_count`), and the old Julia binding must be rewritten to use this new interface.

The migration is well-scoped. The new C API functions already exist with the correct signatures in the C header and have passing C-level tests. The Julia `c_api.jl` file must be regenerated (via the Clang-based generator) to pick up the new function signatures, then the high-level Julia wrapper functions (`update_time_series_group!` and `read_time_series_group`) in `database_update.jl` and `database_read.jl` must be rewritten to marshal kwargs/columnar data to/from the C API. Existing tests must be rewritten to use the new interface and new mixed-type tests added.

**Primary recommendation:** Regenerate `c_api.jl`, rewrite `update_time_series_group!` with kwargs-to-columnar marshaling, rewrite `read_time_series_group` with columnar-to-Dict unmarshaling, rewrite all time series tests.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Pure kwargs syntax: `update_time_series_group!(db, col, grp, id; date_time=[...], temperature=[...], humidity=[...])`
- Each kwarg is a column name matching the schema, value is a typed vector
- Auto-coerce types: Int passed for REAL column converts to Float64, etc. Error on truly incompatible types (String for REAL)
- No Julia-side validation of column names -- let C API validate and surface its error messages (CAPI-05)
- No kwargs = clear all rows: `update_time_series_group!(db, col, grp, id)` clears rows for that element
- Return `Dict{String, Vector}` -- columnar format with column names as keys, typed vectors as values
- Runtime concrete types: `Vector{Int64}` for INTEGER, `Vector{Float64}` for REAL, `Vector{String}` for TEXT
- Empty results return empty `Dict{String, Vector}()` (no keys, no data)
- Regular Dict, no order guarantee on iteration (access by column name)
- Replace in-place -- no deprecation wrappers, no backwards compatibility
- Old row-dict interface (`Vector{Dict{String, Any}}` parameter) removed entirely
- Regenerate c_api.jl from current C headers using `bindings/julia/generator/generator.bat`
- Rewrite existing time series tests to use new kwargs/columnar interface
- Add multi-column mixed-type tests using Phase 11 schema (INTEGER + REAL + TEXT value columns)
- Full DateTime support on dimension column: accept Julia DateTime on update (convert to string), parse to DateTime on read
- Error on parse failure -- if stored string can't parse to DateTime, throw error (forces data consistency)
- Reuse existing `date_time.jl` parsing logic (parse_date_time/format_date_time)
- Only dimension column (identified via metadata) gets DateTime treatment -- other TEXT columns remain String
- Update accepts both String and DateTime for the dimension column

### Claude's Discretion
- Dict value type precision (abstract `Vector` vs `Union{Vector{Int64}, Vector{Float64}, Vector{String}}`)
- Internal marshaling strategy for kwargs -> C API columnar arrays
- GC.@preserve patterns for safe FFI calls
- Test file organization (new file vs extend existing)

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| BIND-01 | Julia update_time_series_group! accepts kwargs: `update_time_series_group!(db, col, group, id; date_time=[...], val=[...])` | Kwargs marshaling pattern documented in Architecture Patterns; C API update signature mapped in Code Examples |
| BIND-02 | Julia read_time_series_group returns multi-column data with correct types per column | Columnar unmarshaling pattern documented; void** -> typed array casting with column_types dispatch documented in Code Examples |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Julia `@ccall` | Built-in | FFI calls to C API | Standard Julia FFI mechanism, already used throughout `c_api.jl` |
| `GC.@preserve` | Built-in | Prevent GC of references during FFI | Required for safe FFI with heap-allocated Julia objects |
| `unsafe_wrap`/`unsafe_load`/`unsafe_string` | Built-in | Unmarshal C-allocated data | Standard pattern used in all existing read functions |
| Clang.jl Generator | Per Project.toml | Regenerate `c_api.jl` from C headers | Project-standard binding generator |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `Dates` (stdlib) | Built-in | DateTime parsing/formatting | Dimension column DateTime conversion |
| `CEnum` | Per Project.toml | C enum types in Julia | Already used for `quiver_data_type_t` etc. |

### Alternatives Considered
None -- all tools are project-established patterns with no alternatives worth considering.

## Architecture Patterns

### Relevant Files
```
bindings/julia/src/c_api.jl              # Generated FFI bindings (REGENERATE)
bindings/julia/src/database_update.jl    # update_time_series_group! (REWRITE)
bindings/julia/src/database_read.jl      # read_time_series_group (REWRITE)
bindings/julia/src/date_time.jl          # DateTime parsing (REUSE as-is)
bindings/julia/src/database_metadata.jl  # get_time_series_metadata (REUSE as-is)
bindings/julia/test/test_database_time_series.jl  # Tests (REWRITE + EXTEND)
tests/schemas/valid/mixed_time_series.sql          # Multi-column schema (EXISTS from Phase 11)
```

### Pattern 1: Kwargs-to-Columnar Marshaling (update)
**What:** Convert Julia kwargs into the C API's parallel arrays: `column_names[]`, `column_types[]`, `column_data[]`, plus `column_count` and `row_count`.
**When to use:** `update_time_series_group!` implementation.
**Key design decisions:**
1. No kwargs = clear: call C API with `column_count=0, row_count=0, NULL arrays`
2. Type inference from Julia vector element types (not from schema metadata): `Vector{<:Integer}` -> `QUIVER_DATA_TYPE_INTEGER`, `Vector{<:Real}` -> `QUIVER_DATA_TYPE_FLOAT`, `Vector{<:AbstractString}` or `Vector{DateTime}` -> `QUIVER_DATA_TYPE_STRING`
3. Auto-coercion: Int values in a Float column vector get converted to Float64 (Julia does this naturally via the `Real` supertype), DateTime values get formatted to String
4. Row count validation: all kwarg vectors must have the same length
5. Dimension column detection for DateTime formatting: use `get_time_series_metadata` to identify which column is the dimension, format DateTime values in that column

**Type coercion strategy for update:**
- For each kwarg value vector, determine C type from Julia element type
- `DateTime` values: format to String via `date_time_to_string()`, pass as `QUIVER_DATA_TYPE_STRING`
- `String`/`AbstractString`: pass as `QUIVER_DATA_TYPE_STRING`
- `Integer` (Int64, Int32, etc.): pass as `QUIVER_DATA_TYPE_INTEGER` with `Int64[]` conversion
- `Real` (Float64, Float32, etc.): pass as `QUIVER_DATA_TYPE_FLOAT` with `Float64[]` conversion
- Mixed types in a single vector: error (each column vector must be homogeneous)

**Note on auto-coercion of Int->Float:** The user decision says "Int passed for REAL column converts to Float64." However, since we're NOT doing Julia-side column name validation (per CAPI-05), we infer the C type from the Julia vector type, not the schema. If the user passes `[20]` (Int vector) for a REAL column, Julia would infer INTEGER type and send that to C API, which would fail with a type mismatch. To honor the auto-coercion decision, the binding should fetch metadata and coerce Int vectors to Float64 when the schema expects REAL. This requires one metadata call per update.

**Recommended approach:** Fetch metadata, build a schema type map, then for each kwarg: if the Julia vector type doesn't match the schema type, attempt coercion (Int->Float via Float64 conversion). If coercion is impossible (String->Float), let the C API reject it.

### Pattern 2: Columnar-to-Dict Unmarshaling (read)
**What:** Convert the C API's multi-column return (`column_names[]`, `column_types[]`, `column_data[]`, `column_count`, `row_count`) into `Dict{String, Vector}`.
**When to use:** `read_time_series_group` implementation.
**Key design decisions:**
1. Empty result (row_count == 0): return `Dict{String, Vector}()`
2. For each column, switch on `column_types[i]`:
   - `QUIVER_DATA_TYPE_INTEGER`: cast `column_data[i]` to `Ptr{Int64}`, `unsafe_wrap` + `copy` -> `Vector{Int64}`
   - `QUIVER_DATA_TYPE_FLOAT`: cast `column_data[i]` to `Ptr{Float64}`, `unsafe_wrap` + `copy` -> `Vector{Float64}`
   - `QUIVER_DATA_TYPE_STRING` or `QUIVER_DATA_TYPE_DATE_TIME`: cast to `Ptr{Ptr{Cchar}}`, iterate and `unsafe_string` -> `Vector{String}` (or `Vector{DateTime}` for dimension column)
3. Dimension column detection: use metadata to identify the dimension column name, parse its strings to `DateTime` via `string_to_date_time()`
4. Free all C memory via `quiver_database_free_time_series_data` after copying
5. Dict value type: use abstract `Vector` as the value type (`Dict{String, Vector}`), concrete vectors are stored (`Vector{Int64}`, etc.)

### Pattern 3: GC.@preserve for Multi-Array FFI Calls
**What:** Ensure all Julia-allocated arrays (column name strings, column data arrays) remain live during the `@ccall`.
**When to use:** Both update and read operations.
**Key considerations:**
- The update call needs to keep alive: column name `Cstring` conversions, integer arrays, float arrays, and string pointer arrays
- Use a collector pattern: accumulate all `refs` in a `Vector{Any}`, then wrap the entire `@ccall` in `GC.@preserve refs`
- Existing patterns in `database_query.jl` (`marshal_params`) and `database_update.jl` (`update_vector_strings!`) show the established approach

### Pattern 4: DateTime Handling on Dimension Column
**What:** The dimension column (identified by `get_time_series_metadata().dimension_column`) receives special DateTime treatment.
**On update:** If a kwarg value is `Vector{DateTime}`, format each element to String. If it's already `Vector{String}`, pass through.
**On read:** Parse the dimension column's String values to `DateTime` using `string_to_date_time()`. Other TEXT columns remain as `String`.
**Error handling:** If `string_to_date_time()` fails to parse, it will throw a Julia error (consistent with existing behavior in `read_scalar_date_time_by_id`).

### Anti-Patterns to Avoid
- **Julia-side column name validation:** The project philosophy (CAPI-05, "Intelligence resides in C++ layer. Bindings remain thin") mandates letting the C API validate column names and types. Do NOT add Julia-side checks for column existence.
- **Preserving old API signatures:** The decision is replace-in-place. Do NOT keep the old `rows::Vector{Dict{String, Any}}` parameter signature even temporarily.
- **Creating DateTime wrappers for non-dimension TEXT columns:** Only the dimension column gets DateTime parsing. Other TEXT columns return plain String.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Column name validation | Julia-side schema checks | C API validation (CAPI-05) | Project philosophy: thin bindings, intelligence in C++ |
| DateTime parsing | Custom regex/format parser | `date_time.jl` existing `string_to_date_time`/`date_time_to_string` | Already handles the `yyyy-mm-ddTHH:MM:SS` format |
| FFI binding generation | Manual `@ccall` signatures | `bindings/julia/generator/generator.bat` | Clang.jl generator ensures type-accurate bindings |
| Type enum mapping | Hardcoded integer constants | `C.QUIVER_DATA_TYPE_INTEGER` etc. from generated `c_api.jl` | Generated enums stay in sync with C headers |

**Key insight:** The C API already handles all validation (column names, type matching, dimension column presence). The Julia binding's only job is type marshaling and GC safety.

## Common Pitfalls

### Pitfall 1: GC Collecting String Arrays During FFI Call
**What goes wrong:** Julia GC collects the `Cstring` conversions before the `@ccall` completes, causing use-after-free / garbage data.
**Why it happens:** `Base.unsafe_convert(Cstring, ...)` returns a raw pointer without preventing GC of the source.
**How to avoid:** Wrap all `@ccall` invocations in `GC.@preserve` blocks that cover all Julia-allocated string/array objects. Use the pattern from `update_vector_strings!` (line 83 of `database_update.jl`): build `cstrings` array, derive `ptrs`, then `GC.@preserve cstrings begin ... end`.
**Warning signs:** Intermittent test failures, corrupted string data, segfaults.

### Pitfall 2: void** Casting for column_data
**What goes wrong:** The C API uses `void**` for `column_data` (each element is a typed array pointer). Incorrect pointer casting causes segfaults or wrong data.
**Why it happens:** Julia's type system is stricter than C's void pointers. Need explicit `Ptr{Cvoid}` wrapping.
**How to avoid:** For update: build a `Vector{Ptr{Cvoid}}` where each element is a `pointer(typed_array)` cast. For read: receive `Ptr{Ptr{Cvoid}}`, load each element, then `reinterpret` to the correct pointer type based on `column_types[i]`.
**Warning signs:** Segfaults, wrong values, type confusion.

### Pitfall 3: Row Count Mismatch Across Kwargs
**What goes wrong:** Different kwarg vectors have different lengths, leading to buffer overruns in the C API.
**Why it happens:** User error (passing vectors of different lengths).
**How to avoid:** Validate all kwarg vector lengths match before building C arrays. Throw `ArgumentError` with clear message. This is data integrity validation, not schema validation, so it belongs in Julia.
**Warning signs:** C-level crashes, memory corruption.

### Pitfall 4: Forgetting to Free C Memory After Read
**What goes wrong:** Memory leak from not calling `quiver_database_free_time_series_data` with the new 5-argument signature.
**Why it happens:** The old free function had a different signature (3 args). The new one requires `column_names, column_types, column_data, column_count, row_count`.
**How to avoid:** Always call free after copying data to Julia-owned containers. Use try-finally if needed.
**Warning signs:** Growing memory consumption, no immediate crash.

### Pitfall 5: Old c_api.jl Bindings with Wrong Signatures
**What goes wrong:** Julia calls the old 7-arg `quiver_database_read_time_series_group` / `quiver_database_update_time_series_group` / `quiver_database_free_time_series_data` but the DLL now exports the new 9-arg / 9-arg / 5-arg signatures.
**Why it happens:** Forgetting to regenerate `c_api.jl` after Phase 11 changed the C API.
**How to avoid:** Regenerate `c_api.jl` FIRST using `bindings/julia/generator/generator.bat` before any code changes.
**Warning signs:** Segfaults, argument count mismatch errors.

### Pitfall 6: Auto-Coercion Requires Metadata
**What goes wrong:** User passes `[20, 30]` (Int vector) for a REAL column. Julia infers INTEGER type, C API rejects type mismatch.
**Why it happens:** Without checking metadata, the binding maps Julia types directly to C types, but the user expects transparent Int->Float coercion.
**How to avoid:** Fetch metadata once in `update_time_series_group!`, compare each kwarg's inferred type against schema type, coerce when safe (Int->Float). Only fetch metadata if kwargs are provided (skip for clear operation).
**Warning signs:** "column has type FLOAT but received INTEGER" errors when users pass Int vectors for REAL columns.

## Code Examples

### Update: Kwargs to C API (Recommended Implementation Skeleton)

```julia
function update_time_series_group!(
    db::Database,
    collection::String,
    group::String,
    id::Int64;
    kwargs...,
)
    # No kwargs = clear operation
    if isempty(kwargs)
        check(C.quiver_database_update_time_series_group(
            db.ptr, collection, group, id,
            C_NULL, C_NULL, C_NULL, Csize_t(0), Csize_t(0),
        ))
        return nothing
    end

    # Validate all vectors same length
    row_count = length(first(values(kwargs)))
    for (k, v) in kwargs
        if length(v) != row_count
            throw(ArgumentError("All column vectors must have the same length, got $(length(v)) for '$(k)' but expected $row_count"))
        end
    end

    # Fetch metadata for type coercion
    metadata = get_time_series_metadata(db, collection, group)
    schema_types = Dict{String, C.quiver_data_type_t}()
    schema_types[metadata.dimension_column] = C.QUIVER_DATA_TYPE_STRING
    for vc in metadata.value_columns
        schema_types[vc.name] = vc.data_type
    end

    column_count = length(kwargs)
    col_names_strs = String[]
    col_types = Cint[]
    col_data_ptrs = Ptr{Cvoid}[]
    refs = Any[]  # Keep references alive for GC.@preserve

    for (k, v) in kwargs
        col_name = String(k)
        push!(col_names_strs, col_name)
        schema_type = get(schema_types, col_name, nothing)

        # Determine C type + marshal data, with auto-coercion
        if v isa Vector{DateTime} || (eltype(v) <: DateTime)
            # DateTime -> format to string
            str_vals = [date_time_to_string(dt) for dt in v]
            cstrs = [Base.cconvert(Cstring, s) for s in str_vals]
            ptrs = Ptr{Cchar}[Base.unsafe_convert(Cstring, cs) for cs in cstrs]
            push!(refs, cstrs)
            push!(refs, ptrs)
            push!(col_types, Cint(C.QUIVER_DATA_TYPE_STRING))
            push!(col_data_ptrs, pointer(ptrs))
        elseif eltype(v) <: AbstractString
            cstrs = [Base.cconvert(Cstring, s) for s in v]
            ptrs = Ptr{Cchar}[Base.unsafe_convert(Cstring, cs) for cs in cstrs]
            push!(refs, cstrs)
            push!(refs, ptrs)
            push!(col_types, Cint(C.QUIVER_DATA_TYPE_STRING))
            push!(col_data_ptrs, pointer(ptrs))
        elseif eltype(v) <: Integer
            # Check if schema expects FLOAT -> auto-coerce
            if schema_type == C.QUIVER_DATA_TYPE_FLOAT
                float_arr = Float64[Float64(x) for x in v]
                push!(refs, float_arr)
                push!(col_types, Cint(C.QUIVER_DATA_TYPE_FLOAT))
                push!(col_data_ptrs, pointer(float_arr))
            else
                int_arr = Int64[Int64(x) for x in v]
                push!(refs, int_arr)
                push!(col_types, Cint(C.QUIVER_DATA_TYPE_INTEGER))
                push!(col_data_ptrs, pointer(int_arr))
            end
        elseif eltype(v) <: Real
            float_arr = Float64[Float64(x) for x in v]
            push!(refs, float_arr)
            push!(col_types, Cint(C.QUIVER_DATA_TYPE_FLOAT))
            push!(col_data_ptrs, pointer(float_arr))
        else
            throw(ArgumentError("Unsupported column type: $(eltype(v)) for column '$col_name'"))
        end
    end

    # Build column_names as Cstring array
    name_cstrs = [Base.cconvert(Cstring, n) for n in col_names_strs]
    name_ptrs = Ptr{Cchar}[Base.unsafe_convert(Cstring, cs) for cs in name_cstrs]
    push!(refs, name_cstrs)
    push!(refs, name_ptrs)

    # Build column_types and column_data arrays
    col_types_arr = Cint[t for t in col_types]
    col_data_arr = Ptr{Cvoid}[p for p in col_data_ptrs]
    push!(refs, col_types_arr)
    push!(refs, col_data_arr)

    GC.@preserve refs begin
        check(C.quiver_database_update_time_series_group(
            db.ptr, collection, group, id,
            name_ptrs, col_types_arr, col_data_arr,
            Csize_t(column_count), Csize_t(row_count),
        ))
    end
    return nothing
end
```

### Read: C API to Dict (Recommended Implementation Skeleton)

```julia
function read_time_series_group(db::Database, collection::String, group::String, id::Int64)
    out_col_names = Ref{Ptr{Ptr{Cchar}}}(C_NULL)
    out_col_types = Ref{Ptr{Cint}}(C_NULL)
    out_col_data = Ref{Ptr{Ptr{Cvoid}}}(C_NULL)
    out_col_count = Ref{Csize_t}(0)
    out_row_count = Ref{Csize_t}(0)

    check(C.quiver_database_read_time_series_group(
        db.ptr, collection, group, id,
        out_col_names, out_col_types, out_col_data, out_col_count, out_row_count,
    ))

    col_count = out_col_count[]
    row_count = out_row_count[]

    if col_count == 0 || row_count == 0
        return Dict{String, Vector}()
    end

    # Get dimension column name for DateTime parsing
    metadata = get_time_series_metadata(db, collection, group)
    dim_col = metadata.dimension_column

    # Unmarshal column names, types, data
    name_ptrs = unsafe_wrap(Array, out_col_names[], col_count)
    type_vals = unsafe_wrap(Array, out_col_types[], col_count)
    data_ptrs = unsafe_wrap(Array, out_col_data[], col_count)

    result = Dict{String, Vector}()
    for i in 1:col_count
        col_name = unsafe_string(name_ptrs[i])
        col_type = type_vals[i]

        if col_type == Cint(C.QUIVER_DATA_TYPE_INTEGER)
            int_ptr = reinterpret(Ptr{Int64}, data_ptrs[i])
            result[col_name] = copy(unsafe_wrap(Array, int_ptr, row_count))
        elseif col_type == Cint(C.QUIVER_DATA_TYPE_FLOAT)
            float_ptr = reinterpret(Ptr{Float64}, data_ptrs[i])
            result[col_name] = copy(unsafe_wrap(Array, float_ptr, row_count))
        elseif col_type == Cint(C.QUIVER_DATA_TYPE_STRING) || col_type == Cint(C.QUIVER_DATA_TYPE_DATE_TIME)
            str_ptr_ptr = reinterpret(Ptr{Ptr{Cchar}}, data_ptrs[i])
            str_ptrs = unsafe_wrap(Array, str_ptr_ptr, row_count)
            if col_name == dim_col
                result[col_name] = DateTime[string_to_date_time(unsafe_string(p)) for p in str_ptrs]
            else
                result[col_name] = String[unsafe_string(p) for p in str_ptrs]
            end
        end
    end

    # Free C-allocated memory
    C.quiver_database_free_time_series_data(out_col_names[], out_col_types[], out_col_data[], col_count, row_count)

    return result
end
```

### C API Signature Comparison (Old vs New)

**Old (current c_api.jl -- WILL BE REGENERATED):**
```julia
# Read: 7 args
function quiver_database_read_time_series_group(db, collection, group, id, out_date_times, out_values, out_row_count)
# Update: 7 args
function quiver_database_update_time_series_group(db, collection, group, id, date_times, values, row_count)
# Free: 3 args
function quiver_database_free_time_series_data(date_times, values, row_count)
```

**New (from C header -- what generator will produce):**
```julia
# Read: 9 args
function quiver_database_read_time_series_group(db, collection, group, id,
    out_column_names, out_column_types, out_column_data, out_column_count, out_row_count)
# Update: 9 args
function quiver_database_update_time_series_group(db, collection, group, id,
    column_names, column_types, column_data, column_count, row_count)
# Free: 5 args
function quiver_database_free_time_series_data(column_names, column_types,
    column_data, column_count, row_count)
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Single value column (date_time + value REAL) | N typed value columns (columnar arrays) | Phase 11 (2026-02-19) | C API signatures changed, Julia bindings now stale |
| `Vector{Dict{String, Any}}` row format (Julia) | `Dict{String, Vector}` columnar format (Julia) | Phase 12 (this phase) | Breaking change to Julia API surface |
| Hard-coded "date_time" + "value" columns | Dynamic column names from schema metadata | Phase 11 | Julia must fetch metadata for type coercion and DateTime detection |

**Deprecated/outdated:**
- Old `quiver_database_read_time_series_group` (3 out-params) -- replaced by 5 out-params
- Old `quiver_database_update_time_series_group` (date_times + values + row_count) -- replaced by columnar parallel arrays
- Old `quiver_database_free_time_series_data` (3 args) -- replaced by 5 args

## Open Questions

1. **Dict value type: `Vector` vs `Union{Vector{Int64}, Vector{Float64}, Vector{String}, Vector{DateTime}}`**
   - What we know: User decision says `Dict{String, Vector}` with runtime concrete types. Abstract `Vector` is simpler but less type-safe.
   - What's unclear: Whether `Dict{String, Vector}` (abstract) causes performance issues in Julia dispatch.
   - Recommendation: Use `Dict{String, Vector}` (abstract) for simplicity. Runtime types will be concrete. Julia's type inference handles this well for Dict values. The `Union` type would make the return type declaration more precise but adds complexity and isn't needed since callers access columns by name and know what types to expect.

2. **Metadata fetch overhead in update**
   - What we know: Auto-coercion requires knowing schema types, which requires a metadata call.
   - What's unclear: Whether this adds noticeable latency.
   - Recommendation: Fetch metadata once per update call. The C++ layer caches schema introspection in SQLite, so it's fast. The alternative (no metadata, pure Julia type mapping) would break the auto-coercion requirement.

3. **GC.@preserve scope for complex marshaling**
   - What we know: Multiple arrays (name strings, data arrays, type arrays) must be kept alive during the ccall.
   - What's unclear: Whether a single `refs` collector pattern is sufficient or if nested `GC.@preserve` is needed.
   - Recommendation: Single `refs::Vector{Any}` collector, single `GC.@preserve refs begin ... end` wrapping the ccall. This matches the pattern used in `marshal_params` for parameterized queries. All intermediate arrays must be pushed to `refs` before computing pointers.

## Sources

### Primary (HIGH confidence)
- **C API header:** `include/quiver/c/database.h` -- definitive function signatures for read/update/free
- **C API implementation:** `src/c/database_time_series.cpp` -- behavior details (column ordering, validation logic, clear semantics)
- **C API tests:** `tests/test_c_api_database_time_series.cpp` -- verified multi-column usage patterns
- **Existing Julia binding:** `bindings/julia/src/database_update.jl`, `database_read.jl` -- current implementation to replace
- **Julia FFI patterns:** `bindings/julia/src/database_query.jl` -- marshal_params pattern for typed parallel arrays
- **Schema:** `tests/schemas/valid/mixed_time_series.sql` -- multi-column test schema (INTEGER + REAL + TEXT)
- **DateTime utilities:** `bindings/julia/src/date_time.jl` -- existing `string_to_date_time`/`date_time_to_string`

### Secondary (MEDIUM confidence)
- Julia documentation on `GC.@preserve`, `unsafe_wrap`, `pointer()` -- standard behavior, verified by existing codebase patterns

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- All tools already in use in the project
- Architecture: HIGH -- C API signatures known, existing Julia FFI patterns well-established, code examples derived from real codebase patterns
- Pitfalls: HIGH -- Identified from existing codebase patterns and FFI expertise

**Research date:** 2026-02-19
**Valid until:** 2026-03-19 (stable -- no external dependencies changing)
