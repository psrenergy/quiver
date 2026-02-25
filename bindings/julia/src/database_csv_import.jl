function import_csv(db::Database, collection::String, group::String, path::String; kwargs...)
    options_ref, temps = build_quiver_csv_options(; kwargs...)
    GC.@preserve temps begin
        check(C.quiver_database_import_csv(db.ptr, collection, group, path, options_ref))
    end
    return nothing
end
