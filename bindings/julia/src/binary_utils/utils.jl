function is_equal(
    metadata1::Binary.Metadata,
    metadata2::Binary.Metadata;
    ignore_labels::Bool = false,
)::Bool
    if get_initial_datetime(metadata1) != get_initial_datetime(metadata2)
        return false
    end
    if !ignore_labels && get_labels(metadata1) != get_labels(metadata2)
        return false
    end
    if get_unit(metadata1) != get_unit(metadata2)
        return false
    end
    if get_version(metadata1) != get_version(metadata2)
        return false
    end
    dimensions1 = Binary.get_dimensions(metadata1)
    dimensions2 = Binary.get_dimensions(metadata2)
    if length(dimensions1) != length(dimensions2)
        return false
    end
    for (dim1, dim2) in zip(dimensions1, dimensions2)
        if dim1 != dim2
            return false
        end
    end
    return true
end
