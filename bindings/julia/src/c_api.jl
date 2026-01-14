module C

#! format: off

using CEnum
using Libdl

const libpsr_database_c = joinpath(@__DIR__, "..", "..", "..", "build", "bin", "libpsr_database_c.dll")  


@cenum psr_error_t::Int32 begin
    PSR_OK = 0
    PSR_ERROR_INVALID_ARGUMENT = -1
    PSR_ERROR_DATABASE = -2
    PSR_ERROR_MIGRATION = -3
    PSR_ERROR_SCHEMA = -4
    PSR_ERROR_CREATE_ELEMENT = -5
    PSR_ERROR_NOT_FOUND = -6
end

function psr_error_string(error)
    @ccall libpsr_database_c.psr_error_string(error::psr_error_t)::Ptr{Cchar}
end

function psr_version()
    @ccall libpsr_database_c.psr_version()::Ptr{Cchar}
end

@cenum psr_log_level_t::UInt32 begin
    PSR_LOG_DEBUG = 0
    PSR_LOG_INFO = 1
    PSR_LOG_WARN = 2
    PSR_LOG_ERROR = 3
    PSR_LOG_OFF = 4
end

struct psr_database_options_t
    read_only::Cint
    console_level::psr_log_level_t
end

function psr_database_options_default()
    @ccall libpsr_database_c.psr_database_options_default()::psr_database_options_t
end

mutable struct psr_database end

const psr_database_t = psr_database

function psr_database_open(path, options)
    @ccall libpsr_database_c.psr_database_open(path::Ptr{Cchar}, options::Ptr{psr_database_options_t})::Ptr{psr_database_t}
end

function psr_database_from_migrations(db_path, migrations_path, options)
    @ccall libpsr_database_c.psr_database_from_migrations(db_path::Ptr{Cchar}, migrations_path::Ptr{Cchar}, options::Ptr{psr_database_options_t})::Ptr{psr_database_t}
end

function psr_database_from_schema(db_path, schema_path, options)
    @ccall libpsr_database_c.psr_database_from_schema(db_path::Ptr{Cchar}, schema_path::Ptr{Cchar}, options::Ptr{psr_database_options_t})::Ptr{psr_database_t}
end

function psr_database_close(db)
    @ccall libpsr_database_c.psr_database_close(db::Ptr{psr_database_t})::Cvoid
end

function psr_database_is_healthy(db)
    @ccall libpsr_database_c.psr_database_is_healthy(db::Ptr{psr_database_t})::Cint
end

function psr_database_path(db)
    @ccall libpsr_database_c.psr_database_path(db::Ptr{psr_database_t})::Ptr{Cchar}
end

function psr_database_current_version(db)
    @ccall libpsr_database_c.psr_database_current_version(db::Ptr{psr_database_t})::Int64
end

function psr_database_set_version(db, version)
    @ccall libpsr_database_c.psr_database_set_version(db::Ptr{psr_database_t}, version::Int64)::psr_error_t
end

function psr_database_migrate_up(db, migrations_path)
    @ccall libpsr_database_c.psr_database_migrate_up(db::Ptr{psr_database_t}, migrations_path::Ptr{Cchar})::psr_error_t
end

function psr_database_begin_transaction(db)
    @ccall libpsr_database_c.psr_database_begin_transaction(db::Ptr{psr_database_t})::psr_error_t
end

function psr_database_commit(db)
    @ccall libpsr_database_c.psr_database_commit(db::Ptr{psr_database_t})::psr_error_t
end

function psr_database_rollback(db)
    @ccall libpsr_database_c.psr_database_rollback(db::Ptr{psr_database_t})::psr_error_t
end

mutable struct psr_element end

const psr_element_t = psr_element

function psr_database_create_element(db, collection, element)
    @ccall libpsr_database_c.psr_database_create_element(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, element::Ptr{psr_element_t})::Int64
end

function psr_database_read_scalar_ints(db, collection, attribute, out_values, out_count)
    @ccall libpsr_database_c.psr_database_read_scalar_ints(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_values::Ptr{Ptr{Int64}}, out_count::Ptr{Csize_t})::psr_error_t
