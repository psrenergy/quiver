mutable struct Database
    ptr::Ptr{C.psr_database}

    function Database(ptr::Ptr{C.psr_database})
        db = new(ptr)
    finalizer(d -> d.ptr != C_NULL && C.psr_database_close(d.ptr), db)
    return db
    end
end

function from_schema(db_path, schema_path)
    options = Ref(C.psr_database_options_t(0, C.PSR_LOG_DEBUG))
    ptr = C.psr_database_from_schema(db_path, schema_path, options)
    if ptr == C_NULL
        throw(DatabaseException("Failed to create database from schema '$schema_path'"))
    end
    return Database(ptr)
end

function from_migrations(db_path, migrations_path)
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
    try
        create_element!(db, collection, e)
    finally
        destroy!(e)
    end
    return nothing
end

function close!(db::Database)
    if db.ptr != C_NULL
        C.psr_database_close(db.ptr)
        db.ptr = C_NULL
    end
    return nothing
end

function set_scalar_relation!(
    db::Database,
    collection::String,
    attribute::String,
    from_label::String,
    to_label::String,
)
    err = C.psr_database_set_scalar_relation(db.ptr, collection, attribute, from_label, to_label)
    if err != C.PSR_OK
        throw(DatabaseException("Failed to set scalar relation '$attribute' in '$collection'"))
    end
    return nothing
end

