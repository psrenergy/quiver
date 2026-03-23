mutable struct File
    ptr::Ptr{C.quiver_binary_file}

    function File(ptr::Ptr{C.quiver_binary_file})
        bf = new(ptr)
        finalizer(x -> x.ptr != C_NULL && C.quiver_binary_file_close(x.ptr), bf)
        return bf
    end
end

function open_file(path::String; mode::Symbol, metadata::Union{Metadata, Nothing} = nothing)
    out_file = Ref{Ptr{C.quiver_binary_file}}(C_NULL)
    if mode == :read
        check(C.quiver_binary_file_open_read(path, out_file))
    elseif mode == :write
        md_ptr = metadata === nothing ? Ptr{C.quiver_binary_metadata}(C_NULL) : metadata.ptr
        check(C.quiver_binary_file_open_write(path, md_ptr, out_file))
    else
        throw(ArgumentError("mode must be :read or :write, got :$mode"))
    end
    return File(out_file[])
end

function close!(file::File)
    if file.ptr != C_NULL
        C.quiver_binary_file_close(file.ptr)
        file.ptr = C_NULL
    end
    return nothing
end

function read(file::File; allow_nulls::Bool = false, dims...)
    dim_names_str = [String(k) for (k, _) in dims]
    dim_values = Int64[Int64(v) for (_, v) in dims]
    c_dim_names = [pointer(s) for s in dim_names_str]

    out_data = Ref{Ptr{Cdouble}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    GC.@preserve dim_names_str begin
        check(
            C.quiver_binary_file_read(
                file.ptr, c_dim_names, dim_values,
                length(dims), allow_nulls ? Cint(1) : Cint(0),
                out_data, out_count,
            ),
        )
    end

    count = out_count[]
    if count == 0 || out_data[] == C_NULL
        return Float64[]
    end

    result = [unsafe_load(out_data[], i) for i in 1:count]
    C.quiver_binary_file_free_float_array(out_data[])
    return result
end

function write!(file::File; data::Vector{Float64}, dims...)
    dim_names_str = [String(k) for (k, _) in dims]
    dim_values = Int64[Int64(v) for (_, v) in dims]
    c_dim_names = [pointer(s) for s in dim_names_str]

    GC.@preserve dim_names_str begin
        check(C.quiver_binary_file_write(
            file.ptr, c_dim_names, dim_values,
            length(dims), data, length(data),
        ))
    end
    return nothing
end

function get_metadata(file::File)
    out_md = Ref{Ptr{C.quiver_binary_metadata}}(C_NULL)
    check(C.quiver_binary_file_get_metadata(file.ptr, out_md))
    return Metadata(out_md[])
end

function get_file_path(file::File)
    out = Ref{Ptr{Cchar}}(C_NULL)
    check(C.quiver_binary_file_get_file_path(file.ptr, out))
    result = unsafe_string(out[])
    C.quiver_binary_file_free_string(out[])
    return result
end
