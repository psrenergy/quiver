function update_element!(db::Database, collection::String, id::Int64, e::Element)
    err = C.quiver_database_update_element(db.ptr, collection, id, e.ptr)
    check_error(err, "Failed to update element $id in collection $collection")
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
    err = C.quiver_database_update_scalar_integer(db.ptr, collection, attribute, id, Int64(value))
    check_error(err, "Failed to update scalar integer '$collection.$attribute' for id $id")
    return nothing
end

function update_scalar_float!(db::Database, collection::String, attribute::String, id::Int64, value::Real)
    err = C.quiver_database_update_scalar_float(db.ptr, collection, attribute, id, Float64(value))
    check_error(err, "Failed to update scalar float '$collection.$attribute' for id $id")
    return nothing
end

function update_scalar_string!(db::Database, collection::String, attribute::String, id::Int64, value::String)
    err = C.quiver_database_update_scalar_string(db.ptr, collection, attribute, id, value)
    check_error(err, "Failed to update scalar string '$collection.$attribute' for id $id")
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
    err = C.quiver_database_update_vector_integers(
        db.ptr,
        collection,
        attribute,
        id,
        integer_values,
        Csize_t(length(integer_values)),
    )
    check_error(err, "Failed to update vector integers '$collection.$attribute' for id $id")
    return nothing
end

function update_vector_floats!(db::Database, collection::String, attribute::String, id::Int64, values::Vector{<:Real})
    float_values = Float64[Float64(v) for v in values]
    err = C.quiver_database_update_vector_floats(
        db.ptr,
        collection,
        attribute,
        id,
        float_values,
        Csize_t(length(float_values)),
    )
    check_error(err, "Failed to update vector floats '$collection.$attribute' for id $id")
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
        err = C.quiver_database_update_vector_strings(db.ptr, collection, attribute, id, ptrs, Csize_t(length(values)))
    end
    check_error(err, "Failed to update vector strings '$collection.$attribute' for id $id")
    return nothing
end

# Update set attribute functions

function update_set_integers!(db::Database, collection::String, attribute::String, id::Int64, values::Vector{<:Integer})
    integer_values = Int64[Int64(v) for v in values]
    err = C.quiver_database_update_set_integers(
        db.ptr,
        collection,
        attribute,
        id,
        integer_values,
        Csize_t(length(integer_values)),
    )
    check_error(err, "Failed to update set integers '$collection.$attribute' for id $id")
    return nothing
end

function update_set_floats!(db::Database, collection::String, attribute::String, id::Int64, values::Vector{<:Real})
    float_values = Float64[Float64(v) for v in values]
    err = C.quiver_database_update_set_floats(
        db.ptr,
        collection,
        attribute,
        id,
        float_values,
        Csize_t(length(float_values)),
    )
    check_error(err, "Failed to update set floats '$collection.$attribute' for id $id")
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
        err = C.quiver_database_update_set_strings(db.ptr, collection, attribute, id, ptrs, Csize_t(length(values)))
    end
    check_error(err, "Failed to update set strings '$collection.$attribute' for id $id")
    return nothing
end

# Update multiple vector/set attributes atomically

function update_element_vectors_sets!(db::Database, collection::String, id::Int64, e::Element)
    err = C.quiver_database_update_element_vectors_sets(db.ptr, collection, id, e.ptr)
    check_error(err, "Failed to update vectors/sets for element $id in collection $collection")
    return nothing
end

function update_element_vectors_sets!(db::Database, collection::String, id::Int64; kwargs...)
    e = Element()
    for (k, v) in kwargs
        e[String(k)] = v
    end
    try
        update_element_vectors_sets!(db, collection, id, e)
    finally
        destroy!(e)
    end
    return nothing
end
