function export_to_csv(db::Database, table::String, path::String)
    err = C.quiver_database_export_to_csv(db.ptr, table, path)
    check_error(err, "Failed to export table '$table' to CSV file '$path'")
    return nothing
end

function import_csv(db::Database, table::String, path::String)
    err = C.quiver_database_import_csv(db.ptr, table, path)
    check_error(err, "Failed to import CSV file '$path' into table '$table'")
    return nothing
end
