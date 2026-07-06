function update_element!(db::Database, collection::String, id::Int64, e::Element)
    check(C.quiver_database_update_element(db.ptr, collection, id, e.ptr))
    return nothing
end

function update_element!(db::Database, collection::String, id::Int64; kwargs...)
    e = Element()
    for (k, v) in kwargs
        e[String(k)] = v
    end
    update_element!(db, collection, id, e)
    return nothing
end

# Update time series functions

function update_time_series_group!(db::Database, collection::String, group::String, id::Int64; kwargs...)
    # No kwargs = clear all rows for this element
    if isempty(kwargs)
        check(
            C.quiver_database_update_time_series_group(
                db.ptr, collection, group, id,
                C_NULL, C_NULL, C_NULL, C_NULL, Csize_t(0), Csize_t(0),
            ),
        )
        return nothing
    end

    # Validate all vectors have the same length
    row_count = length(first(values(kwargs)))
    for (k, v) in kwargs
        if length(v) != row_count
            throw(
                ArgumentError(
                    "All column vectors must have the same length, got $(length(v)) for '$(k)' but expected $row_count",
                ),
            )
        end
    end

    column_count = length(kwargs)
    col_names_strs = String[]
    col_types = Cint[]
    col_data_ptrs = Ptr{Cvoid}[]
    col_mask_ptrs = Ptr{UInt8}[]
    refs = Any[]  # Keep references alive for GC.@preserve

    for (k, v) in kwargs
        col_name = String(k)
        push!(col_names_strs, col_name)

        # Per-cell NULL mask: `nothing` entries become SQL NULL. Always passed
        # (dense columns get an all-ones mask).
        mask = UInt8[isnothing(x) ? 0x0 : 0x1 for x in v]
        push!(refs, mask)
        push!(col_mask_ptrs, pointer(mask))

        # Dispatch on the non-nothing element type. An all-nothing column has
        # eltype Nothing -> nonnothingtype Union{}, handled first; the value is a
        # zeroed FLOAT placeholder (the C API ignores type/data for masked cells).
        T = Base.nonnothingtype(eltype(v))
        if T === Union{}
            float_arr = zeros(Float64, row_count)
            push!(refs, float_arr)
            push!(col_types, Cint(C.QUIVER_DATA_TYPE_FLOAT))
            push!(col_data_ptrs, pointer(float_arr))
        elseif T <: DateTime
            cstrs =
                [isnothing(x) ? Base.cconvert(Cstring, "") : Base.cconvert(Cstring, date_time_to_string(x)) for x in v]
            ptrs = Ptr{Cchar}[
                isnothing(v[j]) ? Ptr{Cchar}(C_NULL) : Base.unsafe_convert(Cstring, cstrs[j]) for j in 1:row_count
            ]
            push!(refs, cstrs)
            push!(refs, ptrs)
            push!(col_types, Cint(C.QUIVER_DATA_TYPE_STRING))
            push!(col_data_ptrs, pointer(ptrs))
        elseif T <: AbstractString
            cstrs = [isnothing(x) ? Base.cconvert(Cstring, "") : Base.cconvert(Cstring, String(x)) for x in v]
            ptrs = Ptr{Cchar}[
                isnothing(v[j]) ? Ptr{Cchar}(C_NULL) : Base.unsafe_convert(Cstring, cstrs[j]) for j in 1:row_count
            ]
            push!(refs, cstrs)
            push!(refs, ptrs)
            push!(col_types, Cint(C.QUIVER_DATA_TYPE_STRING))
            push!(col_data_ptrs, pointer(ptrs))
        elseif T <: Integer
            # Integer columns are sent as-is; the C++ layer accepts integers
            # for REAL columns and SQLite converts them on insert.
            int_arr = Int64[isnothing(x) ? Int64(0) : Int64(x) for x in v]
            push!(refs, int_arr)
            push!(col_types, Cint(C.QUIVER_DATA_TYPE_INTEGER))
            push!(col_data_ptrs, pointer(int_arr))
        elseif T <: Real
            float_arr = Float64[isnothing(x) ? 0.0 : Float64(x) for x in v]
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

    # Build column_types, column_data, and column_has_value arrays
    col_types_arr = Cint[t for t in col_types]
    col_data_arr = Ptr{Cvoid}[p for p in col_data_ptrs]
    col_mask_arr = Ptr{UInt8}[p for p in col_mask_ptrs]
    push!(refs, col_types_arr)
    push!(refs, col_data_arr)
    push!(refs, col_mask_arr)

    GC.@preserve refs begin
        check(
            C.quiver_database_update_time_series_group(
                db.ptr, collection, group, id,
                name_ptrs, col_types_arr, col_data_arr, col_mask_arr,
                Csize_t(column_count), Csize_t(row_count),
            ),
        )
    end
    return nothing
