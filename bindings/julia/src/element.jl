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

# function Base.setindex!(el::Element, value::Vector{<:Integer}, name::String)
#     cname = Base.cconvert(Cstring, name)
#     cvalues = Base.unsafe_convert(Ptr{Int32}, value)
#     count = C.size_t(length(value))
#     err = C.psr_element_set_vector_int(el.ptr, cname, cvalues, count)
#     if err != C.PSR_OK
#         error("Failed to set vector<int> value for '$name'")
#     end
# end