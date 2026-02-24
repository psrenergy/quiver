module C

#! format: off

using CEnum
using Libdl

function library_name()
    if Sys.iswindows()
        return "libquiver_c.dll"
    elseif Sys.isapple()
        return "libquiver_c.dylib"
    else
        return "libquiver_c.so"
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

const libquiver_c = joinpath(@__DIR__, "..", "..", "..", "build", library_dir(), library_name())


@cenum quiver_error_t::UInt32 begin
    QUIVER_OK = 0
    QUIVER_ERROR = 1
end

function quiver_version()
    @ccall libquiver_c.quiver_version()::Ptr{Cchar}
end

function quiver_get_last_error()
    @ccall libquiver_c.quiver_get_last_error()::Ptr{Cchar}
end

function quiver_clear_last_error()
    @ccall libquiver_c.quiver_clear_last_error()::Cvoid
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

struct quiver_csv_export_options_t
    date_time_format::Ptr{Cchar}
    enum_attribute_names::Ptr{Ptr{Cchar}}
    enum_entry_counts::Ptr{Csize_t}
    enum_values::Ptr{Int64}
    enum_labels::Ptr{Ptr{Cchar}}
    enum_attribute_count::Csize_t
end

function quiver_csv_export_options_default()
    @ccall libquiver_c.quiver_csv_export_options_default()::quiver_csv_export_options_t
end

function quiver_database_options_default()
    @ccall libquiver_c.quiver_database_options_default()::quiver_database_options_t
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
    QUIVER_DATA_TYPE_DATE_TIME = 3
    QUIVER_DATA_TYPE_NULL = 4
end

mutable struct quiver_database end

const quiver_database_t = quiver_database

function quiver_database_open(path, options, out_db)
    @ccall libquiver_c.quiver_database_open(path::Ptr{Cchar}, options::Ptr{quiver_database_options_t}, out_db::Ptr{Ptr{quiver_database_t}})::quiver_error_t
end

function quiver_database_from_migrations(db_path, migrations_path, options, out_db)
    @ccall libquiver_c.quiver_database_from_migrations(db_path::Ptr{Cchar}, migrations_path::Ptr{Cchar}, options::Ptr{quiver_database_options_t}, out_db::Ptr{Ptr{quiver_database_t}})::quiver_error_t
end

function quiver_database_from_schema(db_path, schema_path, options, out_db)
    @ccall libquiver_c.quiver_database_from_schema(db_path::Ptr{Cchar}, schema_path::Ptr{Cchar}, options::Ptr{quiver_database_options_t}, out_db::Ptr{Ptr{quiver_database_t}})::quiver_error_t
end

function quiver_database_close(db)
    @ccall libquiver_c.quiver_database_close(db::Ptr{quiver_database_t})::quiver_error_t
end

function quiver_database_is_healthy(db, out_healthy)
    @ccall libquiver_c.quiver_database_is_healthy(db::Ptr{quiver_database_t}, out_healthy::Ptr{Cint})::quiver_error_t
end

function quiver_database_path(db, out_path)
    @ccall libquiver_c.quiver_database_path(db::Ptr{quiver_database_t}, out_path::Ptr{Ptr{Cchar}})::quiver_error_t
end

function quiver_database_begin_transaction(db)
    @ccall libquiver_c.quiver_database_begin_transaction(db::Ptr{quiver_database_t})::quiver_error_t
end

function quiver_database_commit(db)
    @ccall libquiver_c.quiver_database_commit(db::Ptr{quiver_database_t})::quiver_error_t
end

function quiver_database_rollback(db)
    @ccall libquiver_c.quiver_database_rollback(db::Ptr{quiver_database_t})::quiver_error_t
end

function quiver_database_in_transaction(db, out_active)
    @ccall libquiver_c.quiver_database_in_transaction(db::Ptr{quiver_database_t}, out_active::Ptr{Bool})::quiver_error_t
end

function quiver_database_current_version(db, out_version)
    @ccall libquiver_c.quiver_database_current_version(db::Ptr{quiver_database_t}, out_version::Ptr{Int64})::quiver_error_t
