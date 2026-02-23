"""
    build_csv_options(kwargs...) -> (Ref{quiver_csv_export_options_t}, Vector{Any})

Build a C API `quiver_csv_export_options_t` from keyword arguments.
Returns `(opts_ref, temps)` where `temps` holds all temporary objects that must
stay alive during the C call (use inside `GC.@preserve`).

Supported kwargs:

  - `date_time_format::String` — strftime format for DateTime columns
  - `enum_labels::Dict{String, Dict{Int, String}}` — attribute → (value → label) mapping
"""
function build_csv_options(; kwargs...)
    opts = C.quiver_csv_export_options_default()
    temps = Any[]

    # Date-time format
    dtf_ptr = opts.date_time_format
    if haskey(kwargs, :date_time_format)
        dtf_str = kwargs[:date_time_format]::String
        dtf_buf = Vector{UInt8}([codeunits(dtf_str)..., 0x00])
        push!(temps, dtf_buf)
        dtf_ptr = pointer(dtf_buf)
    end

    # Enum labels: flatten Dict{String, Dict{Int, String}} into parallel arrays
    attr_names_ptr = opts.enum_attribute_names
    entry_counts_ptr = opts.enum_entry_counts
    values_ptr = opts.enum_values
    labels_ptr = opts.enum_labels
    attr_count = opts.enum_attribute_count

    if haskey(kwargs, :enum_labels)
        enum_map = kwargs[:enum_labels]::Dict{String, Dict{Int, String}}
        if !isempty(enum_map)
            attr_count = Csize_t(length(enum_map))

            # Build null-terminated C strings for attribute names
            c_attr_name_bufs = [Vector{UInt8}([codeunits(k)..., 0x00]) for k in keys(enum_map)]
            c_attr_names = Ptr{Cchar}[pointer(buf) for buf in c_attr_name_bufs]
            push!(temps, c_attr_name_bufs)
            push!(temps, c_attr_names)
            attr_names_ptr = pointer(c_attr_names)

            # Build entry counts and concatenated values/labels
            c_entry_counts = Csize_t[]
            c_values = Int64[]
            c_label_bufs = Vector{UInt8}[]
            for k in keys(enum_map)
                entries = enum_map[k]
                push!(c_entry_counts, Csize_t(length(entries)))
                for (v, lbl) in entries
                    push!(c_values, Int64(v))
                    push!(c_label_bufs, Vector{UInt8}([codeunits(lbl)..., 0x00]))
                end
            end
            push!(temps, c_entry_counts)
            entry_counts_ptr = pointer(c_entry_counts)

            push!(temps, c_values)
            values_ptr = pointer(c_values)

            c_labels = Ptr{Cchar}[pointer(buf) for buf in c_label_bufs]
            push!(temps, c_label_bufs)
            push!(temps, c_labels)
            labels_ptr = pointer(c_labels)
        end
    end

    opts_new = C.quiver_csv_export_options_t(
        dtf_ptr,
        attr_names_ptr,
        entry_counts_ptr,
        values_ptr,
        labels_ptr,
        attr_count,
    )
    opts_ref = Ref(opts_new)
    push!(temps, opts_ref)

    return (opts_ref, temps)
end

function export_csv(db::Database, collection::String, group::String, path::String; kwargs...)
    opts_ref, temps = build_csv_options(; kwargs...)
    GC.@preserve temps begin
        check(C.quiver_database_export_csv(db.ptr, collection, group, path, opts_ref))
    end
    return nothing
end

function import_csv(db::Database, table::String, path::String)
    check(C.quiver_database_import_csv(db.ptr, table, path))
    return nothing
end
