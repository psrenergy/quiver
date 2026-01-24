module TestMetadata

using Quiver
using Test

include("fixture.jl")

@testset "Metadata" begin
    @testset "Scalar Metadata" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        # Test scalar metadata for label (NOT NULL, TEXT)
        meta = Quiver.get_scalar_metadata(db, "Collection", "label")
        @test meta.name == "label"
        @test meta.data_type == Quiver.QUIVER_DATA_TYPE_STRING
        @test meta.not_null == true
        @test meta.primary_key == false

        # Test scalar metadata for some_integer (nullable INTEGER)
        meta = Quiver.get_scalar_metadata(db, "Collection", "some_integer")
        @test meta.name == "some_integer"
        @test meta.data_type == Quiver.QUIVER_DATA_TYPE_INTEGER
        @test meta.not_null == false
        @test meta.primary_key == false

        # Test scalar metadata for some_float (nullable REAL)
        meta = Quiver.get_scalar_metadata(db, "Collection", "some_float")
        @test meta.name == "some_float"
        @test meta.data_type == Quiver.QUIVER_DATA_TYPE_FLOAT
        @test meta.not_null == false
        @test meta.primary_key == false

        # Test primary key metadata
        meta = Quiver.get_scalar_metadata(db, "Collection", "id")
        @test meta.name == "id"
        @test meta.data_type == Quiver.QUIVER_DATA_TYPE_INTEGER
        @test meta.primary_key == true

        Quiver.close!(db)
    end

    @testset "Vector Metadata" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        # Test vector metadata for 'values' group
        meta = Quiver.get_vector_metadata(db, "Collection", "values")
        @test meta.group_name == "values"
        @test length(meta.value_columns) == 2

        # Find the attributes by name
        attr_names = [attr.name for attr in meta.value_columns]
        @test "value_int" in attr_names
        @test "value_float" in attr_names

        # Check value_int attribute
        value_int_attr = first(filter(a -> a.name == "value_int", meta.value_columns))
        @test value_int_attr.data_type == Quiver.QUIVER_DATA_TYPE_INTEGER

        # Check value_float attribute
        value_float_attr = first(filter(a -> a.name == "value_float", meta.value_columns))
        @test value_float_attr.data_type == Quiver.QUIVER_DATA_TYPE_FLOAT

        Quiver.close!(db)
    end

    @testset "Set Metadata" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        # Test set metadata for 'tags' group
        meta = Quiver.get_set_metadata(db, "Collection", "tags")
        @test meta.group_name == "tags"
        @test length(meta.value_columns) == 1

        # Check tag attribute
        tag_attr = meta.value_columns[1]
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

        attrs = Quiver.list_scalar_attributes(db, "Collection")
        attr_names = [attr.name for attr in attrs]
        @test "id" in attr_names
        @test "label" in attr_names
        @test "some_integer" in attr_names
        @test "some_float" in attr_names

        # Verify metadata is included
        label_attr = first(filter(a -> a.name == "label", attrs))
        @test label_attr.data_type == Quiver.QUIVER_DATA_TYPE_STRING
        @test label_attr.not_null == true

        some_int_attr = first(filter(a -> a.name == "some_integer", attrs))
        @test some_int_attr.data_type == Quiver.QUIVER_DATA_TYPE_INTEGER
        @test some_int_attr.not_null == false

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

end # module
