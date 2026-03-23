function merge(
    output_filename::String,
    filenames::Vector{String};
    digits::Union{Int, Nothing} = nothing,
)
    readers = [Binary.open_file(filename; mode = :read) for filename in filenames]
    metadata = Binary.get_metadata(first(readers))
    dimensions = Binary.get_dimensions(metadata)
    time_dimensions = filter(dim -> dim.is_time_dimension, dimensions)
    dimension_names = [dim.name for dim in dimensions]

    labels = String[]
    number_of_labels_per_file = Int[]

    for reader in readers
        current_metadata = Binary.get_metadata(reader)
        if !is_equal(metadata, current_metadata; ignore_labels = true)
            for r in readers
                Binary.close!(r)
            end
            throw(ArgumentError("Cannot merge files due to a metadata mismatch. Files: $(filenames)"))
        end

        current_labels = Binary.get_labels(current_metadata)
        for label in current_labels
            if label in labels
                for r in readers
                    Binary.close!(r)
                end
                throw(ArgumentError("Cannot merge files due to duplicate labels. Label: $(label). Files: $(filenames)"))
            end
            push!(labels, label)
        end
        push!(number_of_labels_per_file, length(current_labels))
    end

    writer_metadata = Binary.Metadata(
        unit = get_unit(metadata),
        version = get_version(metadata),
        labels = labels,
        dimensions = dimension_names,
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

    data_to_write = zeros(length(labels))
    for _ in 1:maximum_number_of_iterations
        dims = dimension_names .=> current_dimension_values
        for (i, reader) in enumerate(readers)
            if i == 1
                initial_idx = 1
            else
                initial_idx = sum(number_of_labels_per_file[1:(i-1)]) + 1
            end
            final_idx = sum(number_of_labels_per_file[1:i])
            data_to_write[initial_idx:final_idx] = Binary.read(reader; dims...)
        end
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
