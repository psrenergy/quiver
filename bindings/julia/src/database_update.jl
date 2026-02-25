function update_element!(db::Database, collection::String, id::Int64, e::Element)
    check(C.quiver_database_update_element(db.ptr, collection, id, e.ptr))
    return nothing
end

function update_element!(db::Database, collection::String, id::Int64; kwargs...)
    e = Element()
    for (k, v) in kwargs
        e[String(k)] = v
    end
    try
        update_element!(db, collection, id, e)
    finally
        destroy!(e)
    end
    return nothing
end

# Update vector attribute functions

function update_vector_integers!(
    db::Database,
    collection::String,
    attribute::String,
    id::Int64,
    values::Vector{<:Integer},
)
    integer_values = Int64[Int64(v) for v in values]
    check(
        C.quiver_database_update_vector_integers(
            db.ptr,
            collection,
            attribute,
            id,
            integer_values,
            Csize_t(length(integer_values)),
        ),
    )
    return nothing
end

function update_vector_floats!(db::Database, collection::String, attribute::String, id::Int64, values::Vector{<:Real})
    float_values = Float64[Float64(v) for v in values]
    check(
        C.quiver_database_update_vector_floats(
            db.ptr,
            collection,
            attribute,
            id,
            float_values,
            Csize_t(length(float_values)),
        ),
    )
    return nothing
end

function update_vector_strings!(
    db::Database,
    collection::String,
    attribute::String,
    id::Int64,
    values::Vector{<:AbstractString},
)
    cstrings = [Base.cconvert(Cstring, s) for s in values]
    ptrs = [Base.unsafe_convert(Cstring, cs) for cs in cstrings]
    GC.@preserve cstrings begin
        check(C.quiver_database_update_vector_strings(db.ptr, collection, attribute, id, ptrs, Csize_t(length(values))))
    end
    return nothing
end

# Update set attribute functions

function update_set_integers!(db::Database, collection::String, attribute::String, id::Int64, values::Vector{<:Integer})
    integer_values = Int64[Int64(v) for v in values]
    check(
        C.quiver_database_update_set_integers(
            db.ptr,
            collection,
            attribute,
            id,
            integer_values,
            Csize_t(length(integer_values)),
        ),
    )
    return nothing
end

function update_set_floats!(db::Database, collection::String, attribute::String, id::Int64, values::Vector{<:Real})
    float_values = Float64[Float64(v) for v in values]
    check(
        C.quiver_database_update_set_floats(
            db.ptr,
            collection,
            attribute,
            id,
            float_values,
            Csize_t(length(float_values)),
        ),
    )
    return nothing
end

function update_set_strings!(
    db::Database,
    collection::String,
    attribute::String,
    id::Int64,
    values::Vector{<:AbstractString},
)
    cstrings = [Base.cconvert(Cstring, s) for s in values]
    ptrs = [Base.unsafe_convert(Cstring, cs) for cs in cstrings]
    GC.@preserve cstrings begin
        check(C.quiver_database_update_set_strings(db.ptr, collection, attribute, id, ptrs, Csize_t(length(values))))
    end
    return nothing
end

# Update time series functions

function update_time_series_group!(db::Database, collection::String, group::String, id::Int64; kwargs...)
    # No kwargs = clear all rows for this element
    if isempty(kwargs)
        check(
            C.quiver_database_update_time_series_group(
                db.ptr, collection, group, id,
                C_NULL, C_NULL, C_NULL, Csize_t(0), Csize_t(0),
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

    # Fetch metadata for auto-coercion (Int -> Float when schema expects REAL)
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

        if eltype(v) <: DateTime
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
        check(
            C.quiver_database_update_time_series_group(
                db.ptr, collection, group, id,
                name_ptrs, col_types_arr, col_data_arr,
                Csize_t(column_count), Csize_t(row_count),
            ),
        )
    end
    return nothing
end

function update_time_series_files!(db::Database, collection::String, paths::Dict{String, Union{String, Nothing}})
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
