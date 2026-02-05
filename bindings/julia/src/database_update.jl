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

# Update scalar attribute functions

function update_scalar_integer!(db::Database, collection::String, attribute::String, id::Int64, value::Integer)
    check(C.quiver_database_update_scalar_integer(db.ptr, collection, attribute, id, Int64(value)))
    return nothing
end

function update_scalar_float!(db::Database, collection::String, attribute::String, id::Int64, value::Real)
    check(C.quiver_database_update_scalar_float(db.ptr, collection, attribute, id, Float64(value)))
    return nothing
end

function update_scalar_string!(db::Database, collection::String, attribute::String, id::Int64, value::String)
    check(C.quiver_database_update_scalar_string(db.ptr, collection, attribute, id, value))
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
    check(C.quiver_database_update_vector_integers(
        db.ptr,
        collection,
        attribute,
        id,
        integer_values,
        Csize_t(length(integer_values)),
    ))
    return nothing
end

function update_vector_floats!(db::Database, collection::String, attribute::String, id::Int64, values::Vector{<:Real})
    float_values = Float64[Float64(v) for v in values]
    check(C.quiver_database_update_vector_floats(
        db.ptr,
        collection,
        attribute,
        id,
        float_values,
        Csize_t(length(float_values)),
    ))
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
    check(C.quiver_database_update_set_integers(
        db.ptr,
        collection,
        attribute,
        id,
        integer_values,
        Csize_t(length(integer_values)),
    ))
    return nothing
end

function update_set_floats!(db::Database, collection::String, attribute::String, id::Int64, values::Vector{<:Real})
    float_values = Float64[Float64(v) for v in values]
    check(C.quiver_database_update_set_floats(
        db.ptr,
        collection,
        attribute,
        id,
        float_values,
        Csize_t(length(float_values)),
    ))
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

function update_time_series_group!(
    db::Database,
    collection::String,
    group::String,
    id::Int64,
    rows::Vector{Dict{String, Any}},
)
    if isempty(rows)
        check(C.quiver_database_update_time_series_group(
            db.ptr, collection, group, id,
            C_NULL, C_NULL, Csize_t(0),
        ))
        return nothing
    end

    row_count = length(rows)
    date_time_cstrings = [Base.cconvert(Cstring, String(row["date_time"])) for row in rows]
    date_time_ptrs = [Base.unsafe_convert(Cstring, cs) for cs in date_time_cstrings]
    values = Float64[Float64(row["value"]) for row in rows]

    GC.@preserve date_time_cstrings begin
        check(C.quiver_database_update_time_series_group(
            db.ptr, collection, group, id,
            date_time_ptrs, values, Csize_t(row_count),
        ))
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