end

mutable struct quiver_element end

const quiver_element_t = quiver_element

function quiver_database_create_element(db, collection, element, out_id)
    @ccall libquiver_c.quiver_database_create_element(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, element::Ptr{quiver_element_t}, out_id::Ptr{Int64})::quiver_error_t
end

function quiver_database_update_element(db, collection, id, element)
    @ccall libquiver_c.quiver_database_update_element(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, id::Int64, element::Ptr{quiver_element_t})::quiver_error_t
end

function quiver_database_delete_element(db, collection, id)
    @ccall libquiver_c.quiver_database_delete_element(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, id::Int64)::quiver_error_t
end

function quiver_database_read_scalar_integers(db, collection, attribute, out_values, out_count)
    @ccall libquiver_c.quiver_database_read_scalar_integers(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_values::Ptr{Ptr{Int64}}, out_count::Ptr{Csize_t})::quiver_error_t
end

function quiver_database_read_scalar_floats(db, collection, attribute, out_values, out_count)
    @ccall libquiver_c.quiver_database_read_scalar_floats(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_values::Ptr{Ptr{Cdouble}}, out_count::Ptr{Csize_t})::quiver_error_t
end

function quiver_database_read_scalar_strings(db, collection, attribute, out_values, out_count)
    @ccall libquiver_c.quiver_database_read_scalar_strings(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_values::Ptr{Ptr{Ptr{Cchar}}}, out_count::Ptr{Csize_t})::quiver_error_t
end

function quiver_database_read_vector_integers(db, collection, attribute, out_vectors, out_sizes, out_count)
    @ccall libquiver_c.quiver_database_read_vector_integers(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_vectors::Ptr{Ptr{Ptr{Int64}}}, out_sizes::Ptr{Ptr{Csize_t}}, out_count::Ptr{Csize_t})::quiver_error_t
end

function quiver_database_read_vector_floats(db, collection, attribute, out_vectors, out_sizes, out_count)
    @ccall libquiver_c.quiver_database_read_vector_floats(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_vectors::Ptr{Ptr{Ptr{Cdouble}}}, out_sizes::Ptr{Ptr{Csize_t}}, out_count::Ptr{Csize_t})::quiver_error_t
end

function quiver_database_read_vector_strings(db, collection, attribute, out_vectors, out_sizes, out_count)
    @ccall libquiver_c.quiver_database_read_vector_strings(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_vectors::Ptr{Ptr{Ptr{Ptr{Cchar}}}}, out_sizes::Ptr{Ptr{Csize_t}}, out_count::Ptr{Csize_t})::quiver_error_t
end

function quiver_database_read_set_integers(db, collection, attribute, out_sets, out_sizes, out_count)
    @ccall libquiver_c.quiver_database_read_set_integers(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_sets::Ptr{Ptr{Ptr{Int64}}}, out_sizes::Ptr{Ptr{Csize_t}}, out_count::Ptr{Csize_t})::quiver_error_t
end

function quiver_database_read_set_floats(db, collection, attribute, out_sets, out_sizes, out_count)
    @ccall libquiver_c.quiver_database_read_set_floats(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_sets::Ptr{Ptr{Ptr{Cdouble}}}, out_sizes::Ptr{Ptr{Csize_t}}, out_count::Ptr{Csize_t})::quiver_error_t
end

function quiver_database_read_set_strings(db, collection, attribute, out_sets, out_sizes, out_count)
    @ccall libquiver_c.quiver_database_read_set_strings(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_sets::Ptr{Ptr{Ptr{Ptr{Cchar}}}}, out_sizes::Ptr{Ptr{Csize_t}}, out_count::Ptr{Csize_t})::quiver_error_t
end

function quiver_database_read_scalar_integer_by_id(db, collection, attribute, id, out_value, out_has_value)
    @ccall libquiver_c.quiver_database_read_scalar_integer_by_id(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, out_value::Ptr{Int64}, out_has_value::Ptr{Cint})::quiver_error_t
