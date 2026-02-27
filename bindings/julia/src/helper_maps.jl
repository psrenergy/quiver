"""
    scalar_relation_map(
        db::Database,
        collection_from::String,
        collection_to::String,
        relation_type::String,
    )

The function returns a vector of integers that represent the position of
the related collection's element in the list of ids of the related collection.
The vector is ordered according to the order of the elements in the `collection_from`.
If there is no relation, the value is -1.
"""
function scalar_relation_map(
    db::Database,
    collection_from::String,
    collection_to::String,
    relation_type::String,
)
    attribute_on_collection_from = lowercase(collection_to) * "_" * relation_type
    collection_from_ids = read_scalar_integers(db, collection_from, "id")
    collection_to_ids = read_scalar_integers(db, collection_to, "id")
    map_of_indexes = Vector{Int}(undef, length(collection_from_ids))

    for (index_from, id_from) in enumerate(collection_from_ids)
        related_id = read_scalar_integer_by_id(db, collection_from, attribute_on_collection_from, id_from)
        if related_id !== nothing
            # It has to find some match every time
            index_to = findfirst(isequal(related_id), collection_to_ids)
            map_of_indexes[index_from] = index_to
        else
            map_of_indexes[index_from] = -1
        end
    end

    return map_of_indexes
end

"""
    set_relation_map(
        db::Database,
        collection_from::String,
        collection_to::String,
        relation_type::String,
    )

The function returns a vector of vectors of integers that represent the position of
the related collection's elements in the list of ids of the related collection.
The outer vector is ordered according to the order of the elements in the `collection_from`.
If there is no relation, the inner vector is empty.
"""
function set_relation_map(
    db::Database,
    collection_from::String,
    collection_to::String,
    relation_type::String,
)
    attribute_on_collection_from = lowercase(collection_to) * "_" * relation_type
    collection_from_ids = read_scalar_integers(db, collection_from, "id")
    collection_to_ids = read_scalar_integers(db, collection_to, "id")
    map_of_indexes = Vector{Vector{Int}}(undef, length(collection_from_ids))

    for (index_from, id_from) in enumerate(collection_from_ids)
        related_id = read_set_integers_by_id(db, collection_from, attribute_on_collection_from, id_from)
        set_relation_map = Int[]
        for id_to in related_id
            # It has to find some match every time
            index_to = findfirst(isequal(id_to), collection_to_ids)
            push!(set_relation_map, index_to)
        end
        map_of_indexes[index_from] = set_relation_map
    end

    return map_of_indexes
end
