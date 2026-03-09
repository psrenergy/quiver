function bin_to_csv(path::String; aggregate_time_dimensions::Bool = true)
    check(C.quiver_binary_csv_bin_to_csv(path, aggregate_time_dimensions ? Cint(1) : Cint(0)))
    return nothing
end

function csv_to_bin(path::String)
    check(C.quiver_binary_csv_csv_to_bin(path))
    return nothing
end
