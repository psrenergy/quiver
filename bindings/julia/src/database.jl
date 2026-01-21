mutable struct Database
    ptr::Ptr{C.margaux}

    function Database(ptr::Ptr{C.margaux})
        db = new(ptr)
        finalizer(d -> d.ptr != C_NULL && C.margaux_close(d.ptr), db)
        return db
    end
end

function from_schema(db_path, schema_path)
    options = Ref(C.margaux_options_t(0, C.PSR_LOG_DEBUG))
    ptr = C.margaux_from_schema(db_path, schema_path, options)
    if ptr == C_NULL
        throw(DatabaseException("Failed to create database from schema '$schema_path'"))
    end
    return Database(ptr)
end

function from_migrations(db_path, migrations_path)
    options = Ref(C.margaux_options_t(0, C.PSR_LOG_DEBUG))
    ptr = C.margaux_from_migrations(db_path, migrations_path, options)
    if ptr == C_NULL
        throw(DatabaseException("Failed to create database from migrations '$migrations_path'"))
    end
    return Database(ptr)
end

function close!(db::Database)
    if db.ptr != C_NULL
        C.margaux_close(db.ptr)
        db.ptr = C_NULL
    end
    return nothing
end
