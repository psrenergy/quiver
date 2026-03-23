function apply_expression_over_files(
    output_filename::String,
    filenames::Vector{String},
    operation::Function,
)
    readers = [Binary.open_file(filename; mode = :read) for filename in filenames]
    metadata = Binary.get_metadata(first(readers))
    dimensions = Binary.get_dimensions(metadata)
    dimension_names = [dim.name for dim in dimensions]

    for reader in readers
        if Binary.get_metadata(reader) != metadata
            for r in readers
                Binary.close!(r)
            end
            throw(ArgumentError("Cannot apply expression over files due to a metadata mismatch. Files: $(filenames)"))
        end
    end

    initial_dimension_values = ones(Int, length(dimensions))
    for (idx, dimension) in enumerate(dimensions)
        if dimension.is_time_dimension
            initial_dimension_values[idx] = dimension.initial_value
        end
    end

    writer = Binary.open_file(output_filename; mode = :write, metadata = metadata)

    maximum_number_of_iterations = prod([dim.size for dim in dimensions])
    data_all_readers = [zeros(num_labels) for _ in 1:n_readers]
    current_dimension_values = copy(initial_dimension_values)

    for _ in 1:maximum_number_of_iterations
        dims = dimension_names .=> current_dimension_values
        for (idx, reader) in enumerate(readers)
            data_all_readers[idx] = Binary.read(reader; dims...)
        end
        data_to_write = operation.(data_all_readers...)
        Binary.write!(writer; data = data_to_write, dims...)

        # TODO: next_dimensions implemented in another branch
        current_dimension_values = Binary.next_dimensions(writer, current_dimension_values)
        if current_dimension_values === initial_dimension_values
            break
        end
    end

    for reader in readers
        Binary.close!(reader)
    end
    Binary.close!(writer)

    return nothing
end

function apply_expression_over_agents(
    output_filename::String,
    filename::String,
    operation::Function,
    new_labels::Vector{String},
)
    reader = Binary.open_file(filename; mode = :read)
    metadata = Binary.get_metadata(reader)
    dimensions = Binary.get_dimensions(metadata)
    time_dimensions = filter(dim -> dim.is_time_dimension, dimensions)
    labels = Binary.get_labels(metadata)

    test_data = ones(length(labels))
    result = operation(test_data)
    if length(result) != length(new_labels)
        Binary.close!(reader)
        throw(
            ArgumentError(
                "Cannot apply expression over agents. The provided operation does not return the expected number of labels. Expected: $(number_of_new_labels), got: $(length(result)). File: $(filename)",
            ),
        )
    end

    writer_metadata = Binary.Metadata(;
        initial_datetime = get_initial_datetime(metadata),
        unit = get_unit(metadata),
        version = get_version(metadata),
        labels = new_labels,
        dimensions = [dim.name for dim in dimensions],
        dimension_sizes = [dim.size for dim in dimensions],
        time_dimensions = [dim.name for dim in time_dimensions],
        frequencies = [dim.frequency for dim in time_dimensions],
    )
    writer = Binary.open_file(output_filename; mode = :write, metadata = writer_metadata)

    initial_dimension_values = ones(Int, length(dimensions))
    for (idx, dimension) in enumerate(dimensions)
        if dimension.is_time_dimension
            initial_dimension_values[idx] = dimension.initial_value
        end
    end

    maximum_number_of_iterations = prod([dim.size for dim in dimensions])
    current_dimension_values = copy(initial_dimension_values)

    for _ in 1:maximum_number_of_iterations
        dims = dimension_names .=> current_dimension_values
        data = Binary.read(reader; dims...)
        data_to_write = vcat(operation(data))
        Binary.write!(writer; data = data_to_write, dims...)

        # TODO: next_dimensions implemented in another branch
        current_dimension_values = Binary.next_dimensions(writer, current_dimension_values)
        if current_dimension_values === initial_dimension_values
            break
        end
    end

    Binary.close!(reader)
    Binary.close!(writer)

    return nothing
end

function apply_expression_over_dimensions(
    output_filename::String,
    filename::String,
    operation::Function,
    dim_to_operate::String;
    suppress_dimension_order_warning::Bool = false,
)
    reader = Binary.open_file(filename; mode = :read)
    metadata = Binary.get_metadata(reader)
    dimensions = Binary.get_dimensions(metadata)
    time_dimensions = filter(dim -> dim.is_time_dimension, dimensions)
    dimension_names = [dim.name for dim in dimensions]
    dimension_sizes = [dim.size for dim in dimensions]

    dim_index = findfirst(isequal(dim_to_operate), dimension_names)
    if dim_index === nothing
        Binary.close!(reader)
        throw(
            ArgumentError(
                "Cannot apply expression over dimensions because the specified dimension '$dim_to_operate' was not found in the file. Available dimensions: $(dimension_names). File: $(filename)",
            ),
        )
    end

    if dimensions[dim_index].is_time_dimension
        Binary.close!(reader)
        throw(
            ArgumentError(
                "Cannot apply expression over dimensions. This is not implemented for time dimensions. File: $(filename)",
            ),
        )
    end

    if dim_index != length(dimension_names) && !suppress_dimension_order_warning
        @warn "The specified dimension '$dim_to_operate' is not the last dimension in the file. This is not the most efficient way to iterate over dimensions."
    end

    writer_metadata = Binary.Metadata(;
        initial_datetime = get_initial_datetime(metadata),
        unit = get_unit(metadata),
        version = get_version(metadata),
        labels = get_labels(metadata),
        dimensions = dimension_names[1:end .!= dim_index],
        dimension_sizes = dimension_sizes[1:end .!= dim_index],
        time_dimensions = [dim.name for dim in time_dimensions],
        frequencies = [dim.frequency for dim in time_dimensions],
    )
    writer = Binary.open_file(output_filename; mode = :write, metadata = writer_metadata)

    initial_dimension_values = ones(Int, length(dimensions))
    for (idx, dimension) in enumerate(dimensions)
        if dimension.is_time_dimension
            initial_dimension_values[idx] = dimension.initial_value
        end
    end

    maximum_number_of_iterations = prod([dim.size for (idx, dim) in dimensions if idx != dim_index])
    data = zeros(length(get_labels(metadata)), dimension_sizes[dim_index])
    current_dimension_values = copy(initial_dimension_values)

    for _ in 1:maximum_number_of_iterations
        for value_at_dim in 1:dimension_sizes[dim_index]
            current_dimension_values[dim_index] = value_at_dim
            dims = dimension_names .=> current_dimension_values
            data[:, value_at_dim] = Binary.read(reader; dims...)
        end

        data_to_write = operation(data, dims = 2)[:, 1]
        Binary.write!(writer; data = data_to_write, dims...)

        # This ensures the outer loop doesn't waste time iterating through the dimension we're operating over, since we iterate on it in the inner loop.
        # This would not work for time dimensions, as their size might not be constant and independent of the other dimensions.
        current_dimension_values[dim_index] = dimension_sizes[dim_index]
        # TODO: next_dimensions implemented in another branch
        current_dimension_values = Binary.next_dimensions(writer, current_dimension_values)
        if current_dimension_values === initial_dimension_values
            break
        end
    end

    Binary.close!(reader)
    Binary.close!(writer)

    return nothing
end
