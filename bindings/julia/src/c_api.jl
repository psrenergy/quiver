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

function margaux_error_string(error)
    @ccall libmargaux_c.margaux_error_string(error::margaux_error_t)::Ptr{Cchar}
end

function margaux_version()
    @ccall libmargaux_c.margaux_version()::Ptr{Cchar}
end

@cenum margaux_log_level_t::UInt32 begin
    MARGAUX_LOG_DEBUG = 0
    MARGAUX_LOG_INFO = 1
    MARGAUX_LOG_WARN = 2
    MARGAUX_LOG_ERROR = 3
    MARGAUX_LOG_OFF = 4
end

struct database_options_t
    read_only::Cint
    console_level::margaux_log_level_t
end

@cenum margaux_data_structure_t::UInt32 begin
    MARGAUX_DATA_STRUCTURE_SCALAR = 0
    MARGAUX_DATA_STRUCTURE_VECTOR = 1
    MARGAUX_DATA_STRUCTURE_SET = 2
end

@cenum margaux_data_type_t::UInt32 begin
    MARGAUX_DATA_TYPE_INTEGER = 0
    MARGAUX_DATA_TYPE_FLOAT = 1
    MARGAUX_DATA_TYPE_STRING = 2
end

function database_options_default()
    @ccall libmargaux_c.database_options_default()::database_options_t
end

mutable struct database end

const database_t = database

function database_open(path, options)
    @ccall libmargaux_c.database_open(path::Ptr{Cchar}, options::Ptr{database_options_t})::Ptr{database_t}
end

function database_from_migrations(db_path, migrations_path, options)
    @ccall libmargaux_c.database_from_migrations(db_path::Ptr{Cchar}, migrations_path::Ptr{Cchar}, options::Ptr{database_options_t})::Ptr{database_t}
end

function database_from_schema(db_path, schema_path, options)
    @ccall libmargaux_c.database_from_schema(db_path::Ptr{Cchar}, schema_path::Ptr{Cchar}, options::Ptr{database_options_t})::Ptr{database_t}
end

function database_close(db)
    @ccall libmargaux_c.database_close(db::Ptr{database_t})::Cvoid
end

function database_is_healthy(db)
    @ccall libmargaux_c.database_is_healthy(db::Ptr{database_t})::Cint
end

function database_path(db)
    @ccall libmargaux_c.database_path(db::Ptr{database_t})::Ptr{Cchar}
end

function database_current_version(db)
    @ccall libmargaux_c.database_current_version(db::Ptr{database_t})::Int64
end

mutable struct element end

const element_t = element

function database_create_element(db, collection, element)
    @ccall libmargaux_c.database_create_element(db::Ptr{database_t}, collection::Ptr{Cchar}, element::Ptr{element_t})::Int64
end

function database_update_element(db, collection, id, element)
    @ccall libmargaux_c.database_update_element(db::Ptr{database_t}, collection::Ptr{Cchar}, id::Int64, element::Ptr{element_t})::margaux_error_t
end

function database_delete_element_by_id(db, collection, id)
    @ccall libmargaux_c.database_delete_element_by_id(db::Ptr{database_t}, collection::Ptr{Cchar}, id::Int64)::margaux_error_t
end

