function create_element!(db::Database, collection::String, e::Element)
    result = C.quiver_database_create_element(db.ptr, collection, e.ptr)
    if result == -1
        detail = unsafe_string(C.quiver_get_last_error())
        if isempty(detail)
            throw(DatabaseException("Failed to create element in collection $collection"))
        else
            throw(DatabaseException("Failed to create element in collection $collection: $detail"))
        end
    end
    return nothing
end

function create_element!(db::Database, collection::String; kwargs...)
    e = Element()
    for (k, v) in kwargs
        e[String(k)] = v
    end
    try
        create_element!(db, collection, e)
    finally
        destroy!(e)
    end
    return nothing
end

function set_scalar_relation!(
    db::Database,
    collection::String,
    attribute::String,
    from_label::String,
    to_label::String,
)
    err = C.quiver_database_set_scalar_relation(db.ptr, collection, attribute, from_label, to_label)
    check_error(err, "Failed to set scalar relation '$attribute' in '$collection'")
    return nothing
end
