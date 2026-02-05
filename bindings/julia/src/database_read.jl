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

function read_scalar_integer_by_id(db::Database, collection::String, attribute::String, id::Int64)
    out_value = Ref{Int64}(0)
    out_has_value = Ref{Cint}(0)

    err = C.quiver_database_read_scalar_integer_by_id(db.ptr, collection, attribute, id, out_value, out_has_value)
    if err != C.QUIVER_OK
        throw(DatabaseException("Failed to read scalar integer by id from '$collection.$attribute'"))
    end

    if out_has_value[] == 0
        return nothing
    end
    return out_value[]
end

function read_scalar_float_by_id(db::Database, collection::String, attribute::String, id::Int64)
    out_value = Ref{Float64}(0.0)
    out_has_value = Ref{Cint}(0)

    err = C.quiver_database_read_scalar_float_by_id(db.ptr, collection, attribute, id, out_value, out_has_value)
    if err != C.QUIVER_OK
        throw(DatabaseException("Failed to read scalar float by id from '$collection.$attribute'"))
    end

    if out_has_value[] == 0
        return nothing
    end
    return out_value[]
end

function read_scalar_string_by_id(db::Database, collection::String, attribute::String, id::Int64)
    out_value = Ref{Ptr{Cchar}}(C_NULL)
    out_has_value = Ref{Cint}(0)

    err = C.quiver_database_read_scalar_string_by_id(db.ptr, collection, attribute, id, out_value, out_has_value)
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

function read_scalar_date_time_by_id(db::Database, collection::String, attribute::String, id::Int64)
    return string_to_date_time(read_scalar_string_by_id(db, collection, attribute, id))
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

function read_vector_date_time_by_id(db::Database, collection::String, attribute::String, id::Int64)
    return [string_to_date_time(s) for s in read_vector_strings_by_id(db, collection, attribute, id)]
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

function read_set_date_time_by_id(db::Database, collection::String, attribute::String, id::Int64)
    return [string_to_date_time(s) for s in read_set_strings_by_id(db, collection, attribute, id)]
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

function _get_value_data_type(value_columns::Vector{ScalarMetadata})
    if !isempty(value_columns)
        return value_columns[1].data_type
    end
    return C.QUIVER_DATA_TYPE_STRING
end

function read_all_scalars_by_id(db::Database, collection::String, id::Int64)
    result = Dict{String, Any}()
    for attribute in list_scalar_attributes(db, collection)
        name = attribute.name
        if attribute.data_type == C.QUIVER_DATA_TYPE_INTEGER
            result[name] = read_scalar_integer_by_id(db, collection, name, id)
        elseif attribute.data_type == C.QUIVER_DATA_TYPE_FLOAT
            result[name] = read_scalar_float_by_id(db, collection, name, id)
        elseif attribute.data_type == C.QUIVER_DATA_TYPE_STRING
            result[name] = read_scalar_string_by_id(db, collection, name, id)
        elseif attribute.data_type == C.QUIVER_DATA_TYPE_DATE_TIME
            result[name] = read_scalar_date_time_by_id(db, collection, name, id)
        else
            throw(DatabaseException("Unsupported scalar data type for '$collection.$name'"))
        end
    end
    return result
end

function read_all_vectors_by_id(db::Database, collection::String, id::Int64)
    result = Dict{String, Vector{Any}}()
    for group in list_vector_groups(db, collection)
        name = group.group_name
        data_type = _get_value_data_type(group.value_columns)
        if data_type == C.QUIVER_DATA_TYPE_INTEGER
            result[name] = read_vector_integers_by_id(db, collection, name, id)
        elseif data_type == C.QUIVER_DATA_TYPE_FLOAT
            result[name] = read_vector_floats_by_id(db, collection, name, id)
        elseif data_type == C.QUIVER_DATA_TYPE_STRING
            result[name] = read_vector_strings_by_id(db, collection, name, id)
        elseif data_type == C.QUIVER_DATA_TYPE_DATE_TIME
            result[name] = read_vector_date_time_by_id(db, collection, name, id)
        else
            throw(DatabaseException("Unsupported vector data type for '$collection.$name'"))
        end
    end
    return result
end

function read_all_sets_by_id(db::Database, collection::String, id::Int64)
    result = Dict{String, Vector{Any}}()
    for group in list_set_groups(db, collection)
        name = group.group_name
        data_type = _get_value_data_type(group.value_columns)
        if data_type == C.QUIVER_DATA_TYPE_INTEGER
            result[name] = read_set_integers_by_id(db, collection, name, id)
        elseif data_type == C.QUIVER_DATA_TYPE_FLOAT
            result[name] = read_set_floats_by_id(db, collection, name, id)
        elseif data_type == C.QUIVER_DATA_TYPE_STRING
            result[name] = read_set_strings_by_id(db, collection, name, id)
        elseif data_type == C.QUIVER_DATA_TYPE_DATE_TIME
            result[name] = read_set_date_time_by_id(db, collection, name, id)
        else
            throw(DatabaseException("Unsupported set data type for '$collection.$name'"))
        end
    end
    return result
end

