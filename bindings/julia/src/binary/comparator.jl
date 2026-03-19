function compare_files(
    path1::String, path2::String;
    absolute_tolerance::Union{Float64, Nothing} = nothing,
    relative_tolerance::Union{Float64, Nothing} = nothing,
    detailed_report::Union{Bool, Nothing} = nothing,
    max_report_lines::Union{Int, Nothing} = nothing,
)
    options = C.quiver_compare_options_default()
    if absolute_tolerance !== nothing
        options.absolute_tolerance = absolute_tolerance
    end
    if relative_tolerance !== nothing
        options.relative_tolerance = relative_tolerance
    end
    if detailed_report !== nothing
        options.detailed_report = detailed_report ? Cint(1) : Cint(0)
    end
    if max_report_lines !== nothing
        options.max_report_lines = Cint(max_report_lines)
    end

    out_status = Ref{C.quiver_compare_status_t}(C.QUIVER_COMPARE_FILE_MATCH)
    out_report = Ref{Ptr{Cchar}}(C_NULL)

    check(C.quiver_binary_compare_files(path1, path2, Ref(options), out_status, out_report))

    status = if out_status[] == C.QUIVER_COMPARE_FILE_MATCH
        C.QUIVER_COMPARE_FILE_MATCH
    elseif out_status[] == C.QUIVER_COMPARE_METADATA_MISMATCH
        C.QUIVER_COMPARE_METADATA_MISMATCH
    else
        C.QUIVER_COMPARE_DATA_MISMATCH
    end

    report = if out_report[] != C_NULL
        r = unsafe_string(out_report[])
        C.quiver_binary_comparator_free_string(out_report[])
        r
    else
        nothing
    end

    return (; status, report)
end
