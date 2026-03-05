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
        finalizer(m -> m.ptr != C_NULL && C.quiver_blob_metadata_destroy(m.ptr), md)
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
    dimensions::Vector{Pair{String, Int64}} = Pair{String, Int64}[],
    time_dimensions::Vector{Tuple{String, Int64, String}} = Tuple{String, Int64, String}[],
)
    out_md = Ref{Ptr{C.quiver_blob_metadata}}(C_NULL)
    check(C.quiver_blob_metadata_create(out_md))
    md = BlobMetadata(out_md[])

    if !isempty(version)
        check(C.quiver_blob_metadata_set_version(md.ptr, version))
    end
    if !isempty(initial_datetime)
        check(C.quiver_blob_metadata_set_initial_datetime(md.ptr, initial_datetime))
    end
    if !isempty(unit)
        check(C.quiver_blob_metadata_set_unit(md.ptr, unit))
    end
    if !isempty(labels)
        c_labels = [pointer(l) for l in labels]
        GC.@preserve labels begin
            check(C.quiver_blob_metadata_set_labels(md.ptr, c_labels, length(labels)))
        end
    end
    for (name, size) in dimensions
        check(C.quiver_blob_metadata_add_dimension(md.ptr, name, size))
    end
    for (name, size, frequency) in time_dimensions
        check(C.quiver_blob_metadata_add_time_dimension(md.ptr, name, size, frequency))
    end

    return md
end

function blob_metadata_from_toml(toml::AbstractString)
    out_md = Ref{Ptr{C.quiver_blob_metadata}}(C_NULL)
    check(C.quiver_blob_metadata_from_toml(toml, out_md))
    return BlobMetadata(out_md[])
end

function blob_metadata_from_element(el::Element)
    out_md = Ref{Ptr{C.quiver_blob_metadata}}(C_NULL)
    check(C.quiver_blob_metadata_from_element(el.ptr, out_md))
    return BlobMetadata(out_md[])
end

function destroy!(md::BlobMetadata)
    if md.ptr != C_NULL
        C.quiver_blob_metadata_destroy(md.ptr)
        md.ptr = C_NULL
    end
    return nothing
end

function get_unit(md::BlobMetadata)
    out = Ref{Ptr{Cchar}}(C_NULL)
    check(C.quiver_blob_metadata_get_unit(md.ptr, out))
    return unsafe_string(out[])
end

function get_version(md::BlobMetadata)
    out = Ref{Ptr{Cchar}}(C_NULL)
    check(C.quiver_blob_metadata_get_version(md.ptr, out))
    return unsafe_string(out[])
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
