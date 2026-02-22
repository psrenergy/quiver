mutable struct Database
    ptr::Ptr{C.quiver_database}

    function Database(ptr::Ptr{C.quiver_database})
        db = new(ptr)
        finalizer(d -> d.ptr != C_NULL && C.quiver_database_close(d.ptr), db)
        return db
    end
end

function build_options(; kwargs...)
    options = Ref(C.quiver_database_options_default())
    if haskey(kwargs, :read_only)
        options[].read_only = kwargs[:read_only] ? 1 : 0
    end
    if haskey(kwargs, :console_level)
        options[].console_level = kwargs[:console_level]
    end
    return options
end

function from_schema(db_path::String, schema_path::String; kwargs...)
    options = build_options(; kwargs...)
    out_db = Ref{Ptr{C.quiver_database}}(C_NULL)
    check(C.quiver_database_from_schema(db_path, schema_path, options, out_db))
    return Database(out_db[])
end

function from_migrations(db_path::String, migrations_path::String; kwargs...)
    options = build_options(; kwargs...)
    out_db = Ref{Ptr{C.quiver_database}}(C_NULL)
    check(C.quiver_database_from_migrations(db_path, migrations_path, options, out_db))
    return Database(out_db[])
end

function close!(db::Database)
    if db.ptr != C_NULL
        C.quiver_database_close(db.ptr)
        db.ptr = C_NULL
    end
    return nothing
end

function current_version(db::Database)
    out_version = Ref{Int64}(0)
    check(C.quiver_database_current_version(db.ptr, out_version))
    return out_version[]
end

function describe(db::Database)
    check(C.quiver_database_describe(db.ptr))
    return nothing
end
