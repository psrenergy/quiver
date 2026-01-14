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

function read_scalar_ints(db::Database, collection::String, attribute::String)
    out_values = Ref{Ptr{Int64}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    err = C.psr_database_read_scalar_ints(db.ptr, collection, attribute, out_values, out_count)
    if err != C.PSR_OK
        throw(DatabaseException("Failed to read scalar ints from '$collection.$attribute'"))
    end

    count = out_count[]
    if count == 0 || out_values[] == C_NULL
        return Int64[]
    end

    result = unsafe_wrap(Array, out_values[], count) |> copy
    C.psr_free_int_array(out_values[])
    return result
end

function read_scalar_doubles(db::Database, collection::String, attribute::String)
    out_values = Ref{Ptr{Float64}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    err = C.psr_database_read_scalar_doubles(db.ptr, collection, attribute, out_values, out_count)
    if err != C.PSR_OK
        throw(DatabaseException("Failed to read scalar doubles from '$collection.$attribute'"))
    end

    count = out_count[]
    if count == 0 || out_values[] == C_NULL
        return Float64[]
    end

    result = unsafe_wrap(Array, out_values[], count) |> copy
    C.psr_free_double_array(out_values[])
    return result
end

function read_scalar_strings(db::Database, collection::String, attribute::String)
    out_values = Ref{Ptr{Ptr{Cchar}}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    err = C.psr_database_read_scalar_strings(db.ptr, collection, attribute, out_values, out_count)
    if err != C.PSR_OK
        throw(DatabaseException("Failed to read scalar strings from '$collection.$attribute'"))
    end

    count = out_count[]
    if count == 0 || out_values[] == C_NULL
        return String[]
    end

    ptrs = unsafe_wrap(Array, out_values[], count)
    result = [unsafe_string(ptr) for ptr in ptrs]
    C.psr_free_string_array(out_values[], count)
    return result
end
