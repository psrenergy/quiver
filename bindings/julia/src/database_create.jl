function create_element!(db::Database, collection::String, e::Element)
    out_id = Ref{Int64}(0)
    check(C.quiver_database_create_element(db.ptr, collection, e.ptr, out_id))
    return out_id[]
end

function create_element!(db::Database, collection::String; kwargs...)
    e = Element()
    for (k, v) in kwargs
        e[String(k)] = v
    end
    try
        return create_element!(db, collection, e)
    finally
        destroy!(e)
    end
end
