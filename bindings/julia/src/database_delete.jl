function delete_element_by_id!(db::Database, collection::String, id::Int64)
    err = C.quiver_database_delete_element_by_id(db.ptr, collection, id)
    check_error(err, "Failed to delete element $id from '$collection'")
    return nothing
end
