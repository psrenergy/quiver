function delete_element_by_id!(db::Database, collection::String, id::Int64)
    check(C.quiver_database_delete_element(db.ptr, collection, id))
    return nothing
end