end

function quiver_database_read_scalar_float_by_id(db, collection, attribute, id, out_value, out_has_value)
    @ccall libquiver_c.quiver_database_read_scalar_float_by_id(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, out_value::Ptr{Cdouble}, out_has_value::Ptr{Cint})::quiver_error_t
end

function quiver_database_read_scalar_string_by_id(db, collection, attribute, id, out_value, out_has_value)
    @ccall libquiver_c.quiver_database_read_scalar_string_by_id(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, out_value::Ptr{Ptr{Cchar}}, out_has_value::Ptr{Cint})::quiver_error_t
end

function quiver_database_read_vector_integers_by_id(db, collection, attribute, id, out_values, out_count)
    @ccall libquiver_c.quiver_database_read_vector_integers_by_id(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, out_values::Ptr{Ptr{Int64}}, out_count::Ptr{Csize_t})::quiver_error_t
end

function quiver_database_read_vector_floats_by_id(db, collection, attribute, id, out_values, out_count)
    @ccall libquiver_c.quiver_database_read_vector_floats_by_id(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, out_values::Ptr{Ptr{Cdouble}}, out_count::Ptr{Csize_t})::quiver_error_t
end

function quiver_database_read_vector_strings_by_id(db, collection, attribute, id, out_values, out_count)
    @ccall libquiver_c.quiver_database_read_vector_strings_by_id(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, out_values::Ptr{Ptr{Ptr{Cchar}}}, out_count::Ptr{Csize_t})::quiver_error_t
end

function quiver_database_read_set_integers_by_id(db, collection, attribute, id, out_values, out_count)
    @ccall libquiver_c.quiver_database_read_set_integers_by_id(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, out_values::Ptr{Ptr{Int64}}, out_count::Ptr{Csize_t})::quiver_error_t
end

function quiver_database_read_set_floats_by_id(db, collection, attribute, id, out_values, out_count)
    @ccall libquiver_c.quiver_database_read_set_floats_by_id(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, out_values::Ptr{Ptr{Cdouble}}, out_count::Ptr{Csize_t})::quiver_error_t
end

function quiver_database_read_set_strings_by_id(db, collection, attribute, id, out_values, out_count)
    @ccall libquiver_c.quiver_database_read_set_strings_by_id(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, out_values::Ptr{Ptr{Ptr{Cchar}}}, out_count::Ptr{Csize_t})::quiver_error_t
end

function quiver_database_read_element_ids(db, collection, out_ids, out_count)
    @ccall libquiver_c.quiver_database_read_element_ids(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, out_ids::Ptr{Ptr{Int64}}, out_count::Ptr{Csize_t})::quiver_error_t
end

struct quiver_scalar_metadata_t
    name::Ptr{Cchar}
    data_type::quiver_data_type_t
    not_null::Cint
    primary_key::Cint
    default_value::Ptr{Cchar}
    is_foreign_key::Cint
    references_collection::Ptr{Cchar}
    references_column::Ptr{Cchar}
end

struct quiver_group_metadata_t
    group_name::Ptr{Cchar}
    dimension_column::Ptr{Cchar}
    value_columns::Ptr{quiver_scalar_metadata_t}
    value_column_count::Csize_t
end

function quiver_database_get_scalar_metadata(db, collection, attribute, out_metadata)
    @ccall libquiver_c.quiver_database_get_scalar_metadata(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, out_metadata::Ptr{quiver_scalar_metadata_t})::quiver_error_t
end

function quiver_database_get_vector_metadata(db, collection, group_name, out_metadata)
    @ccall libquiver_c.quiver_database_get_vector_metadata(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, group_name::Ptr{Cchar}, out_metadata::Ptr{quiver_group_metadata_t})::quiver_error_t
end

function quiver_database_get_set_metadata(db, collection, group_name, out_metadata)
    @ccall libquiver_c.quiver_database_get_set_metadata(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, group_name::Ptr{Cchar}, out_metadata::Ptr{quiver_group_metadata_t})::quiver_error_t
