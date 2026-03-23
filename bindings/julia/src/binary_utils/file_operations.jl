function merge(
    output_filename::String,
    filenames::Vector{String};
    digits::Union{Int, Nothing} = nothing,
)
    return nothing
end

function file_to_array(
    filename::String;
    labels_to_read::Vector{String} = String[],
)
    return nothing
end

function array_to_file(
    filename::String,
    data::Array{T, N},
    metadata::Binary.Metadata;
    digits::Union{Int, Nothing} = nothing,
) where {T, N}
    return nothing
end
