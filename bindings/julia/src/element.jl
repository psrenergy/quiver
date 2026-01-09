struct Element
    ptr::Ptr{C.psr_element}

    function Element()
        ptr = C.psr_element_create()
        if ptr == C_NULL
            error("Failed to create Element")
        end
        obj = new(ptr)
        # finalizer(C.psr_element_destroy, obj)
        return obj
    end
end

function Base.setindex!(el::Element, value::Integer, name::String)
    cname = Base.cconvert(Cstring, name)
    err = C.psr_element_set_int(el.ptr, cname, Int64(value))
    if err != C.PSR_OK
        error("Failed to set int value for '$name'")
    end
end

function Base.setindex!(el::Element, value::Float64, name::String)
    cname = Base.cconvert(Cstring, name)
    err = C.psr_element_set_double(el.ptr, cname, value)
    if err != C.PSR_OK
        error("Failed to set double value for '$name'")
    end
end

function Base.setindex!(el::Element, value::String, name::String)
    cname = Base.cconvert(Cstring, name)
    cvalue = Base.cconvert(Cstring, value)
    err = C.psr_element_set_string(el.ptr, cname, cvalue)
    if err != C.PSR_OK
        error("Failed to set string value for '$name'")
    end
end

function Base.setindex!(el::Element, value::Vector{<:Integer}, name::String)
    cname = Base.cconvert(Cstring, name)
    int_values = Int64[Int64(v) for v in value]
    err = C.psr_element_set_array_int(el.ptr, cname, int_values, Int32(length(int_values)))
    if err != C.PSR_OK
        error("Failed to set array<int> value for '$name'")
    end
end

function Base.setindex!(el::Element, value::Vector{<:Float64}, name::String)
    cname = Base.cconvert(Cstring, name)
    err = C.psr_element_set_array_double(el.ptr, cname, value, Int32(length(value)))
    if err != C.PSR_OK
        error("Failed to set array<double> value for '$name'")
    end
end

function Base.setindex!(el::Element, value::Vector{<:AbstractString}, name::String)
    cname = Base.cconvert(Cstring, name)
    # Convert strings to null-terminated C strings
    cstrings = [Base.cconvert(Cstring, s) for s in value]
    ptrs = [Base.unsafe_convert(Cstring, cs) for cs in cstrings]
    GC.@preserve cstrings begin
        err = C.psr_element_set_array_string(el.ptr, cname, ptrs, Int32(length(value)))
    end
    if err != C.PSR_OK
        error("Failed to set array<string> value for '$name'")
    end
end

function Base.show(io::IO, e::Element)
    cstr = C.psr_element_to_string(e.ptr)
    str = unsafe_string(cstr)
    C.psr_string_free(cstr)
    print(io, str)
    return nothing
end