function read_vector_group_by_id(db::Database, collection::String, group::String, id::Int64)
    # Find the matching group from the list of vector groups
    all_groups = list_vector_groups(db, collection)
    metadata = nothing
    for g in all_groups
        if g.group_name == group
            metadata = g
            break
        end
    end
    if metadata === nothing
        throw(DatabaseException("Vector group '$group' not found in collection '$collection'"))
    end
    columns = metadata.value_columns

    if isempty(columns)
        return Vector{Dict{String, Any}}()
    end

    # Read each column's data
    column_data = Dict{String, Vector{Any}}()
    row_count = 0

    for col in columns
        name = col.name
        values = if col.data_type == C.QUIVER_DATA_TYPE_INTEGER
            read_vector_integers_by_id(db, collection, name, id)
        elseif col.data_type == C.QUIVER_DATA_TYPE_FLOAT
            read_vector_floats_by_id(db, collection, name, id)
        elseif col.data_type == C.QUIVER_DATA_TYPE_STRING
            read_vector_strings_by_id(db, collection, name, id)
        elseif col.data_type == C.QUIVER_DATA_TYPE_DATE_TIME
            read_vector_date_time_by_id(db, collection, name, id)
        else
            throw(DatabaseException("Unknown data type: $(col.data_type)"))
        end

        column_data[name] = values
        row_count = length(values)
    end

    # Transpose columns to rows
    rows = Vector{Dict{String, Any}}()
    for i in 1:row_count
        row = Dict{String, Any}()
        for (name, values) in column_data
            row[name] = values[i]
        end
        push!(rows, row)
    end

    return rows
end

function read_set_group_by_id(db::Database, collection::String, group::String, id::Int64)
    # Find the matching group from the list of set groups
    all_groups = list_set_groups(db, collection)
    metadata = nothing
    for g in all_groups
        if g.group_name == group
            metadata = g
            break
        end
    end
    if metadata === nothing
        throw(DatabaseException("Set group '$group' not found in collection '$collection'"))
    end
    columns = metadata.value_columns

    if isempty(columns)
        return Vector{Dict{String, Any}}()
    end

    # Read each column's data
    column_data = Dict{String, Vector{Any}}()
    row_count = 0

    for col in columns
        name = col.name
        values = if col.data_type == C.QUIVER_DATA_TYPE_INTEGER
            read_set_integers_by_id(db, collection, name, id)
        elseif col.data_type == C.QUIVER_DATA_TYPE_FLOAT
            read_set_floats_by_id(db, collection, name, id)
        elseif col.data_type == C.QUIVER_DATA_TYPE_STRING
            read_set_strings_by_id(db, collection, name, id)
        elseif col.data_type == C.QUIVER_DATA_TYPE_DATE_TIME
            read_set_date_time_by_id(db, collection, name, id)
        else
            throw(DatabaseException("Unknown data type: $(col.data_type)"))
        end

        column_data[name] = values
        row_count = length(values)
    end

    # Transpose columns to rows
    rows = Vector{Dict{String, Any}}()
    for i in 1:row_count
        row = Dict{String, Any}()
        for (name, values) in column_data
            row[name] = values[i]
        end
        push!(rows, row)
    end

    return rows
end

function read_time_series_group_by_id(db::Database, collection::String, group::String, id::Int64)
    out_date_times = Ref{Ptr{Ptr{Cchar}}}(C_NULL)
    out_values = Ref{Ptr{Cdouble}}(C_NULL)
    out_row_count = Ref{Csize_t}(0)

    err = C.quiver_database_read_time_series_group_by_id(
        db.ptr, collection, group, id,
        out_date_times, out_values, out_row_count,
    )
    if err != C.QUIVER_OK
        throw(DatabaseException("Failed to read time series group '$group' from '$collection' for id $id"))
    end

    row_count = out_row_count[]
    if row_count == 0 || out_date_times[] == C_NULL
        return Vector{Dict{String, Any}}()
    end

    date_time_ptrs = unsafe_wrap(Array, out_date_times[], row_count)
    values = unsafe_wrap(Array, out_values[], row_count) |> copy

    rows = Vector{Dict{String, Any}}()
    for i in 1:row_count
        push!(rows, Dict{String, Any}(
            "date_time" => unsafe_string(date_time_ptrs[i]),
            "value" => values[i],
        ))
    end

    C.quiver_free_time_series_data(out_date_times[], out_values[], row_count)
    return rows
end

function read_time_series_files(db::Database, collection::String)
    out_columns = Ref{Ptr{Ptr{Cchar}}}(C_NULL)
    out_paths = Ref{Ptr{Ptr{Cchar}}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    err = C.quiver_database_read_time_series_files(db.ptr, collection, out_columns, out_paths, out_count)
    if err != C.QUIVER_OK
        throw(DatabaseException("Failed to read time series files for '$collection'"))
    end

    count = out_count[]
    if count == 0 || out_columns[] == C_NULL
        return Dict{String, Union{String, Nothing}}()
    end

    column_ptrs = unsafe_wrap(Array, out_columns[], count)
    path_ptrs = unsafe_wrap(Array, out_paths[], count)

    result = Dict{String, Union{String, Nothing}}()
    for i in 1:count
        col_name = unsafe_string(column_ptrs[i])
        if path_ptrs[i] == C_NULL
            result[col_name] = nothing
        else
            result[col_name] = unsafe_string(path_ptrs[i])
        end
    end

    C.quiver_free_time_series_files(out_columns[], out_paths[], count)
    return result
end
