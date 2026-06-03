function export_csv(
    db::Database,
    collection::String,
    group::String,
    path::String;
    date_time_format = nothing,
    enum_labels = nothing,
)
    opts_ref, temps = build_quiver_csv_options(; date_time_format, enum_labels)
    GC.@preserve temps begin
        check(C.quiver_database_export_csv(db.ptr, collection, group, path, opts_ref))
    end
    return nothing
end