end

function quiver_database_get_time_series_metadata(db, collection, group_name, out_metadata)
    @ccall libquiver_c.quiver_database_get_time_series_metadata(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, group_name::Ptr{Cchar}, out_metadata::Ptr{quiver_group_metadata_t})::quiver_error_t
end

function quiver_database_free_scalar_metadata(metadata)
    @ccall libquiver_c.quiver_database_free_scalar_metadata(metadata::Ptr{quiver_scalar_metadata_t})::quiver_error_t
end

function quiver_database_free_group_metadata(metadata)
    @ccall libquiver_c.quiver_database_free_group_metadata(metadata::Ptr{quiver_group_metadata_t})::quiver_error_t
end

function quiver_database_list_scalar_attributes(db, collection, out_metadata, out_count)
    @ccall libquiver_c.quiver_database_list_scalar_attributes(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, out_metadata::Ptr{Ptr{quiver_scalar_metadata_t}}, out_count::Ptr{Csize_t})::quiver_error_t
end

function quiver_database_list_vector_groups(db, collection, out_metadata, out_count)
    @ccall libquiver_c.quiver_database_list_vector_groups(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, out_metadata::Ptr{Ptr{quiver_group_metadata_t}}, out_count::Ptr{Csize_t})::quiver_error_t
end

function quiver_database_list_set_groups(db, collection, out_metadata, out_count)
    @ccall libquiver_c.quiver_database_list_set_groups(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, out_metadata::Ptr{Ptr{quiver_group_metadata_t}}, out_count::Ptr{Csize_t})::quiver_error_t
end

function quiver_database_list_time_series_groups(db, collection, out_metadata, out_count)
    @ccall libquiver_c.quiver_database_list_time_series_groups(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, out_metadata::Ptr{Ptr{quiver_group_metadata_t}}, out_count::Ptr{Csize_t})::quiver_error_t
end

function quiver_database_free_scalar_metadata_array(metadata, count)
    @ccall libquiver_c.quiver_database_free_scalar_metadata_array(metadata::Ptr{quiver_scalar_metadata_t}, count::Csize_t)::quiver_error_t
end

function quiver_database_free_group_metadata_array(metadata, count)
    @ccall libquiver_c.quiver_database_free_group_metadata_array(metadata::Ptr{quiver_group_metadata_t}, count::Csize_t)::quiver_error_t
end

function quiver_database_update_scalar_integer(db, collection, attribute, id, value)
    @ccall libquiver_c.quiver_database_update_scalar_integer(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, value::Int64)::quiver_error_t
end

function quiver_database_update_scalar_float(db, collection, attribute, id, value)
    @ccall libquiver_c.quiver_database_update_scalar_float(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, value::Cdouble)::quiver_error_t
end

function quiver_database_update_scalar_string(db, collection, attribute, id, value)
    @ccall libquiver_c.quiver_database_update_scalar_string(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, value::Ptr{Cchar})::quiver_error_t
end

function quiver_database_update_vector_integers(db, collection, attribute, id, values, count)
    @ccall libquiver_c.quiver_database_update_vector_integers(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, values::Ptr{Int64}, count::Csize_t)::quiver_error_t
end

function quiver_database_update_vector_floats(db, collection, attribute, id, values, count)
    @ccall libquiver_c.quiver_database_update_vector_floats(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, values::Ptr{Cdouble}, count::Csize_t)::quiver_error_t
end

function quiver_database_update_vector_strings(db, collection, attribute, id, values, count)
    @ccall libquiver_c.quiver_database_update_vector_strings(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, values::Ptr{Ptr{Cchar}}, count::Csize_t)::quiver_error_t
end

function quiver_database_update_set_integers(db, collection, attribute, id, values, count)
    @ccall libquiver_c.quiver_database_update_set_integers(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, values::Ptr{Int64}, count::Csize_t)::quiver_error_t
end

function quiver_database_update_set_floats(db, collection, attribute, id, values, count)
    @ccall libquiver_c.quiver_database_update_set_floats(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, values::Ptr{Cdouble}, count::Csize_t)::quiver_error_t
