module C

#! format: off

using CEnum
using Libdl

function library_name()
    if Sys.iswindows()
        return "libmargaux_c.dll"
    elseif Sys.isapple()
        return "libmargaux_c.dylib"
    else
        return "libmargaux_c.so"
    end
end

# On Windows, DLLs go to bin/; on Linux/macOS, shared libs go to lib/
function library_dir()
    if Sys.iswindows()
        return "bin"
    else
        return "lib"
    end
end

const libmargaux_c = joinpath(@__DIR__, "..", "..", "..", "build", library_dir(), library_name())


@cenum margaux_error_t::Int32 begin
    MARGAUX_OK = 0
    MARGAUX_ERROR_INVALID_ARGUMENT = -1
    MARGAUX_ERROR_DATABASE = -2
    MARGAUX_ERROR_MIGRATION = -3
    MARGAUX_ERROR_SCHEMA = -4
    MARGAUX_ERROR_CREATE_ELEMENT = -5
    MARGAUX_ERROR_NOT_FOUND = -6
end

function psr_error_string(error)
    @ccall libmargaux_c.psr_error_string(error::margaux_error_t)::Ptr{Cchar}
end

function psr_version()
    @ccall libmargaux_c.psr_version()::Ptr{Cchar}
end

@cenum psr_log_level_t::UInt32 begin
    MARGAUX_LOG_DEBUG = 0
    MARGAUX_LOG_INFO = 1
    MARGAUX_LOG_WARN = 2
    MARGAUX_LOG_ERROR = 3
    MARGAUX_LOG_OFF = 4
end

struct psr_database_options_t
    read_only::Cint
    console_level::psr_log_level_t
end

@cenum psr_data_structure_t::UInt32 begin
    MARGAUX_DATA_STRUCTURE_SCALAR = 0
    MARGAUX_DATA_STRUCTURE_VECTOR = 1
    MARGAUX_DATA_STRUCTURE_SET = 2
end

@cenum psr_data_type_t::UInt32 begin
    MARGAUX_DATA_TYPE_INTEGER = 0
    MARGAUX_DATA_TYPE_FLOAT = 1
    MARGAUX_DATA_TYPE_STRING = 2
end

function psr_database_options_default()
    @ccall libmargaux_c.psr_database_options_default()::psr_database_options_t
end

mutable struct psr_database end

const psr_database_t = psr_database

function psr_database_open(path, options)
    @ccall libmargaux_c.psr_database_open(path::Ptr{Cchar}, options::Ptr{psr_database_options_t})::Ptr{psr_database_t}
end

function psr_database_from_migrations(db_path, migrations_path, options)
    @ccall libmargaux_c.psr_database_from_migrations(db_path::Ptr{Cchar}, migrations_path::Ptr{Cchar}, options::Ptr{psr_database_options_t})::Ptr{psr_database_t}
end

function psr_database_from_schema(db_path, schema_path, options)
    @ccall libmargaux_c.psr_database_from_schema(db_path::Ptr{Cchar}, schema_path::Ptr{Cchar}, options::Ptr{psr_database_options_t})::Ptr{psr_database_t}
end

function psr_database_close(db)
    @ccall libmargaux_c.psr_database_close(db::Ptr{psr_database_t})::Cvoid
end

function psr_database_is_healthy(db)
    @ccall libmargaux_c.psr_database_is_healthy(db::Ptr{psr_database_t})::Cint
end

function psr_database_path(db)
    @ccall libmargaux_c.psr_database_path(db::Ptr{psr_database_t})::Ptr{Cchar}
end

function psr_database_current_version(db)
    @ccall libmargaux_c.psr_database_current_version(db::Ptr{psr_database_t})::Int64
end

mutable struct margaux_element end

const margaux_element_t = margaux_element

function psr_database_create_element(db, collection, element)
    @ccall libmargaux_c.psr_database_create_element(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, element::Ptr{margaux_element_t})::Int64
end

function psr_database_update_element(db, collection, id, element)
    @ccall libmargaux_c.psr_database_update_element(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, id::Int64, element::Ptr{margaux_element_t})::margaux_error_t
end

function psr_database_delete_element_by_id(db, collection, id)
    @ccall libmargaux_c.psr_database_delete_element_by_id(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, id::Int64)::margaux_error_t
end

function psr_database_set_scalar_relation(db, collection, attribute, from_label, to_label)
    @ccall libmargaux_c.psr_database_set_scalar_relation(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, from_label::Ptr{Cchar}, to_label::Ptr{Cchar})::margaux_error_t
end

