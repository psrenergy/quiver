function export_csv(db::Database, collection::String, group::String, path::String; kwargs...)
    opts_ref, temps = build_quiver_csv_options(; kwargs...)
    GC.@preserve temps begin
        check(C.quiver_database_export_csv(db.ptr, collection, group, path, opts_ref))
    end
    return nothing
end