end

function quiver_database_update_set_strings(db, collection, attribute, id, values, count)
    @ccall libquiver_c.quiver_database_update_set_strings(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, attribute::Ptr{Cchar}, id::Int64, values::Ptr{Ptr{Cchar}}, count::Csize_t)::quiver_error_t
end

function quiver_database_read_time_series_group(db, collection, group, id, out_column_names, out_column_types, out_column_data, out_column_count, out_row_count)
    @ccall libquiver_c.quiver_database_read_time_series_group(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, group::Ptr{Cchar}, id::Int64, out_column_names::Ptr{Ptr{Ptr{Cchar}}}, out_column_types::Ptr{Ptr{Cint}}, out_column_data::Ptr{Ptr{Ptr{Cvoid}}}, out_column_count::Ptr{Csize_t}, out_row_count::Ptr{Csize_t})::quiver_error_t
end

function quiver_database_update_time_series_group(db, collection, group, id, column_names, column_types, column_data, column_count, row_count)
    @ccall libquiver_c.quiver_database_update_time_series_group(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, group::Ptr{Cchar}, id::Int64, column_names::Ptr{Ptr{Cchar}}, column_types::Ptr{Cint}, column_data::Ptr{Ptr{Cvoid}}, column_count::Csize_t, row_count::Csize_t)::quiver_error_t
end

function quiver_database_free_time_series_data(column_names, column_types, column_data, column_count, row_count)
    @ccall libquiver_c.quiver_database_free_time_series_data(column_names::Ptr{Ptr{Cchar}}, column_types::Ptr{Cint}, column_data::Ptr{Ptr{Cvoid}}, column_count::Csize_t, row_count::Csize_t)::quiver_error_t
end

function quiver_database_has_time_series_files(db, collection, out_result)
    @ccall libquiver_c.quiver_database_has_time_series_files(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, out_result::Ptr{Cint})::quiver_error_t
end

function quiver_database_list_time_series_files_columns(db, collection, out_columns, out_count)
    @ccall libquiver_c.quiver_database_list_time_series_files_columns(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, out_columns::Ptr{Ptr{Ptr{Cchar}}}, out_count::Ptr{Csize_t})::quiver_error_t
end

function quiver_database_read_time_series_files(db, collection, out_columns, out_paths, out_count)
    @ccall libquiver_c.quiver_database_read_time_series_files(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, out_columns::Ptr{Ptr{Ptr{Cchar}}}, out_paths::Ptr{Ptr{Ptr{Cchar}}}, out_count::Ptr{Csize_t})::quiver_error_t
end

function quiver_database_update_time_series_files(db, collection, columns, paths, count)
    @ccall libquiver_c.quiver_database_update_time_series_files(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, columns::Ptr{Ptr{Cchar}}, paths::Ptr{Ptr{Cchar}}, count::Csize_t)::quiver_error_t
end

function quiver_database_free_time_series_files(columns, paths, count)
    @ccall libquiver_c.quiver_database_free_time_series_files(columns::Ptr{Ptr{Cchar}}, paths::Ptr{Ptr{Cchar}}, count::Csize_t)::quiver_error_t
end

function quiver_database_free_integer_array(values)
    @ccall libquiver_c.quiver_database_free_integer_array(values::Ptr{Int64})::quiver_error_t
end

function quiver_database_free_float_array(values)
    @ccall libquiver_c.quiver_database_free_float_array(values::Ptr{Cdouble})::quiver_error_t
end

function quiver_database_free_string_array(values, count)
    @ccall libquiver_c.quiver_database_free_string_array(values::Ptr{Ptr{Cchar}}, count::Csize_t)::quiver_error_t
end

function quiver_database_free_integer_vectors(vectors, sizes, count)
    @ccall libquiver_c.quiver_database_free_integer_vectors(vectors::Ptr{Ptr{Int64}}, sizes::Ptr{Csize_t}, count::Csize_t)::quiver_error_t