function psr_database_read_scalar_relation(db, collection, attribute, out_values, out_count)
    @ccall libmargaux_c.psr_database_read_scalar_relation(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_values::Ptr{Ptr{Ptr{Cchar}}}, out_count::Ptr{Csize_t})::margaux_error_t
end

function psr_database_read_scalar_integers(db, collection, attribute, out_values, out_count)
    @ccall libmargaux_c.psr_database_read_scalar_integers(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_values::Ptr{Ptr{Int64}}, out_count::Ptr{Csize_t})::margaux_error_t
end

function psr_database_read_scalar_floats(db, collection, attribute, out_values, out_count)
    @ccall libmargaux_c.psr_database_read_scalar_floats(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_values::Ptr{Ptr{Cdouble}}, out_count::Ptr{Csize_t})::margaux_error_t
end

function psr_database_read_scalar_strings(db, collection, attribute, out_values, out_count)
    @ccall libmargaux_c.psr_database_read_scalar_strings(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_values::Ptr{Ptr{Ptr{Cchar}}}, out_count::Ptr{Csize_t})::margaux_error_t
end

function psr_database_read_vector_integers(db, collection, attribute, out_vectors, out_sizes, out_count)
    @ccall libmargaux_c.psr_database_read_vector_integers(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_vectors::Ptr{Ptr{Ptr{Int64}}}, out_sizes::Ptr{Ptr{Csize_t}}, out_count::Ptr{Csize_t})::margaux_error_t
end

function psr_database_read_vector_floats(db, collection, attribute, out_vectors, out_sizes, out_count)
    @ccall libmargaux_c.psr_database_read_vector_floats(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_vectors::Ptr{Ptr{Ptr{Cdouble}}}, out_sizes::Ptr{Ptr{Csize_t}}, out_count::Ptr{Csize_t})::margaux_error_t
end

function psr_database_read_vector_strings(db, collection, attribute, out_vectors, out_sizes, out_count)
    @ccall libmargaux_c.psr_database_read_vector_strings(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_vectors::Ptr{Ptr{Ptr{Ptr{Cchar}}}}, out_sizes::Ptr{Ptr{Csize_t}}, out_count::Ptr{Csize_t})::margaux_error_t
end

function psr_database_read_set_integers(db, collection, attribute, out_sets, out_sizes, out_count)
    @ccall libmargaux_c.psr_database_read_set_integers(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_sets::Ptr{Ptr{Ptr{Int64}}}, out_sizes::Ptr{Ptr{Csize_t}}, out_count::Ptr{Csize_t})::margaux_error_t
end

function psr_database_read_set_floats(db, collection, attribute, out_sets, out_sizes, out_count)
    @ccall libmargaux_c.psr_database_read_set_floats(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_sets::Ptr{Ptr{Ptr{Cdouble}}}, out_sizes::Ptr{Ptr{Csize_t}}, out_count::Ptr{Csize_t})::margaux_error_t
end

function psr_database_read_set_strings(db, collection, attribute, out_sets, out_sizes, out_count)
    @ccall libmargaux_c.psr_database_read_set_strings(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_sets::Ptr{Ptr{Ptr{Ptr{Cchar}}}}, out_sizes::Ptr{Ptr{Csize_t}}, out_count::Ptr{Csize_t})::margaux_error_t
end

function psr_database_read_scalar_integers_by_id(db, collection, attribute, id, out_value, out_has_value)
    @ccall libmargaux_c.psr_database_read_scalar_integers_by_id(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, out_value::Ptr{Int64}, out_has_value::Ptr{Cint})::margaux_error_t
end

function psr_database_read_scalar_floats_by_id(db, collection, attribute, id, out_value, out_has_value)
    @ccall libmargaux_c.psr_database_read_scalar_floats_by_id(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, out_value::Ptr{Cdouble}, out_has_value::Ptr{Cint})::margaux_error_t
end

function psr_database_read_scalar_strings_by_id(db, collection, attribute, id, out_value, out_has_value)
    @ccall libmargaux_c.psr_database_read_scalar_strings_by_id(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, out_value::Ptr{Ptr{Cchar}}, out_has_value::Ptr{Cint})::margaux_error_t
end

function psr_database_read_vector_integers_by_id(db, collection, attribute, id, out_values, out_count)
    @ccall libmargaux_c.psr_database_read_vector_integers_by_id(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, out_values::Ptr{Ptr{Int64}}, out_count::Ptr{Csize_t})::margaux_error_t
end

