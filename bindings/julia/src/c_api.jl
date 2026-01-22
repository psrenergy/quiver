module C

#! format: off

using CEnum
using Libdl

function library_name()
    if Sys.iswindows()
        return "libquiver_database_c.dll"
    elseif Sys.isapple()
        return "libquiver_database_c.dylib"
    else
        return "libquiver_database_c.so"
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

const libquiver_database_c = joinpath(@__DIR__, "..", "..", "..", "build", library_dir(), library_name())


@cenum quiver_error_t::Int32 begin
    QUIVER_OK = 0
    QUIVER_ERROR_INVALID_ARGUMENT = -1
    QUIVER_ERROR_DATABASE = -2
    QUIVER_ERROR_MIGRATION = -3
    QUIVER_ERROR_SCHEMA = -4
    QUIVER_ERROR_CREATE_ELEMENT = -5
    QUIVER_ERROR_NOT_FOUND = -6
end

function quiver_error_string(error)
    @ccall libquiver_database_c.quiver_error_string(error::quiver_error_t)::Ptr{Cchar}
end

function quiver_version()
    @ccall libquiver_database_c.quiver_version()::Ptr{Cchar}
end

@cenum quiver_log_level_t::UInt32 begin
    QUIVER_LOG_DEBUG = 0
    QUIVER_LOG_INFO = 1
    QUIVER_LOG_WARN = 2
    QUIVER_LOG_ERROR = 3
    QUIVER_LOG_OFF = 4
end

struct quiver_database_options_t
    read_only::Cint
    console_level::quiver_log_level_t
end

@cenum quiver_data_structure_t::UInt32 begin
    QUIVER_DATA_STRUCTURE_SCALAR = 0
    QUIVER_DATA_STRUCTURE_VECTOR = 1
    QUIVER_DATA_STRUCTURE_SET = 2
end

@cenum quiver_data_type_t::UInt32 begin
    QUIVER_DATA_TYPE_INTEGER = 0
    QUIVER_DATA_TYPE_FLOAT = 1
    QUIVER_DATA_TYPE_STRING = 2
end

function quiver_database_options_default()
    @ccall libquiver_database_c.quiver_database_options_default()::quiver_database_options_t
end

mutable struct quiver_database end

const quiver_database_t = quiver_database

function quiver_database_open(path, options)
    @ccall libquiver_database_c.quiver_database_open(path::Ptr{Cchar}, options::Ptr{quiver_database_options_t})::Ptr{quiver_database_t}
end

function quiver_database_from_migrations(db_path, migrations_path, options)
    @ccall libquiver_database_c.quiver_database_from_migrations(db_path::Ptr{Cchar}, migrations_path::Ptr{Cchar}, options::Ptr{quiver_database_options_t})::Ptr{quiver_database_t}
end

function quiver_database_from_schema(db_path, schema_path, options)
    @ccall libquiver_database_c.quiver_database_from_schema(db_path::Ptr{Cchar}, schema_path::Ptr{Cchar}, options::Ptr{quiver_database_options_t})::Ptr{quiver_database_t}
end

function quiver_database_close(db)
    @ccall libquiver_database_c.quiver_database_close(db::Ptr{quiver_database_t})::Cvoid
end

function quiver_database_is_healthy(db)
    @ccall libquiver_database_c.quiver_database_is_healthy(db::Ptr{quiver_database_t})::Cint
end

function quiver_database_path(db)
    @ccall libquiver_database_c.quiver_database_path(db::Ptr{quiver_database_t})::Ptr{Cchar}
end

function quiver_database_current_version(db)
    @ccall libquiver_database_c.quiver_database_current_version(db::Ptr{quiver_database_t})::Int64
end

mutable struct quiver_element end

const quiver_element_t = quiver_element

function quiver_database_create_element(db, collection, element)
    @ccall libquiver_database_c.quiver_database_create_element(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, element::Ptr{quiver_element_t})::Int64
end

function quiver_database_update_element(db, collection, id, element)
    @ccall libquiver_database_c.quiver_database_update_element(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, id::Int64, element::Ptr{quiver_element_t})::quiver_error_t
end

function quiver_database_delete_element_by_id(db, collection, id)
    @ccall libquiver_database_c.quiver_database_delete_element_by_id(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, id::Int64)::quiver_error_t
end

function quiver_database_set_scalar_relation(db, collection, attribute, from_label, to_label)
    @ccall libquiver_database_c.quiver_database_set_scalar_relation(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, from_label::Ptr{Cchar}, to_label::Ptr{Cchar})::quiver_error_t
end

