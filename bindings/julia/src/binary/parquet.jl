function bin_to_parquet(path::String)
    check(C.quiver_bin_to_parquet(path))
    return nothing
end