function psr_database_read_vector_floats_by_id(db, collection, attribute, id, out_values, out_count)
    @ccall libmargaux_c.psr_database_read_vector_floats_by_id(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, out_values::Ptr{Ptr{Cdouble}}, out_count::Ptr{Csize_t})::margaux_error_t
end

function psr_database_read_vector_strings_by_id(db, collection, attribute, id, out_values, out_count)
    @ccall libmargaux_c.psr_database_read_vector_strings_by_id(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, out_values::Ptr{Ptr{Ptr{Cchar}}}, out_count::Ptr{Csize_t})::margaux_error_t
end

function psr_database_read_set_integers_by_id(db, collection, attribute, id, out_values, out_count)
    @ccall libmargaux_c.psr_database_read_set_integers_by_id(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, out_values::Ptr{Ptr{Int64}}, out_count::Ptr{Csize_t})::margaux_error_t
end

function psr_database_read_set_floats_by_id(db, collection, attribute, id, out_values, out_count)
    @ccall libmargaux_c.psr_database_read_set_floats_by_id(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, out_values::Ptr{Ptr{Cdouble}}, out_count::Ptr{Csize_t})::margaux_error_t
end

function psr_database_read_set_strings_by_id(db, collection, attribute, id, out_values, out_count)
    @ccall libmargaux_c.psr_database_read_set_strings_by_id(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, out_values::Ptr{Ptr{Ptr{Cchar}}}, out_count::Ptr{Csize_t})::margaux_error_t
end

function psr_database_read_element_ids(db, collection, out_ids, out_count)
    @ccall libmargaux_c.psr_database_read_element_ids(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, out_ids::Ptr{Ptr{Int64}}, out_count::Ptr{Csize_t})::margaux_error_t
end

function psr_database_get_attribute_type(db, collection, attribute, out_data_structure, out_data_type)
    @ccall libmargaux_c.psr_database_get_attribute_type(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_data_structure::Ptr{psr_data_structure_t}, out_data_type::Ptr{psr_data_type_t})::margaux_error_t
end

function psr_database_update_scalar_integer(db, collection, attribute, id, value)
    @ccall libmargaux_c.psr_database_update_scalar_integer(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, value::Int64)::margaux_error_t
end

function psr_database_update_scalar_float(db, collection, attribute, id, value)
    @ccall libmargaux_c.psr_database_update_scalar_float(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, value::Cdouble)::margaux_error_t
end

function psr_database_update_scalar_string(db, collection, attribute, id, value)
    @ccall libmargaux_c.psr_database_update_scalar_string(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, value::Ptr{Cchar})::margaux_error_t
end

function psr_database_update_vector_integers(db, collection, attribute, id, values, count)
    @ccall libmargaux_c.psr_database_update_vector_integers(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, values::Ptr{Int64}, count::Csize_t)::margaux_error_t
end

function psr_database_update_vector_floats(db, collection, attribute, id, values, count)
    @ccall libmargaux_c.psr_database_update_vector_floats(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, values::Ptr{Cdouble}, count::Csize_t)::margaux_error_t
end

function psr_database_update_vector_strings(db, collection, attribute, id, values, count)
    @ccall libmargaux_c.psr_database_update_vector_strings(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, values::Ptr{Ptr{Cchar}}, count::Csize_t)::margaux_error_t
end

function psr_database_update_set_integers(db, collection, attribute, id, values, count)
    @ccall libmargaux_c.psr_database_update_set_integers(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, values::Ptr{Int64}, count::Csize_t)::margaux_error_t
end

function psr_database_update_set_floats(db, collection, attribute, id, values, count)
    @ccall libmargaux_c.psr_database_update_set_floats(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, values::Ptr{Cdouble}, count::Csize_t)::margaux_error_t
end

