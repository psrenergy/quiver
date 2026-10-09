struct DatabaseException <: Exception
    msg::String
end

Base.showerror(io::IO, e::DatabaseException) = print(io, e.msg)

function check(err)
    if err != C.QUIVER_OK
        detail = unsafe_string(C.quiver_get_last_error())
        if isempty(detail)
            @warn "check: C API returned error but quiver_get_last_error() is empty"
            throw(DatabaseException("Unknown error"))
        end
        throw(DatabaseException(detail))
    end
    return nothing
end
