mutable struct Element
    ptr::Ptr{C.quiver_element}

    function Element()
        ptr = C.quiver_element_create()
        if ptr == C_NULL
            error("Failed to create Element")
        end
        return new(ptr)
    end
end

function destroy!(el::Element)
    if el.ptr != C_NULL
        C.quiver_element_destroy(el.ptr)
        el.ptr = C_NULL
    end
    return nothing
end

function Base.setindex!(el::Element, value::Integer, name::String)
    cname = Base.cconvert(Cstring, name)
    err = C.quiver_element_set_integer(el.ptr, cname, Int64(value))
    if err != C.QUIVER_OK
        error("Failed to set int value for '$name'")
    end
end

function Base.setindex!(el::Element, value::Real, name::String)
    cname = Base.cconvert(Cstring, name)
    err = C.quiver_element_set_float(el.ptr, cname, Float64(value))
    if err != C.QUIVER_OK
        error("Failed to set float value for '$name'")
    end
end

function Base.setindex!(el::Element, value::String, name::String)
    cname = Base.cconvert(Cstring, name)
    cvalue = Base.cconvert(Cstring, value)
    err = C.quiver_element_set_string(el.ptr, cname, cvalue)
    if err != C.QUIVER_OK
        error("Failed to set string value for '$name'")
    end
end

function Base.setindex!(el::Element, value::Vector{<:Integer}, name::String)
    cname = Base.cconvert(Cstring, name)
    integer_values = Int64[Int64(v) for v in value]
    err = C.quiver_element_set_array_integer(el.ptr, cname, integer_values, Int32(length(integer_values)))
    if err != C.QUIVER_OK
        error("Failed to set array<int> value for '$name'")
    end
end

function Base.setindex!(el::Element, value::Vector{<:Real}, name::String)
    cname = Base.cconvert(Cstring, name)
    float_values = Float64[Float64(v) for v in value]
    err = C.quiver_element_set_array_float(el.ptr, cname, float_values, Int32(length(value)))
    if err != C.QUIVER_OK
        error("Failed to set array<float> value for '$name'")
    end
end

function Base.setindex!(el::Element, value::Vector{<:AbstractString}, name::String)
    cname = Base.cconvert(Cstring, name)
    # Convert strings to null-terminated C strings
    cstrings = [Base.cconvert(Cstring, s) for s in value]
    ptrs = [Base.unsafe_convert(Cstring, cs) for cs in cstrings]
    GC.@preserve cstrings begin
        err = C.quiver_element_set_array_string(el.ptr, cname, ptrs, Int32(length(value)))
    end
    if err != C.QUIVER_OK
        error("Failed to set array<string> value for '$name'")
    end
end

# Handle empty arrays (Vector{Any}) - throw a DatabaseException
function Base.setindex!(el::Element, value::Vector{Any}, name::String)
    if isempty(value)
        throw(DatabaseException("Empty array not allowed for '$name'"))
    end
    # For non-empty Vector{Any}, try to determine the element type
    first_val = first(value)
    if first_val isa Integer
        el[name] = Int64[Int64(v) for v in value]
    elseif first_val isa AbstractFloat
        el[name] = Float64[Float64(v) for v in value]
    elseif first_val isa AbstractString
        el[name] = String[String(v) for v in value]
    else
        error("Unsupported array element type for '$name': $(typeof(first_val))")
    end
end

function Base.show(io::IO, e::Element)
    cstr = C.quiver_element_to_string(e.ptr)
    str = unsafe_string(cstr)
    C.quiver_string_free(cstr)
    print(io, str)
    return nothing
end
