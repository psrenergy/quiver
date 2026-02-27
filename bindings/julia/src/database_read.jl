function read_scalar_integers(db::Database, collection::String, attribute::String)
    out_values = Ref{Ptr{Int64}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    check(C.quiver_database_read_scalar_integers(db.ptr, collection, attribute, out_values, out_count))

    count = out_count[]
    if count == 0 || out_values[] == C_NULL
        return Int64[]
    end

    result = unsafe_wrap(Array, out_values[], count) |> copy
    C.quiver_database_free_integer_array(out_values[])
    return result
end

function read_scalar_floats(db::Database, collection::String, attribute::String)
    out_values = Ref{Ptr{Float64}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    check(C.quiver_database_read_scalar_floats(db.ptr, collection, attribute, out_values, out_count))

    count = out_count[]
    if count == 0 || out_values[] == C_NULL
        return Float64[]
    end

    result = unsafe_wrap(Array, out_values[], count) |> copy
    C.quiver_database_free_float_array(out_values[])
    return result
end

function read_scalar_strings(db::Database, collection::String, attribute::String)
    out_values = Ref{Ptr{Ptr{Cchar}}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    check(C.quiver_database_read_scalar_strings(db.ptr, collection, attribute, out_values, out_count))

    count = out_count[]
    if count == 0 || out_values[] == C_NULL
        return String[]
    end

    ptrs = unsafe_wrap(Array, out_values[], count)
    result = [unsafe_string(ptr) for ptr in ptrs]
    C.quiver_database_free_string_array(out_values[], count)
    return result
end

function read_vector_integers(db::Database, collection::String, attribute::String)
    out_vectors = Ref{Ptr{Ptr{Int64}}}(C_NULL)
    out_sizes = Ref{Ptr{Csize_t}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    check(C.quiver_database_read_vector_integers(db.ptr, collection, attribute, out_vectors, out_sizes, out_count))

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
    C.quiver_database_free_integer_vectors(out_vectors[], out_sizes[], count)
    return result
end

function read_vector_floats(db::Database, collection::String, attribute::String)
    out_vectors = Ref{Ptr{Ptr{Cdouble}}}(C_NULL)
    out_sizes = Ref{Ptr{Csize_t}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    check(C.quiver_database_read_vector_floats(db.ptr, collection, attribute, out_vectors, out_sizes, out_count))

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
    C.quiver_database_free_float_vectors(out_vectors[], out_sizes[], count)
    return result
end

function read_vector_strings(db::Database, collection::String, attribute::String)
    out_vectors = Ref{Ptr{Ptr{Ptr{Cchar}}}}(C_NULL)
    out_sizes = Ref{Ptr{Csize_t}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    check(C.quiver_database_read_vector_strings(db.ptr, collection, attribute, out_vectors, out_sizes, out_count))

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
    C.quiver_database_free_string_vectors(out_vectors[], out_sizes[], count)
    return result
end

function read_set_integers(db::Database, collection::String, attribute::String)
    out_sets = Ref{Ptr{Ptr{Int64}}}(C_NULL)
    out_sizes = Ref{Ptr{Csize_t}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    check(C.quiver_database_read_set_integers(db.ptr, collection, attribute, out_sets, out_sizes, out_count))

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
    C.quiver_database_free_integer_vectors(out_sets[], out_sizes[], count)
    return result
end

function read_set_floats(db::Database, collection::String, attribute::String)
    out_sets = Ref{Ptr{Ptr{Cdouble}}}(C_NULL)
    out_sizes = Ref{Ptr{Csize_t}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    check(C.quiver_database_read_set_floats(db.ptr, collection, attribute, out_sets, out_sizes, out_count))

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
    C.quiver_database_free_float_vectors(out_sets[], out_sizes[], count)
    return result
end

function read_set_strings(db::Database, collection::String, attribute::String)
    out_sets = Ref{Ptr{Ptr{Ptr{Cchar}}}}(C_NULL)
    out_sizes = Ref{Ptr{Csize_t}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    check(C.quiver_database_read_set_strings(db.ptr, collection, attribute, out_sets, out_sizes, out_count))

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
    C.quiver_database_free_string_vectors(out_sets[], out_sizes[], count)
    return result
end

function read_scalar_integer_by_id(db::Database, collection::String, attribute::String, id::Int64)
    out_value = Ref{Int64}(0)
    out_has_value = Ref{Cint}(0)

    check(C.quiver_database_read_scalar_integer_by_id(db.ptr, collection, attribute, id, out_value, out_has_value))

    if out_has_value[] == 0
        return nothing
    end
    return out_value[]
end

function read_scalar_float_by_id(db::Database, collection::String, attribute::String, id::Int64)
    out_value = Ref{Float64}(0.0)
    out_has_value = Ref{Cint}(0)

    check(C.quiver_database_read_scalar_float_by_id(db.ptr, collection, attribute, id, out_value, out_has_value))

    if out_has_value[] == 0
        return nothing
    end
    return out_value[]
end

function read_scalar_string_by_id(db::Database, collection::String, attribute::String, id::Int64)
    out_value = Ref{Ptr{Cchar}}(C_NULL)
    out_has_value = Ref{Cint}(0)

    check(C.quiver_database_read_scalar_string_by_id(db.ptr, collection, attribute, id, out_value, out_has_value))

    if out_has_value[] == 0 || out_value[] == C_NULL
        return nothing
    end
    result = unsafe_string(out_value[])
    C.quiver_element_free_string(out_value[])
    return result
end

function read_scalar_date_time_by_id(db::Database, collection::String, attribute::String, id::Int64)
    return string_to_date_time(read_scalar_string_by_id(db, collection, attribute, id))
end

function read_vector_integers_by_id(db::Database, collection::String, attribute::String, id::Int64)
    out_values = Ref{Ptr{Int64}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    check(C.quiver_database_read_vector_integers_by_id(db.ptr, collection, attribute, id, out_values, out_count))

    count = out_count[]
    if count == 0 || out_values[] == C_NULL
        return Int64[]
    end

    result = unsafe_wrap(Array, out_values[], count) |> copy
    C.quiver_database_free_integer_array(out_values[])
    return result
end

function read_vector_floats_by_id(db::Database, collection::String, attribute::String, id::Int64)
    out_values = Ref{Ptr{Float64}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    check(C.quiver_database_read_vector_floats_by_id(db.ptr, collection, attribute, id, out_values, out_count))

    count = out_count[]
    if count == 0 || out_values[] == C_NULL
        return Float64[]
    end

    result = unsafe_wrap(Array, out_values[], count) |> copy
    C.quiver_database_free_float_array(out_values[])
    return result
end

function read_vector_strings_by_id(db::Database, collection::String, attribute::String, id::Int64)
    out_values = Ref{Ptr{Ptr{Cchar}}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    check(C.quiver_database_read_vector_strings_by_id(db.ptr, collection, attribute, id, out_values, out_count))

    count = out_count[]
    if count == 0 || out_values[] == C_NULL
        return String[]
    end

    ptrs = unsafe_wrap(Array, out_values[], count)
    result = [unsafe_string(ptr) for ptr in ptrs]
    C.quiver_database_free_string_array(out_values[], count)
    return result
end

function read_vector_date_time_by_id(db::Database, collection::String, attribute::String, id::Int64)
    return [string_to_date_time(s) for s in read_vector_strings_by_id(db, collection, attribute, id)]
end

function read_set_integers_by_id(db::Database, collection::String, attribute::String, id::Int64)
    out_values = Ref{Ptr{Int64}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    check(C.quiver_database_read_set_integers_by_id(db.ptr, collection, attribute, id, out_values, out_count))

    count = out_count[]
    if count == 0 || out_values[] == C_NULL
        return Int64[]
    end

    result = unsafe_wrap(Array, out_values[], count) |> copy
    C.quiver_database_free_integer_array(out_values[])
    return result
end

function read_set_floats_by_id(db::Database, collection::String, attribute::String, id::Int64)
    out_values = Ref{Ptr{Float64}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    check(C.quiver_database_read_set_floats_by_id(db.ptr, collection, attribute, id, out_values, out_count))

    count = out_count[]
    if count == 0 || out_values[] == C_NULL
        return Float64[]
    end

    result = unsafe_wrap(Array, out_values[], count) |> copy
    C.quiver_database_free_float_array(out_values[])
    return result
end

function read_set_strings_by_id(db::Database, collection::String, attribute::String, id::Int64)
    out_values = Ref{Ptr{Ptr{Cchar}}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    check(C.quiver_database_read_set_strings_by_id(db.ptr, collection, attribute, id, out_values, out_count))

    count = out_count[]
    if count == 0 || out_values[] == C_NULL
        return String[]
    end

    ptrs = unsafe_wrap(Array, out_values[], count)
    result = [unsafe_string(ptr) for ptr in ptrs]
    C.quiver_database_free_string_array(out_values[], count)
    return result
end

function read_set_date_time_by_id(db::Database, collection::String, attribute::String, id::Int64)
    return [string_to_date_time(s) for s in read_set_strings_by_id(db, collection, attribute, id)]
end

function read_element_ids(db::Database, collection::String)
    out_ids = Ref{Ptr{Int64}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    check(C.quiver_database_read_element_ids(db.ptr, collection, out_ids, out_count))

    count = out_count[]
    if count == 0 || out_ids[] == C_NULL
        return Int64[]
    end

    result = unsafe_wrap(Array, out_ids[], count) |> copy
    C.quiver_database_free_integer_array(out_ids[])
    return result
end

function read_scalars_by_id(db::Database, collection::String, id::Int64)
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
            throw(ArgumentError("Unsupported scalar data type $(attribute.data_type) for '$collection.$name'"))
        end
    end
    return result
end

function read_vectors_by_id(db::Database, collection::String, id::Int64)
    result = Dict{String, Any}()
    for group in list_vector_groups(db, collection)
        for col in group.value_columns
            name = col.name
            if col.data_type == C.QUIVER_DATA_TYPE_INTEGER
                result[name] = read_vector_integers_by_id(db, collection, name, id)
            elseif col.data_type == C.QUIVER_DATA_TYPE_FLOAT
                result[name] = read_vector_floats_by_id(db, collection, name, id)
            elseif col.data_type == C.QUIVER_DATA_TYPE_STRING
                result[name] = read_vector_strings_by_id(db, collection, name, id)
            elseif col.data_type == C.QUIVER_DATA_TYPE_DATE_TIME
                result[name] = read_vector_date_time_by_id(db, collection, name, id)
            else
                throw(ArgumentError("Unsupported vector data type $(col.data_type) for '$collection.$name'"))
            end
        end
    end
    return result
end

function read_sets_by_id(db::Database, collection::String, id::Int64)
    result = Dict{String, Any}()
    for group in list_set_groups(db, collection)
        for col in group.value_columns
            name = col.name
            if col.data_type == C.QUIVER_DATA_TYPE_INTEGER
                result[name] = read_set_integers_by_id(db, collection, name, id)
            elseif col.data_type == C.QUIVER_DATA_TYPE_FLOAT
                result[name] = read_set_floats_by_id(db, collection, name, id)
            elseif col.data_type == C.QUIVER_DATA_TYPE_STRING
                result[name] = read_set_strings_by_id(db, collection, name, id)
            elseif col.data_type == C.QUIVER_DATA_TYPE_DATE_TIME
                result[name] = read_set_date_time_by_id(db, collection, name, id)
            else
                throw(ArgumentError("Unsupported set data type $(col.data_type) for '$collection.$name'"))
            end
        end
    end
    return result
end

function read_vector_group_by_id(db::Database, collection::String, group::String, id::Int64)
    metadata = get_vector_metadata(db, collection, group)
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
            throw(ArgumentError("Unsupported data type $(col.data_type) for column '$(col.name)'"))
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
    metadata = get_set_metadata(db, collection, group)
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
            throw(ArgumentError("Unsupported data type $(col.data_type) for column '$(col.name)'"))
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

function read_time_series_group(db::Database, collection::String, group::String, id::Int64)
    out_col_names = Ref{Ptr{Ptr{Cchar}}}(C_NULL)
    out_col_types = Ref{Ptr{Cint}}(C_NULL)
    out_col_data = Ref{Ptr{Ptr{Cvoid}}}(C_NULL)
    out_col_count = Ref{Csize_t}(0)
    out_row_count = Ref{Csize_t}(0)

    check(
        C.quiver_database_read_time_series_group(
            db.ptr, collection, group, id,
            out_col_names, out_col_types, out_col_data, out_col_count, out_row_count,
        ),
    )

    col_count = out_col_count[]
    row_count = out_row_count[]

    if col_count == 0 || row_count == 0
        return Dict{String, Vector}()
    end

    # Get dimension column name for DateTime parsing
    metadata = get_time_series_metadata(db, collection, group)
    dim_col = metadata.dimension_column

    # Unmarshal column names, types, data
    name_ptrs = unsafe_wrap(Array, out_col_names[], col_count)
    type_vals = unsafe_wrap(Array, out_col_types[], col_count)
    data_ptrs = unsafe_wrap(Array, out_col_data[], col_count)

    result = Dict{String, Vector}()
    for i in 1:col_count
        col_name = unsafe_string(name_ptrs[i])
        col_type = type_vals[i]

        if col_type == Cint(C.QUIVER_DATA_TYPE_INTEGER)
            int_ptr = reinterpret(Ptr{Int64}, data_ptrs[i])
            result[col_name] = copy(unsafe_wrap(Array, int_ptr, row_count))
        elseif col_type == Cint(C.QUIVER_DATA_TYPE_FLOAT)
            float_ptr = reinterpret(Ptr{Float64}, data_ptrs[i])
            result[col_name] = copy(unsafe_wrap(Array, float_ptr, row_count))
        elseif col_type == Cint(C.QUIVER_DATA_TYPE_STRING) || col_type == Cint(C.QUIVER_DATA_TYPE_DATE_TIME)
            str_ptr_ptr = reinterpret(Ptr{Ptr{Cchar}}, data_ptrs[i])
            str_ptrs = unsafe_wrap(Array, str_ptr_ptr, row_count)
            if col_name == dim_col
                result[col_name] = DateTime[string_to_date_time(unsafe_string(p)) for p in str_ptrs]
            else
                result[col_name] = String[unsafe_string(p) for p in str_ptrs]
            end
        end
    end

    # Free C-allocated memory
    C.quiver_database_free_time_series_data(
        out_col_names[], out_col_types[], out_col_data[],
        Csize_t(col_count), Csize_t(row_count),
    )

    return result
end

function read_time_series_files(db::Database, collection::String)
    out_columns = Ref{Ptr{Ptr{Cchar}}}(C_NULL)
    out_paths = Ref{Ptr{Ptr{Cchar}}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    check(C.quiver_database_read_time_series_files(db.ptr, collection, out_columns, out_paths, out_count))

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

    C.quiver_database_free_time_series_files(out_columns[], out_paths[], count)
    return result
end

function read_element_by_id(db::Database, collection::String, id::Int64)
    scalars = read_scalars_by_id(db, collection, id)
    vectors = read_vectors_by_id(db, collection, id)
    sets = read_sets_by_id(db, collection, id)

    # delete!(result, "id")

    # # Early exit for nonexistent element (label is NOT NULL, so nothing means no row)
    # if haskey(result, "label") && result["label"] === nothing
    #     return Dict{String, Any}()
    # end

    return merge(scalars, vectors, sets)
end
