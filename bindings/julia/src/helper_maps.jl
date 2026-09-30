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
    position = Dict(id => index for (index, id) in enumerate(read_element_ids(db, collection_to)))
    related_ids = read_scalar_integers(db, collection_from, attribute_on_collection_from)
    return Int[isnothing(related_id) ? -1 : position[related_id] for related_id in related_ids]
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
If there is no relation, the inner vector is empty. A null cell in the set group (a nullable
relation column with an empty row) is skipped, so the inner vector holds only real targets.
"""
function set_relation_map(
    db::Database,
    collection_from::String,
    collection_to::String,
    relation_type::String,
)
    attribute_on_collection_from = lowercase(collection_to) * "_" * relation_type
    position = Dict(id => index for (index, id) in enumerate(read_element_ids(db, collection_to)))
    related_ids = read_set_integers(db, collection_from, attribute_on_collection_from)
    # A null cell is an empty relation, not a target to look up.
    return Vector{Int}[Int[position[id] for id in ids if !isnothing(id)] for ids in related_ids]
end
