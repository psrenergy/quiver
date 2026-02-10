function export_csv(db::Database, table::String, path::String)
    check(C.quiver_database_export_csv(db.ptr, table, path))
    return nothing
end

function import_csv(db::Database, table::String, path::String)
    check(C.quiver_database_import_csv(db.ptr, table, path))
    return nothing
end