end

function upsert_time_series_row!(db::Database, collection::String, group::String, id::Int64; kwargs...)
    column_count = length(kwargs)
    col_names_strs = String[]
    col_types = Cint[]
    col_data_ptrs = Ptr{Cvoid}[]
    refs = Any[]  # Keep references alive for GC.@preserve

    for (k, v) in kwargs
        col_name = String(k)
        push!(col_names_strs, col_name)

        if v isa DateTime
            # DateTime scalar -> format to string, build 1-element Cstring array
            str_val = date_time_to_string(v)
            cstr = Base.cconvert(Cstring, str_val)
            ptrs = Ptr{Cchar}[Base.unsafe_convert(Cstring, cstr)]
            push!(refs, cstr)
            push!(refs, ptrs)
            push!(col_types, Cint(C.QUIVER_DATA_TYPE_STRING))
            push!(col_data_ptrs, pointer(ptrs))
        elseif v isa AbstractString
            cstr = Base.cconvert(Cstring, v)
            ptrs = Ptr{Cchar}[Base.unsafe_convert(Cstring, cstr)]
            push!(refs, cstr)
            push!(refs, ptrs)
            push!(col_types, Cint(C.QUIVER_DATA_TYPE_STRING))
            push!(col_data_ptrs, pointer(ptrs))
        elseif v isa Integer
            int_arr = Int64[Int64(v)]
            push!(refs, int_arr)
            push!(col_types, Cint(C.QUIVER_DATA_TYPE_INTEGER))
            push!(col_data_ptrs, pointer(int_arr))
        elseif v isa Real
            float_arr = Float64[Float64(v)]
            push!(refs, float_arr)
            push!(col_types, Cint(C.QUIVER_DATA_TYPE_FLOAT))
            push!(col_data_ptrs, pointer(float_arr))
        else
            throw(ArgumentError("Unsupported column type: $(typeof(v)) for column '$col_name'"))
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
        check(
            C.quiver_database_upsert_time_series_row(
                db.ptr, collection, group, id,
                name_ptrs, col_types_arr, col_data_arr,
                Csize_t(column_count),
            ),
        )
    end
    return nothing
end

function update_time_series_files!(db::Database, collection::String, paths::AbstractDict{String, <:Optional{String}})
    if isempty(paths)
        return nothing
    end

    count = length(paths)
    columns = collect(keys(paths))
    path_values = [paths[col] for col in columns]

    column_cstrings = [Base.cconvert(Cstring, col) for col in columns]
    column_ptrs = [Base.unsafe_convert(Cstring, cs) for cs in column_cstrings]

    # Build path pointers - null for nothing values
    path_ptrs = Vector{Ptr{Cchar}}(undef, count)
    path_cstrings = []
    for i in 1:count
        if path_values[i] === nothing
            path_ptrs[i] = C_NULL
        else
            cs = Base.cconvert(Cstring, path_values[i])
            push!(path_cstrings, cs)
            path_ptrs[i] = Base.unsafe_convert(Cstring, cs)
        end
    end

    GC.@preserve column_cstrings path_cstrings begin
        check(C.quiver_database_update_time_series_files(
            db.ptr, collection,
            column_ptrs, path_ptrs, Csize_t(count),
        ))
    end
    return nothing
end
