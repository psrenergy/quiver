# Time series operations

using JSON3

struct TimeSeriesMetadata
    group_name::String
    dimension_columns::Vector{String}
    value_columns::Vector{ScalarMetadata}
end

function add_time_series_row!(
    db::Database,
    collection::String,
    attribute::String,
    element_id::Int64,
    value::Real,
    date_time::Union{String, DateTime};
    dimensions::Dict{String, Int64} = Dict{String, Int64}()
)
    date_str = date_time isa DateTime ? date_time_to_string(date_time) : date_time

    dim_count = length(dimensions)
    if dim_count == 0
        err = C.quiver_database_add_time_series_row(
            db.ptr, collection, attribute, element_id, Float64(value), date_str,
            C_NULL, C_NULL, Csize_t(0)
        )
    else
        dim_names = collect(keys(dimensions))
        dim_values = Int64[dimensions[k] for k in dim_names]
        cstrings = [Base.cconvert(Cstring, s) for s in dim_names]
        ptrs = [Base.unsafe_convert(Cstring, cs) for cs in cstrings]
        GC.@preserve cstrings begin
            err = C.quiver_database_add_time_series_row(
                db.ptr, collection, attribute, element_id, Float64(value), date_str,
                ptrs, dim_values, Csize_t(dim_count)
            )
        end
    end
    check_error(err, "Failed to add time series row for $collection.$attribute id=$element_id")
    return nothing
end

function update_time_series_row!(
    db::Database,
    collection::String,
    attribute::String,
    element_id::Int64,
    value::Real,
    date_time::Union{String, DateTime};
    dimensions::Dict{String, Int64} = Dict{String, Int64}()
)
    date_str = date_time isa DateTime ? date_time_to_string(date_time) : date_time

    dim_count = length(dimensions)
    if dim_count == 0
        err = C.quiver_database_update_time_series_row(
            db.ptr, collection, attribute, element_id, Float64(value), date_str,
            C_NULL, C_NULL, Csize_t(0)
        )
    else
        dim_names = collect(keys(dimensions))
        dim_values = Int64[dimensions[k] for k in dim_names]
        cstrings = [Base.cconvert(Cstring, s) for s in dim_names]
        ptrs = [Base.unsafe_convert(Cstring, cs) for cs in cstrings]
        GC.@preserve cstrings begin
            err = C.quiver_database_update_time_series_row(
                db.ptr, collection, attribute, element_id, Float64(value), date_str,
                ptrs, dim_values, Csize_t(dim_count)
            )
        end
    end
    check_error(err, "Failed to update time series row for $collection.$attribute id=$element_id")
    return nothing
end

function delete_time_series!(db::Database, collection::String, group::String, element_id::Int64)
    err = C.quiver_database_delete_time_series(db.ptr, collection, group, element_id)
    check_error(err, "Failed to delete time series $collection.$group for id=$element_id")
    return nothing
end

function read_time_series_table(db::Database, collection::String, group::String, element_id::Int64)
    json_ptr = Ref{Ptr{Cchar}}(C_NULL)
    err = C.quiver_database_read_time_series_table(db.ptr, collection, group, element_id, json_ptr)
    check_error(err, "Failed to read time series table $collection.$group for id=$element_id")

    if json_ptr[] == C_NULL
        return Vector{Dict{String, Any}}()
    end

    json_str = unsafe_string(json_ptr[])
    C.quiver_free_string(json_ptr[])

    # Parse JSON
    rows = JSON3.read(json_str)
    return [Dict{String, Any}(String(k) => v for (k, v) in pairs(row)) for row in rows]
end

function get_time_series_metadata(db::Database, collection::String, group_name::String)
    meta = Ref{C.quiver_time_series_metadata_t}()
    err = C.quiver_database_get_time_series_metadata(db.ptr, collection, group_name, meta)
    check_error(err, "Failed to get time series metadata for $collection.$group_name")

    result = _convert_time_series_metadata(meta[])
    C.quiver_free_time_series_metadata(Ref(meta[]))
    return result
end

function list_time_series_groups(db::Database, collection::String)
    out_metadata = Ref{Ptr{C.quiver_time_series_metadata_t}}(C_NULL)
    out_count = Ref{Csize_t}(0)

    err = C.quiver_database_list_time_series_groups(db.ptr, collection, out_metadata, out_count)
    check_error(err, "Failed to list time series groups for $collection")

    count = out_count[]
    if count == 0 || out_metadata[] == C_NULL
        return TimeSeriesMetadata[]
    end

    result = TimeSeriesMetadata[]
    metadata_ptr = out_metadata[]
    for i in 1:count
        meta = unsafe_load(metadata_ptr, i)
        push!(result, _convert_time_series_metadata(meta))
    end

    C.quiver_free_time_series_metadata_array(out_metadata[], count)
    return result
end

function _convert_time_series_metadata(meta::C.quiver_time_series_metadata_t)
    group_name = unsafe_string(meta.group_name)

    dimension_columns = String[]
    for i in 1:meta.dimension_count
        dim_ptr = unsafe_load(meta.dimension_columns, i)
        push!(dimension_columns, unsafe_string(dim_ptr))
    end

    value_columns = ScalarMetadata[]
    for i in 1:meta.value_column_count
        col = unsafe_load(meta.value_columns, i)
        push!(value_columns, ScalarMetadata(
            unsafe_string(col.name),
            col.data_type,
            col.not_null != 0,
            col.primary_key != 0,
            col.default_value == C_NULL ? nothing : unsafe_string(col.default_value)
        ))
    end

    return TimeSeriesMetadata(group_name, dimension_columns, value_columns)
end
