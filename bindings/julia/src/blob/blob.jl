mutable struct Blob
    ptr::Ptr{C.quiver_blob}

    function Blob(ptr::Ptr{C.quiver_blob})
        b = new(ptr)
        finalizer(x -> x.ptr != C_NULL && C.quiver_blob_close(x.ptr), b)
        return b
    end
end

function open_blob(path::AbstractString, mode::Symbol; metadata::Union{BlobMetadata, Nothing} = nothing)
    out_blob = Ref{Ptr{C.quiver_blob}}(C_NULL)
    if mode == :read
        check(C.quiver_blob_open_read(path, out_blob))
    elseif mode == :write
        if metadata === nothing
            throw(ArgumentError("metadata is required for write mode"))
        end
        check(C.quiver_blob_open_write(path, metadata.ptr, out_blob))
    else
        throw(ArgumentError("mode must be :read or :write, got :$mode"))
    end
    return Blob(out_blob[])
end

function close!(blob::Blob)
    if blob.ptr != C_NULL
        C.quiver_blob_close(blob.ptr)
        blob.ptr = C_NULL
    end
    return nothing
end

function blob_read(blob::Blob, dims::Dict{String, Int64}; allow_nulls::Bool = false)
    dim_names_str = collect(keys(dims))
    dim_values = Int64[dims[k] for k in dim_names_str]
    c_dim_names = [pointer(s) for s in dim_names_str]

    out_data = Ref{Ptr{Cdouble}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    GC.@preserve dim_names_str begin
        check(C.quiver_blob_read(
            blob.ptr, c_dim_names, dim_values,
            length(dims), allow_nulls ? Cint(1) : Cint(0),
            out_data, out_count,
        ))
    end

    count = out_count[]
    result = [unsafe_load(out_data[], i) for i in 1:count]
    C.quiver_blob_free_float_array(out_data[])
    return result
end

function blob_write!(blob::Blob, dims::Dict{String, Int64}, data::Vector{Float64})
    dim_names_str = collect(keys(dims))
    dim_values = Int64[dims[k] for k in dim_names_str]
    c_dim_names = [pointer(s) for s in dim_names_str]

    GC.@preserve dim_names_str begin
        check(C.quiver_blob_write(
            blob.ptr, c_dim_names, dim_values,
            length(dims), data, length(data),
        ))
    end
    return nothing
end

function get_metadata(blob::Blob)
    out_md = Ref{Ptr{C.quiver_blob_metadata}}(C_NULL)
    check(C.quiver_blob_get_metadata(blob.ptr, out_md))
    return BlobMetadata(out_md[])
end

function get_file_path(blob::Blob)
    out = Ref{Ptr{Cchar}}(C_NULL)
    check(C.quiver_blob_get_file_path(blob.ptr, out))
    return unsafe_string(out[])
end
