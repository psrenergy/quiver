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
    err = C.psr_element_set_int(el.ptr, cname, Int32(value))
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
    cvalues = Base.unsafe_convert(Ptr{Int32}, value)
    count = Int32(length(value))
    err = C.psr_element_set_vector_int(el.ptr, cname, cvalues, count)
    if err != C.PSR_OK
        error("Failed to set vector<int> value for '$name'")
    end
end

function Base.setindex!(el::Element, value::Vector{<:Float64}, name::String)
    cname = Base.cconvert(Cstring, name)
    cvalues = Base.unsafe_convert(Ptr{Float64}, value)
    count = Int32(length(value))
    err = C.psr_element_set_vector_double(el.ptr, cname, cvalues, count)
    if err != C.PSR_OK
        error("Failed to set vector<double> value for '$name'")
    end
end

# function Base.setindex!(el::Element, value::Vector{<:String}, name::String)
#     cname = Base.cconvert(Cstring, name)
#     cvalues = Vector{Ptr{Cchar}}(undef, length(value))
#     for (i, str) in enumerate(value)
#         cvalues[i] = Base.cconvert(Cstring, str)
#     end
#     cvalues_ptr = Base.unsafe_convert(Ptr{Ptr{Cchar}}, cvalues)
#     count = Int32(length(value))
#     err = C.psr_element_set_vector_string(el.ptr, cname, cvalues_ptr, count)
#     if err != C.PSR_OK
#         error("Failed to set vector<string> value for '$name'")
#     end
# end

function Base.show(io::IO, e::Element)
    cstr = C.psr_element_to_string(e.ptr)
    str = unsafe_string(cstr)
    C.psr_string_free(cstr)
    print(io, str)
    return nothing
end