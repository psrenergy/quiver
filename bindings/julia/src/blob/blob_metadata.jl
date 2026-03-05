struct BlobDimension
    name::String
    size::Int64
    is_time_dimension::Bool
    frequency::Union{String, Nothing}
    initial_value::Union{Int64, Nothing}
    parent_dimension_index::Union{Int64, Nothing}
end

mutable struct BlobMetadata
    ptr::Ptr{C.quiver_blob_metadata}

    function BlobMetadata(ptr::Ptr{C.quiver_blob_metadata})
        md = new(ptr)
        finalizer(m -> m.ptr != C_NULL && C.quiver_blob_metadata_free(m.ptr), md)
        return md
    end
end

const FREQUENCY_MAP = Dict(
    C.QUIVER_TIME_FREQUENCY_YEARLY => "yearly",
    C.QUIVER_TIME_FREQUENCY_MONTHLY => "monthly",
    C.QUIVER_TIME_FREQUENCY_WEEKLY => "weekly",
    C.QUIVER_TIME_FREQUENCY_DAILY => "daily",
    C.QUIVER_TIME_FREQUENCY_HOURLY => "hourly",
)

function BlobMetadata(;
    initial_datetime::String = "",
    unit::String = "",
    version::String = "1",
    labels::Vector{String} = String[],
    dimensions::Vector{String} = String[],
    dimension_sizes::Vector{Int64} = Int64[],
    time_dimensions::Vector{String} = String[],
    frequencies::Vector{String} = String[],
)
    el = Element()
    el["version"] = version
    el["initial_datetime"] = initial_datetime
    el["unit"] = unit
    el["labels"] = labels
    el["dimensions"] = dimensions
    el["dimension_sizes"] = dimension_sizes
    el["time_dimensions"] = time_dimensions
    el["frequencies"] = frequencies
    return from_element(el)
end

function from_toml(toml::String)
    out_md = Ref{Ptr{C.quiver_blob_metadata}}(C_NULL)
    check(C.quiver_blob_metadata_from_toml(toml, out_md))
    return BlobMetadata(out_md[])
end

function from_element(el::Element)
    out_md = Ref{Ptr{C.quiver_blob_metadata}}(C_NULL)
    check(C.quiver_blob_metadata_from_element(el.ptr, out_md))
    return BlobMetadata(out_md[])
end

function get_unit(md::BlobMetadata)
    out = Ref{Ptr{Cchar}}(C_NULL)
    check(C.quiver_blob_metadata_get_unit(md.ptr, out))
    result = unsafe_string(out[])
    C.quiver_blob_metadata_free_string(out[])
    return result
end

function get_version(md::BlobMetadata)
    out = Ref{Ptr{Cchar}}(C_NULL)
    check(C.quiver_blob_metadata_get_version(md.ptr, out))
    result = unsafe_string(out[])
    C.quiver_blob_metadata_free_string(out[])
    return result
end

function get_initial_datetime(md::BlobMetadata)
    out = Ref{Ptr{Cchar}}(C_NULL)
    check(C.quiver_blob_metadata_get_initial_datetime(md.ptr, out))
    result = unsafe_string(out[])
    C.quiver_blob_metadata_free_string(out[])
    return result
end

function get_labels(md::BlobMetadata)
    out = Ref{Ptr{Ptr{Cchar}}}(C_NULL)
    out_count = Ref{Csize_t}(0)
    check(C.quiver_blob_metadata_get_labels(md.ptr, out, out_count))

    count = out_count[]
    if count == 0 || out[] == C_NULL
        return String[]
    end

    ptrs = unsafe_wrap(Array, out[], count)
    result = [unsafe_string(ptr) for ptr in ptrs]
    C.quiver_blob_metadata_free_string_array(out[], count)
    return result
end

function get_dimensions(md::BlobMetadata)
    out_count = Ref{Csize_t}(0)
    check(C.quiver_blob_metadata_get_dimension_count(md.ptr, out_count))
    count = out_count[]

    dims = BlobDimension[]
    for i in 0:(count-1)
        dim_ref = Ref(C.quiver_dimension_t(
            C_NULL, 0, 0,
            C.quiver_time_properties_t(C.QUIVER_TIME_FREQUENCY_YEARLY, 0, 0),
        ))
        check(C.quiver_blob_metadata_get_dimension(md.ptr, i, dim_ref))
        dim = dim_ref[]

        is_time = dim.is_time_dimension != 0
        freq = is_time ? get(FREQUENCY_MAP, dim.time_properties.frequency, "unknown") : nothing
        init_val = is_time ? dim.time_properties.initial_value : nothing
        parent_idx = is_time ? dim.time_properties.parent_dimension_index : nothing

        name = unsafe_string(dim.name)
        C.quiver_blob_metadata_free_dimension(dim_ref)

        push!(dims, BlobDimension(name, dim.size, is_time, freq, init_val, parent_idx))
    end

    return dims
end

function get_number_of_time_dimensions(md::BlobMetadata)
    out = Ref{Int64}(0)
    check(C.quiver_blob_metadata_get_number_of_time_dimensions(md.ptr, out))
    return out[]
end

function to_toml(md::BlobMetadata)
    out = Ref{Ptr{Cchar}}(C_NULL)
    check(C.quiver_blob_metadata_to_toml(md.ptr, out))
    result = unsafe_string(out[])
    C.quiver_blob_metadata_free_string(out[])
    return result
end
