"""
    build_quiver_csv_options(kwargs...) -> (Ref{quiver_csv_options_t}, Vector{Any})

Build a C API `quiver_csv_options_t` from keyword arguments.
Returns `(options_ref, temps)` where `temps` holds all temporary objects that must
stay alive during the C call (use inside `GC.@preserve`).

Supported kwargs:

  - `date_time_format::String` — strftime format for DateTime columns
  - `enum_labels::Dict{String, Dict{String, Dict{String, Int}}}` — attribute → locale → (label → value)
"""
function build_quiver_csv_options(; kwargs...)
    options = C.quiver_csv_options_default()
    temps = Any[]

    # Date-time format
    dtf_ptr = options.date_time_format
    if haskey(kwargs, :date_time_format)
        dtf_str = kwargs[:date_time_format]::String
        dtf_buf = Vector{UInt8}([codeunits(dtf_str)..., 0x00])
        push!(temps, dtf_buf)
        dtf_ptr = pointer(dtf_buf)
    end

    # Enum labels: flatten Dict{String, Dict{String, Dict{String, Int}}} into grouped parallel arrays
    attr_names_ptr = options.enum_attribute_names
    locale_names_ptr = options.enum_locale_names
    entry_counts_ptr = options.enum_entry_counts
    labels_ptr = options.enum_labels
    values_ptr = options.enum_values
    group_count = options.enum_group_count

    if haskey(kwargs, :enum_labels)
        enum_map = kwargs[:enum_labels]::Dict{String, Dict{String, Dict{String, Int}}}
        if !isempty(enum_map)
            # Flatten attribute -> locale -> entries into groups
            c_attr_name_bufs = Vector{UInt8}[]
            c_locale_name_bufs = Vector{UInt8}[]
            c_entry_counts = Csize_t[]
            c_label_bufs = Vector{UInt8}[]
            c_values = Int64[]

            for (attr_name, locales) in enum_map
                for (locale_name, entries) in locales
                    push!(c_attr_name_bufs, Vector{UInt8}([codeunits(attr_name)..., 0x00]))
                    push!(c_locale_name_bufs, Vector{UInt8}([codeunits(locale_name)..., 0x00]))
                    push!(c_entry_counts, Csize_t(length(entries)))
                    for (lbl, v) in entries
                        push!(c_label_bufs, Vector{UInt8}([codeunits(lbl)..., 0x00]))
                        push!(c_values, Int64(v))
                    end
                end
            end

            group_count = Csize_t(length(c_attr_name_bufs))

            c_attr_names = Ptr{Cchar}[pointer(buf) for buf in c_attr_name_bufs]
            push!(temps, c_attr_name_bufs)
            push!(temps, c_attr_names)
            attr_names_ptr = pointer(c_attr_names)

            c_locale_names = Ptr{Cchar}[pointer(buf) for buf in c_locale_name_bufs]
            push!(temps, c_locale_name_bufs)
            push!(temps, c_locale_names)
            locale_names_ptr = pointer(c_locale_names)

            push!(temps, c_entry_counts)
            entry_counts_ptr = pointer(c_entry_counts)

            c_labels = Ptr{Cchar}[pointer(buf) for buf in c_label_bufs]
            push!(temps, c_label_bufs)
            push!(temps, c_labels)
            labels_ptr = pointer(c_labels)

            push!(temps, c_values)
            values_ptr = pointer(c_values)
        end
    end

    options_new = C.quiver_csv_options_t(
        dtf_ptr,
        attr_names_ptr,
        locale_names_ptr,
        entry_counts_ptr,
        labels_ptr,
        values_ptr,
        group_count,
    )
    options_ref = Ref(options_new)
    push!(temps, options_ref)

    return (options_ref, temps)
end
