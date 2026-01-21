struct DatabaseException <: Exception
    msg::String
end

margaux_sqlite_error(msg::String) = throw(DatabaseException(msg))
Base.showerror(io::IO, e::DatabaseException) = print(io, e.msg)