end

function quiver_database_free_float_vectors(vectors, sizes, count)
    @ccall libquiver_c.quiver_database_free_float_vectors(vectors::Ptr{Ptr{Cdouble}}, sizes::Ptr{Csize_t}, count::Csize_t)::quiver_error_t
end

function quiver_database_free_string_vectors(vectors, sizes, count)
    @ccall libquiver_c.quiver_database_free_string_vectors(vectors::Ptr{Ptr{Ptr{Cchar}}}, sizes::Ptr{Csize_t}, count::Csize_t)::quiver_error_t
end

function quiver_database_export_csv(db, collection, group, path, opts)
    @ccall libquiver_c.quiver_database_export_csv(db::Ptr{quiver_database_t}, collection::Ptr{Cchar}, group::Ptr{Cchar}, path::Ptr{Cchar}, opts::Ptr{quiver_csv_export_options_t})::quiver_error_t
end

function quiver_database_import_csv(db, table, path)
    @ccall libquiver_c.quiver_database_import_csv(db::Ptr{quiver_database_t}, table::Ptr{Cchar}, path::Ptr{Cchar})::quiver_error_t
end

function quiver_database_query_string(db, sql, out_value, out_has_value)
    @ccall libquiver_c.quiver_database_query_string(db::Ptr{quiver_database_t}, sql::Ptr{Cchar}, out_value::Ptr{Ptr{Cchar}}, out_has_value::Ptr{Cint})::quiver_error_t
end

function quiver_database_query_integer(db, sql, out_value, out_has_value)
    @ccall libquiver_c.quiver_database_query_integer(db::Ptr{quiver_database_t}, sql::Ptr{Cchar}, out_value::Ptr{Int64}, out_has_value::Ptr{Cint})::quiver_error_t
end

function quiver_database_query_float(db, sql, out_value, out_has_value)
    @ccall libquiver_c.quiver_database_query_float(db::Ptr{quiver_database_t}, sql::Ptr{Cchar}, out_value::Ptr{Cdouble}, out_has_value::Ptr{Cint})::quiver_error_t
end

function quiver_database_query_string_params(db, sql, param_types, param_values, param_count, out_value, out_has_value)
    @ccall libquiver_c.quiver_database_query_string_params(db::Ptr{quiver_database_t}, sql::Ptr{Cchar}, param_types::Ptr{Cint}, param_values::Ptr{Ptr{Cvoid}}, param_count::Csize_t, out_value::Ptr{Ptr{Cchar}}, out_has_value::Ptr{Cint})::quiver_error_t
end

function quiver_database_query_integer_params(db, sql, param_types, param_values, param_count, out_value, out_has_value)
    @ccall libquiver_c.quiver_database_query_integer_params(db::Ptr{quiver_database_t}, sql::Ptr{Cchar}, param_types::Ptr{Cint}, param_values::Ptr{Ptr{Cvoid}}, param_count::Csize_t, out_value::Ptr{Int64}, out_has_value::Ptr{Cint})::quiver_error_t
end

function quiver_database_query_float_params(db, sql, param_types, param_values, param_count, out_value, out_has_value)
    @ccall libquiver_c.quiver_database_query_float_params(db::Ptr{quiver_database_t}, sql::Ptr{Cchar}, param_types::Ptr{Cint}, param_values::Ptr{Ptr{Cvoid}}, param_count::Csize_t, out_value::Ptr{Cdouble}, out_has_value::Ptr{Cint})::quiver_error_t
end

function quiver_database_describe(db)
    @ccall libquiver_c.quiver_database_describe(db::Ptr{quiver_database_t})::quiver_error_t
end

function quiver_element_create(out_element)
    @ccall libquiver_c.quiver_element_create(out_element::Ptr{Ptr{quiver_element_t}})::quiver_error_t
end

function quiver_element_destroy(element)
    @ccall libquiver_c.quiver_element_destroy(element::Ptr{quiver_element_t})::quiver_error_t
end

