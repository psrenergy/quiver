struct ScalarMetadata
    name::String
    data_type::Symbol
    not_null::Bool
    primary_key::Bool
    default_value::Union{String, Nothing}
end

struct VectorMetadata
    group_name::String
    attributes::Vector{ScalarMetadata}
end

struct SetMetadata
    group_name::String
    attributes::Vector{ScalarMetadata}
end

function _data_type_symbol(dt::C.quiver_data_type_t)
    if dt == C.QUIVER_DATA_TYPE_INTEGER
        return :integer
    elseif dt == C.QUIVER_DATA_TYPE_FLOAT
        return :real
    elseif dt == C.QUIVER_DATA_TYPE_STRING
        return :text
    else
        return :unknown
    end
end

function get_scalar_metadata(db::Database, collection::AbstractString, attribute::AbstractString)
    metadata = Ref(C.quiver_scalar_metadata_t(C_NULL, C.QUIVER_DATA_TYPE_INTEGER, 0, 0, C_NULL))
    err = C.quiver_database_get_scalar_metadata(db.ptr, collection, attribute, metadata)
    if err != C.QUIVER_OK
        throw(DatabaseException("Failed to get scalar metadata for '$collection.$attribute'"))
    end

    result = ScalarMetadata(
        unsafe_string(metadata[].name),
        _data_type_symbol(metadata[].data_type),
        metadata[].not_null != 0,
        metadata[].primary_key != 0,
        metadata[].default_value == C_NULL ? nothing : unsafe_string(metadata[].default_value),
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

    attributes = ScalarMetadata[]
    for i in 1:metadata[].attribute_count
        attr_ptr = metadata[].attributes + (i - 1) * sizeof(C.quiver_scalar_metadata_t)
        attr = unsafe_load(Ptr{C.quiver_scalar_metadata_t}(attr_ptr))
        push!(
            attributes,
            ScalarMetadata(
                unsafe_string(attr.name),
                _data_type_symbol(attr.data_type),
                attr.not_null != 0,
                attr.primary_key != 0,
                attr.default_value == C_NULL ? nothing : unsafe_string(attr.default_value),
            ),
        )
    end

    result = VectorMetadata(
        unsafe_string(metadata[].group_name),
        attributes,
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

    attributes = ScalarMetadata[]
    for i in 1:metadata[].attribute_count
        attr_ptr = metadata[].attributes + (i - 1) * sizeof(C.quiver_scalar_metadata_t)
        attr = unsafe_load(Ptr{C.quiver_scalar_metadata_t}(attr_ptr))
        push!(
            attributes,
            ScalarMetadata(
                unsafe_string(attr.name),
                _data_type_symbol(attr.data_type),
                attr.not_null != 0,
                attr.primary_key != 0,
                attr.default_value == C_NULL ? nothing : unsafe_string(attr.default_value),
            ),
        )
    end

    result = SetMetadata(
        unsafe_string(metadata[].group_name),
        attributes,
    )

    C.quiver_free_set_metadata(metadata)
    return result
end

function list_scalar_attributes(db::Database, collection::AbstractString)
    out_attrs = Ref(Ptr{Ptr{Cchar}}(C_NULL))
    out_count = Ref(Csize_t(0))
    err = C.quiver_database_list_scalar_attributes(db.ptr, collection, out_attrs, out_count)
    if err != C.QUIVER_OK
        throw(DatabaseException("Failed to list scalar attributes for '$collection'"))
    end

    count = out_count[]
    if count == 0 || out_attrs[] == C_NULL
        return String[]
    end

    result = String[]
    for i in 1:count
        ptr = unsafe_load(out_attrs[], i)
        push!(result, unsafe_string(ptr))
    end
    C.quiver_free_string_array(out_attrs[], count)
    return result
end

function list_vector_groups(db::Database, collection::AbstractString)
    out_groups = Ref(Ptr{Ptr{Cchar}}(C_NULL))
    out_count = Ref(Csize_t(0))
    err = C.quiver_database_list_vector_groups(db.ptr, collection, out_groups, out_count)
    if err != C.QUIVER_OK
        throw(DatabaseException("Failed to list vector groups for '$collection'"))
    end

    count = out_count[]
    if count == 0 || out_groups[] == C_NULL
        return String[]
    end

    result = String[]
    for i in 1:count
        ptr = unsafe_load(out_groups[], i)
        push!(result, unsafe_string(ptr))
    end
    C.quiver_free_string_array(out_groups[], count)
    return result
end

function list_set_groups(db::Database, collection::AbstractString)
    out_groups = Ref(Ptr{Ptr{Cchar}}(C_NULL))
    out_count = Ref(Csize_t(0))
    err = C.quiver_database_list_set_groups(db.ptr, collection, out_groups, out_count)
    if err != C.QUIVER_OK
        throw(DatabaseException("Failed to list set groups for '$collection'"))
    end

    count = out_count[]
    if count == 0 || out_groups[] == C_NULL
        return String[]
    end

    result = String[]
    for i in 1:count
        ptr = unsafe_load(out_groups[], i)
        push!(result, unsafe_string(ptr))
    end
    C.quiver_free_string_array(out_groups[], count)
    return result
end
