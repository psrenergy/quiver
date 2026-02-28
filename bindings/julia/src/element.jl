mutable struct Element
    ptr::Ptr{C.quiver_element}

    function Element()
        out_element = Ref{Ptr{C.quiver_element}}(C_NULL)
        check(C.quiver_element_create(out_element))
        return new(out_element[])
    end
end

function destroy!(el::Element)
    if el.ptr != C_NULL
        check(C.quiver_element_destroy(el.ptr))
        el.ptr = C_NULL
    end
    return nothing
end

function clear!(el::Element)
    check(C.quiver_element_clear(el.ptr))
    return nothing
end

function Base.setindex!(el::Element, value::Integer, name::String)
    cname = Base.cconvert(Cstring, name)
    check(C.quiver_element_set_integer(el.ptr, cname, Int64(value)))
    return nothing
end

function Base.setindex!(el::Element, value::Real, name::String)
    cname = Base.cconvert(Cstring, name)
    check(C.quiver_element_set_float(el.ptr, cname, Float64(value)))
    return nothing
end

function Base.setindex!(el::Element, value::String, name::String)
    cname = Base.cconvert(Cstring, name)
    cvalue = Base.cconvert(Cstring, value)
    check(C.quiver_element_set_string(el.ptr, cname, cvalue))
    return nothing
end

function Base.setindex!(el::Element, value::DateTime, name::String)
    el[name] = date_time_to_string(value)
    return nothing
end

function Base.setindex!(el::Element, value::Vector{DateTime}, name::String)
    el[name] = [date_time_to_string(dt) for dt in value]
    return nothing
end

function Base.setindex!(el::Element, value::Dict{String, <:Any}, name::String)
    for (k, v) in value
        el[k] = v
    end
    return nothing
end

function Base.setindex!(el::Element, value::Vector{<:Integer}, name::String)
    cname = Base.cconvert(Cstring, name)
    integer_values = Int64[Int64(v) for v in value]
    return check(C.quiver_element_set_array_integer(el.ptr, cname, integer_values, Int32(length(integer_values))))
end

function Base.setindex!(el::Element, value::Vector{<:Real}, name::String)
    cname = Base.cconvert(Cstring, name)
    float_values = Float64[Float64(v) for v in value]
    return check(C.quiver_element_set_array_float(el.ptr, cname, float_values, Int32(length(value))))
end

function Base.setindex!(el::Element, value::Vector{<:AbstractString}, name::String)
    cname = Base.cconvert(Cstring, name)
    # Convert strings to null-terminated C strings
    cstrings = [Base.cconvert(Cstring, s) for s in value]
    ptrs = [Base.unsafe_convert(Cstring, cs) for cs in cstrings]
    GC.@preserve cstrings begin
        check(C.quiver_element_set_array_string(el.ptr, cname, ptrs, Int32(length(value))))
    end
end

function Base.setindex!(el::Element, value::Vector{Any}, name::String)
    if isempty(value)
        throw(ArgumentError("Cannot determine array element type for '$name': array is empty"))
    end
    first_val = first(value)
    if first_val isa Integer
        el[name] = Int64[Int64(v) for v in value]
    elseif first_val isa AbstractFloat
        el[name] = Float64[Float64(v) for v in value]
    elseif first_val isa AbstractString
        el[name] = String[String(v) for v in value]
    else
        throw(ArgumentError("Unsupported array element type for '$name': $(typeof(first_val))"))
    end
end

function Base.show(io::IO, e::Element)
    out_string = Ref{Ptr{Cchar}}(C_NULL)
    err = C.quiver_element_to_string(e.ptr, out_string)
    if err != C.QUIVER_OK
        print(io, "<Element: error getting string representation>")
        return nothing
    end
    str = unsafe_string(out_string[])
    C.quiver_database_free_string(out_string[])
    print(io, str)
    return nothing
end
