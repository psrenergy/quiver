const PARAM_TYPE_INTEGER = Cint(0)
const PARAM_TYPE_FLOAT = Cint(1)
const PARAM_TYPE_STRING = Cint(2)
const PARAM_TYPE_NULL = Cint(4)

"""
Marshal a Julia Vector of parameters into C arrays of types and value pointers.
Returns (param_types, param_values, refs) where refs must be kept alive during the call.
"""
function marshal_params(params::Vector)
    n = length(params)
    param_types = Vector{Cint}(undef, n)
    param_values = Vector{Ptr{Cvoid}}(undef, n)
    # Keep references alive to prevent GC
    refs = Vector{Any}(undef, n)

    for i in 1:n
        p = params[i]
        if p === nothing
            param_types[i] = PARAM_TYPE_NULL
            param_values[i] = C_NULL
            refs[i] = nothing
        elseif p isa Integer
            ref = Ref{Int64}(Int64(p))
            param_types[i] = PARAM_TYPE_INTEGER
            param_values[i] = Base.unsafe_convert(Ptr{Cvoid}, Base.cconvert(Ref{Int64}, ref))
            refs[i] = ref
        elseif p isa AbstractFloat
            ref = Ref{Float64}(Float64(p))
            param_types[i] = PARAM_TYPE_FLOAT
            param_values[i] = Base.unsafe_convert(Ptr{Cvoid}, Base.cconvert(Ref{Float64}, ref))
            refs[i] = ref
        elseif p isa AbstractString
            cstr = Base.cconvert(Ptr{Cchar}, p)
            param_types[i] = PARAM_TYPE_STRING
            param_values[i] = Base.unsafe_convert(Ptr{Cvoid}, Base.unsafe_convert(Ptr{Cchar}, cstr))
            refs[i] = cstr
        else
            throw(ArgumentError("Unsupported parameter type: $(typeof(p))"))
        end
    end

    return param_types, param_values, refs
end

"""
    query_string(db::Database, sql::String) -> Union{String, Nothing}

Execute a SQL query and return the first column of the first row as a String.
Returns `nothing` if the query returns no rows.
"""
function query_string(db::Database, sql::String)
    out_value = Ref{Ptr{Cchar}}(C_NULL)
    out_has_value = Ref{Cint}(0)

    check(C.quiver_database_query_string(db.ptr, sql, out_value, out_has_value))

    if out_has_value[] == 0 || out_value[] == C_NULL
        return nothing
    end
    result = unsafe_string(out_value[])
    C.quiver_element_free_string(out_value[])
    return result
end

"""
    query_integer(db::Database, sql::String) -> Union{Int64, Nothing}

Execute a SQL query and return the first column of the first row as an Int64.
Returns `nothing` if the query returns no rows.
"""
function query_integer(db::Database, sql::String)
    out_value = Ref{Int64}(0)
    out_has_value = Ref{Cint}(0)

    check(C.quiver_database_query_integer(db.ptr, sql, out_value, out_has_value))

    if out_has_value[] == 0
        return nothing
    end
    return out_value[]
end

"""
    query_float(db::Database, sql::String) -> Union{Float64, Nothing}

Execute a SQL query and return the first column of the first row as a Float64.
Returns `nothing` if the query returns no rows.
"""
function query_float(db::Database, sql::String)
    out_value = Ref{Float64}(0.0)
    out_has_value = Ref{Cint}(0)

    check(C.quiver_database_query_float(db.ptr, sql, out_value, out_has_value))

    if out_has_value[] == 0
        return nothing
    end
    return out_value[]
end

"""
    query_string(db::Database, sql::String, params::Vector) -> Union{String, Nothing}

Execute a parameterized SQL query and return the first column of the first row as a String.
Returns `nothing` if the query returns no rows.
"""
function query_string(db::Database, sql::String, params::Vector)
    param_types, param_values, refs = marshal_params(params)
    out_value = Ref{Ptr{Cchar}}(C_NULL)
    out_has_value = Ref{Cint}(0)

    check(
        C.quiver_database_query_string_params(
            db.ptr,
            sql,
            param_types,
            param_values,
            length(params),
            out_value,
            out_has_value,
        ),
    )

    if out_has_value[] == 0 || out_value[] == C_NULL
        return nothing
    end
    result = unsafe_string(out_value[])
    C.quiver_element_free_string(out_value[])
    return result
end

"""
    query_integer(db::Database, sql::String, params::Vector) -> Union{Int64, Nothing}

Execute a parameterized SQL query and return the first column of the first row as an Int64.
Returns `nothing` if the query returns no rows.
"""
function query_integer(db::Database, sql::String, params::Vector)
    param_types, param_values, refs = marshal_params(params)
    out_value = Ref{Int64}(0)
    out_has_value = Ref{Cint}(0)

    check(
        C.quiver_database_query_integer_params(
            db.ptr,
            sql,
            param_types,
            param_values,
            length(params),
            out_value,
            out_has_value,
        ),
    )

    if out_has_value[] == 0
        return nothing
    end
    return out_value[]
end

"""
    query_float(db::Database, sql::String, params::Vector) -> Union{Float64, Nothing}

Execute a parameterized SQL query and return the first column of the first row as a Float64.
Returns `nothing` if the query returns no rows.
"""
function query_float(db::Database, sql::String, params::Vector)
    param_types, param_values, refs = marshal_params(params)
    out_value = Ref{Float64}(0.0)
    out_has_value = Ref{Cint}(0)

    check(
        C.quiver_database_query_float_params(
            db.ptr,
            sql,
            param_types,
            param_values,
            length(params),
            out_value,
            out_has_value,
        ),
    )

    if out_has_value[] == 0
        return nothing
    end
    return out_value[]
end

"""
    query_date_time(db::Database, sql::String) -> Union{DateTime, Nothing}

Execute a SQL query and return the first column of the first row as a DateTime.
Returns `nothing` if the query returns no rows.
"""
function query_date_time(db::Database, sql::String)
    result = query_string(db, sql)
    if result === nothing
        return nothing
    end
    return string_to_date_time(result)
end

"""
    query_date_time(db::Database, sql::String, params::Vector) -> Union{DateTime, Nothing}

Execute a parameterized SQL query and return the first column of the first row as a DateTime.
Returns `nothing` if the query returns no rows.
"""
function query_date_time(db::Database, sql::String, params::Vector)
    result = query_string(db, sql, params)
    if result === nothing
        return nothing
    end
    return string_to_date_time(result)
end
