function update_element!(db::Database, collection::String, id::Int64, e::Element)
    err = C.margaux_update_element(db.ptr, collection, id, e.ptr)
    if err != C.MARGAUX_OK
        throw(DatabaseException("Failed to update element $id in collection $collection"))
    end
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
    err = C.margaux_update_scalar_integer(db.ptr, collection, attribute, id, Int64(value))
    if err != C.MARGAUX_OK
        throw(DatabaseException("Failed to update scalar integer '$collection.$attribute' for id $id"))
    end
    return nothing
end

function update_scalar_float!(db::Database, collection::String, attribute::String, id::Int64, value::Real)
    err = C.margaux_update_scalar_float(db.ptr, collection, attribute, id, Float64(value))
    if err != C.MARGAUX_OK
        throw(DatabaseException("Failed to update scalar float '$collection.$attribute' for id $id"))
    end
    return nothing
end

function update_scalar_string!(db::Database, collection::String, attribute::String, id::Int64, value::String)
    err = C.margaux_update_scalar_string(db.ptr, collection, attribute, id, value)
    if err != C.MARGAUX_OK
        throw(DatabaseException("Failed to update scalar string '$collection.$attribute' for id $id"))
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
    err = C.margaux_update_vector_integers(
        db.ptr,
        collection,
        attribute,
        id,
        integer_values,
        Csize_t(length(integer_values)),
    )
    if err != C.MARGAUX_OK
        throw(DatabaseException("Failed to update vector integers '$collection.$attribute' for id $id"))
    end
    return nothing
end

function update_vector_floats!(db::Database, collection::String, attribute::String, id::Int64, values::Vector{<:Real})
    float_values = Float64[Float64(v) for v in values]
    err = C.margaux_update_vector_floats(
        db.ptr,
        collection,
        attribute,
        id,
        float_values,
        Csize_t(length(float_values)),
    )
    if err != C.MARGAUX_OK
        throw(DatabaseException("Failed to update vector floats '$collection.$attribute' for id $id"))
    end
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
        err = C.margaux_update_vector_strings(db.ptr, collection, attribute, id, ptrs, Csize_t(length(values)))
    end
    if err != C.MARGAUX_OK
        throw(DatabaseException("Failed to update vector strings '$collection.$attribute' for id $id"))
    end
    return nothing
end

# Update set attribute functions

function update_set_integers!(db::Database, collection::String, attribute::String, id::Int64, values::Vector{<:Integer})
    integer_values = Int64[Int64(v) for v in values]
    err = C.margaux_update_set_integers(
        db.ptr,
        collection,
        attribute,
        id,
        integer_values,
        Csize_t(length(integer_values)),
    )
    if err != C.MARGAUX_OK
        throw(DatabaseException("Failed to update set integers '$collection.$attribute' for id $id"))
    end
    return nothing
end

function update_set_floats!(db::Database, collection::String, attribute::String, id::Int64, values::Vector{<:Real})
    float_values = Float64[Float64(v) for v in values]
    err = C.margaux_update_set_floats(
        db.ptr,
        collection,
        attribute,
        id,
        float_values,
        Csize_t(length(float_values)),
    )
    if err != C.MARGAUX_OK
        throw(DatabaseException("Failed to update set floats '$collection.$attribute' for id $id"))
    end
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
        err = C.margaux_update_set_strings(db.ptr, collection, attribute, id, ptrs, Csize_t(length(values)))
    end
    if err != C.MARGAUX_OK
        throw(DatabaseException("Failed to update set strings '$collection.$attribute' for id $id"))
    end
    return nothing
end
