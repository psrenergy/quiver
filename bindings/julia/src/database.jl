struct Database
    ptr::Ptr{C.psr_database}
end

function create_empty_db_from_schema(db_path, schema_path; force::Bool = true)
    options = Ref(C.psr_database_options_t(0, C.PSR_LOG_DEBUG))
    ptr = C.psr_database_from_schema(db_path, schema_path, options)
    if ptr == C_NULL
        throw(DatabaseException("Failed to create database from schema '$schema_path'"))
    end
    return Database(ptr)
end

function create_element!(db::Database, collection::String, e::Element)
    if C.psr_database_create_element(db.ptr, collection, e.ptr) == -1
        throw(DatabaseException("Failed to create element in collection $collection"))
    end
    return nothing
end

function create_element!(db::Database, collection::String; kwargs...)
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

function extract_value_from_ptr(val_ptr::Ptr{C.psr_value_t}; default=nothing)
    # Read type field directly (at offset 0)
    type_ptr = Ptr{C.psr_value_type_t}(UInt(val_ptr))
    type_val = unsafe_load(type_ptr)

    if type_val == C.PSR_VALUE_NULL
        return default
    elseif type_val == C.PSR_VALUE_INT64
        int_ptr = Ptr{Int64}(UInt(val_ptr) + 8)
        return unsafe_load(int_ptr)
    elseif type_val == C.PSR_VALUE_DOUBLE
        double_ptr = Ptr{Cdouble}(UInt(val_ptr) + 8)
        result = unsafe_load(double_ptr)
        return isnan(result) && default !== nothing ? default : result
    elseif type_val == C.PSR_VALUE_STRING
        string_ptr_ptr = Ptr{Ptr{Cchar}}(UInt(val_ptr) + 8)
        return unsafe_string(unsafe_load(string_ptr_ptr))
    elseif type_val == C.PSR_VALUE_ARRAY
        elements_ptr_ptr = Ptr{Ptr{C.psr_value_t}}(UInt(val_ptr) + 8)
        count_ptr = Ptr{Csize_t}(UInt(val_ptr) + 16)
        elements_ptr = unsafe_load(elements_ptr_ptr)
        count = unsafe_load(count_ptr)
        return [extract_value_at_index(elements_ptr, i; default) for i in 1:count]
    end
    return default
end

function extract_value_at_index(values_ptr::Ptr{C.psr_value_t}, index::Integer; default=nothing)
    val_ptr = Ptr{C.psr_value_t}(UInt(values_ptr) + (index - 1) * 24)
    return extract_value_from_ptr(val_ptr; default)
end

function read_scalar_parameters(db::Database, collection::String, attribute::String; default=nothing)
    result = C.psr_database_read_scalar_parameters(db.ptr, collection, attribute)

    if result.error == C.PSR_ERROR_NOT_FOUND
        throw(DatabaseException("Attribute '$attribute' not found in collection '$collection'"))
    elseif result.error != C.PSR_OK
        throw(DatabaseException("Failed to read scalar parameters '$attribute' from collection '$collection'"))
    end

    try
        return [extract_value_at_index(result.values, i; default) for i in 1:result.count]
    finally
        C.psr_read_result_free(Ref(result))
    end
end

function read_scalar_parameter(db::Database, collection::String, attribute::String, label::String; default=nothing)
    result = C.psr_database_read_scalar_parameter(db.ptr, collection, attribute, label)

    if result.error == C.PSR_ERROR_NOT_FOUND
        throw(DatabaseException("Element with label '$label' not found in collection '$collection'"))
    elseif result.error != C.PSR_OK
        throw(DatabaseException("Failed to read scalar parameter '$attribute' for '$label' from collection '$collection'"))
    end

    try
        return extract_value_from_ptr(result.values; default)
    finally
        C.psr_read_result_free(Ref(result))
    end
end

function read_vector_parameters(db::Database, collection::String, attribute::String; default=nothing)
    result = C.psr_database_read_vector_parameters(db.ptr, collection, attribute)

    if result.error == C.PSR_ERROR_NOT_FOUND
        throw(DatabaseException("Attribute '$attribute' not found in collection '$collection'"))
    elseif result.error != C.PSR_OK
        throw(DatabaseException("Failed to read vector parameters '$attribute' from collection '$collection'"))
    end

    try
        return [extract_value_at_index(result.values, i; default) for i in 1:result.count]
    finally
        C.psr_read_result_free(Ref(result))
    end
end

function read_vector_parameter(db::Database, collection::String, attribute::String, label::String; default=nothing)
    result = C.psr_database_read_vector_parameter(db.ptr, collection, attribute, label)

    if result.error == C.PSR_ERROR_NOT_FOUND
        throw(DatabaseException("Element with label '$label' not found in collection '$collection'"))
    elseif result.error != C.PSR_OK
        throw(DatabaseException("Failed to read vector parameter '$attribute' for '$label' from collection '$collection'"))
    end

    try
        return [extract_value_at_index(result.values, i; default) for i in 1:result.count]
    finally
        C.psr_read_result_free(Ref(result))
    end
end

function read_set_parameters(db::Database, collection::String, attribute::String; default=nothing)
    result = C.psr_database_read_set_parameters(db.ptr, collection, attribute)

    if result.error == C.PSR_ERROR_NOT_FOUND
        throw(DatabaseException("Attribute '$attribute' not found in collection '$collection'"))
    elseif result.error != C.PSR_OK
        throw(DatabaseException("Failed to read set parameters '$attribute' from collection '$collection'"))
    end

    try
        return [extract_value_at_index(result.values, i; default) for i in 1:result.count]
    finally
        C.psr_read_result_free(Ref(result))
    end
end

function read_set_parameter(db::Database, collection::String, attribute::String, label::String; default=nothing)
    result = C.psr_database_read_set_parameter(db.ptr, collection, attribute, label)

    if result.error == C.PSR_ERROR_NOT_FOUND
        throw(DatabaseException("Element with label '$label' not found in collection '$collection'"))
    elseif result.error != C.PSR_OK
        throw(DatabaseException("Failed to read set parameter '$attribute' for '$label' from collection '$collection'"))
    end

    try
        return [extract_value_at_index(result.values, i; default) for i in 1:result.count]
    finally
        C.psr_read_result_free(Ref(result))
    end
end
