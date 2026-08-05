function delete_element!(db::Database, collection::String, id::Int64)
    check(C.quiver_database_delete_element(db.ptr, collection, id))
    return nothing
end

function delete_element!(db::Database, collection::String, label::String)
    check(C.quiver_database_delete_element_by_label(db.ptr, collection, label))
    return nothing
end
