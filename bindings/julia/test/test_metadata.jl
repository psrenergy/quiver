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
        @test meta.data_type == :text
        @test meta.not_null == true
        @test meta.primary_key == false

        # Test scalar metadata for some_integer (nullable INTEGER)
        meta = Quiver.get_scalar_metadata(db, "Collection", "some_integer")
        @test meta.name == "some_integer"
        @test meta.data_type == :integer
        @test meta.not_null == false
        @test meta.primary_key == false

        # Test scalar metadata for some_float (nullable REAL)
        meta = Quiver.get_scalar_metadata(db, "Collection", "some_float")
        @test meta.name == "some_float"
        @test meta.data_type == :real
        @test meta.not_null == false
        @test meta.primary_key == false

        # Test primary key metadata
        meta = Quiver.get_scalar_metadata(db, "Collection", "id")
        @test meta.name == "id"
        @test meta.data_type == :integer
        @test meta.primary_key == true

        Quiver.close!(db)
    end

    @testset "Vector Metadata" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        # Test vector metadata for 'values' group
        meta = Quiver.get_vector_metadata(db, "Collection", "values")
        @test meta.group_name == "values"
        @test length(meta.attributes) == 2

        # Find the attributes by name
        attr_names = [attr.name for attr in meta.attributes]
        @test "value_int" in attr_names
        @test "value_float" in attr_names

        # Check value_int attribute
        value_int_attr = first(filter(a -> a.name == "value_int", meta.attributes))
        @test value_int_attr.data_type == :integer

        # Check value_float attribute
        value_float_attr = first(filter(a -> a.name == "value_float", meta.attributes))
        @test value_float_attr.data_type == :real

        Quiver.close!(db)
    end

    @testset "Set Metadata" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        # Test set metadata for 'tags' group
        meta = Quiver.get_set_metadata(db, "Collection", "tags")
        @test meta.group_name == "tags"
        @test length(meta.attributes) == 1

        # Check tag attribute
        tag_attr = meta.attributes[1]
        @test tag_attr.name == "tag"
        @test tag_attr.data_type == :text

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
end

end # module
