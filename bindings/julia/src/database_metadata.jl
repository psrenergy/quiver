struct ScalarMetadata
    name::String
    data_type::C.quiver_data_type_t
    not_null::Bool
    primary_key::Bool
    default_value::Union{String, Nothing}
    is_foreign_key::Bool
    references_collection::Union{String, Nothing}
    references_column::Union{String, Nothing}
end

struct VectorMetadata
    group_name::String
    value_columns::Vector{ScalarMetadata}
end

struct SetMetadata
    group_name::String
    value_columns::Vector{ScalarMetadata}
end

function get_scalar_metadata(db::Database, collection::AbstractString, attribute::AbstractString)
    metadata = Ref(C.quiver_scalar_metadata_t(C_NULL, C.QUIVER_DATA_TYPE_INTEGER, 0, 0, C_NULL, 0, C_NULL, C_NULL))
    err = C.quiver_database_get_scalar_metadata(db.ptr, collection, attribute, metadata)
    if err != C.QUIVER_OK
        throw(DatabaseException("Failed to get scalar metadata for '$collection.$attribute'"))
    end

    result = ScalarMetadata(
        unsafe_string(metadata[].name),
        metadata[].data_type,
        metadata[].not_null != 0,
        metadata[].primary_key != 0,
        metadata[].default_value == C_NULL ? nothing : unsafe_string(metadata[].default_value),
        metadata[].is_foreign_key != 0,
        metadata[].references_collection == C_NULL ? nothing : unsafe_string(metadata[].references_collection),
        metadata[].references_column == C_NULL ? nothing : unsafe_string(metadata[].references_column),
    )

    C.quiver_free_scalar_metadata(metadata)
    return result
end

function get_vector_metadata(db::Database, collection::AbstractString, group_name::AbstractString)
    metadata = Ref(C.quiver_vector_metadata_t(C_NULL, C_NULL, 0))
    err = C.quiver_database_get_vector_metadata(db.ptr, collection, group_name, metadata)
    if err != C.QUIVER_OK
        throw(DatabaseException("Failed to get vector metadata for '$collection.$group_name'"))
    end

    value_columns = ScalarMetadata[]
    for i in 1:metadata[].value_column_count
        col_ptr = metadata[].value_columns + (i - 1) * sizeof(C.quiver_scalar_metadata_t)
        col = unsafe_load(Ptr{C.quiver_scalar_metadata_t}(col_ptr))
        push!(
            value_columns,
            ScalarMetadata(
                unsafe_string(col.name),
                col.data_type,
                col.not_null != 0,
                col.primary_key != 0,
                col.default_value == C_NULL ? nothing : unsafe_string(col.default_value),
                col.is_foreign_key != 0,
                col.references_collection == C_NULL ? nothing : unsafe_string(col.references_collection),
                col.references_column == C_NULL ? nothing : unsafe_string(col.references_column),
            ),
        )
    end

    result = VectorMetadata(
        unsafe_string(metadata[].group_name),
        value_columns,
    )

    C.quiver_free_vector_metadata(metadata)
    return result
end

function get_set_metadata(db::Database, collection::AbstractString, group_name::AbstractString)
    metadata = Ref(C.quiver_set_metadata_t(C_NULL, C_NULL, 0))
    err = C.quiver_database_get_set_metadata(db.ptr, collection, group_name, metadata)
    if err != C.QUIVER_OK
        throw(DatabaseException("Failed to get set metadata for '$collection.$group_name'"))
    end

    value_columns = ScalarMetadata[]
    for i in 1:metadata[].value_column_count
        col_ptr = metadata[].value_columns + (i - 1) * sizeof(C.quiver_scalar_metadata_t)
        col = unsafe_load(Ptr{C.quiver_scalar_metadata_t}(col_ptr))
        push!(
            value_columns,
            ScalarMetadata(
                unsafe_string(col.name),
                col.data_type,
                col.not_null != 0,
                col.primary_key != 0,
                col.default_value == C_NULL ? nothing : unsafe_string(col.default_value),
                col.is_foreign_key != 0,
                col.references_collection == C_NULL ? nothing : unsafe_string(col.references_collection),
                col.references_column == C_NULL ? nothing : unsafe_string(col.references_column),
            ),
        )
    end

    result = SetMetadata(
        unsafe_string(metadata[].group_name),
        value_columns,
    )

    C.quiver_free_set_metadata(metadata)
    return result
end

