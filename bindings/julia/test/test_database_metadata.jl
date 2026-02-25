module TestDatabaseMetadata

using Quiver
using Test

include("fixture.jl")

@testset "Metadata" begin
    @testset "Scalar Metadata" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        # Test scalar metadata for label (NOT NULL, TEXT)
        metadata = Quiver.get_scalar_metadata(db, "Collection", "label")
        @test metadata.name == "label"
        @test metadata.data_type == Quiver.QUIVER_DATA_TYPE_STRING
        @test metadata.not_null == true
        @test metadata.primary_key == false

        # Test scalar metadata for some_integer (nullable INTEGER)
        metadata = Quiver.get_scalar_metadata(db, "Collection", "some_integer")
        @test metadata.name == "some_integer"
        @test metadata.data_type == Quiver.QUIVER_DATA_TYPE_INTEGER
        @test metadata.not_null == false
        @test metadata.primary_key == false

        # Test scalar metadata for some_float (nullable REAL)
        metadata = Quiver.get_scalar_metadata(db, "Collection", "some_float")
        @test metadata.name == "some_float"
        @test metadata.data_type == Quiver.QUIVER_DATA_TYPE_FLOAT
        @test metadata.not_null == false
        @test metadata.primary_key == false

        # Test primary key metadata
        metadata = Quiver.get_scalar_metadata(db, "Collection", "id")
        @test metadata.name == "id"
        @test metadata.data_type == Quiver.QUIVER_DATA_TYPE_INTEGER
        @test metadata.primary_key == true

        Quiver.close!(db)
    end

    @testset "Vector Metadata" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        # Test vector metadata for 'values' group
        metadata = Quiver.get_vector_metadata(db, "Collection", "values")
        @test metadata.group_name == "values"
        @test length(metadata.value_columns) == 2

        # Find the attributes by name
        attr_names = [attribute.name for attribute in metadata.value_columns]
        @test "value_int" in attr_names
        @test "value_float" in attr_names

        # Check value_int attribute
        value_int_attr = first(filter(a -> a.name == "value_int", metadata.value_columns))
        @test value_int_attr.data_type == Quiver.QUIVER_DATA_TYPE_INTEGER

        # Check value_float attribute
        value_float_attr = first(filter(a -> a.name == "value_float", metadata.value_columns))
        @test value_float_attr.data_type == Quiver.QUIVER_DATA_TYPE_FLOAT

        Quiver.close!(db)
    end

    @testset "Set Metadata" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        # Test set metadata for 'tags' group
        metadata = Quiver.get_set_metadata(db, "Collection", "tags")
        @test metadata.group_name == "tags"
        @test length(metadata.value_columns) == 1

        # Check tag attribute
        tag_attr = metadata.value_columns[1]
        @test tag_attr.name == "tag"
        @test tag_attr.data_type == Quiver.QUIVER_DATA_TYPE_STRING

        Quiver.close!(db)
    end

    @testset "Error Cases" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        # Non-existent collection
        @test_throws Quiver.DatabaseException Quiver.get_scalar_metadata(db, "NonExistent", "label")

        # Non-existent attribute
        @test_throws Quiver.DatabaseException Quiver.get_scalar_metadata(db, "Collection", "nonexistent")

        # Non-existent vector group
        @test_throws Quiver.DatabaseException Quiver.get_vector_metadata(db, "Collection", "nonexistent")

        # Non-existent set group
        @test_throws Quiver.DatabaseException Quiver.get_set_metadata(db, "Collection", "nonexistent")

        Quiver.close!(db)
    end

    @testset "List Scalar Attributes" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        attributes = Quiver.list_scalar_attributes(db, "Collection")
        attr_names = [attribute.name for attribute in attributes]
        @test "id" in attr_names
        @test "label" in attr_names
        @test "some_integer" in attr_names
        @test "some_float" in attr_names

        # Verify metadata is included
        label_attr = first(filter(a -> a.name == "label", attributes))
        @test label_attr.data_type == Quiver.QUIVER_DATA_TYPE_STRING
        @test label_attr.not_null == true

        some_int_attr = first(filter(a -> a.name == "some_integer", attributes))
        @test some_int_attr.data_type == Quiver.QUIVER_DATA_TYPE_INTEGER
        @test some_int_attr.not_null == false

        Quiver.close!(db)
    end

    @testset "Foreign Key Metadata" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        # parent_id is a foreign key referencing Parent(id)
        metadata = Quiver.get_scalar_metadata(db, "Child", "parent_id")
        @test metadata.is_foreign_key == true
        @test metadata.references_collection == "Parent"
        @test metadata.references_column == "id"

        # sibling_id is a self-referencing foreign key
        metadata = Quiver.get_scalar_metadata(db, "Child", "sibling_id")
        @test metadata.is_foreign_key == true
        @test metadata.references_collection == "Child"
        @test metadata.references_column == "id"

        # label is not a foreign key
        metadata = Quiver.get_scalar_metadata(db, "Child", "label")
        @test metadata.is_foreign_key == false
        @test metadata.references_collection === nothing
        @test metadata.references_column === nothing

        # list_scalar_attributes should also include FK info
        attributes = Quiver.list_scalar_attributes(db, "Child")
        parent_attr = first(filter(a -> a.name == "parent_id", attributes))
        @test parent_attr.is_foreign_key == true
        @test parent_attr.references_collection == "Parent"

        label_attr = first(filter(a -> a.name == "label", attributes))
        @test label_attr.is_foreign_key == false
        @test label_attr.references_collection === nothing

        Quiver.close!(db)
    end

    @testset "List Vector Groups" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        groups = Quiver.list_vector_groups(db, "Collection")
        group_names = [g.group_name for g in groups]
        @test "values" in group_names

        # Verify metadata is included
        values_group = first(filter(g -> g.group_name == "values", groups))
        @test length(values_group.value_columns) == 2
        attr_names = [a.name for a in values_group.value_columns]
        @test "value_int" in attr_names
        @test "value_float" in attr_names

        Quiver.close!(db)
    end

    @testset "List Set Groups" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        groups = Quiver.list_set_groups(db, "Collection")
        group_names = [g.group_name for g in groups]
        @test "tags" in group_names

        # Verify metadata is included
        tags_group = first(filter(g -> g.group_name == "tags", groups))
        @test length(tags_group.value_columns) == 1
        @test tags_group.value_columns[1].name == "tag"
        @test tags_group.value_columns[1].data_type == Quiver.QUIVER_DATA_TYPE_STRING

        Quiver.close!(db)
    end
end

@testset "Read Set Group By ID" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "all_types.sql")
    db = Quiver.from_schema(":memory:", path_schema)

    Quiver.create_element!(db, "AllTypes"; label = "Item 1")
    Quiver.update_element!(db, "AllTypes", Int64(1); code = [10, 20, 30])
    Quiver.update_element!(db, "AllTypes", Int64(1); weight = [1.1, 2.2])

    result = Quiver.read_set_group_by_id(db, "AllTypes", "codes", Int64(1))
    @test !isempty(result)
    # Verify it returns rows with data
    codes = [row["code"] for row in result]
    @test sort(codes) == [10, 20, 30]

    Quiver.close!(db)
end
end # module
