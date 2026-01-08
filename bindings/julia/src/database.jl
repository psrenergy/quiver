struct Database
    ptr::Ptr{C.psr_database}
end

# function from_schema(path::String, schema_path::String; read_only::Bool, console_level::C.psr_log_level_t)
#     options = Ref(C.psr_database_options_t(Int32(read_only), C.psr_log_level_t(console_level)))
#     ptr = C.psr_database_from_schema(Base.cconvert(Cstring, path), Base.cconvert(Cstring, schema_path), options)
#     if ptr == C_NULL
#         error("Failed to create database from schema")
#     end
#     return Database(ptr)
# end

function create_element!(db::Database, collection::String, e::Element)
    C.psr_database_create_element(db.ptr, collection, e.ptr)
    return nothing
end

function close!(db::Database)
    C.psr_database_close(db.ptr)
    return nothing
end

function debug()
    schema_path = raw"C:\Development\Database\database\tests\test_create\test_create_parameters.sql"
    options = Ref(C.psr_database_options_t(0, C.PSR_LOG_DEBUG))
    db = Database(C.psr_database_from_schema(":memory:", schema_path, options))

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