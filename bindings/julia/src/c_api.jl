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

function psr_database_read_scalar_parameters_double(db, collection, attribute, out_values)
    @ccall libpsr_database_c.psr_database_read_scalar_parameters_double(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_values::Ptr{Ptr{Cdouble}})::Int64
end

function psr_database_read_scalar_parameters_string(db, collection, attribute, out_values)
    @ccall libpsr_database_c.psr_database_read_scalar_parameters_string(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_values::Ptr{Ptr{Ptr{Cchar}}})::Int64
end

function psr_database_read_scalar_parameters_int(db, collection, attribute, out_values)
    @ccall libpsr_database_c.psr_database_read_scalar_parameters_int(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_values::Ptr{Ptr{Int64}})::Int64
end

function psr_database_read_scalar_parameter_double(db, collection, attribute, label, out_value, is_null)
    @ccall libpsr_database_c.psr_database_read_scalar_parameter_double(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, label::Ptr{Cchar}, out_value::Ptr{Cdouble}, is_null::Ptr{Cint})::psr_error_t
end

function psr_database_read_scalar_parameter_string(db, collection, attribute, label, out_value)
    @ccall libpsr_database_c.psr_database_read_scalar_parameter_string(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, label::Ptr{Cchar}, out_value::Ptr{Ptr{Cchar}})::psr_error_t
end

function psr_database_read_scalar_parameter_int(db, collection, attribute, label, out_value, is_null)
    @ccall libpsr_database_c.psr_database_read_scalar_parameter_int(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, label::Ptr{Cchar}, out_value::Ptr{Int64}, is_null::Ptr{Cint})::psr_error_t
end

function psr_double_array_free(arr)
    @ccall libpsr_database_c.psr_double_array_free(arr::Ptr{Cdouble})::Cvoid
end

function psr_string_array_free(arr, count)
    @ccall libpsr_database_c.psr_string_array_free(arr::Ptr{Ptr{Cchar}}, count::Csize_t)::Cvoid
end

function psr_int_array_free(arr)
    @ccall libpsr_database_c.psr_int_array_free(arr::Ptr{Int64})::Cvoid
end

# Schema introspection
function psr_database_get_set_tables(db, collection, out_count)
    @ccall libpsr_database_c.psr_database_get_set_tables(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, out_count::Ptr{Int64})::Ptr{Ptr{Cchar}}
end

struct psr_column_info_t
    name::Ptr{Cchar}
    fk_table::Ptr{Cchar}
    fk_column::Ptr{Cchar}
end

function psr_database_get_table_columns(db, table, out_count)
    @ccall libpsr_database_c.psr_database_get_table_columns(db::Ptr{psr_database_t}, table::Ptr{Cchar}, out_count::Ptr{Int64})::Ptr{psr_column_info_t}
end

function psr_column_info_array_free(arr, count)
    @ccall libpsr_database_c.psr_column_info_array_free(arr::Ptr{psr_column_info_t}, count::Csize_t)::Cvoid
end

function psr_database_find_element_id(db, collection, label)
    @ccall libpsr_database_c.psr_database_find_element_id(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, label::Ptr{Cchar})::Int64
end

mutable struct psr_vector_group end

const psr_vector_group_t = psr_vector_group

mutable struct psr_set_group end

const psr_set_group_t = psr_set_group

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

function psr_vector_group_create()
    @ccall libpsr_database_c.psr_vector_group_create()::Ptr{psr_vector_group_t}
end

function psr_vector_group_destroy(group)
    @ccall libpsr_database_c.psr_vector_group_destroy(group::Ptr{psr_vector_group_t})::Cvoid
end

function psr_vector_group_add_row(group)
    @ccall libpsr_database_c.psr_vector_group_add_row(group::Ptr{psr_vector_group_t})::psr_error_t
end

function psr_vector_group_set_int(group, column, value)
    @ccall libpsr_database_c.psr_vector_group_set_int(group::Ptr{psr_vector_group_t}, column::Ptr{Cchar}, value::Int64)::psr_error_t
end

function psr_vector_group_set_double(group, column, value)
    @ccall libpsr_database_c.psr_vector_group_set_double(group::Ptr{psr_vector_group_t}, column::Ptr{Cchar}, value::Cdouble)::psr_error_t
end

function psr_vector_group_set_string(group, column, value)
    @ccall libpsr_database_c.psr_vector_group_set_string(group::Ptr{psr_vector_group_t}, column::Ptr{Cchar}, value::Ptr{Cchar})::psr_error_t
end

function psr_element_add_vector_group(element, name, group)
    @ccall libpsr_database_c.psr_element_add_vector_group(element::Ptr{psr_element_t}, name::Ptr{Cchar}, group::Ptr{psr_vector_group_t})::psr_error_t
end

function psr_set_group_create()
    @ccall libpsr_database_c.psr_set_group_create()::Ptr{psr_set_group_t}
end

function psr_set_group_destroy(group)
    @ccall libpsr_database_c.psr_set_group_destroy(group::Ptr{psr_set_group_t})::Cvoid
end

function psr_set_group_add_row(group)
    @ccall libpsr_database_c.psr_set_group_add_row(group::Ptr{psr_set_group_t})::psr_error_t
end

function psr_set_group_set_int(group, column, value)
    @ccall libpsr_database_c.psr_set_group_set_int(group::Ptr{psr_set_group_t}, column::Ptr{Cchar}, value::Int64)::psr_error_t
end

function psr_set_group_set_double(group, column, value)
    @ccall libpsr_database_c.psr_set_group_set_double(group::Ptr{psr_set_group_t}, column::Ptr{Cchar}, value::Cdouble)::psr_error_t
end

function psr_set_group_set_string(group, column, value)
    @ccall libpsr_database_c.psr_set_group_set_string(group::Ptr{psr_set_group_t}, column::Ptr{Cchar}, value::Ptr{Cchar})::psr_error_t
end

function psr_element_add_set_group(element, name, group)
    @ccall libpsr_database_c.psr_element_add_set_group(element::Ptr{psr_element_t}, name::Ptr{Cchar}, group::Ptr{psr_set_group_t})::psr_error_t
end

function psr_element_has_scalars(element)
    @ccall libpsr_database_c.psr_element_has_scalars(element::Ptr{psr_element_t})::Cint
end

function psr_element_has_vector_groups(element)
    @ccall libpsr_database_c.psr_element_has_vector_groups(element::Ptr{psr_element_t})::Cint
end

function psr_element_has_set_groups(element)
    @ccall libpsr_database_c.psr_element_has_set_groups(element::Ptr{psr_element_t})::Cint
end

function psr_element_scalar_count(element)
    @ccall libpsr_database_c.psr_element_scalar_count(element::Ptr{psr_element_t})::Csize_t
end

function psr_element_vector_group_count(element)
    @ccall libpsr_database_c.psr_element_vector_group_count(element::Ptr{psr_element_t})::Csize_t
end

function psr_element_set_group_count(element)
    @ccall libpsr_database_c.psr_element_set_group_count(element::Ptr{psr_element_t})::Csize_t
end

function psr_element_to_string(element)
    @ccall libpsr_database_c.psr_element_to_string(element::Ptr{psr_element_t})::Ptr{Cchar}
end

function psr_string_free(str)
    @ccall libpsr_database_c.psr_string_free(str::Ptr{Cchar})::Cvoid
end

#! format: on


end # module
