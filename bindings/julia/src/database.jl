struct Database
    ptr::Ptr{C.psr_database}
end

function from_schema(db_path, schema_path; force::Bool = true)
    options = Ref(C.psr_database_options_t(0, C.PSR_LOG_DEBUG))
    ptr = C.psr_database_from_schema(db_path, schema_path, options)
    if ptr == C_NULL
        throw(DatabaseException("Failed to create database from schema '$schema_path'"))
    end
    return Database(ptr)
end

function from_migrations(db_path, migrations_path; force::Bool = true)
    options = Ref(C.psr_database_options_t(0, C.PSR_LOG_DEBUG))
    ptr = C.psr_database_from_migrations(db_path, migrations_path, options)
    if ptr == C_NULL
        throw(DatabaseException("Failed to create database from migrations '$migrations_path'"))
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

function set_scalar_relation!(db::Database, collection::String, attribute::String,
                              from_label::String, to_label::String)
    err = C.psr_database_set_scalar_relation(db.ptr, collection, attribute, from_label, to_label)
    if err != C.PSR_OK
        throw(DatabaseException("Failed to set scalar relation '$attribute' in '$collection'"))
    end
    return nothing
end

function read_scalar_integers(db::Database, collection::String, attribute::String)
    out_values = Ref{Ptr{Int64}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    err = C.psr_database_read_scalar_integers(db.ptr, collection, attribute, out_values, out_count)
    if err != C.PSR_OK
        throw(DatabaseException("Failed to read scalar integers from '$collection.$attribute'"))
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

function read_vector_integers(db::Database, collection::String, attribute::String)
    out_vectors = Ref{Ptr{Ptr{Int64}}}(C_NULL)
    out_sizes = Ref{Ptr{Csize_t}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    err = C.psr_database_read_vector_integers(db.ptr, collection, attribute, out_vectors, out_sizes, out_count)
    if err != C.PSR_OK
        throw(DatabaseException("Failed to read vector integers from '$collection.$attribute'"))
    end

    count = out_count[]
    if count == 0 || out_vectors[] == C_NULL
        return Vector{Int64}[]
    end

    vectors_ptr = unsafe_wrap(Array, out_vectors[], count)
    sizes_ptr = unsafe_wrap(Array, out_sizes[], count)
    result = Vector{Int64}[]
    for i in 1:count
        if vectors_ptr[i] == C_NULL || sizes_ptr[i] == 0
            push!(result, Int64[])
        else
            push!(result, copy(unsafe_wrap(Array, vectors_ptr[i], sizes_ptr[i])))
        end
    end
    C.psr_free_int_vectors(out_vectors[], out_sizes[], count)
    return result
end

function read_vector_doubles(db::Database, collection::String, attribute::String)
    out_vectors = Ref{Ptr{Ptr{Cdouble}}}(C_NULL)
    out_sizes = Ref{Ptr{Csize_t}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    err = C.psr_database_read_vector_doubles(db.ptr, collection, attribute, out_vectors, out_sizes, out_count)
    if err != C.PSR_OK
        throw(DatabaseException("Failed to read vector doubles from '$collection.$attribute'"))
    end

    count = out_count[]
    if count == 0 || out_vectors[] == C_NULL
        return Vector{Float64}[]
    end

    vectors_ptr = unsafe_wrap(Array, out_vectors[], count)
    sizes_ptr = unsafe_wrap(Array, out_sizes[], count)
    result = Vector{Float64}[]
    for i in 1:count
        if vectors_ptr[i] == C_NULL || sizes_ptr[i] == 0
            push!(result, Float64[])
        else
            push!(result, copy(unsafe_wrap(Array, vectors_ptr[i], sizes_ptr[i])))
        end
    end
    C.psr_free_double_vectors(out_vectors[], out_sizes[], count)
    return result
end

function read_vector_strings(db::Database, collection::String, attribute::String)
    out_vectors = Ref{Ptr{Ptr{Ptr{Cchar}}}}(C_NULL)
    out_sizes = Ref{Ptr{Csize_t}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    err = C.psr_database_read_vector_strings(db.ptr, collection, attribute, out_vectors, out_sizes, out_count)
    if err != C.PSR_OK
        throw(DatabaseException("Failed to read vector strings from '$collection.$attribute'"))
    end

    count = out_count[]
    if count == 0 || out_vectors[] == C_NULL
        return Vector{String}[]
    end

    vectors_ptr = unsafe_wrap(Array, out_vectors[], count)
    sizes_ptr = unsafe_wrap(Array, out_sizes[], count)
    result = Vector{String}[]
    for i in 1:count
        if vectors_ptr[i] == C_NULL || sizes_ptr[i] == 0
            push!(result, String[])
        else
            str_ptrs = unsafe_wrap(Array, vectors_ptr[i], sizes_ptr[i])
            push!(result, [unsafe_string(ptr) for ptr in str_ptrs])
        end
    end
    C.psr_free_string_vectors(out_vectors[], out_sizes[], count)
    return result
end

function read_set_integers(db::Database, collection::String, attribute::String)
    out_sets = Ref{Ptr{Ptr{Int64}}}(C_NULL)
    out_sizes = Ref{Ptr{Csize_t}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    err = C.psr_database_read_set_integers(db.ptr, collection, attribute, out_sets, out_sizes, out_count)
    if err != C.PSR_OK
        throw(DatabaseException("Failed to read set integers from '$collection.$attribute'"))
    end

    count = out_count[]
    if count == 0 || out_sets[] == C_NULL
        return Vector{Int64}[]
    end

    sets_ptr = unsafe_wrap(Array, out_sets[], count)
    sizes_ptr = unsafe_wrap(Array, out_sizes[], count)
    result = Vector{Int64}[]
    for i in 1:count
        if sets_ptr[i] == C_NULL || sizes_ptr[i] == 0
            push!(result, Int64[])
        else
            push!(result, copy(unsafe_wrap(Array, sets_ptr[i], sizes_ptr[i])))
        end
    end
    C.psr_free_int_vectors(out_sets[], out_sizes[], count)
    return result
end

function read_set_doubles(db::Database, collection::String, attribute::String)
    out_sets = Ref{Ptr{Ptr{Cdouble}}}(C_NULL)
    out_sizes = Ref{Ptr{Csize_t}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    err = C.psr_database_read_set_doubles(db.ptr, collection, attribute, out_sets, out_sizes, out_count)
    if err != C.PSR_OK
        throw(DatabaseException("Failed to read set doubles from '$collection.$attribute'"))
    end

    count = out_count[]
    if count == 0 || out_sets[] == C_NULL
        return Vector{Float64}[]
    end

    sets_ptr = unsafe_wrap(Array, out_sets[], count)
    sizes_ptr = unsafe_wrap(Array, out_sizes[], count)
    result = Vector{Float64}[]
    for i in 1:count
        if sets_ptr[i] == C_NULL || sizes_ptr[i] == 0
            push!(result, Float64[])
        else
            push!(result, copy(unsafe_wrap(Array, sets_ptr[i], sizes_ptr[i])))
        end
    end
    C.psr_free_double_vectors(out_sets[], out_sizes[], count)
    return result
end

function read_set_strings(db::Database, collection::String, attribute::String)
    out_sets = Ref{Ptr{Ptr{Ptr{Cchar}}}}(C_NULL)
    out_sizes = Ref{Ptr{Csize_t}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    err = C.psr_database_read_set_strings(db.ptr, collection, attribute, out_sets, out_sizes, out_count)
    if err != C.PSR_OK
        throw(DatabaseException("Failed to read set strings from '$collection.$attribute'"))
    end

    count = out_count[]
    if count == 0 || out_sets[] == C_NULL
        return Vector{String}[]
    end

    sets_ptr = unsafe_wrap(Array, out_sets[], count)
    sizes_ptr = unsafe_wrap(Array, out_sizes[], count)
    result = Vector{String}[]
    for i in 1:count
        if sets_ptr[i] == C_NULL || sizes_ptr[i] == 0
            push!(result, String[])
        else
            str_ptrs = unsafe_wrap(Array, sets_ptr[i], sizes_ptr[i])
            push!(result, [unsafe_string(ptr) for ptr in str_ptrs])
        end
    end
    C.psr_free_string_vectors(out_sets[], out_sizes[], count)
    return result
end