function quiver_database_read_scalar_relation(db, collection, attribute, out_values, out_count)
    @ccall libquiver_database_c.quiver_database_read_scalar_relation(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_values::Ptr{Ptr{Ptr{Cchar}}}, out_count::Ptr{Csize_t})::quiver_error_t
end

function quiver_database_read_scalar_integers(db, collection, attribute, out_values, out_count)
    @ccall libquiver_database_c.quiver_database_read_scalar_integers(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_values::Ptr{Ptr{Int64}}, out_count::Ptr{Csize_t})::quiver_error_t
end

function quiver_database_read_scalar_floats(db, collection, attribute, out_values, out_count)
    @ccall libquiver_database_c.quiver_database_read_scalar_floats(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_values::Ptr{Ptr{Cdouble}}, out_count::Ptr{Csize_t})::quiver_error_t
end

function quiver_database_read_scalar_strings(db, collection, attribute, out_values, out_count)
    @ccall libquiver_database_c.quiver_database_read_scalar_strings(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_values::Ptr{Ptr{Ptr{Cchar}}}, out_count::Ptr{Csize_t})::quiver_error_t
end

function quiver_database_read_vector_integers(db, collection, attribute, out_vectors, out_sizes, out_count)
    @ccall libquiver_database_c.quiver_database_read_vector_integers(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_vectors::Ptr{Ptr{Ptr{Int64}}}, out_sizes::Ptr{Ptr{Csize_t}}, out_count::Ptr{Csize_t})::quiver_error_t
end

function quiver_database_read_vector_floats(db, collection, attribute, out_vectors, out_sizes, out_count)
    @ccall libquiver_database_c.quiver_database_read_vector_floats(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_vectors::Ptr{Ptr{Ptr{Cdouble}}}, out_sizes::Ptr{Ptr{Csize_t}}, out_count::Ptr{Csize_t})::quiver_error_t
end

function quiver_database_read_vector_strings(db, collection, attribute, out_vectors, out_sizes, out_count)
    @ccall libquiver_database_c.quiver_database_read_vector_strings(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_vectors::Ptr{Ptr{Ptr{Ptr{Cchar}}}}, out_sizes::Ptr{Ptr{Csize_t}}, out_count::Ptr{Csize_t})::quiver_error_t
end

function quiver_database_read_set_integers(db, collection, attribute, out_sets, out_sizes, out_count)
    @ccall libquiver_database_c.quiver_database_read_set_integers(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_sets::Ptr{Ptr{Ptr{Int64}}}, out_sizes::Ptr{Ptr{Csize_t}}, out_count::Ptr{Csize_t})::quiver_error_t
end

function quiver_database_read_set_floats(db, collection, attribute, out_sets, out_sizes, out_count)
    @ccall libquiver_database_c.quiver_database_read_set_floats(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_sets::Ptr{Ptr{Ptr{Cdouble}}}, out_sizes::Ptr{Ptr{Csize_t}}, out_count::Ptr{Csize_t})::quiver_error_t
end

function quiver_database_read_set_strings(db, collection, attribute, out_sets, out_sizes, out_count)
    @ccall libquiver_database_c.quiver_database_read_set_strings(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_sets::Ptr{Ptr{Ptr{Ptr{Cchar}}}}, out_sizes::Ptr{Ptr{Csize_t}}, out_count::Ptr{Csize_t})::quiver_error_t
end

function quiver_database_read_scalar_integers_by_id(db, collection, attribute, id, out_value, out_has_value)
    @ccall libquiver_database_c.quiver_database_read_scalar_integers_by_id(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, out_value::Ptr{Int64}, out_has_value::Ptr{Cint})::quiver_error_t
end

function quiver_database_read_scalar_floats_by_id(db, collection, attribute, id, out_value, out_has_value)
    @ccall libquiver_database_c.quiver_database_read_scalar_floats_by_id(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, out_value::Ptr{Cdouble}, out_has_value::Ptr{Cint})::quiver_error_t
end

function quiver_database_read_scalar_strings_by_id(db, collection, attribute, id, out_value, out_has_value)
    @ccall libquiver_database_c.quiver_database_read_scalar_strings_by_id(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, out_value::Ptr{Ptr{Cchar}}, out_has_value::Ptr{Cint})::quiver_error_t
end

function quiver_database_read_vector_integers_by_id(db, collection, attribute, id, out_values, out_count)
    @ccall libquiver_database_c.quiver_database_read_vector_integers_by_id(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, out_values::Ptr{Ptr{Int64}}, out_count::Ptr{Csize_t})::quiver_error_t
end

function quiver_database_read_vector_floats_by_id(db, collection, attribute, id, out_values, out_count)
    @ccall libquiver_database_c.quiver_database_read_vector_floats_by_id(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, out_values::Ptr{Ptr{Cdouble}}, out_count::Ptr{Csize_t})::quiver_error_t
