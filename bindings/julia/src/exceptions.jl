struct DatabaseException <: Exception
    msg::String
end

quiver_database_sqlite_error(msg::String) = throw(DatabaseException(msg))
Base.showerror(io::IO, e::DatabaseException) = print(io, e.msg)

"""
    check_error(err::C.quiver_error_t, context::String)

Check for C API errors and throw DatabaseException with detailed message.
"""
function check_error(err, context::String)
    if err != C.QUIVER_OK
        detail = unsafe_string(C.quiver_get_last_error())
        if isempty(detail)
            throw(DatabaseException(context))
        else
            throw(DatabaseException("$context: $detail"))
        end
    end
    return nothing
end
