mutable struct Database
    ptr::Ptr{C.quiver_database}

    function Database(ptr::Ptr{C.quiver_database})
        db = new(ptr)
        finalizer(d -> d.ptr != C_NULL && C.quiver_database_close(d.ptr), db)
        return db
    end
end

function from_schema(db_path, schema_path)
    options = Ref(C.quiver_database_options_t(0, C.QUIVER_LOG_DEBUG))
    ptr = C.quiver_database_from_schema(db_path, schema_path, options)
    if ptr == C_NULL
        detail = unsafe_string(C.quiver_get_last_error())
        context = "Failed to create database from schema '$schema_path'"
        throw(DatabaseException(isempty(detail) ? context : "$context: $detail"))
    end
    return Database(ptr)
end

function from_migrations(db_path, migrations_path)
    options = Ref(C.quiver_database_options_t(0, C.QUIVER_LOG_DEBUG))
    ptr = C.quiver_database_from_migrations(db_path, migrations_path, options)
    if ptr == C_NULL
        detail = unsafe_string(C.quiver_get_last_error())
        context = "Failed to create database from migrations '$migrations_path'"
        throw(DatabaseException(isempty(detail) ? context : "$context: $detail"))
    end
    return Database(ptr)
end

function close!(db::Database)
    if db.ptr != C_NULL
        C.quiver_database_close(db.ptr)
        db.ptr = C_NULL
    end
    return nothing
end

function current_version(db::Database)
    version = C.quiver_database_current_version(db.ptr)
    if version < 0
        detail = unsafe_string(C.quiver_get_last_error())
        context = "Failed to get current version"
        throw(DatabaseException(isempty(detail) ? context : "$context: $detail"))
    end
    return version
end

function describe(db::Database)
    result = C.quiver_database_describe(db.ptr)
    check_error(result, "Failed to describe database")
    return nothing
end
