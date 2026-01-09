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

function read_scalar_parameters(db::Database, collection::String, attribute::String; default=nothing)
    # Try double first (most common for numeric data)
    out_values = Ref{Ptr{Cdouble}}(C_NULL)
    count = C.psr_database_read_scalar_parameters_double(db.ptr, collection, attribute, out_values)

    if count >= 0
        try
            result = Vector{Float64}(undef, count)
            for i in 1:count
                val = unsafe_load(out_values[], i)
                if isnan(val) && default !== nothing
                    result[i] = Float64(default)
                else
                    result[i] = val
                end
            end
            return result
        finally
            C.psr_double_array_free(out_values[])
        end
    end

    # Try string
    out_strings = Ref{Ptr{Ptr{Cchar}}}(C_NULL)
    count = C.psr_database_read_scalar_parameters_string(db.ptr, collection, attribute, out_strings)

    if count >= 0
        try
            result = Vector{String}(undef, count)
            for i in 1:count
                str_ptr = unsafe_load(out_strings[], i)
                result[i] = unsafe_string(str_ptr)
            end
            return result
        finally
            C.psr_string_array_free(out_strings[], count)
        end
    end

    # Try int
    out_ints = Ref{Ptr{Int64}}(C_NULL)
    count = C.psr_database_read_scalar_parameters_int(db.ptr, collection, attribute, out_ints)

    if count >= 0
        try
            result = Vector{Int64}(undef, count)
            for i in 1:count
                result[i] = unsafe_load(out_ints[], i)
            end
            return result
        finally
            C.psr_int_array_free(out_ints[])
        end
    end

    throw(DatabaseException("Failed to read scalar parameters '$attribute' from collection '$collection'"))
end

function read_scalar_parameter(db::Database, collection::String, attribute::String, label::String)
    # Try double first
    out_value = Ref{Cdouble}(0.0)
    is_null = Ref{Cint}(0)
    err = C.psr_database_read_scalar_parameter_double(db.ptr, collection, attribute, label, out_value, is_null)

    if err == C.PSR_OK
        return is_null[] != 0 ? NaN : out_value[]
    elseif err == C.PSR_ERROR_NOT_FOUND
        throw(DatabaseException("Element with label '$label' not found in collection '$collection'"))
    end

    # Try string
    out_str = Ref{Ptr{Cchar}}(C_NULL)
    err = C.psr_database_read_scalar_parameter_string(db.ptr, collection, attribute, label, out_str)

    if err == C.PSR_OK
        try
            return unsafe_string(out_str[])
        finally
            C.psr_string_free(out_str[])
        end
    elseif err == C.PSR_ERROR_NOT_FOUND
        throw(DatabaseException("Element with label '$label' not found in collection '$collection'"))
    end

    # Try int
    out_int = Ref{Int64}(0)
    is_null = Ref{Cint}(0)
    err = C.psr_database_read_scalar_parameter_int(db.ptr, collection, attribute, label, out_int, is_null)

    if err == C.PSR_OK
        return is_null[] != 0 ? 0 : out_int[]
    elseif err == C.PSR_ERROR_NOT_FOUND
        throw(DatabaseException("Element with label '$label' not found in collection '$collection'"))
    end

    throw(DatabaseException("Failed to read scalar parameter '$attribute' for '$label' from collection '$collection'"))
end
