"""
Marshal a Julia Vector of parameters into C arrays of types and value pointers.
Returns (param_types, param_values, refs) where refs must be kept alive during the call.
"""
function marshal_params(parameters::Vector)
    n = length(parameters)
    param_types = Vector{Cint}(undef, n)
    param_values = Vector{Ptr{Cvoid}}(undef, n)
    # Keep references alive to prevent GC
    refs = Vector{Any}(undef, n)

    for i in 1:n
        p = parameters[i]
        if p === nothing
            param_types[i] = Cint(C.QUIVER_DATA_TYPE_NULL)
            param_values[i] = C_NULL
            refs[i] = nothing
        elseif p isa Integer
            ref = Ref{Int64}(Int64(p))
            param_types[i] = Cint(C.QUIVER_DATA_TYPE_INTEGER)
            param_values[i] = Base.unsafe_convert(Ptr{Cvoid}, Base.cconvert(Ref{Int64}, ref))
            refs[i] = ref
        elseif p isa AbstractFloat
            ref = Ref{Float64}(Float64(p))
            param_types[i] = Cint(C.QUIVER_DATA_TYPE_FLOAT)
            param_values[i] = Base.unsafe_convert(Ptr{Cvoid}, Base.cconvert(Ref{Float64}, ref))
            refs[i] = ref
        elseif p isa AbstractString
            cstr = Base.cconvert(Ptr{Cchar}, p)
            param_types[i] = Cint(C.QUIVER_DATA_TYPE_STRING)
            param_values[i] = Base.unsafe_convert(Ptr{Cvoid}, Base.unsafe_convert(Ptr{Cchar}, cstr))
            refs[i] = cstr
        else
            throw(ArgumentError("Unsupported parameter type: $(typeof(p))"))
        end
    end

    return param_types, param_values, refs
end

"""
    query_string(db::Database, sql::String, parameters::Vector = []) -> Optional{String}

Execute a SQL query and return the first column of the first row as a String.
`parameters` bind positionally to `?` placeholders.
Returns `nothing` if the query returns no rows.
"""
function query_string(db::Database, sql::String, parameters::Vector = [])
    param_types, param_values, refs = marshal_params(parameters)
    out_value = Ref{Ptr{Cchar}}(C_NULL)
    out_has_value = Ref{Cint}(0)

    GC.@preserve refs check(
        C.quiver_database_query_string(
            db.ptr,
            sql,
            param_types,
            param_values,
            length(parameters),
            out_value,
            out_has_value,
        ),
    )

    if out_has_value[] == 0 || out_value[] == C_NULL
        return nothing
    end
    result = unsafe_string(out_value[])
    C.quiver_database_free_string(out_value[])
    return result
end

"""
    query_integer(db::Database, sql::String, parameters::Vector = []) -> Optional{Int64}

Execute a SQL query and return the first column of the first row as an Int64.
`parameters` bind positionally to `?` placeholders.
Returns `nothing` if the query returns no rows.
"""
function query_integer(db::Database, sql::String, parameters::Vector = [])
    param_types, param_values, refs = marshal_params(parameters)
    out_value = Ref{Int64}(0)
    out_has_value = Ref{Cint}(0)

    GC.@preserve refs check(
        C.quiver_database_query_integer(
            db.ptr,
            sql,
            param_types,
            param_values,
            length(parameters),
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
    query_boolean(db::Database, sql::String, parameters::Vector = []) -> Optional{Bool}

Execute a SQL query and return the first column of the first row as a Bool.
`parameters` bind positionally to `?` placeholders.
Returns `nothing` if the query returns no rows.
"""
function query_boolean(db::Database, sql::String, parameters::Vector = [])
    return _integer_to_boolean(query_integer(db, sql, parameters))
end

"""
    query_float(db::Database, sql::String, parameters::Vector = []) -> Optional{Float64}

Execute a SQL query and return the first column of the first row as a Float64.
`parameters` bind positionally to `?` placeholders.
Returns `nothing` if the query returns no rows.
"""
function query_float(db::Database, sql::String, parameters::Vector = [])
    param_types, param_values, refs = marshal_params(parameters)
    out_value = Ref{Float64}(0.0)
    out_has_value = Ref{Cint}(0)

    GC.@preserve refs check(
        C.quiver_database_query_float(
            db.ptr,
            sql,
            param_types,
            param_values,
            length(parameters),
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
    query_date_time(db::Database, sql::String, parameters::Vector = []) -> Optional{DateTime}

Execute a SQL query and return the first column of the first row as a DateTime.
`parameters` bind positionally to `?` placeholders.
Returns `nothing` if the query returns no rows.
"""
function query_date_time(db::Database, sql::String, parameters::Vector = [])
    return string_to_date_time(query_string(db, sql, parameters))
end