function quiver_element_clear(element)
    @ccall libquiver_c.quiver_element_clear(element::Ptr{quiver_element_t})::quiver_error_t
end

function quiver_element_set_integer(element, name, value)
    @ccall libquiver_c.quiver_element_set_integer(element::Ptr{quiver_element_t}, name::Ptr{Cchar}, value::Int64)::quiver_error_t
end

function quiver_element_set_float(element, name, value)
    @ccall libquiver_c.quiver_element_set_float(element::Ptr{quiver_element_t}, name::Ptr{Cchar}, value::Cdouble)::quiver_error_t
end

function quiver_element_set_string(element, name, value)
    @ccall libquiver_c.quiver_element_set_string(element::Ptr{quiver_element_t}, name::Ptr{Cchar}, value::Ptr{Cchar})::quiver_error_t
end

function quiver_element_set_null(element, name)
    @ccall libquiver_c.quiver_element_set_null(element::Ptr{quiver_element_t}, name::Ptr{Cchar})::quiver_error_t
end

function quiver_element_set_array_integer(element, name, values, count)
    @ccall libquiver_c.quiver_element_set_array_integer(element::Ptr{quiver_element_t}, name::Ptr{Cchar}, values::Ptr{Int64}, count::Int32)::quiver_error_t
end

function quiver_element_set_array_float(element, name, values, count)
    @ccall libquiver_c.quiver_element_set_array_float(element::Ptr{quiver_element_t}, name::Ptr{Cchar}, values::Ptr{Cdouble}, count::Int32)::quiver_error_t
end

function quiver_element_set_array_string(element, name, values, count)
    @ccall libquiver_c.quiver_element_set_array_string(element::Ptr{quiver_element_t}, name::Ptr{Cchar}, values::Ptr{Ptr{Cchar}}, count::Int32)::quiver_error_t
end

function quiver_element_has_scalars(element, out_result)
    @ccall libquiver_c.quiver_element_has_scalars(element::Ptr{quiver_element_t}, out_result::Ptr{Cint})::quiver_error_t
end

function quiver_element_has_arrays(element, out_result)
    @ccall libquiver_c.quiver_element_has_arrays(element::Ptr{quiver_element_t}, out_result::Ptr{Cint})::quiver_error_t
end

function quiver_element_scalar_count(element, out_count)
    @ccall libquiver_c.quiver_element_scalar_count(element::Ptr{quiver_element_t}, out_count::Ptr{Csize_t})::quiver_error_t
end

function quiver_element_array_count(element, out_count)
    @ccall libquiver_c.quiver_element_array_count(element::Ptr{quiver_element_t}, out_count::Ptr{Csize_t})::quiver_error_t
end

function quiver_element_to_string(element, out_string)
    @ccall libquiver_c.quiver_element_to_string(element::Ptr{quiver_element_t}, out_string::Ptr{Ptr{Cchar}})::quiver_error_t
end

function quiver_element_free_string(str)
    @ccall libquiver_c.quiver_element_free_string(str::Ptr{Cchar})::quiver_error_t
end

mutable struct quiver_lua_runner end

const quiver_lua_runner_t = quiver_lua_runner

function quiver_lua_runner_new(db, out_runner)
    @ccall libquiver_c.quiver_lua_runner_new(db::Ptr{quiver_database_t}, out_runner::Ptr{Ptr{quiver_lua_runner_t}})::quiver_error_t
end

function quiver_lua_runner_free(runner)
    @ccall libquiver_c.quiver_lua_runner_free(runner::Ptr{quiver_lua_runner_t})::quiver_error_t
end

function quiver_lua_runner_run(runner, script)
    @ccall libquiver_c.quiver_lua_runner_run(runner::Ptr{quiver_lua_runner_t}, script::Ptr{Cchar})::quiver_error_t
end

function quiver_lua_runner_get_error(runner, out_error)
    @ccall libquiver_c.quiver_lua_runner_get_error(runner::Ptr{quiver_lua_runner_t}, out_error::Ptr{Ptr{Cchar}})::quiver_error_t
end

#! format: on


end # module
