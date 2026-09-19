mutable struct Database
    ptr::Ptr{C.quiver_database}

    function Database(ptr::Ptr{C.quiver_database})
        db = new(ptr)
        finalizer(d -> d.ptr != C_NULL && C.quiver_database_close(d.ptr), db)
        return db
    end
end

"""
    build_quiver_database_options(; read_only=nothing, console_level=nothing, ui_config_dir=nothing, ui_locale=nothing) -> (Ref{quiver_database_options_t}, Vector{Any})

Build a C API `quiver_database_options_t` from keyword arguments.
Returns `(options_ref, keepalive)` where `keepalive` holds every backing buffer that must
stay alive during the C call (use inside `GC.@preserve`) -- a pointer stored in a struct field
is not itself a reference, so Julia is free to collect the buffer the moment the last binding
(other than the struct's raw pointer) drops.

An omitted or empty `ui_config_dir`/`ui_locale` writes `Ptr{Cchar}(C_NULL)`, never a pointer to
an empty string -- NULL means "not specified" (convention path / `"en"`), per the binding's
documented null-string rule.
"""
function build_quiver_database_options(;
    read_only::Optional{Bool} = nothing,
    console_level::Optional{C.quiver_log_level_t} = nothing,
    ui_config_dir::Optional{String} = nothing,
    ui_locale::Optional{String} = nothing,
)
    options = Ref(C.quiver_database_options_default())
    if !isnothing(read_only)
        options[].read_only = read_only ? 1 : 0
    end
    if !isnothing(console_level)
        options[].console_level = console_level
    end

    keepalive = Any[]

    if !isnothing(ui_config_dir) && !isempty(ui_config_dir)
        buf = Base.cconvert(Cstring, ui_config_dir)
        push!(keepalive, buf)
        options[].ui_config_dir = Ptr{Cchar}(Base.unsafe_convert(Cstring, buf))
    else
        options[].ui_config_dir = Ptr{Cchar}(C_NULL)
    end

    if !isnothing(ui_locale) && !isempty(ui_locale)
        buf = Base.cconvert(Cstring, ui_locale)
        push!(keepalive, buf)
        options[].ui_locale = Ptr{Cchar}(Base.unsafe_convert(Cstring, buf))
    else
        options[].ui_locale = Ptr{Cchar}(C_NULL)
    end

    push!(keepalive, options)
    return (options, keepalive)
end

function from_schema(db_path::String, schema_path::String; kwargs...)
    options, keepalive = build_quiver_database_options(; kwargs...)
    out_db = Ref{Ptr{C.quiver_database}}(C_NULL)
    GC.@preserve keepalive check(C.quiver_database_from_schema(db_path, schema_path, options, out_db))
    return Database(out_db[])
end

function from_migrations(db_path::String, migrations_path::String; kwargs...)
    options, keepalive = build_quiver_database_options(; kwargs...)
    out_db = Ref{Ptr{C.quiver_database}}(C_NULL)
    GC.@preserve keepalive check(C.quiver_database_from_migrations(db_path, migrations_path, options, out_db))
    return Database(out_db[])
end

function validate_migrations(migrations_path::String)
    check(C.quiver_database_validate_migrations(migrations_path))
    return nothing
end

function open(db_path::String; kwargs...)
    options, keepalive = build_quiver_database_options(; kwargs...)
    out_db = Ref{Ptr{C.quiver_database}}(C_NULL)
    GC.@preserve keepalive check(C.quiver_database_open(db_path, options, out_db))
    return Database(out_db[])
end

function close!(db::Database)
    if db.ptr != C_NULL
        C.quiver_database_close(db.ptr)
        db.ptr = C_NULL
    end
    return nothing
end

function _with_database(fn, db::Database)
    try
        return fn(db)
    finally
        close!(db)
    end
end

function from_schema(fn::Function, db_path::String, schema_path::String; kwargs...)
    return _with_database(fn, from_schema(db_path, schema_path; kwargs...))
end

function from_migrations(fn::Function, db_path::String, migrations_path::String; kwargs...)
    return _with_database(fn, from_migrations(db_path, migrations_path; kwargs...))
end

function open(fn::Function, db_path::String; kwargs...)
    return _with_database(fn, open(db_path; kwargs...))
end

function current_version(db::Database)
    out_version = Ref{Int64}(0)
    check(C.quiver_database_current_version(db.ptr, out_version))
    return out_version[]
end

function is_healthy(db::Database)
    out = Ref{Cint}(0)
    check(C.quiver_database_is_healthy(db.ptr, out))
    return out[] != 0
end

"""
    has_ui_config(db::Database) -> Bool

`true` when a UI sidecar loaded (explicit `ui_config_dir` or the `<db_dir>/ui/` convention),
`false` when the directory was absent or malformed. Never throws -- a read, not a mutation,
hence no `!` suffix.
"""
function has_ui_config(db::Database)
    out = Ref{Cint}(0)
    check(C.quiver_database_has_ui_config(db.ptr, out))
    return out[] != 0
end

function path(db::Database)
    out = Ref{Ptr{Cchar}}(C_NULL)
    check(C.quiver_database_path(db.ptr, out))
    return unsafe_string(out[])
end
