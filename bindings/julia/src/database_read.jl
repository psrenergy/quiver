function read_scalar_integers(db::Database, collection::String, attribute::String)
    # ponytail: one extra metadata FFI round-trip per read (cached-schema read, no SQL; struct
    # alloc/free); thread not_null out of the C read API if it ever matters.
    not_null = get_scalar_metadata(db, collection, attribute).not_null
    out_values = Ref{Ptr{Int64}}(C_NULL)
    out_mask = Ref{Ptr{UInt8}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    check(C.quiver_database_read_scalar_integers(db.ptr, collection, attribute, out_values, out_mask, out_count))

    count = out_count[]
    if count == 0 || out_values[] == C_NULL
        return not_null ? Int64[] : Optional{Int64}[]
    end

    values = unsafe_wrap(Array, out_values[], count)
    mask = unsafe_wrap(Array, out_mask[], count)
    result = if not_null
        Int64[values[i] for i in 1:count]
    else
        Optional{Int64}[mask[i] != 0 ? values[i] : nothing for i in 1:count]
    end
    C.quiver_database_free_integer_array(out_values[])
    C.quiver_database_free_mask(out_mask[])
    return result
end

function read_scalar_booleans(db::Database, collection::String, attribute::String)
    values = read_scalar_integers(db, collection, attribute)
    # The delegate's container type carries the schema's nullability, so no second metadata read.
    T = values isa Vector{Int64} ? Bool : Optional{Bool}
    return T[_integer_to_boolean(value, collection, attribute) for value in values]
end

function read_scalar_floats(db::Database, collection::String, attribute::String)
    # ponytail: one extra metadata FFI round-trip per read (cached-schema read, no SQL; struct
    # alloc/free); thread not_null out of the C read API if it ever matters.
    not_null = get_scalar_metadata(db, collection, attribute).not_null
    out_values = Ref{Ptr{Float64}}(C_NULL)
    out_mask = Ref{Ptr{UInt8}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    check(C.quiver_database_read_scalar_floats(db.ptr, collection, attribute, out_values, out_mask, out_count))

    count = out_count[]
    if count == 0 || out_values[] == C_NULL
        return not_null ? Float64[] : Optional{Float64}[]
    end

    values = unsafe_wrap(Array, out_values[], count)
    mask = unsafe_wrap(Array, out_mask[], count)
    result = if not_null
        Float64[values[i] for i in 1:count]
    else
        Optional{Float64}[mask[i] != 0 ? values[i] : nothing for i in 1:count]
    end
    C.quiver_database_free_float_array(out_values[])
    C.quiver_database_free_mask(out_mask[])
    return result
end

function read_scalar_strings(db::Database, collection::String, attribute::String)
    # ponytail: one extra metadata FFI round-trip per read (cached-schema read, no SQL; struct
    # alloc/free); thread not_null out of the C read API if it ever matters.
    not_null = get_scalar_metadata(db, collection, attribute).not_null
    out_values = Ref{Ptr{Ptr{Cchar}}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    check(C.quiver_database_read_scalar_strings(db.ptr, collection, attribute, out_values, out_count))

    count = out_count[]
    if count == 0 || out_values[] == C_NULL
        return not_null ? String[] : Optional{String}[]
    end

    ptrs = unsafe_wrap(Array, out_values[], count)
    result = if not_null
        String[unsafe_string(ptr) for ptr in ptrs]
    else
        Optional{String}[ptr == C_NULL ? nothing : unsafe_string(ptr) for ptr in ptrs]
    end
    C.quiver_database_free_string_array(out_values[], count)
    return result
end

function read_scalar_date_times(db::Database, collection::String, attribute::String)
    values = read_scalar_strings(db, collection, attribute)
    # Preserve the delegate's schema-derived concrete-vs-optional element type.
    T = values isa Vector{String} ? DateTime : Optional{DateTime}
    return T[string_to_date_time(value, collection, attribute) for value in values]
end

# ponytail: one metadata FFI round-trip per read (a list_*_groups call, no SQL) plus a linear scan
# mapping the attribute to its group; thread not_null out of the C read API if it ever matters.
# An unknown attribute falls through as nullable and the read itself raises the core's error.
# Resolves like `Schema::find_{vector,set}_table` (a group named after the attribute wins), so the
# element type describes the very column the core reads.
function _group_value_not_null(groups::Vector{GroupMetadata}, attribute::String)
    for group in sort(groups; by = g -> g.group_name != attribute), column in group.value_columns
        column.name == attribute && return column.not_null
    end
    return false
end

_vector_value_not_null(db::Database, collection::String, attribute::String) =
    _group_value_not_null(list_vector_groups(db, collection), attribute)

_set_value_not_null(db::Database, collection::String, attribute::String) =
    _group_value_not_null(list_set_groups(db, collection), attribute)

# One element's cells, always read through the C presence mask. With a concrete T (a NOT NULL
# column) a masked cell raises instead of passing the C placeholder (0 / 0.0) off as data: the core
# also masks a cell the reader's type cannot hold, and a non-STRICT composite key can hold a NULL.
function _masked_cells(::Type{T}, values::Ptr, mask::Ptr{UInt8}, n::Integer) where {T}
    n == 0 && return T[]
    v = unsafe_wrap(Array, values, n)
    m = unsafe_wrap(Array, mask, n)
    return T[m[j] != 0 ? v[j] : nothing for j in 1:n]
end

function _string_cells(::Type{T}, ptrs::Ptr{Ptr{Cchar}}, n::Integer) where {T}
    n == 0 && return T[]
    return T[ptr == C_NULL ? nothing : unsafe_string(ptr) for ptr in unsafe_wrap(Array, ptrs, n)]
end

# Keeps the strings' schema-derived concrete-vs-optional element type (shared by the by-id
# DateTime wrappers and the composites).
function _to_date_times(values::Vector, collection::String, attribute::String)
    T = values isa Vector{String} ? DateTime : Optional{DateTime}
    return T[string_to_date_time(value, collection, attribute) for value in values]
end

# Every vector/set reader below looks the metadata up only after the C read, so an unknown
# collection reports the reader and not list_vector_groups / list_set_groups, and it frees the C
# arrays in `finally`, since a masked cell in a NOT NULL column raises mid-decode.

function read_vector_integers(db::Database, collection::String, attribute::String)
    out_vectors = Ref{Ptr{Ptr{Int64}}}(C_NULL)
    out_masks = Ref{Ptr{Ptr{UInt8}}}(C_NULL)
    out_sizes = Ref{Ptr{Csize_t}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    check(
        C.quiver_database_read_vector_integers(
            db.ptr, collection, attribute, out_vectors, out_masks, out_sizes, out_count,
        ),
    )

    count = out_count[]
    try
        T = _vector_value_not_null(db, collection, attribute) ? Int64 : Optional{Int64}
        count == 0 && return Vector{T}[]
        vectors = unsafe_wrap(Array, out_vectors[], count)
        masks = unsafe_wrap(Array, out_masks[], count)
        sizes = unsafe_wrap(Array, out_sizes[], count)
        return Vector{T}[_masked_cells(T, vectors[i], masks[i], sizes[i]) for i in 1:count]
    finally
        C.quiver_database_free_integer_vectors(out_vectors[], out_sizes[], count)
        C.quiver_database_free_masks(out_masks[], count)
    end
end

function read_vector_booleans(db::Database, collection::String, attribute::String)
    vectors = read_vector_integers(db, collection, attribute)
    # The delegate's container type carries the schema's nullability, so no second metadata read.
    T = vectors isa Vector{Vector{Int64}} ? Bool : Optional{Bool}
    return Vector{T}[
        T[_integer_to_boolean(value, collection, attribute) for value in values] for values in vectors
    ]
end

function read_vector_floats(db::Database, collection::String, attribute::String)
    out_vectors = Ref{Ptr{Ptr{Cdouble}}}(C_NULL)
    out_masks = Ref{Ptr{Ptr{UInt8}}}(C_NULL)
    out_sizes = Ref{Ptr{Csize_t}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    check(
        C.quiver_database_read_vector_floats(
            db.ptr, collection, attribute, out_vectors, out_masks, out_sizes, out_count,
        ),
    )

    count = out_count[]
    try
        T = _vector_value_not_null(db, collection, attribute) ? Float64 : Optional{Float64}
        count == 0 && return Vector{T}[]
        vectors = unsafe_wrap(Array, out_vectors[], count)
        masks = unsafe_wrap(Array, out_masks[], count)
        sizes = unsafe_wrap(Array, out_sizes[], count)
        return Vector{T}[_masked_cells(T, vectors[i], masks[i], sizes[i]) for i in 1:count]
    finally
        C.quiver_database_free_float_vectors(out_vectors[], out_sizes[], count)
        C.quiver_database_free_masks(out_masks[], count)
    end
end

function read_vector_strings(db::Database, collection::String, attribute::String)
    out_vectors = Ref{Ptr{Ptr{Ptr{Cchar}}}}(C_NULL)
    out_sizes = Ref{Ptr{Csize_t}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    check(C.quiver_database_read_vector_strings(db.ptr, collection, attribute, out_vectors, out_sizes, out_count))

    count = out_count[]
    try
        T = _vector_value_not_null(db, collection, attribute) ? String : Optional{String}
        count == 0 && return Vector{T}[]
        vectors = unsafe_wrap(Array, out_vectors[], count)
        sizes = unsafe_wrap(Array, out_sizes[], count)
        return Vector{T}[_string_cells(T, vectors[i], sizes[i]) for i in 1:count]
    finally
        C.quiver_database_free_string_vectors(out_vectors[], out_sizes[], count)
    end
end

function read_vector_date_times(db::Database, collection::String, attribute::String)
    vectors = read_vector_strings(db, collection, attribute)
    # The delegate's container type carries the schema's nullability, so no second metadata read.
    T = vectors isa Vector{Vector{String}} ? DateTime : Optional{DateTime}
    return Vector{T}[
        T[string_to_date_time(value, collection, attribute) for value in values] for values in vectors
    ]
end

function read_set_integers(db::Database, collection::String, attribute::String)
    out_sets = Ref{Ptr{Ptr{Int64}}}(C_NULL)
    out_masks = Ref{Ptr{Ptr{UInt8}}}(C_NULL)
    out_sizes = Ref{Ptr{Csize_t}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    check(
        C.quiver_database_read_set_integers(
            db.ptr, collection, attribute, out_sets, out_masks, out_sizes, out_count,
        ),
    )

    count = out_count[]
    try
        T = _set_value_not_null(db, collection, attribute) ? Int64 : Optional{Int64}
        count == 0 && return Vector{T}[]
        sets = unsafe_wrap(Array, out_sets[], count)
        masks = unsafe_wrap(Array, out_masks[], count)
        sizes = unsafe_wrap(Array, out_sizes[], count)
        return Vector{T}[_masked_cells(T, sets[i], masks[i], sizes[i]) for i in 1:count]
    finally
        C.quiver_database_free_integer_vectors(out_sets[], out_sizes[], count)
        C.quiver_database_free_masks(out_masks[], count)
    end
end

function read_set_booleans(db::Database, collection::String, attribute::String)
    sets = read_set_integers(db, collection, attribute)
    # The delegate's container type carries the schema's nullability, so no second metadata read.
    T = sets isa Vector{Vector{Int64}} ? Bool : Optional{Bool}
    return Vector{T}[
        T[_integer_to_boolean(value, collection, attribute) for value in values] for values in sets
    ]
end

function read_set_floats(db::Database, collection::String, attribute::String)
    out_sets = Ref{Ptr{Ptr{Cdouble}}}(C_NULL)
    out_masks = Ref{Ptr{Ptr{UInt8}}}(C_NULL)
    out_sizes = Ref{Ptr{Csize_t}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    check(
        C.quiver_database_read_set_floats(
            db.ptr, collection, attribute, out_sets, out_masks, out_sizes, out_count,
        ),
    )

    count = out_count[]
    try
        T = _set_value_not_null(db, collection, attribute) ? Float64 : Optional{Float64}
        count == 0 && return Vector{T}[]
        sets = unsafe_wrap(Array, out_sets[], count)
        masks = unsafe_wrap(Array, out_masks[], count)
        sizes = unsafe_wrap(Array, out_sizes[], count)
        return Vector{T}[_masked_cells(T, sets[i], masks[i], sizes[i]) for i in 1:count]
    finally
        C.quiver_database_free_float_vectors(out_sets[], out_sizes[], count)
        C.quiver_database_free_masks(out_masks[], count)
    end
end

function read_set_strings(db::Database, collection::String, attribute::String)
    out_sets = Ref{Ptr{Ptr{Ptr{Cchar}}}}(C_NULL)
    out_sizes = Ref{Ptr{Csize_t}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    check(C.quiver_database_read_set_strings(db.ptr, collection, attribute, out_sets, out_sizes, out_count))

    count = out_count[]
    try
        T = _set_value_not_null(db, collection, attribute) ? String : Optional{String}
        count == 0 && return Vector{T}[]
        sets = unsafe_wrap(Array, out_sets[], count)
        sizes = unsafe_wrap(Array, out_sizes[], count)
        return Vector{T}[_string_cells(T, sets[i], sizes[i]) for i in 1:count]
    finally
        C.quiver_database_free_string_vectors(out_sets[], out_sizes[], count)
    end
end

function read_set_date_times(db::Database, collection::String, attribute::String)
    sets = read_set_strings(db, collection, attribute)
    # The delegate's container type carries the schema's nullability, so no second metadata read.
    T = sets isa Vector{Vector{String}} ? DateTime : Optional{DateTime}
    return Vector{T}[
        T[string_to_date_time(value, collection, attribute) for value in values] for values in sets
    ]
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

function read_scalar_boolean_by_id(db::Database, collection::String, attribute::String, id::Int64)
    return _integer_to_boolean(read_scalar_integer_by_id(db, collection, attribute, id), collection, attribute)
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
    C.quiver_database_free_string(out_value[])
    return result
end

function read_scalar_date_time_by_id(db::Database, collection::String, attribute::String, id::Int64)
    result = read_scalar_string_by_id(db, collection, attribute, id)
    return string_to_date_time(result, collection, attribute)
end

# The by-id kernels take the column's `not_null`: `nothing` looks it up after the read (the
# public readers); the composites pass the answer they already hold instead of paying another
# list_*_groups round-trip per column, and `false` decodes as Optional without any lookup.

function _read_vector_integers_by_id(
    db::Database,
    collection::String,
    attribute::String,
    id::Int64,
    not_null::Union{Bool, Nothing},
)
    out_values = Ref{Ptr{Int64}}(C_NULL)
    out_mask = Ref{Ptr{UInt8}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    check(
        C.quiver_database_read_vector_integers_by_id(
            db.ptr, collection, attribute, id, out_values, out_mask, out_count,
        ),
    )

    try
        concrete = isnothing(not_null) ? _vector_value_not_null(db, collection, attribute) : not_null
        return _masked_cells(concrete ? Int64 : Optional{Int64}, out_values[], out_mask[], out_count[])
    finally
        C.quiver_database_free_integer_array(out_values[])
        C.quiver_database_free_mask(out_mask[])
    end
end

read_vector_integers_by_id(db::Database, collection::String, attribute::String, id::Int64) =
    _read_vector_integers_by_id(db, collection, attribute, id, nothing)

function read_vector_booleans_by_id(db::Database, collection::String, attribute::String, id::Int64)
    values = read_vector_integers_by_id(db, collection, attribute, id)
    # The delegate's container type carries the schema's nullability, so no second metadata read.
    T = values isa Vector{Int64} ? Bool : Optional{Bool}
    return T[_integer_to_boolean(value, collection, attribute) for value in values]
end

function _read_vector_floats_by_id(
    db::Database,
    collection::String,
    attribute::String,
    id::Int64,
    not_null::Union{Bool, Nothing},
)
    out_values = Ref{Ptr{Float64}}(C_NULL)
    out_mask = Ref{Ptr{UInt8}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    check(
        C.quiver_database_read_vector_floats_by_id(
            db.ptr, collection, attribute, id, out_values, out_mask, out_count,
        ),
    )

    try
        concrete = isnothing(not_null) ? _vector_value_not_null(db, collection, attribute) : not_null
        return _masked_cells(concrete ? Float64 : Optional{Float64}, out_values[], out_mask[], out_count[])
    finally
        C.quiver_database_free_float_array(out_values[])
        C.quiver_database_free_mask(out_mask[])
    end
end

read_vector_floats_by_id(db::Database, collection::String, attribute::String, id::Int64) =
    _read_vector_floats_by_id(db, collection, attribute, id, nothing)

function _read_vector_strings_by_id(
    db::Database,
    collection::String,
    attribute::String,
    id::Int64,
    not_null::Union{Bool, Nothing},
)
    out_values = Ref{Ptr{Ptr{Cchar}}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    check(C.quiver_database_read_vector_strings_by_id(db.ptr, collection, attribute, id, out_values, out_count))

    count = out_count[]
    try
        concrete = isnothing(not_null) ? _vector_value_not_null(db, collection, attribute) : not_null
        return _string_cells(concrete ? String : Optional{String}, out_values[], count)
    finally
        C.quiver_database_free_string_array(out_values[], count)
    end
end

read_vector_strings_by_id(db::Database, collection::String, attribute::String, id::Int64) =
    _read_vector_strings_by_id(db, collection, attribute, id, nothing)

read_vector_date_times_by_id(db::Database, collection::String, attribute::String, id::Int64) =
    _to_date_times(read_vector_strings_by_id(db, collection, attribute, id), collection, attribute)

function _read_set_integers_by_id(
    db::Database,
    collection::String,
    attribute::String,
    id::Int64,
    not_null::Union{Bool, Nothing},
)
    out_values = Ref{Ptr{Int64}}(C_NULL)
    out_mask = Ref{Ptr{UInt8}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    check(
        C.quiver_database_read_set_integers_by_id(
            db.ptr, collection, attribute, id, out_values, out_mask, out_count,
        ),
    )

    try
        concrete = isnothing(not_null) ? _set_value_not_null(db, collection, attribute) : not_null
        return _masked_cells(concrete ? Int64 : Optional{Int64}, out_values[], out_mask[], out_count[])
    finally
        C.quiver_database_free_integer_array(out_values[])
        C.quiver_database_free_mask(out_mask[])
    end
end

read_set_integers_by_id(db::Database, collection::String, attribute::String, id::Int64) =
    _read_set_integers_by_id(db, collection, attribute, id, nothing)

function read_set_booleans_by_id(db::Database, collection::String, attribute::String, id::Int64)
    values = read_set_integers_by_id(db, collection, attribute, id)
    # The delegate's container type carries the schema's nullability, so no second metadata read.
    T = values isa Vector{Int64} ? Bool : Optional{Bool}
    return T[_integer_to_boolean(value, collection, attribute) for value in values]
end

function _read_set_floats_by_id(
    db::Database,
    collection::String,
    attribute::String,
    id::Int64,
    not_null::Union{Bool, Nothing},
)
    out_values = Ref{Ptr{Float64}}(C_NULL)
    out_mask = Ref{Ptr{UInt8}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    check(
        C.quiver_database_read_set_floats_by_id(
            db.ptr, collection, attribute, id, out_values, out_mask, out_count,
        ),
    )

    try
        concrete = isnothing(not_null) ? _set_value_not_null(db, collection, attribute) : not_null
        return _masked_cells(concrete ? Float64 : Optional{Float64}, out_values[], out_mask[], out_count[])
    finally
        C.quiver_database_free_float_array(out_values[])
        C.quiver_database_free_mask(out_mask[])
    end
end

read_set_floats_by_id(db::Database, collection::String, attribute::String, id::Int64) =
    _read_set_floats_by_id(db, collection, attribute, id, nothing)

function _read_set_strings_by_id(
    db::Database,
    collection::String,
    attribute::String,
    id::Int64,
    not_null::Union{Bool, Nothing},
)
    out_values = Ref{Ptr{Ptr{Cchar}}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    check(C.quiver_database_read_set_strings_by_id(db.ptr, collection, attribute, id, out_values, out_count))

    count = out_count[]
    try
        concrete = isnothing(not_null) ? _set_value_not_null(db, collection, attribute) : not_null
        return _string_cells(concrete ? String : Optional{String}, out_values[], count)
    finally
        C.quiver_database_free_string_array(out_values[], count)
    end
end

read_set_strings_by_id(db::Database, collection::String, attribute::String, id::Int64) =
    _read_set_strings_by_id(db, collection, attribute, id, nothing)

read_set_date_times_by_id(db::Database, collection::String, attribute::String, id::Int64) =
    _to_date_times(read_set_strings_by_id(db, collection, attribute, id), collection, attribute)

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

"""
    number_of_elements(db::Database, collection::AbstractString) -> Int64

Return the current number of elements in `collection`.
"""
function number_of_elements(db::Database, collection::AbstractString)
    out_count = Ref{Int64}(0)
    check(C.quiver_database_number_of_elements(db.ptr, collection, out_count))
    return out_count[]
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
    groups = list_vector_groups(db, collection)
    for group in groups
        for col in group.value_columns
            name = col.name
            # Resolved from the groups already in hand, the same way the per-column read resolves
            # `name`, instead of one more list_vector_groups call per column.
            not_null = _group_value_not_null(groups, name)
            if col.data_type == C.QUIVER_DATA_TYPE_INTEGER
                result[name] = _read_vector_integers_by_id(db, collection, name, id, not_null)
            elseif col.data_type == C.QUIVER_DATA_TYPE_FLOAT
                result[name] = _read_vector_floats_by_id(db, collection, name, id, not_null)
            elseif col.data_type == C.QUIVER_DATA_TYPE_STRING
                result[name] = _read_vector_strings_by_id(db, collection, name, id, not_null)
            elseif col.data_type == C.QUIVER_DATA_TYPE_DATE_TIME
                strings = _read_vector_strings_by_id(db, collection, name, id, not_null)
                result[name] = _to_date_times(strings, collection, name)
            else
                throw(ArgumentError("Unsupported vector data type $(col.data_type) for '$collection.$name'"))
            end
        end
    end
    return result
end

function read_sets_by_id(db::Database, collection::String, id::Int64)
    result = Dict{String, Any}()
    groups = list_set_groups(db, collection)
    for group in groups
        for col in group.value_columns
            name = col.name
            # Resolved from the groups already in hand, the same way the per-column read resolves
            # `name`, instead of one more list_set_groups call per column.
            not_null = _group_value_not_null(groups, name)
            if col.data_type == C.QUIVER_DATA_TYPE_INTEGER
                result[name] = _read_set_integers_by_id(db, collection, name, id, not_null)
            elseif col.data_type == C.QUIVER_DATA_TYPE_FLOAT
                result[name] = _read_set_floats_by_id(db, collection, name, id, not_null)
            elseif col.data_type == C.QUIVER_DATA_TYPE_STRING
                result[name] = _read_set_strings_by_id(db, collection, name, id, not_null)
            elseif col.data_type == C.QUIVER_DATA_TYPE_DATE_TIME
                strings = _read_set_strings_by_id(db, collection, name, id, not_null)
                result[name] = _to_date_times(strings, collection, name)
            else
                throw(ArgumentError("Unsupported set data type $(col.data_type) for '$collection.$name'"))
            end
        end
    end
    return result
end

function read_vector_group_by_id(db::Database, collection::String, group::String, id::Int64)
    return _read_group_rows(db, C.quiver_database_read_vector_group_by_id, collection, group, id)
end

function read_set_group_by_id(db::Database, collection::String, group::String, id::Int64)
    return _read_group_rows(db, C.quiver_database_read_set_group_by_id, collection, group, id)
end

# Shared by the two whole-group readers; `read_group` is the C entry point (the
# `_update_group_columns` convention). The native reader runs one SELECT over the named group's own
# table, so a column name another group shares cannot pull that group's rows in, and every column
# comes from one snapshot: a masked cell is `nothing`, a DATE_TIME column is parsed.
# read_time_series_group keeps its own decode: it returns columns and parses only the dimension column.
function _read_group_rows(db::Database, read_group::Function, collection::String, group::String, id::Int64)
    out_col_names = Ref{Ptr{Ptr{Cchar}}}(C_NULL)
    out_col_types = Ref{Ptr{Cint}}(C_NULL)
    out_col_data = Ref{Ptr{Ptr{Cvoid}}}(C_NULL)
    out_col_has_value = Ref{Ptr{Ptr{UInt8}}}(C_NULL)
    out_col_count = Ref{Csize_t}(0)
    out_row_count = Ref{Csize_t}(0)

    check(
        read_group(
            db.ptr, collection, group, id,
            out_col_names, out_col_types, out_col_data, out_col_has_value, out_col_count, out_row_count,
        ),
    )

    col_count = out_col_count[]
    row_count = out_row_count[]
    if col_count == 0 || row_count == 0
        return Dict{String, Any}[]
    end

    try
        name_ptrs = unsafe_wrap(Array, out_col_names[], col_count)
        type_vals = unsafe_wrap(Array, out_col_types[], col_count)
        data_ptrs = unsafe_wrap(Array, out_col_data[], col_count)
        mask_ptrs = unsafe_wrap(Array, out_col_has_value[], col_count)

        rows = [Dict{String, Any}() for _ in 1:row_count]
        for c in 1:col_count
            name = unsafe_string(name_ptrs[c])
            col_type = type_vals[c]
            mask = unsafe_wrap(Array, mask_ptrs[c], row_count)
            for r in 1:row_count
                if mask[r] == 0
                    rows[r][name] = nothing
                elseif col_type == Cint(C.QUIVER_DATA_TYPE_INTEGER)
                    rows[r][name] = unsafe_load(reinterpret(Ptr{Int64}, data_ptrs[c]), r)
                elseif col_type == Cint(C.QUIVER_DATA_TYPE_FLOAT)
                    rows[r][name] = unsafe_load(reinterpret(Ptr{Float64}, data_ptrs[c]), r)
                else
                    # STRING or DATE_TIME. The mask test above keeps a NULL char* out of unsafe_string.
                    s = unsafe_string(unsafe_load(reinterpret(Ptr{Ptr{Cchar}}, data_ptrs[c]), r))
                    rows[r][name] =
                        col_type == Cint(C.QUIVER_DATA_TYPE_DATE_TIME) ? string_to_date_time(s, collection, name) : s
                end
            end
        end
        return rows
    finally
        C.quiver_database_free_time_series_data(
            out_col_names[], out_col_types[], out_col_data[], out_col_has_value[],
            Csize_t(col_count), Csize_t(row_count),
        )
    end
end

function read_time_series_group(db::Database, collection::String, group::String, id::Int64)
    out_col_names = Ref{Ptr{Ptr{Cchar}}}(C_NULL)
    out_col_types = Ref{Ptr{Cint}}(C_NULL)
    out_col_data = Ref{Ptr{Ptr{Cvoid}}}(C_NULL)
    out_col_has_value = Ref{Ptr{Ptr{UInt8}}}(C_NULL)
    out_col_count = Ref{Csize_t}(0)
    out_row_count = Ref{Csize_t}(0)

    check(
        C.quiver_database_read_time_series_group(
            db.ptr, collection, group, id,
            out_col_names, out_col_types, out_col_data, out_col_has_value, out_col_count, out_row_count,
        ),
    )

    col_count = out_col_count[]
    row_count = out_row_count[]

    if col_count == 0 || row_count == 0
        return Dict{String, Vector}()
    end

    # Free in `finally`: the metadata lookup, the DateTime parse of a malformed dimension cell and
    # the unsupported-type branch can all throw while the C buffers are held. The decode copies
    # everything out (unsafe_string, fresh Vectors) before the free runs.
    try
        # Get dimension column name for DateTime parsing
        metadata = get_time_series_metadata(db, collection, group)
        dim_col = metadata.dimension_column

        # Unmarshal column names, types, data, and per-cell NULL masks
        name_ptrs = unsafe_wrap(Array, out_col_names[], col_count)
        type_vals = unsafe_wrap(Array, out_col_types[], col_count)
        data_ptrs = unsafe_wrap(Array, out_col_data[], col_count)
        mask_ptrs = unsafe_wrap(Array, out_col_has_value[], col_count)

        # Value columns are typed Optional{T}: mask[r] == 0 surfaces as `nothing`. The
        # dimension column's mask is always all 1, so it stays a dense Vector{DateTime}.
        result = Dict{String, Vector}()
        for i in 1:col_count
            col_name = unsafe_string(name_ptrs[i])
            col_type = type_vals[i]
            mask = unsafe_wrap(Array, mask_ptrs[i], row_count)

            if col_type == Cint(C.QUIVER_DATA_TYPE_INTEGER)
                int_arr = unsafe_wrap(Array, reinterpret(Ptr{Int64}, data_ptrs[i]), row_count)
                result[col_name] = Optional{Int64}[mask[r] != 0 ? int_arr[r] : nothing for r in 1:row_count]
            elseif col_type == Cint(C.QUIVER_DATA_TYPE_FLOAT)
                float_arr = unsafe_wrap(Array, reinterpret(Ptr{Float64}, data_ptrs[i]), row_count)
                result[col_name] = Optional{Float64}[mask[r] != 0 ? float_arr[r] : nothing for r in 1:row_count]
            elseif col_type == Cint(C.QUIVER_DATA_TYPE_STRING) || col_type == Cint(C.QUIVER_DATA_TYPE_DATE_TIME)
                str_ptr_ptr = reinterpret(Ptr{Ptr{Cchar}}, data_ptrs[i])
                str_ptrs = unsafe_wrap(Array, str_ptr_ptr, row_count)
                if col_name == dim_col
                    result[col_name] =
                        DateTime[string_to_date_time(unsafe_string(p), collection, col_name) for p in str_ptrs]
                else
                    # Never unsafe_string a masked-out (NULL) pointer.
                    result[col_name] =
                        Optional{String}[mask[r] != 0 ? unsafe_string(str_ptrs[r]) : nothing for r in 1:row_count]
                end
            else
                throw(ArgumentError("Unsupported data type $(col_type) for column '$col_name'"))
            end
        end
        return result
    finally
        C.quiver_database_free_time_series_data(
            out_col_names[], out_col_types[], out_col_data[], out_col_has_value[],
            Csize_t(col_count), Csize_t(row_count),
        )
    end
end

function read_time_series_row(db::Database, collection::String, group::String, attribute::String; date_time::DateTime)
    out_data_type = Ref{Cint}(0)
    out_values = Ref{Ptr{Cvoid}}(C_NULL)
    out_mask = Ref{Ptr{UInt8}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    dt_str = date_time_to_string(date_time)

    check(
        C.quiver_database_read_time_series_row(
            db.ptr, collection, group, attribute, dt_str,
            out_data_type, out_values, out_mask, out_count,
        ),
    )

    count = out_count[]
    data_type = out_data_type[]

    # Always Vector{Optional{T}} with T keyed on the column's data type, empty or not: a `nothing`
    # (mask 0) means "no data at or before date_time", so the optional is inherent to this reader.
    if data_type == Cint(C.QUIVER_DATA_TYPE_INTEGER)
        count == 0 && return Optional{Int64}[]
        int_ptr = reinterpret(Ptr{Int64}, out_values[])
        values = unsafe_wrap(Array, int_ptr, count)
        mask = unsafe_wrap(Array, out_mask[], count)
        result = Optional{Int64}[mask[i] != 0 ? values[i] : nothing for i in 1:count]
        C.quiver_database_free_integer_array(int_ptr)
        C.quiver_database_free_mask(out_mask[])
        return result
    elseif data_type == Cint(C.QUIVER_DATA_TYPE_FLOAT)
        count == 0 && return Optional{Float64}[]
        float_ptr = reinterpret(Ptr{Float64}, out_values[])
        values = unsafe_wrap(Array, float_ptr, count)
        mask = unsafe_wrap(Array, out_mask[], count)
        result = Optional{Float64}[mask[i] != 0 ? values[i] : nothing for i in 1:count]
        C.quiver_database_free_float_array(float_ptr)
        C.quiver_database_free_mask(out_mask[])
        return result
    elseif data_type == Cint(C.QUIVER_DATA_TYPE_STRING) || data_type == Cint(C.QUIVER_DATA_TYPE_DATE_TIME)
        count == 0 && return Optional{String}[]
        str_ptr_ptr = reinterpret(Ptr{Ptr{Cchar}}, out_values[])
        str_ptrs = unsafe_wrap(Array, str_ptr_ptr, count)
        mask = unsafe_wrap(Array, out_mask[], count)
        # Never unsafe_string a masked-out (NULL) pointer.
        result = Optional{String}[mask[i] != 0 ? unsafe_string(str_ptrs[i]) : nothing for i in 1:count]
        C.quiver_database_free_string_array(str_ptr_ptr, Csize_t(count))
        C.quiver_database_free_mask(out_mask[])
        return result
    end

    return throw(ArgumentError("Unsupported data type $(data_type) for attribute '$attribute'"))
end

function read_time_series_files(db::Database, collection::String)
    out_columns = Ref{Ptr{Ptr{Cchar}}}(C_NULL)
    out_paths = Ref{Ptr{Ptr{Cchar}}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    check(C.quiver_database_read_time_series_files(db.ptr, collection, out_columns, out_paths, out_count))

    count = out_count[]
    if count == 0 || out_columns[] == C_NULL
        return Dict{String, Optional{String}}()
    end

    column_ptrs = unsafe_wrap(Array, out_columns[], count)
    path_ptrs = unsafe_wrap(Array, out_paths[], count)

    result = Dict{String, Optional{String}}()
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
    return merge(scalars, vectors, sets)
end
