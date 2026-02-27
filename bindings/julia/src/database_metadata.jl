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

struct GroupMetadata
    group_name::String
    dimension_column::String  # empty for vector/set groups
    value_columns::Vector{ScalarMetadata}
end

function parse_scalar_metadata(col::C.quiver_scalar_metadata_t)
    return ScalarMetadata(
        unsafe_string(col.name),
        col.data_type,
        col.not_null != 0,
        col.primary_key != 0,
        col.default_value == C_NULL ? nothing : unsafe_string(col.default_value),
        col.is_foreign_key != 0,
        col.references_collection == C_NULL ? nothing : unsafe_string(col.references_collection),
        col.references_column == C_NULL ? nothing : unsafe_string(col.references_column),
    )
end

function parse_group_metadata(metadata::C.quiver_group_metadata_t)
    value_columns = ScalarMetadata[]
    for i in 1:metadata.value_column_count
        col_ptr = metadata.value_columns + (i - 1) * sizeof(C.quiver_scalar_metadata_t)
        col = unsafe_load(Ptr{C.quiver_scalar_metadata_t}(col_ptr))
        push!(value_columns, parse_scalar_metadata(col))
    end
    return GroupMetadata(
        unsafe_string(metadata.group_name),
        metadata.dimension_column == C_NULL ? "" : unsafe_string(metadata.dimension_column),
        value_columns,
    )
end

function get_scalar_metadata(db::Database, collection::AbstractString, attribute::AbstractString)
    metadata = Ref(C.quiver_scalar_metadata_t(C_NULL, C.QUIVER_DATA_TYPE_INTEGER, 0, 0, C_NULL, 0, C_NULL, C_NULL))
    check(C.quiver_database_get_scalar_metadata(db.ptr, collection, attribute, metadata))

    result = parse_scalar_metadata(metadata[])

    C.quiver_database_free_scalar_metadata(metadata)
    return result
end

function get_vector_metadata(db::Database, collection::AbstractString, group_name::AbstractString)
    metadata = Ref(C.quiver_group_metadata_t(C_NULL, C_NULL, C_NULL, 0))
    check(C.quiver_database_get_vector_metadata(db.ptr, collection, group_name, metadata))

    result = parse_group_metadata(metadata[])

    C.quiver_database_free_group_metadata(metadata)
    return result
end

function get_set_metadata(db::Database, collection::AbstractString, group_name::AbstractString)
    metadata = Ref(C.quiver_group_metadata_t(C_NULL, C_NULL, C_NULL, 0))
    check(C.quiver_database_get_set_metadata(db.ptr, collection, group_name, metadata))

    result = parse_group_metadata(metadata[])

    C.quiver_database_free_group_metadata(metadata)
    return result
end

function list_scalar_attributes(db::Database, collection::AbstractString)
    out_metadata = Ref(Ptr{C.quiver_scalar_metadata_t}(C_NULL))
    out_count = Ref(Csize_t(0))
    check(C.quiver_database_list_scalar_attributes(db.ptr, collection, out_metadata, out_count))

    count = out_count[]
    if count == 0 || out_metadata[] == C_NULL
        return ScalarMetadata[]
    end

    result = ScalarMetadata[]
    for i in 1:count
        meta_ptr = out_metadata[] + (i - 1) * sizeof(C.quiver_scalar_metadata_t)
        metadata = unsafe_load(Ptr{C.quiver_scalar_metadata_t}(meta_ptr))
        push!(result, parse_scalar_metadata(metadata))
    end
    C.quiver_database_free_scalar_metadata_array(out_metadata[], count)
    return result
end

function list_vector_groups(db::Database, collection::AbstractString)
    out_metadata = Ref(Ptr{C.quiver_group_metadata_t}(C_NULL))
    out_count = Ref(Csize_t(0))
    check(C.quiver_database_list_vector_groups(db.ptr, collection, out_metadata, out_count))

    count = out_count[]
    if count == 0 || out_metadata[] == C_NULL
        return GroupMetadata[]
    end

    result = GroupMetadata[]
    for i in 1:count
        meta_ptr = out_metadata[] + (i - 1) * sizeof(C.quiver_group_metadata_t)
        metadata = unsafe_load(Ptr{C.quiver_group_metadata_t}(meta_ptr))
        push!(result, parse_group_metadata(metadata))
    end
    C.quiver_database_free_group_metadata_array(out_metadata[], count)
    return result
end

function list_set_groups(db::Database, collection::AbstractString)
    out_metadata = Ref(Ptr{C.quiver_group_metadata_t}(C_NULL))
    out_count = Ref(Csize_t(0))
    check(C.quiver_database_list_set_groups(db.ptr, collection, out_metadata, out_count))

    count = out_count[]
    if count == 0 || out_metadata[] == C_NULL
        return GroupMetadata[]
    end

    result = GroupMetadata[]
    for i in 1:count
        meta_ptr = out_metadata[] + (i - 1) * sizeof(C.quiver_group_metadata_t)
        metadata = unsafe_load(Ptr{C.quiver_group_metadata_t}(meta_ptr))
        push!(result, parse_group_metadata(metadata))
    end
    C.quiver_database_free_group_metadata_array(out_metadata[], count)
    return result
end

function get_time_series_metadata(db::Database, collection::AbstractString, group_name::AbstractString)
    metadata = Ref(C.quiver_group_metadata_t(C_NULL, C_NULL, C_NULL, 0))
    check(C.quiver_database_get_time_series_metadata(db.ptr, collection, group_name, metadata))

    result = parse_group_metadata(metadata[])

    C.quiver_database_free_group_metadata(metadata)
    return result
end

function list_time_series_groups(db::Database, collection::AbstractString)
    out_metadata = Ref(Ptr{C.quiver_group_metadata_t}(C_NULL))
    out_count = Ref(Csize_t(0))
    check(C.quiver_database_list_time_series_groups(db.ptr, collection, out_metadata, out_count))

    count = out_count[]
    if count == 0 || out_metadata[] == C_NULL
        return GroupMetadata[]
    end

    result = GroupMetadata[]
    for i in 1:count
        meta_ptr = out_metadata[] + (i - 1) * sizeof(C.quiver_group_metadata_t)
        metadata = unsafe_load(Ptr{C.quiver_group_metadata_t}(meta_ptr))
        push!(result, parse_group_metadata(metadata))
    end
    C.quiver_database_free_group_metadata_array(out_metadata[], count)
    return result
end

function has_time_series_files(db::Database, collection::String)
    out_result = Ref{Cint}(0)
    check(C.quiver_database_has_time_series_files(db.ptr, collection, out_result))
    return out_result[] != 0
end

function list_time_series_files_columns(db::Database, collection::String)
    out_columns = Ref{Ptr{Ptr{Cchar}}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    check(C.quiver_database_list_time_series_files_columns(db.ptr, collection, out_columns, out_count))

    count = out_count[]
    if count == 0 || out_columns[] == C_NULL
        return String[]
    end

    ptrs = unsafe_wrap(Array, out_columns[], count)
    result = [unsafe_string(ptr) for ptr in ptrs]
    C.quiver_database_free_string_array(out_columns[], count)
    return result
end
