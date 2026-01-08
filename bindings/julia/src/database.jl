struct Database
    ptr::Ptr{C.psr_database}
end

function create_empty_db_from_schema(db_path, schema_path; force::Bool = true)
    options = Ref(C.psr_database_options_t(0, C.PSR_LOG_DEBUG))
    return Database(C.psr_database_from_schema(db_path, schema_path, options))
end

function create_element!(db::Database, collection::String, e::Element)
    C.psr_database_create_element(db.ptr, collection, e.ptr)
    return nothing
end

function create_element!(db::Database, collection::String; kwargs...)
    @show kwargs
    e = Element()
    for (k, v) in kwargs
        e[String(k)] = v
    end
    create_element!(db, collection, e)
    return nothing
end

function close!(db::Database)
    C.psr_database_close(db.ptr)
    return nothing
end

function debug()
    db_path = raw"C:\Development\Database\database\bindings\julia\test\test_database.db"
    schema_path = raw"C:\Development\Database\database\tests\test_create\test_create_parameters.sql"
    db = create_empty_db_from_schema(db_path, schema_path)

    # db = from_schema(":memory:", schema_path; read_only=false, console_level=C.PSR_LOG_DEBUG)

    configuration = Element()
    configuration["id"] = 1
    configuration["label"] = "Test Config"
    configuration["value1"] = 42.0
    configuration["enum1"] = "B"
    create_element!(db, "Configuration", configuration)

    resource = Element()
    resource["id"] = 1
    resource["label"] = "Test Resource"
    resource["type"] = "D"
    create_element!(db, "Resource", resource)

    close!(db)

    return nothing
end