function export_to_csv(db::Database, table::String, path::String)
    err = C.quiver_database_export_to_csv(db.ptr, table, path)
    check_error(err)
    return nothing
end

function import_from_csv(db::Database, table::String, path::String)
    err = C.quiver_database_import_from_csv(db.ptr, table, path)
    check_error(err)
    return nothing
end