function psr_database_update_set_strings(db, collection, attribute, id, values, count)
    @ccall libmargaux_c.psr_database_update_set_strings(db::Ptr{psr_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, values::Ptr{Ptr{Cchar}}, count::Csize_t)::margaux_error_t
end

function psr_free_integer_array(values)
    @ccall libmargaux_c.psr_free_integer_array(values::Ptr{Int64})::Cvoid
end

function psr_free_float_array(values)
    @ccall libmargaux_c.psr_free_float_array(values::Ptr{Cdouble})::Cvoid
end

function psr_free_string_array(values, count)
    @ccall libmargaux_c.psr_free_string_array(values::Ptr{Ptr{Cchar}}, count::Csize_t)::Cvoid
end

function psr_free_integer_vectors(vectors, sizes, count)
    @ccall libmargaux_c.psr_free_integer_vectors(vectors::Ptr{Ptr{Int64}}, sizes::Ptr{Csize_t}, count::Csize_t)::Cvoid
end

function psr_free_float_vectors(vectors, sizes, count)
    @ccall libmargaux_c.psr_free_float_vectors(vectors::Ptr{Ptr{Cdouble}}, sizes::Ptr{Csize_t}, count::Csize_t)::Cvoid
end

function psr_free_string_vectors(vectors, sizes, count)
    @ccall libmargaux_c.psr_free_string_vectors(vectors::Ptr{Ptr{Ptr{Cchar}}}, sizes::Ptr{Csize_t}, count::Csize_t)::Cvoid
end

function margaux_element_create()
    @ccall libmargaux_c.margaux_element_create()::Ptr{margaux_element_t}
end

function margaux_element_destroy(element)
    @ccall libmargaux_c.margaux_element_destroy(element::Ptr{margaux_element_t})::Cvoid
end

function margaux_element_clear(element)
    @ccall libmargaux_c.margaux_element_clear(element::Ptr{margaux_element_t})::Cvoid
end

function margaux_element_set_integer(element, name, value)
    @ccall libmargaux_c.margaux_element_set_integer(element::Ptr{margaux_element_t}, name::Ptr{Cchar}, value::Int64)::margaux_error_t
end

function margaux_element_set_float(element, name, value)
    @ccall libmargaux_c.margaux_element_set_float(element::Ptr{margaux_element_t}, name::Ptr{Cchar}, value::Cdouble)::margaux_error_t
end

function margaux_element_set_string(element, name, value)
    @ccall libmargaux_c.margaux_element_set_string(element::Ptr{margaux_element_t}, name::Ptr{Cchar}, value::Ptr{Cchar})::margaux_error_t
end

function margaux_element_set_null(element, name)
    @ccall libmargaux_c.margaux_element_set_null(element::Ptr{margaux_element_t}, name::Ptr{Cchar})::margaux_error_t
end

function margaux_element_set_array_integer(element, name, values, count)
    @ccall libmargaux_c.margaux_element_set_array_integer(element::Ptr{margaux_element_t}, name::Ptr{Cchar}, values::Ptr{Int64}, count::Int32)::margaux_error_t
end

function margaux_element_set_array_float(element, name, values, count)
    @ccall libmargaux_c.margaux_element_set_array_float(element::Ptr{margaux_element_t}, name::Ptr{Cchar}, values::Ptr{Cdouble}, count::Int32)::margaux_error_t
end

function margaux_element_set_array_string(element, name, values, count)
    @ccall libmargaux_c.margaux_element_set_array_string(element::Ptr{margaux_element_t}, name::Ptr{Cchar}, values::Ptr{Ptr{Cchar}}, count::Int32)::margaux_error_t
end

function margaux_element_has_scalars(element)
    @ccall libmargaux_c.margaux_element_has_scalars(element::Ptr{margaux_element_t})::Cint
end

function margaux_element_has_arrays(element)
    @ccall libmargaux_c.margaux_element_has_arrays(element::Ptr{margaux_element_t})::Cint
end

function margaux_element_scalar_count(element)
    @ccall libmargaux_c.margaux_element_scalar_count(element::Ptr{margaux_element_t})::Csize_t
end

function margaux_element_array_count(element)
    @ccall libmargaux_c.margaux_element_array_count(element::Ptr{margaux_element_t})::Csize_t
end

function margaux_element_to_string(element)
    @ccall libmargaux_c.margaux_element_to_string(element::Ptr{margaux_element_t})::Ptr{Cchar}
end

function psr_string_free(str)
    @ccall libmargaux_c.psr_string_free(str::Ptr{Cchar})::Cvoid
end

mutable struct psr_lua_runner end

const psr_lua_runner_t = psr_lua_runner

function psr_lua_runner_new(db)
    @ccall libmargaux_c.psr_lua_runner_new(db::Ptr{psr_database_t})::Ptr{psr_lua_runner_t}
end

function psr_lua_runner_free(runner)
    @ccall libmargaux_c.psr_lua_runner_free(runner::Ptr{psr_lua_runner_t})::Cvoid
end

function psr_lua_runner_run(runner, script)
    @ccall libmargaux_c.psr_lua_runner_run(runner::Ptr{psr_lua_runner_t}, script::Ptr{Cchar})::margaux_error_t
end

function psr_lua_runner_get_error(runner)
    @ccall libmargaux_c.psr_lua_runner_get_error(runner::Ptr{psr_lua_runner_t})::Ptr{Cchar}
end

#! format: on


end # module
