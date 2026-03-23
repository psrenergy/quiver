function apply_expression_over_files(
    output_filename::String,
    filenames::Vector{String},
    operation::Function;
    digits::Union{Int, Nothing} = nothing,
)
    return nothing
end

function apply_expression_over_agents(
    output_filename::String,
    filename::String,
    operation::Function,
    new_labels::Vector{String};
    digits::Union{Int, Nothing} = nothing,
)
    return nothing
end

function apply_expression_over_dimensions(
    output_filename::String,
    filename::String,
    operation::Function,
    dim_to_operate::String;
    digits::Union{Int, Nothing} = nothing,
    suppress_dimension_order_warning::Bool = false,
)
    return nothing
end
