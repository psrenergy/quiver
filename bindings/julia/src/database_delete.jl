function delete_element_by_id!(db::Database, collection::String, id::Int64)
    err = C.margaux_delete_element_by_id(db.ptr, collection, id)
    if err != C.PSR_OK
        throw(DatabaseException("Failed to delete element $id from '$collection'"))
    end
    return nothing
end