function read_scalar_relation(db::Database, collection::String, attribute::String)
    out_values = Ref{Ptr{Ptr{Cchar}}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    err = C.psr_database_read_scalar_relation(db.ptr, collection, attribute, out_values, out_count)
    if err != C.PSR_OK
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
    C.psr_free_string_array(out_values[], count)
    return result
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

# Read scalar by ID functions

function read_scalar_integers_by_id(db::Database, collection::String, attribute::String, id::Int64)
    out_value = Ref{Int64}(0)
    out_has_value = Ref{Cint}(0)

    err = C.psr_database_read_scalar_integers_by_id(db.ptr, collection, attribute, id, out_value, out_has_value)
    if err != C.PSR_OK
        throw(DatabaseException("Failed to read scalar integer by id from '$collection.$attribute'"))
    end

    if out_has_value[] == 0
        return nothing
    end
    return out_value[]
end

function read_scalar_doubles_by_id(db::Database, collection::String, attribute::String, id::Int64)
    out_value = Ref{Float64}(0.0)
    out_has_value = Ref{Cint}(0)

    err = C.psr_database_read_scalar_doubles_by_id(db.ptr, collection, attribute, id, out_value, out_has_value)
    if err != C.PSR_OK
        throw(DatabaseException("Failed to read scalar double by id from '$collection.$attribute'"))
    end

    if out_has_value[] == 0
        return nothing
    end
    return out_value[]
end

function read_scalar_strings_by_id(db::Database, collection::String, attribute::String, id::Int64)
    out_value = Ref{Ptr{Cchar}}(C_NULL)
    out_has_value = Ref{Cint}(0)

    err = C.psr_database_read_scalar_strings_by_id(db.ptr, collection, attribute, id, out_value, out_has_value)
    if err != C.PSR_OK
        throw(DatabaseException("Failed to read scalar string by id from '$collection.$attribute'"))
    end

    if out_has_value[] == 0 || out_value[] == C_NULL
        return nothing
    end
    result = unsafe_string(out_value[])
    C.psr_string_free(out_value[])
    return result
end

# Read vector by ID functions

function read_vector_integers_by_id(db::Database, collection::String, attribute::String, id::Int64)
    out_values = Ref{Ptr{Int64}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    err = C.psr_database_read_vector_integers_by_id(db.ptr, collection, attribute, id, out_values, out_count)
    if err != C.PSR_OK
        throw(DatabaseException("Failed to read vector integers by id from '$collection.$attribute'"))
    end

    count = out_count[]
    if count == 0 || out_values[] == C_NULL
        return Int64[]
    end

    result = unsafe_wrap(Array, out_values[], count) |> copy
    C.psr_free_int_array(out_values[])
    return result
end

function read_vector_doubles_by_id(db::Database, collection::String, attribute::String, id::Int64)
    out_values = Ref{Ptr{Float64}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    err = C.psr_database_read_vector_doubles_by_id(db.ptr, collection, attribute, id, out_values, out_count)
    if err != C.PSR_OK
        throw(DatabaseException("Failed to read vector doubles by id from '$collection.$attribute'"))
    end

    count = out_count[]
    if count == 0 || out_values[] == C_NULL
        return Float64[]
    end

    result = unsafe_wrap(Array, out_values[], count) |> copy
    C.psr_free_double_array(out_values[])
    return result
end

function read_vector_strings_by_id(db::Database, collection::String, attribute::String, id::Int64)
    out_values = Ref{Ptr{Ptr{Cchar}}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    err = C.psr_database_read_vector_strings_by_id(db.ptr, collection, attribute, id, out_values, out_count)
    if err != C.PSR_OK
        throw(DatabaseException("Failed to read vector strings by id from '$collection.$attribute'"))
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

# Read set by ID functions

function read_set_integers_by_id(db::Database, collection::String, attribute::String, id::Int64)
    out_values = Ref{Ptr{Int64}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    err = C.psr_database_read_set_integers_by_id(db.ptr, collection, attribute, id, out_values, out_count)
    if err != C.PSR_OK
        throw(DatabaseException("Failed to read set integers by id from '$collection.$attribute'"))
    end

    count = out_count[]
    if count == 0 || out_values[] == C_NULL
        return Int64[]
    end

    result = unsafe_wrap(Array, out_values[], count) |> copy
    C.psr_free_int_array(out_values[])
    return result
end

function read_set_doubles_by_id(db::Database, collection::String, attribute::String, id::Int64)
    out_values = Ref{Ptr{Float64}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    err = C.psr_database_read_set_doubles_by_id(db.ptr, collection, attribute, id, out_values, out_count)
    if err != C.PSR_OK
        throw(DatabaseException("Failed to read set doubles by id from '$collection.$attribute'"))
    end

    count = out_count[]
    if count == 0 || out_values[] == C_NULL
        return Float64[]
    end

    result = unsafe_wrap(Array, out_values[], count) |> copy
    C.psr_free_double_array(out_values[])
    return result
end

function read_set_strings_by_id(db::Database, collection::String, attribute::String, id::Int64)
    out_values = Ref{Ptr{Ptr{Cchar}}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    err = C.psr_database_read_set_strings_by_id(db.ptr, collection, attribute, id, out_values, out_count)
    if err != C.PSR_OK
        throw(DatabaseException("Failed to read set strings by id from '$collection.$attribute'"))
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

function read_element_ids(db::Database, collection::String)
    out_ids = Ref{Ptr{Int64}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    err = C.psr_database_read_element_ids(db.ptr, collection, out_ids, out_count)
    if err != C.PSR_OK
        throw(DatabaseException("Failed to read element ids from '$collection'"))
    end

    count = out_count[]
    if count == 0 || out_ids[] == C_NULL
        return Int64[]
    end

    result = unsafe_wrap(Array, out_ids[], count) |> copy
    C.psr_free_int_array(out_ids[])
    return result
end

function get_attribute_type(db::Database, collection::String, attribute::String)
    out_data_structure = Ref{C.psr_data_structure_t}(C.PSR_DATA_STRUCTURE_SCALAR)
    out_data_type = Ref{C.psr_data_type_t}(C.PSR_DATA_TYPE_INTEGER)

    err = C.psr_database_get_attribute_type(db.ptr, collection, attribute, out_data_structure, out_data_type)
    if err != C.PSR_OK
        throw(DatabaseException("Failed to get attribute type for '$collection.$attribute'"))
    end

    return (data_structure = out_data_structure[], data_type = out_data_type[])
end

function delete_element_by_id!(db::Database, collection::String, id::Int64)
    err = C.psr_database_delete_element_by_id(db.ptr, collection, id)
    if err != C.PSR_OK
        throw(DatabaseException("Failed to delete element $id from '$collection'"))
    end
    return nothing
end

function update_element!(db::Database, collection::String, id::Int64, e::Element)
    err = C.psr_database_update_element(db.ptr, collection, id, e.ptr)
    if err != C.PSR_OK
        throw(DatabaseException("Failed to update element $id in collection $collection"))
    end
    return nothing
end

function update_element!(db::Database, collection::String, id::Int64; kwargs...)
    e = Element()
    for (k, v) in kwargs
        e[String(k)] = v
    end
    try
        update_element!(db, collection, id, e)
    finally
        destroy!(e)
    end
    return nothing
end

# Update scalar attribute functions

function update_scalar_integer!(db::Database, collection::String, attribute::String, id::Int64, value::Integer)
    err = C.psr_database_update_scalar_integer(db.ptr, collection, attribute, id, Int64(value))
    if err != C.PSR_OK
        throw(DatabaseException("Failed to update scalar integer '$collection.$attribute' for id $id"))
    end
    return nothing
end

function update_scalar_double!(db::Database, collection::String, attribute::String, id::Int64, value::Real)
    err = C.psr_database_update_scalar_double(db.ptr, collection, attribute, id, Float64(value))
    if err != C.PSR_OK
        throw(DatabaseException("Failed to update scalar double '$collection.$attribute' for id $id"))
    end
    return nothing
end

function update_scalar_string!(db::Database, collection::String, attribute::String, id::Int64, value::String)
    err = C.psr_database_update_scalar_string(db.ptr, collection, attribute, id, value)
    if err != C.PSR_OK
        throw(DatabaseException("Failed to update scalar string '$collection.$attribute' for id $id"))
    end
    return nothing
end

# Update vector attribute functions

function update_vector_integers!(db::Database, collection::String, attribute::String, id::Int64, values::Vector{<:Integer})
    int_values = Int64[Int64(v) for v in values]
    err = C.psr_database_update_vector_integers(db.ptr, collection, attribute, id, int_values, Csize_t(length(int_values)))
    if err != C.PSR_OK
        throw(DatabaseException("Failed to update vector integers '$collection.$attribute' for id $id"))
    end
    return nothing
end

function update_vector_doubles!(db::Database, collection::String, attribute::String, id::Int64, values::Vector{<:Real})
    float_values = Float64[Float64(v) for v in values]
    err = C.psr_database_update_vector_doubles(db.ptr, collection, attribute, id, float_values, Csize_t(length(float_values)))
    if err != C.PSR_OK
        throw(DatabaseException("Failed to update vector doubles '$collection.$attribute' for id $id"))
    end
    return nothing
end

function update_vector_strings!(db::Database, collection::String, attribute::String, id::Int64, values::Vector{<:AbstractString})
    cstrings = [Base.cconvert(Cstring, s) for s in values]
    ptrs = [Base.unsafe_convert(Cstring, cs) for cs in cstrings]
    GC.@preserve cstrings begin
        err = C.psr_database_update_vector_strings(db.ptr, collection, attribute, id, ptrs, Csize_t(length(values)))
    end
    if err != C.PSR_OK
        throw(DatabaseException("Failed to update vector strings '$collection.$attribute' for id $id"))
    end
    return nothing
end

# Update set attribute functions

function update_set_integers!(db::Database, collection::String, attribute::String, id::Int64, values::Vector{<:Integer})
    int_values = Int64[Int64(v) for v in values]
    err = C.psr_database_update_set_integers(db.ptr, collection, attribute, id, int_values, Csize_t(length(int_values)))
    if err != C.PSR_OK
        throw(DatabaseException("Failed to update set integers '$collection.$attribute' for id $id"))
    end
    return nothing
end

function update_set_doubles!(db::Database, collection::String, attribute::String, id::Int64, values::Vector{<:Real})
    float_values = Float64[Float64(v) for v in values]
    err = C.psr_database_update_set_doubles(db.ptr, collection, attribute, id, float_values, Csize_t(length(float_values)))
    if err != C.PSR_OK
        throw(DatabaseException("Failed to update set doubles '$collection.$attribute' for id $id"))
    end
    return nothing
end

function update_set_strings!(db::Database, collection::String, attribute::String, id::Int64, values::Vector{<:AbstractString})
    cstrings = [Base.cconvert(Cstring, s) for s in values]
    ptrs = [Base.unsafe_convert(Cstring, cs) for cs in cstrings]
    GC.@preserve cstrings begin
        err = C.psr_database_update_set_strings(db.ptr, collection, attribute, id, ptrs, Csize_t(length(values)))
    end
    if err != C.PSR_OK
        throw(DatabaseException("Failed to update set strings '$collection.$attribute' for id $id"))
    end
    return nothing
end
