function create_element!(db::Database, collection::String, e::Element)
    if C.psr_database_create_element(db.ptr, collection, e.ptr) == -1
        throw(DatabaseException("Failed to create element in collection $collection"))
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
    err = C.psr_database_set_scalar_relation(db.ptr, collection, attribute, from_label, to_label)
    if err != C.PSR_OK
        throw(DatabaseException("Failed to set scalar relation '$attribute' in '$collection'"))
    end
    return nothing
end