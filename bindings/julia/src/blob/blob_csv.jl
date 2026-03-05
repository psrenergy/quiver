function blob_bin_to_csv(path::AbstractString; aggregate_time_dimensions::Bool = true)
    check(C.quiver_blob_csv_bin_to_csv(path, aggregate_time_dimensions ? Cint(1) : Cint(0)))
    return nothing
end

function blob_csv_to_bin(path::AbstractString)
    check(C.quiver_blob_csv_csv_to_bin(path))
    return nothing
end