function list_scalar_attributes(db::Database, collection::AbstractString)
    out_metadata = Ref(Ptr{C.quiver_scalar_metadata_t}(C_NULL))
    out_count = Ref(Csize_t(0))
    err = C.quiver_database_list_scalar_attributes(db.ptr, collection, out_metadata, out_count)
    if err != C.QUIVER_OK
        throw(DatabaseException("Failed to list scalar attributes for '$collection'"))
    end

    count = out_count[]
    if count == 0 || out_metadata[] == C_NULL
        return ScalarMetadata[]
    end

    result = ScalarMetadata[]
    for i in 1:count
        meta_ptr = out_metadata[] + (i - 1) * sizeof(C.quiver_scalar_metadata_t)
        meta = unsafe_load(Ptr{C.quiver_scalar_metadata_t}(meta_ptr))
        push!(
            result,
            ScalarMetadata(
                unsafe_string(meta.name),
                meta.data_type,
                meta.not_null != 0,
                meta.primary_key != 0,
                meta.default_value == C_NULL ? nothing : unsafe_string(meta.default_value),
                meta.is_foreign_key != 0,
                meta.references_collection == C_NULL ? nothing : unsafe_string(meta.references_collection),
                meta.references_column == C_NULL ? nothing : unsafe_string(meta.references_column),
            ),
        )
    end
    C.quiver_free_scalar_metadata_array(out_metadata[], count)
    return result
end

function list_vector_groups(db::Database, collection::AbstractString)
    out_metadata = Ref(Ptr{C.quiver_vector_metadata_t}(C_NULL))
    out_count = Ref(Csize_t(0))
    err = C.quiver_database_list_vector_groups(db.ptr, collection, out_metadata, out_count)
    if err != C.QUIVER_OK
        throw(DatabaseException("Failed to list vector groups for '$collection'"))
    end

    count = out_count[]
    if count == 0 || out_metadata[] == C_NULL
        return VectorMetadata[]
    end

    result = VectorMetadata[]
    for i in 1:count
        meta_ptr = out_metadata[] + (i - 1) * sizeof(C.quiver_vector_metadata_t)
        meta = unsafe_load(Ptr{C.quiver_vector_metadata_t}(meta_ptr))

        value_columns = ScalarMetadata[]
        for j in 1:meta.value_column_count
            col_ptr = meta.value_columns + (j - 1) * sizeof(C.quiver_scalar_metadata_t)
            col = unsafe_load(Ptr{C.quiver_scalar_metadata_t}(col_ptr))
            push!(
                value_columns,
                ScalarMetadata(
                    unsafe_string(col.name),
                    col.data_type,
                    col.not_null != 0,
                    col.primary_key != 0,
                    col.default_value == C_NULL ? nothing : unsafe_string(col.default_value),
                    col.is_foreign_key != 0,
                    col.references_collection == C_NULL ? nothing : unsafe_string(col.references_collection),
                    col.references_column == C_NULL ? nothing : unsafe_string(col.references_column),
                ),
            )
        end

        push!(result, VectorMetadata(unsafe_string(meta.group_name), value_columns))
    end
    C.quiver_free_vector_metadata_array(out_metadata[], count)
    return result
end

function list_set_groups(db::Database, collection::AbstractString)
    out_metadata = Ref(Ptr{C.quiver_set_metadata_t}(C_NULL))
    out_count = Ref(Csize_t(0))
    err = C.quiver_database_list_set_groups(db.ptr, collection, out_metadata, out_count)
    if err != C.QUIVER_OK
        throw(DatabaseException("Failed to list set groups for '$collection'"))
    end

    count = out_count[]
    if count == 0 || out_metadata[] == C_NULL
        return SetMetadata[]
    end

    result = SetMetadata[]
    for i in 1:count
        meta_ptr = out_metadata[] + (i - 1) * sizeof(C.quiver_set_metadata_t)
        meta = unsafe_load(Ptr{C.quiver_set_metadata_t}(meta_ptr))

        value_columns = ScalarMetadata[]
        for j in 1:meta.value_column_count
            col_ptr = meta.value_columns + (j - 1) * sizeof(C.quiver_scalar_metadata_t)
            col = unsafe_load(Ptr{C.quiver_scalar_metadata_t}(col_ptr))
            push!(
                value_columns,
                ScalarMetadata(
                    unsafe_string(col.name),
                    col.data_type,
                    col.not_null != 0,
                    col.primary_key != 0,
                    col.default_value == C_NULL ? nothing : unsafe_string(col.default_value),
                    col.is_foreign_key != 0,
                    col.references_collection == C_NULL ? nothing : unsafe_string(col.references_collection),
                    col.references_column == C_NULL ? nothing : unsafe_string(col.references_column),
                ),
            )
        end

        push!(result, SetMetadata(unsafe_string(meta.group_name), value_columns))
    end
    C.quiver_free_set_metadata_array(out_metadata[], count)
    return result
