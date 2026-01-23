function read_scalar_relation(db::Database, collection::String, attribute::String)
    out_values = Ref{Ptr{Ptr{Cchar}}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    err = C.quiver_database_read_scalar_relation(db.ptr, collection, attribute, out_values, out_count)
    if err != C.QUIVER_OK
        throw(DatabaseException("Failed to read scalar relation '$attribute' from '$collection'"))
    end

    count = out_count[]
    if count == 0 || out_values[] == C_NULL
        return Union{String, Nothing}[]
    end

    ptrs = unsafe_wrap(Array, out_values[], count)
    result = Union{String, Nothing}[]
    for ptr in ptrs
        if ptr == C_NULL
            push!(result, nothing)
        else
            s = unsafe_string(ptr)
            push!(result, isempty(s) ? nothing : s)
        end
    end
    C.quiver_free_string_array(out_values[], count)
    return result
end

function read_scalar_integers(db::Database, collection::String, attribute::String)
    out_values = Ref{Ptr{Int64}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    err = C.quiver_database_read_scalar_integers(db.ptr, collection, attribute, out_values, out_count)
    if err != C.QUIVER_OK
        throw(DatabaseException("Failed to read scalar integers from '$collection.$attribute'"))
    end

    count = out_count[]
    if count == 0 || out_values[] == C_NULL
        return Int64[]
    end

    result = unsafe_wrap(Array, out_values[], count) |> copy
    C.quiver_free_integer_array(out_values[])
    return result
end

function read_scalar_floats(db::Database, collection::String, attribute::String)
    out_values = Ref{Ptr{Float64}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    err = C.quiver_database_read_scalar_floats(db.ptr, collection, attribute, out_values, out_count)
    if err != C.QUIVER_OK
        throw(DatabaseException("Failed to read scalar floats from '$collection.$attribute'"))
    end

    count = out_count[]
    if count == 0 || out_values[] == C_NULL
        return Float64[]
    end

    result = unsafe_wrap(Array, out_values[], count) |> copy
    C.quiver_free_float_array(out_values[])
    return result
end

function read_scalar_strings(db::Database, collection::String, attribute::String)
    out_values = Ref{Ptr{Ptr{Cchar}}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    err = C.quiver_database_read_scalar_strings(db.ptr, collection, attribute, out_values, out_count)
    if err != C.QUIVER_OK
        throw(DatabaseException("Failed to read scalar strings from '$collection.$attribute'"))
    end

    count = out_count[]
    if count == 0 || out_values[] == C_NULL
        return String[]
    end

    ptrs = unsafe_wrap(Array, out_values[], count)
    result = [unsafe_string(ptr) for ptr in ptrs]
    C.quiver_free_string_array(out_values[], count)
    return result
end

function read_vector_integers(db::Database, collection::String, attribute::String)
    out_vectors = Ref{Ptr{Ptr{Int64}}}(C_NULL)
    out_sizes = Ref{Ptr{Csize_t}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    err = C.quiver_database_read_vector_integers(db.ptr, collection, attribute, out_vectors, out_sizes, out_count)
    if err != C.QUIVER_OK
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
    C.quiver_free_integer_vectors(out_vectors[], out_sizes[], count)
    return result
end

function read_vector_floats(db::Database, collection::String, attribute::String)
    out_vectors = Ref{Ptr{Ptr{Cdouble}}}(C_NULL)
    out_sizes = Ref{Ptr{Csize_t}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    err = C.quiver_database_read_vector_floats(db.ptr, collection, attribute, out_vectors, out_sizes, out_count)
    if err != C.QUIVER_OK
        throw(DatabaseException("Failed to read vector floats from '$collection.$attribute'"))
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
    C.quiver_free_float_vectors(out_vectors[], out_sizes[], count)
    return result
end

function read_vector_strings(db::Database, collection::String, attribute::String)
    out_vectors = Ref{Ptr{Ptr{Ptr{Cchar}}}}(C_NULL)
    out_sizes = Ref{Ptr{Csize_t}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    err = C.quiver_database_read_vector_strings(db.ptr, collection, attribute, out_vectors, out_sizes, out_count)
    if err != C.QUIVER_OK
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
    C.quiver_free_string_vectors(out_vectors[], out_sizes[], count)
    return result
end

function read_set_integers(db::Database, collection::String, attribute::String)
    out_sets = Ref{Ptr{Ptr{Int64}}}(C_NULL)
    out_sizes = Ref{Ptr{Csize_t}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    err = C.quiver_database_read_set_integers(db.ptr, collection, attribute, out_sets, out_sizes, out_count)
    if err != C.QUIVER_OK
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
    C.quiver_free_integer_vectors(out_sets[], out_sizes[], count)
    return result
end

function read_set_floats(db::Database, collection::String, attribute::String)
    out_sets = Ref{Ptr{Ptr{Cdouble}}}(C_NULL)
    out_sizes = Ref{Ptr{Csize_t}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    err = C.quiver_database_read_set_floats(db.ptr, collection, attribute, out_sets, out_sizes, out_count)
    if err != C.QUIVER_OK
        throw(DatabaseException("Failed to read set floats from '$collection.$attribute'"))
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
    C.quiver_free_float_vectors(out_sets[], out_sizes[], count)
    return result
end

function read_set_strings(db::Database, collection::String, attribute::String)
    out_sets = Ref{Ptr{Ptr{Ptr{Cchar}}}}(C_NULL)
    out_sizes = Ref{Ptr{Csize_t}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    err = C.quiver_database_read_set_strings(db.ptr, collection, attribute, out_sets, out_sizes, out_count)
    if err != C.QUIVER_OK
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
    C.quiver_free_string_vectors(out_sets[], out_sizes[], count)
    return result
end

function read_scalar_integers_by_id(db::Database, collection::String, attribute::String, id::Int64)
    out_value = Ref{Int64}(0)
    out_has_value = Ref{Cint}(0)

    err = C.quiver_database_read_scalar_integers_by_id(db.ptr, collection, attribute, id, out_value, out_has_value)
    if err != C.QUIVER_OK
        throw(DatabaseException("Failed to read scalar integer by id from '$collection.$attribute'"))
    end

    if out_has_value[] == 0
        return nothing
    end
    return out_value[]
end

function read_scalar_floats_by_id(db::Database, collection::String, attribute::String, id::Int64)
    out_value = Ref{Float64}(0.0)
    out_has_value = Ref{Cint}(0)

    err = C.quiver_database_read_scalar_floats_by_id(db.ptr, collection, attribute, id, out_value, out_has_value)
    if err != C.QUIVER_OK
        throw(DatabaseException("Failed to read scalar float by id from '$collection.$attribute'"))
    end

    if out_has_value[] == 0
        return nothing
    end
    return out_value[]
end

function read_scalar_strings_by_id(db::Database, collection::String, attribute::String, id::Int64)
    out_value = Ref{Ptr{Cchar}}(C_NULL)
    out_has_value = Ref{Cint}(0)

    err = C.quiver_database_read_scalar_strings_by_id(db.ptr, collection, attribute, id, out_value, out_has_value)
    if err != C.QUIVER_OK
        throw(DatabaseException("Failed to read scalar string by id from '$collection.$attribute'"))
    end

    if out_has_value[] == 0 || out_value[] == C_NULL
        return nothing
    end
    result = unsafe_string(out_value[])
    C.quiver_string_free(out_value[])
    return result
end

function read_vector_integers_by_id(db::Database, collection::String, attribute::String, id::Int64)
    out_values = Ref{Ptr{Int64}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    err = C.quiver_database_read_vector_integers_by_id(db.ptr, collection, attribute, id, out_values, out_count)
    if err != C.QUIVER_OK
        throw(DatabaseException("Failed to read vector integers by id from '$collection.$attribute'"))
    end

    count = out_count[]
    if count == 0 || out_values[] == C_NULL
        return Int64[]
    end

    result = unsafe_wrap(Array, out_values[], count) |> copy
    C.quiver_free_integer_array(out_values[])
    return result
end

function read_vector_floats_by_id(db::Database, collection::String, attribute::String, id::Int64)
    out_values = Ref{Ptr{Float64}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    err = C.quiver_database_read_vector_floats_by_id(db.ptr, collection, attribute, id, out_values, out_count)
    if err != C.QUIVER_OK
        throw(DatabaseException("Failed to read vector floats by id from '$collection.$attribute'"))
    end

    count = out_count[]
    if count == 0 || out_values[] == C_NULL
        return Float64[]
    end

    result = unsafe_wrap(Array, out_values[], count) |> copy
    C.quiver_free_float_array(out_values[])
    return result
end

function read_vector_strings_by_id(db::Database, collection::String, attribute::String, id::Int64)
    out_values = Ref{Ptr{Ptr{Cchar}}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    err = C.quiver_database_read_vector_strings_by_id(db.ptr, collection, attribute, id, out_values, out_count)
    if err != C.QUIVER_OK
        throw(DatabaseException("Failed to read vector strings by id from '$collection.$attribute'"))
    end

    count = out_count[]
    if count == 0 || out_values[] == C_NULL
        return String[]
    end

    ptrs = unsafe_wrap(Array, out_values[], count)
    result = [unsafe_string(ptr) for ptr in ptrs]
    C.quiver_free_string_array(out_values[], count)
    return result
end

function read_set_integers_by_id(db::Database, collection::String, attribute::String, id::Int64)
    out_values = Ref{Ptr{Int64}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    err = C.quiver_database_read_set_integers_by_id(db.ptr, collection, attribute, id, out_values, out_count)
    if err != C.QUIVER_OK
        throw(DatabaseException("Failed to read set integers by id from '$collection.$attribute'"))
    end

    count = out_count[]
    if count == 0 || out_values[] == C_NULL
        return Int64[]
    end

    result = unsafe_wrap(Array, out_values[], count) |> copy
    C.quiver_free_integer_array(out_values[])
    return result
end

function read_set_floats_by_id(db::Database, collection::String, attribute::String, id::Int64)
    out_values = Ref{Ptr{Float64}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    err = C.quiver_database_read_set_floats_by_id(db.ptr, collection, attribute, id, out_values, out_count)
    if err != C.QUIVER_OK
        throw(DatabaseException("Failed to read set floats by id from '$collection.$attribute'"))
    end

    count = out_count[]
    if count == 0 || out_values[] == C_NULL
        return Float64[]
    end

    result = unsafe_wrap(Array, out_values[], count) |> copy
    C.quiver_free_float_array(out_values[])
    return result
end

function read_set_strings_by_id(db::Database, collection::String, attribute::String, id::Int64)
    out_values = Ref{Ptr{Ptr{Cchar}}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    err = C.quiver_database_read_set_strings_by_id(db.ptr, collection, attribute, id, out_values, out_count)
    if err != C.QUIVER_OK
        throw(DatabaseException("Failed to read set strings by id from '$collection.$attribute'"))
    end

    count = out_count[]
    if count == 0 || out_values[] == C_NULL
        return String[]
    end

    ptrs = unsafe_wrap(Array, out_values[], count)
    result = [unsafe_string(ptr) for ptr in ptrs]
    C.quiver_free_string_array(out_values[], count)
    return result
end

function read_element_ids(db::Database, collection::String)
    out_ids = Ref{Ptr{Int64}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    err = C.quiver_database_read_element_ids(db.ptr, collection, out_ids, out_count)
    if err != C.QUIVER_OK
        throw(DatabaseException("Failed to read element ids from '$collection'"))
    end

    count = out_count[]
    if count == 0 || out_ids[] == C_NULL
        return Int64[]
    end

    result = unsafe_wrap(Array, out_ids[], count) |> copy
    C.quiver_free_integer_array(out_ids[])
    return result
end

function get_attribute_type(db::Database, collection::String, attribute::String)
    out_data_structure = Ref{C.quiver_data_structure_t}(C.QUIVER_DATA_STRUCTURE_SCALAR)
    out_data_type = Ref{C.quiver_data_type_t}(C.QUIVER_DATA_TYPE_INTEGER)

    err = C.quiver_database_get_attribute_type(db.ptr, collection, attribute, out_data_structure, out_data_type)
    if err != C.QUIVER_OK
        throw(DatabaseException("Failed to get attribute type for '$collection.$attribute'"))
    end

    return (data_structure = out_data_structure[], data_type = out_data_type[])
end

function read(db::Database, collection::String, attribute::String)
    attribute_type = get_attribute_type(db, collection, attribute)

    if attribute_type.data_structure == C.QUIVER_DATA_STRUCTURE_SCALAR
        if attribute_type.data_type == C.QUIVER_DATA_TYPE_INTEGER
            return read_scalar_integers(db, collection, attribute)
        elseif attribute_type.data_type == C.QUIVER_DATA_TYPE_FLOAT
            return read_scalar_floats(db, collection, attribute)
        elseif attribute_type.data_type == C.QUIVER_DATA_TYPE_STRING
            return read_scalar_strings(db, collection, attribute)
        else
            throw(DatabaseException("Unsupported data type for '$collection.$attribute'"))
        end
    elseif attribute_type.data_structure == C.QUIVER_DATA_STRUCTURE_VECTOR
        if attribute_type.data_type == C.QUIVER_DATA_TYPE_INTEGER
            return read_vector_integers(db, collection, attribute)
        elseif attribute_type.data_type == C.QUIVER_DATA_TYPE_FLOAT
            return read_vector_floats(db, collection, attribute)
        elseif attribute_type.data_type == C.QUIVER_DATA_TYPE_STRING
            return read_vector_strings(db, collection, attribute)
        else
            throw(DatabaseException("Unsupported data type for '$collection.$attribute'"))
        end
    elseif attribute_type.data_structure == C.QUIVER_DATA_STRUCTURE_SET
        if attribute_type.data_type == C.QUIVER_DATA_TYPE_INTEGER
            return read_set_integers(db, collection, attribute)
        elseif attribute_type.data_type == C.QUIVER_DATA_TYPE_FLOAT
            return read_set_floats(db, collection, attribute)
        elseif attribute_type.data_type == C.QUIVER_DATA_TYPE_STRING
            return read_set_strings(db, collection, attribute)
        else
            throw(DatabaseException("Unsupported data type for '$collection.$attribute'"))
        end
    else
        throw(DatabaseException("Unsupported data structure for '$collection.$attribute'"))
    end
end

function read_by_id(db::Database, collection::String, attribute::String, id::Int64)
    attribute_type = get_attribute_type(db, collection, attribute)

    if attribute_type.data_structure == C.QUIVER_DATA_STRUCTURE_SCALAR
        if attribute_type.data_type == C.QUIVER_DATA_TYPE_INTEGER
            return read_scalar_integers_by_id(db, collection, attribute, id)
        elseif attribute_type.data_type == C.QUIVER_DATA_TYPE_FLOAT
            return read_scalar_floats_by_id(db, collection, attribute, id)
        elseif attribute_type.data_type == C.QUIVER_DATA_TYPE_STRING
            return read_scalar_strings_by_id(db, collection, attribute, id)
        else
            throw(DatabaseException("Unsupported data type for '$collection.$attribute'"))
        end
    elseif attribute_type.data_structure == C.QUIVER_DATA_STRUCTURE_VECTOR
        if attribute_type.data_type == C.QUIVER_DATA_TYPE_INTEGER
            return read_vector_integers_by_id(db, collection, attribute, id)
        elseif attribute_type.data_type == C.QUIVER_DATA_TYPE_FLOAT
            return read_vector_floats_by_id(db, collection, attribute, id)
        elseif attribute_type.data_type == C.QUIVER_DATA_TYPE_STRING
            return read_vector_strings_by_id(db, collection, attribute, id)
        else
            throw(DatabaseException("Unsupported data type for '$collection.$attribute'"))
        end
    elseif attribute_type.data_structure == C.QUIVER_DATA_STRUCTURE_SET
        if attribute_type.data_type == C.QUIVER_DATA_TYPE_INTEGER
            return read_set_integers_by_id(db, collection, attribute, id)
        elseif attribute_type.data_type == C.QUIVER_DATA_TYPE_FLOAT
            return read_set_floats_by_id(db, collection, attribute, id)
        elseif attribute_type.data_type == C.QUIVER_DATA_TYPE_STRING
            return read_set_strings_by_id(db, collection, attribute, id)
        else
            throw(DatabaseException("Unsupported data type for '$collection.$attribute'"))
        end
    else
        throw(DatabaseException("Unsupported data structure for '$collection.$attribute'"))
    end
end