end

function quiver_database_read_vector_strings_by_id(db, collection, attribute, id, out_values, out_count)
    @ccall libquiver_database_c.quiver_database_read_vector_strings_by_id(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, out_values::Ptr{Ptr{Ptr{Cchar}}}, out_count::Ptr{Csize_t})::quiver_error_t
end

function quiver_database_read_set_integers_by_id(db, collection, attribute, id, out_values, out_count)
    @ccall libquiver_database_c.quiver_database_read_set_integers_by_id(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, out_values::Ptr{Ptr{Int64}}, out_count::Ptr{Csize_t})::quiver_error_t
end

function quiver_database_read_set_floats_by_id(db, collection, attribute, id, out_values, out_count)
    @ccall libquiver_database_c.quiver_database_read_set_floats_by_id(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, out_values::Ptr{Ptr{Cdouble}}, out_count::Ptr{Csize_t})::quiver_error_t
end

function quiver_database_read_set_strings_by_id(db, collection, attribute, id, out_values, out_count)
    @ccall libquiver_database_c.quiver_database_read_set_strings_by_id(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, out_values::Ptr{Ptr{Ptr{Cchar}}}, out_count::Ptr{Csize_t})::quiver_error_t
end

function quiver_database_read_element_ids(db, collection, out_ids, out_count)
    @ccall libquiver_database_c.quiver_database_read_element_ids(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, out_ids::Ptr{Ptr{Int64}}, out_count::Ptr{Csize_t})::quiver_error_t
end

function quiver_database_get_attribute_type(db, collection, attribute, out_data_structure, out_data_type)
    @ccall libquiver_database_c.quiver_database_get_attribute_type(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_data_structure::Ptr{quiver_data_structure_t}, out_data_type::Ptr{quiver_data_type_t})::quiver_error_t
end

function quiver_database_update_scalar_integer(db, collection, attribute, id, value)
    @ccall libquiver_database_c.quiver_database_update_scalar_integer(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, value::Int64)::quiver_error_t
end

function quiver_database_update_scalar_float(db, collection, attribute, id, value)
    @ccall libquiver_database_c.quiver_database_update_scalar_float(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, value::Cdouble)::quiver_error_t
end

function quiver_database_update_scalar_string(db, collection, attribute, id, value)
    @ccall libquiver_database_c.quiver_database_update_scalar_string(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, value::Ptr{Cchar})::quiver_error_t
end

function quiver_database_update_vector_integers(db, collection, attribute, id, values, count)
    @ccall libquiver_database_c.quiver_database_update_vector_integers(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, values::Ptr{Int64}, count::Csize_t)::quiver_error_t
end

function quiver_database_update_vector_floats(db, collection, attribute, id, values, count)
    @ccall libquiver_database_c.quiver_database_update_vector_floats(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, values::Ptr{Cdouble}, count::Csize_t)::quiver_error_t
end

function quiver_database_update_vector_strings(db, collection, attribute, id, values, count)
    @ccall libquiver_database_c.quiver_database_update_vector_strings(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, values::Ptr{Ptr{Cchar}}, count::Csize_t)::quiver_error_t
end

function quiver_database_update_set_integers(db, collection, attribute, id, values, count)
    @ccall libquiver_database_c.quiver_database_update_set_integers(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, values::Ptr{Int64}, count::Csize_t)::quiver_error_t
end

function quiver_database_update_set_floats(db, collection, attribute, id, values, count)
    @ccall libquiver_database_c.quiver_database_update_set_floats(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, values::Ptr{Cdouble}, count::Csize_t)::quiver_error_t
end

function quiver_database_update_set_strings(db, collection, attribute, id, values, count)
    @ccall libquiver_database_c.quiver_database_update_set_strings(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, values::Ptr{Ptr{Cchar}}, count::Csize_t)::quiver_error_t
end

function quiver_free_integer_array(values)
    @ccall libquiver_database_c.quiver_free_integer_array(values::Ptr{Int64})::Cvoid
end

function quiver_free_float_array(values)
    @ccall libquiver_database_c.quiver_free_float_array(values::Ptr{Cdouble})::Cvoid
end

function quiver_free_string_array(values, count)
    @ccall libquiver_database_c.quiver_free_string_array(values::Ptr{Ptr{Cchar}}, count::Csize_t)::Cvoid
end

function quiver_free_integer_vectors(vectors, sizes, count)
    @ccall libquiver_database_c.quiver_free_integer_vectors(vectors::Ptr{Ptr{Int64}}, sizes::Ptr{Csize_t}, count::Csize_t)::Cvoid