end

struct TimeSeriesMetadata
    group_name::String
    dimension_column::String
    value_columns::Vector{ScalarMetadata}
end

function get_time_series_metadata(db::Database, collection::AbstractString, group_name::AbstractString)
    metadata = Ref(C.quiver_time_series_metadata_t(C_NULL, C_NULL, C_NULL, 0))
    err = C.quiver_database_get_time_series_metadata(db.ptr, collection, group_name, metadata)
    if err != C.QUIVER_OK
        throw(DatabaseException("Failed to get time series metadata for '$collection.$group_name'"))
    end

    value_columns = ScalarMetadata[]
    for i in 1:metadata[].value_column_count
        col_ptr = metadata[].value_columns + (i - 1) * sizeof(C.quiver_scalar_metadata_t)
        col = unsafe_load(Ptr{C.quiver_scalar_metadata_t}(col_ptr))
        push!(
            value_columns,
            ScalarMetadata(
                unsafe_string(col.name),
                col.data_type,
                col.not_null != 0,
                col.primary_key != 0,
                col.default_value == C_NULL ? nothing : unsafe_string(col.default_value),
                col.is_foreign_key != 0,
                col.references_collection == C_NULL ? nothing : unsafe_string(col.references_collection),
                col.references_column == C_NULL ? nothing : unsafe_string(col.references_column),
            ),
        )
    end

    result = TimeSeriesMetadata(
        unsafe_string(metadata[].group_name),
        unsafe_string(metadata[].dimension_column),
        value_columns,
    )

    C.quiver_free_time_series_metadata(metadata)
    return result
end

function list_time_series_groups(db::Database, collection::AbstractString)
    out_metadata = Ref(Ptr{C.quiver_time_series_metadata_t}(C_NULL))
    out_count = Ref(Csize_t(0))
    err = C.quiver_database_list_time_series_groups(db.ptr, collection, out_metadata, out_count)
    if err != C.QUIVER_OK
        throw(DatabaseException("Failed to list time series groups for '$collection'"))
    end

    count = out_count[]
    if count == 0 || out_metadata[] == C_NULL
        return TimeSeriesMetadata[]
    end

    result = TimeSeriesMetadata[]
    for i in 1:count
        meta_ptr = out_metadata[] + (i - 1) * sizeof(C.quiver_time_series_metadata_t)
        meta = unsafe_load(Ptr{C.quiver_time_series_metadata_t}(meta_ptr))

        value_columns = ScalarMetadata[]
        for j in 1:meta.value_column_count
            col_ptr = meta.value_columns + (j - 1) * sizeof(C.quiver_scalar_metadata_t)
            col = unsafe_load(Ptr{C.quiver_scalar_metadata_t}(col_ptr))
            push!(
                value_columns,
                ScalarMetadata(
                    unsafe_string(col.name),
                    col.data_type,
                    col.not_null != 0,
                    col.primary_key != 0,
                    col.default_value == C_NULL ? nothing : unsafe_string(col.default_value),
                    col.is_foreign_key != 0,
                    col.references_collection == C_NULL ? nothing : unsafe_string(col.references_collection),
                    col.references_column == C_NULL ? nothing : unsafe_string(col.references_column),
                ),
            )
        end

        push!(
            result,
            TimeSeriesMetadata(unsafe_string(meta.group_name), unsafe_string(meta.dimension_column), value_columns),
        )
    end
    C.quiver_free_time_series_metadata_array(out_metadata[], count)
    return result
end

function has_time_series_files(db::Database, collection::String)
    out_result = Ref{Cint}(0)
    err = C.quiver_database_has_time_series_files(db.ptr, collection, out_result)
    if err != C.QUIVER_OK
        throw(DatabaseException("Failed to check time series files for '$collection'"))
    end
    return out_result[] != 0
end

function list_time_series_files_columns(db::Database, collection::String)
    out_columns = Ref{Ptr{Ptr{Cchar}}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    err = C.quiver_database_list_time_series_files_columns(db.ptr, collection, out_columns, out_count)
    if err != C.QUIVER_OK
        throw(DatabaseException("Failed to list time series files columns for '$collection'"))
    end

    count = out_count[]
    if count == 0 || out_columns[] == C_NULL
        return String[]
    end

    ptrs = unsafe_wrap(Array, out_columns[], count)
    result = [unsafe_string(ptr) for ptr in ptrs]
    C.quiver_free_string_array(out_columns[], count)
    return result
end