end

function psr_database_read_scalar_doubles(db, collection, attribute, out_values, out_count)
    @ccall libpsr_database_c.psr_database_read_scalar_doubles(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_values::Ptr{Ptr{Cdouble}}, out_count::Ptr{Csize_t})::psr_error_t
end

function psr_database_read_scalar_strings(db, collection, attribute, out_values, out_count)
    @ccall libpsr_database_c.psr_database_read_scalar_strings(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_values::Ptr{Ptr{Ptr{Cchar}}}, out_count::Ptr{Csize_t})::psr_error_t
end

function psr_free_int_array(values)
    @ccall libpsr_database_c.psr_free_int_array(values::Ptr{Int64})::Cvoid
end

function psr_free_double_array(values)
    @ccall libpsr_database_c.psr_free_double_array(values::Ptr{Cdouble})::Cvoid
end

function psr_free_string_array(values, count)
    @ccall libpsr_database_c.psr_free_string_array(values::Ptr{Ptr{Cchar}}, count::Csize_t)::Cvoid
end

function psr_element_create()
    @ccall libpsr_database_c.psr_element_create()::Ptr{psr_element_t}
end

function psr_element_destroy(element)
    @ccall libpsr_database_c.psr_element_destroy(element::Ptr{psr_element_t})::Cvoid
end

function psr_element_clear(element)
    @ccall libpsr_database_c.psr_element_clear(element::Ptr{psr_element_t})::Cvoid
end

function psr_element_set_int(element, name, value)
    @ccall libpsr_database_c.psr_element_set_int(element::Ptr{psr_element_t}, name::Ptr{Cchar}, value::Int64)::psr_error_t
end

function psr_element_set_double(element, name, value)
    @ccall libpsr_database_c.psr_element_set_double(element::Ptr{psr_element_t}, name::Ptr{Cchar}, value::Cdouble)::psr_error_t
end

function psr_element_set_string(element, name, value)
    @ccall libpsr_database_c.psr_element_set_string(element::Ptr{psr_element_t}, name::Ptr{Cchar}, value::Ptr{Cchar})::psr_error_t
end

function psr_element_set_null(element, name)
    @ccall libpsr_database_c.psr_element_set_null(element::Ptr{psr_element_t}, name::Ptr{Cchar})::psr_error_t
end

function psr_element_set_array_int(element, name, values, count)
    @ccall libpsr_database_c.psr_element_set_array_int(element::Ptr{psr_element_t}, name::Ptr{Cchar}, values::Ptr{Int64}, count::Int32)::psr_error_t
end

function psr_element_set_array_double(element, name, values, count)
    @ccall libpsr_database_c.psr_element_set_array_double(element::Ptr{psr_element_t}, name::Ptr{Cchar}, values::Ptr{Cdouble}, count::Int32)::psr_error_t
end

function psr_element_set_array_string(element, name, values, count)
    @ccall libpsr_database_c.psr_element_set_array_string(element::Ptr{psr_element_t}, name::Ptr{Cchar}, values::Ptr{Ptr{Cchar}}, count::Int32)::psr_error_t
end

function psr_element_has_scalars(element)
    @ccall libpsr_database_c.psr_element_has_scalars(element::Ptr{psr_element_t})::Cint
end

function psr_element_has_arrays(element)
    @ccall libpsr_database_c.psr_element_has_arrays(element::Ptr{psr_element_t})::Cint
end

function psr_element_scalar_count(element)
    @ccall libpsr_database_c.psr_element_scalar_count(element::Ptr{psr_element_t})::Csize_t
end

function psr_element_array_count(element)
    @ccall libpsr_database_c.psr_element_array_count(element::Ptr{psr_element_t})::Csize_t
end

function psr_element_to_string(element)
    @ccall libpsr_database_c.psr_element_to_string(element::Ptr{psr_element_t})::Ptr{Cchar}
end

function psr_string_free(str)
    @ccall libpsr_database_c.psr_string_free(str::Ptr{Cchar})::Cvoid
end

#! format: on


end # module
