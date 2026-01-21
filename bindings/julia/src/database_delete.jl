function delete_element_by_id!(db::Database, collection::String, id::Int64)
    err = C.database_delete_element_by_id(db.ptr, collection, id)
    if err != C.DECK_DATABASE_OK
        throw(DatabaseException("Failed to delete element $id from '$collection'"))
    end
    return nothing
end
