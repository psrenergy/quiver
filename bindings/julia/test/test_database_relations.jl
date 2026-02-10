module TestDatabaseRelations

using Quiver
using Test

include("fixture.jl")

@testset "Relations" begin
    @testset "Set and Read Scalar Relations" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        # Create parents
        Quiver.create_element!(db, "Parent"; label = "Parent 1")
        Quiver.create_element!(db, "Parent"; label = "Parent 2")

        # Create children
        Quiver.create_element!(db, "Child"; label = "Child 1")
        Quiver.create_element!(db, "Child"; label = "Child 2")
        Quiver.create_element!(db, "Child"; label = "Child 3")

        # Set relations
        Quiver.update_scalar_relation!(db, "Child", "parent_id", "Child 1", "Parent 1")
        Quiver.update_scalar_relation!(db, "Child", "parent_id", "Child 3", "Parent 2")

        # Read relations
        labels = Quiver.read_scalar_relation(db, "Child", "parent_id")
        @test length(labels) == 3
        @test labels[1] == "Parent 1"
        @test labels[2] === nothing  # Child 2 has no parent
        @test labels[3] == "Parent 2"

        Quiver.close!(db)
    end

    @testset "Scalar Relations Self-Reference" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        # Create children (Child references itself via sibling_id)
        Quiver.create_element!(db, "Child"; label = "Child 1")
        Quiver.create_element!(db, "Child"; label = "Child 2")

        # Set sibling relation (self-reference)
        Quiver.update_scalar_relation!(db, "Child", "sibling_id", "Child 1", "Child 2")

        # Read sibling relations
        labels = Quiver.read_scalar_relation(db, "Child", "sibling_id")
        @test length(labels) == 2
        @test labels[1] == "Child 2"  # Child 1's sibling is Child 2
        @test labels[2] === nothing   # Child 2 has no sibling set

        Quiver.close!(db)
    end

    @testset "Scalar Relations Empty Result" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        # No Child elements created
        labels = Quiver.read_scalar_relation(db, "Child", "parent_id")
        @test labels == Union{String, Nothing}[]

        Quiver.close!(db)
    end

    @testset "Scalar Relation Invalid Collection" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        @test_throws Quiver.DatabaseException Quiver.read_scalar_relation(
            db,
            "NonexistentCollection",
            "parent_id",
        )

        Quiver.close!(db)
    end
end

end