function database_set_scalar_relation(db, collection, attribute, from_label, to_label)
    @ccall libmargaux_c.database_set_scalar_relation(db::Ptr{database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, from_label::Ptr{Cchar}, to_label::Ptr{Cchar})::margaux_error_t
end

function database_read_scalar_relation(db, collection, attribute, out_values, out_count)
    @ccall libmargaux_c.database_read_scalar_relation(db::Ptr{database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_values::Ptr{Ptr{Ptr{Cchar}}}, out_count::Ptr{Csize_t})::margaux_error_t
end

function database_read_scalar_integers(db, collection, attribute, out_values, out_count)
    @ccall libmargaux_c.database_read_scalar_integers(db::Ptr{database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_values::Ptr{Ptr{Int64}}, out_count::Ptr{Csize_t})::margaux_error_t
end

function database_read_scalar_floats(db, collection, attribute, out_values, out_count)
    @ccall libmargaux_c.database_read_scalar_floats(db::Ptr{database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_values::Ptr{Ptr{Cdouble}}, out_count::Ptr{Csize_t})::margaux_error_t
end

function database_read_scalar_strings(db, collection, attribute, out_values, out_count)
    @ccall libmargaux_c.database_read_scalar_strings(db::Ptr{database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_values::Ptr{Ptr{Ptr{Cchar}}}, out_count::Ptr{Csize_t})::margaux_error_t
end

function database_read_vector_integers(db, collection, attribute, out_vectors, out_sizes, out_count)
    @ccall libmargaux_c.database_read_vector_integers(db::Ptr{database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_vectors::Ptr{Ptr{Ptr{Int64}}}, out_sizes::Ptr{Ptr{Csize_t}}, out_count::Ptr{Csize_t})::margaux_error_t
end

function database_read_vector_floats(db, collection, attribute, out_vectors, out_sizes, out_count)
    @ccall libmargaux_c.database_read_vector_floats(db::Ptr{database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_vectors::Ptr{Ptr{Ptr{Cdouble}}}, out_sizes::Ptr{Ptr{Csize_t}}, out_count::Ptr{Csize_t})::margaux_error_t
end

function database_read_vector_strings(db, collection, attribute, out_vectors, out_sizes, out_count)
    @ccall libmargaux_c.database_read_vector_strings(db::Ptr{database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_vectors::Ptr{Ptr{Ptr{Ptr{Cchar}}}}, out_sizes::Ptr{Ptr{Csize_t}}, out_count::Ptr{Csize_t})::margaux_error_t
end

function database_read_set_integers(db, collection, attribute, out_sets, out_sizes, out_count)
    @ccall libmargaux_c.database_read_set_integers(db::Ptr{database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_sets::Ptr{Ptr{Ptr{Int64}}}, out_sizes::Ptr{Ptr{Csize_t}}, out_count::Ptr{Csize_t})::margaux_error_t
end

function database_read_set_floats(db, collection, attribute, out_sets, out_sizes, out_count)
    @ccall libmargaux_c.database_read_set_floats(db::Ptr{database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_sets::Ptr{Ptr{Ptr{Cdouble}}}, out_sizes::Ptr{Ptr{Csize_t}}, out_count::Ptr{Csize_t})::margaux_error_t
end

function database_read_set_strings(db, collection, attribute, out_sets, out_sizes, out_count)
    @ccall libmargaux_c.database_read_set_strings(db::Ptr{database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_sets::Ptr{Ptr{Ptr{Ptr{Cchar}}}}, out_sizes::Ptr{Ptr{Csize_t}}, out_count::Ptr{Csize_t})::margaux_error_t
end

function database_read_scalar_integers_by_id(db, collection, attribute, id, out_value, out_has_value)
    @ccall libmargaux_c.database_read_scalar_integers_by_id(db::Ptr{database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, out_value::Ptr{Int64}, out_has_value::Ptr{Cint})::margaux_error_t
end

function database_read_scalar_floats_by_id(db, collection, attribute, id, out_value, out_has_value)
    @ccall libmargaux_c.database_read_scalar_floats_by_id(db::Ptr{database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, out_value::Ptr{Cdouble}, out_has_value::Ptr{Cint})::margaux_error_t
end

function database_read_scalar_strings_by_id(db, collection, attribute, id, out_value, out_has_value)
    @ccall libmargaux_c.database_read_scalar_strings_by_id(db::Ptr{database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, out_value::Ptr{Ptr{Cchar}}, out_has_value::Ptr{Cint})::margaux_error_t
end

function database_read_vector_integers_by_id(db, collection, attribute, id, out_values, out_count)
    @ccall libmargaux_c.database_read_vector_integers_by_id(db::Ptr{database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, out_values::Ptr{Ptr{Int64}}, out_count::Ptr{Csize_t})::margaux_error_t
end

function database_read_vector_floats_by_id(db, collection, attribute, id, out_values, out_count)
    @ccall libmargaux_c.database_read_vector_floats_by_id(db::Ptr{database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, out_values::Ptr{Ptr{Cdouble}}, out_count::Ptr{Csize_t})::margaux_error_t
end

function database_read_vector_strings_by_id(db, collection, attribute, id, out_values, out_count)
    @ccall libmargaux_c.database_read_vector_strings_by_id(db::Ptr{database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, out_values::Ptr{Ptr{Ptr{Cchar}}}, out_count::Ptr{Csize_t})::margaux_error_t
end

function database_read_set_integers_by_id(db, collection, attribute, id, out_values, out_count)
    @ccall libmargaux_c.database_read_set_integers_by_id(db::Ptr{database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, out_values::Ptr{Ptr{Int64}}, out_count::Ptr{Csize_t})::margaux_error_t
end

function database_read_set_floats_by_id(db, collection, attribute, id, out_values, out_count)
    @ccall libmargaux_c.database_read_set_floats_by_id(db::Ptr{database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, out_values::Ptr{Ptr{Cdouble}}, out_count::Ptr{Csize_t})::margaux_error_t
end

function database_read_set_strings_by_id(db, collection, attribute, id, out_values, out_count)
    @ccall libmargaux_c.database_read_set_strings_by_id(db::Ptr{database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, out_values::Ptr{Ptr{Ptr{Cchar}}}, out_count::Ptr{Csize_t})::margaux_error_t
end

function database_read_element_ids(db, collection, out_ids, out_count)
    @ccall libmargaux_c.database_read_element_ids(db::Ptr{database_t}, collection::Ptr{Cchar}, out_ids::Ptr{Ptr{Int64}}, out_count::Ptr{Csize_t})::margaux_error_t
end

function database_get_attribute_type(db, collection, attribute, out_data_structure, out_data_type)
    @ccall libmargaux_c.database_get_attribute_type(db::Ptr{database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_data_structure::Ptr{margaux_data_structure_t}, out_data_type::Ptr{margaux_data_type_t})::margaux_error_t
end

function database_update_scalar_integer(db, collection, attribute, id, value)
    @ccall libmargaux_c.database_update_scalar_integer(db::Ptr{database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, value::Int64)::margaux_error_t
end

function database_update_scalar_float(db, collection, attribute, id, value)
    @ccall libmargaux_c.database_update_scalar_float(db::Ptr{database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, value::Cdouble)::margaux_error_t
end

function database_update_scalar_string(db, collection, attribute, id, value)
    @ccall libmargaux_c.database_update_scalar_string(db::Ptr{database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, value::Ptr{Cchar})::margaux_error_t
end

function database_update_vector_integers(db, collection, attribute, id, values, count)
    @ccall libmargaux_c.database_update_vector_integers(db::Ptr{database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, values::Ptr{Int64}, count::Csize_t)::margaux_error_t
end

function database_update_vector_floats(db, collection, attribute, id, values, count)
    @ccall libmargaux_c.database_update_vector_floats(db::Ptr{database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, values::Ptr{Cdouble}, count::Csize_t)::margaux_error_t
end

function database_update_vector_strings(db, collection, attribute, id, values, count)
    @ccall libmargaux_c.database_update_vector_strings(db::Ptr{database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, values::Ptr{Ptr{Cchar}}, count::Csize_t)::margaux_error_t
end

function database_update_set_integers(db, collection, attribute, id, values, count)
    @ccall libmargaux_c.database_update_set_integers(db::Ptr{database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, values::Ptr{Int64}, count::Csize_t)::margaux_error_t
end

function database_update_set_floats(db, collection, attribute, id, values, count)
    @ccall libmargaux_c.database_update_set_floats(db::Ptr{database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, values::Ptr{Cdouble}, count::Csize_t)::margaux_error_t
end

function database_update_set_strings(db, collection, attribute, id, values, count)
    @ccall libmargaux_c.database_update_set_strings(db::Ptr{database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, values::Ptr{Ptr{Cchar}}, count::Csize_t)::margaux_error_t
end

function margaux_free_integer_array(values)
    @ccall libmargaux_c.margaux_free_integer_array(values::Ptr{Int64})::Cvoid
end

function margaux_free_float_array(values)
    @ccall libmargaux_c.margaux_free_float_array(values::Ptr{Cdouble})::Cvoid
end

function margaux_free_string_array(values, count)
    @ccall libmargaux_c.margaux_free_string_array(values::Ptr{Ptr{Cchar}}, count::Csize_t)::Cvoid
end

function margaux_free_integer_vectors(vectors, sizes, count)
    @ccall libmargaux_c.margaux_free_integer_vectors(vectors::Ptr{Ptr{Int64}}, sizes::Ptr{Csize_t}, count::Csize_t)::Cvoid
end

function margaux_free_float_vectors(vectors, sizes, count)
    @ccall libmargaux_c.margaux_free_float_vectors(vectors::Ptr{Ptr{Cdouble}}, sizes::Ptr{Csize_t}, count::Csize_t)::Cvoid
end

function margaux_free_string_vectors(vectors, sizes, count)
    @ccall libmargaux_c.margaux_free_string_vectors(vectors::Ptr{Ptr{Ptr{Cchar}}}, sizes::Ptr{Csize_t}, count::Csize_t)::Cvoid
end

function element_create()
    @ccall libmargaux_c.element_create()::Ptr{element_t}
end

function element_destroy(element)
    @ccall libmargaux_c.element_destroy(element::Ptr{element_t})::Cvoid
end

function element_clear(element)
    @ccall libmargaux_c.element_clear(element::Ptr{element_t})::Cvoid
end

function element_set_integer(element, name, value)
    @ccall libmargaux_c.element_set_integer(element::Ptr{element_t}, name::Ptr{Cchar}, value::Int64)::margaux_error_t
end

function element_set_float(element, name, value)
    @ccall libmargaux_c.element_set_float(element::Ptr{element_t}, name::Ptr{Cchar}, value::Cdouble)::margaux_error_t
end

function element_set_string(element, name, value)
    @ccall libmargaux_c.element_set_string(element::Ptr{element_t}, name::Ptr{Cchar}, value::Ptr{Cchar})::margaux_error_t
end

function element_set_null(element, name)
    @ccall libmargaux_c.element_set_null(element::Ptr{element_t}, name::Ptr{Cchar})::margaux_error_t
end

function element_set_array_integer(element, name, values, count)
    @ccall libmargaux_c.element_set_array_integer(element::Ptr{element_t}, name::Ptr{Cchar}, values::Ptr{Int64}, count::Int32)::margaux_error_t
end

function element_set_array_float(element, name, values, count)
    @ccall libmargaux_c.element_set_array_float(element::Ptr{element_t}, name::Ptr{Cchar}, values::Ptr{Cdouble}, count::Int32)::margaux_error_t
end

function element_set_array_string(element, name, values, count)
    @ccall libmargaux_c.element_set_array_string(element::Ptr{element_t}, name::Ptr{Cchar}, values::Ptr{Ptr{Cchar}}, count::Int32)::margaux_error_t
end

function element_has_scalars(element)
    @ccall libmargaux_c.element_has_scalars(element::Ptr{element_t})::Cint
end

function element_has_arrays(element)
    @ccall libmargaux_c.element_has_arrays(element::Ptr{element_t})::Cint
end

function element_scalar_count(element)
    @ccall libmargaux_c.element_scalar_count(element::Ptr{element_t})::Csize_t
end

function element_array_count(element)
    @ccall libmargaux_c.element_array_count(element::Ptr{element_t})::Csize_t
end

function element_to_string(element)
    @ccall libmargaux_c.element_to_string(element::Ptr{element_t})::Ptr{Cchar}
end

function margaux_string_free(str)
    @ccall libmargaux_c.margaux_string_free(str::Ptr{Cchar})::Cvoid
end

mutable struct lua_runner end

const lua_runner_t = lua_runner

function lua_runner_new(db)
    @ccall libmargaux_c.lua_runner_new(db::Ptr{database_t})::Ptr{lua_runner_t}
end

function lua_runner_free(runner)
    @ccall libmargaux_c.lua_runner_free(runner::Ptr{lua_runner_t})::Cvoid
end

function lua_runner_run(runner, script)
    @ccall libmargaux_c.lua_runner_run(runner::Ptr{lua_runner_t}, script::Ptr{Cchar})::margaux_error_t
end

function lua_runner_get_error(runner)
    @ccall libmargaux_c.lua_runner_get_error(runner::Ptr{lua_runner_t})::Ptr{Cchar}
end

#! format: on


end # module