end

function quiver_free_float_vectors(vectors, sizes, count)
    @ccall libquiver_database_c.quiver_free_float_vectors(vectors::Ptr{Ptr{Cdouble}}, sizes::Ptr{Csize_t}, count::Csize_t)::Cvoid
end

function quiver_free_string_vectors(vectors, sizes, count)
    @ccall libquiver_database_c.quiver_free_string_vectors(vectors::Ptr{Ptr{Ptr{Cchar}}}, sizes::Ptr{Csize_t}, count::Csize_t)::Cvoid
end

function quiver_element_create()
    @ccall libquiver_database_c.quiver_element_create()::Ptr{quiver_element_t}
end

function quiver_element_destroy(element)
    @ccall libquiver_database_c.quiver_element_destroy(element::Ptr{quiver_element_t})::Cvoid
end

function quiver_element_clear(element)
    @ccall libquiver_database_c.quiver_element_clear(element::Ptr{quiver_element_t})::Cvoid
end

function quiver_element_set_integer(element, name, value)
    @ccall libquiver_database_c.quiver_element_set_integer(element::Ptr{quiver_element_t}, name::Ptr{Cchar}, value::Int64)::quiver_error_t
end

function quiver_element_set_float(element, name, value)
    @ccall libquiver_database_c.quiver_element_set_float(element::Ptr{quiver_element_t}, name::Ptr{Cchar}, value::Cdouble)::quiver_error_t
end

function quiver_element_set_string(element, name, value)
    @ccall libquiver_database_c.quiver_element_set_string(element::Ptr{quiver_element_t}, name::Ptr{Cchar}, value::Ptr{Cchar})::quiver_error_t
end

function quiver_element_set_null(element, name)
    @ccall libquiver_database_c.quiver_element_set_null(element::Ptr{quiver_element_t}, name::Ptr{Cchar})::quiver_error_t
end

function quiver_element_set_array_integer(element, name, values, count)
    @ccall libquiver_database_c.quiver_element_set_array_integer(element::Ptr{quiver_element_t}, name::Ptr{Cchar}, values::Ptr{Int64}, count::Int32)::quiver_error_t
end

function quiver_element_set_array_float(element, name, values, count)
    @ccall libquiver_database_c.quiver_element_set_array_float(element::Ptr{quiver_element_t}, name::Ptr{Cchar}, values::Ptr{Cdouble}, count::Int32)::quiver_error_t
end

function quiver_element_set_array_string(element, name, values, count)
    @ccall libquiver_database_c.quiver_element_set_array_string(element::Ptr{quiver_element_t}, name::Ptr{Cchar}, values::Ptr{Ptr{Cchar}}, count::Int32)::quiver_error_t
end

function quiver_element_has_scalars(element)
    @ccall libquiver_database_c.quiver_element_has_scalars(element::Ptr{quiver_element_t})::Cint
end

function quiver_element_has_arrays(element)
    @ccall libquiver_database_c.quiver_element_has_arrays(element::Ptr{quiver_element_t})::Cint
end

function quiver_element_scalar_count(element)
    @ccall libquiver_database_c.quiver_element_scalar_count(element::Ptr{quiver_element_t})::Csize_t
end

function quiver_element_array_count(element)
    @ccall libquiver_database_c.quiver_element_array_count(element::Ptr{quiver_element_t})::Csize_t
end

function quiver_element_to_string(element)
    @ccall libquiver_database_c.quiver_element_to_string(element::Ptr{quiver_element_t})::Ptr{Cchar}
end

function quiver_string_free(str)
    @ccall libquiver_database_c.quiver_string_free(str::Ptr{Cchar})::Cvoid
end

mutable struct quiver_lua_runner end

const quiver_lua_runner_t = quiver_lua_runner

function quiver_lua_runner_new(db)
    @ccall libquiver_database_c.quiver_lua_runner_new(db::Ptr{quiver_database_t})::Ptr{quiver_lua_runner_t}
end

function quiver_lua_runner_free(runner)
    @ccall libquiver_database_c.quiver_lua_runner_free(runner::Ptr{quiver_lua_runner_t})::Cvoid
end

function quiver_lua_runner_run(runner, script)
    @ccall libquiver_database_c.quiver_lua_runner_run(runner::Ptr{quiver_lua_runner_t}, script::Ptr{Cchar})::quiver_error_t
end

function quiver_lua_runner_get_error(runner)
    @ccall libquiver_database_c.quiver_lua_runner_get_error(runner::Ptr{quiver_lua_runner_t})::Ptr{Cchar}
end

#! format: on


end # module
