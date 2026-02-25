module TestDatabaseDelete

using Quiver
using Test

include("fixture.jl")

@testset "Delete" begin
    @testset "Element By ID" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        # Create elements
        Quiver.create_element!(db, "Configuration"; label = "Config 1")
        Quiver.create_element!(db, "Configuration"; label = "Config 2")
        Quiver.create_element!(db, "Configuration"; label = "Config 3")

        # Verify elements exist
        ids = Quiver.read_element_ids(db, "Configuration")
        @test length(ids) == 3

        # Delete element with id 2
        Quiver.delete_element!(db, "Configuration", Int64(2))

        # Verify element is deleted
        ids = Quiver.read_element_ids(db, "Configuration")
        @test length(ids) == 2
        @test 2 ∉ ids
        @test 1 ∈ ids
        @test 3 ∈ ids

        Quiver.close!(db)
    end

    @testset "Element With Vector Data (CASCADE)" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        # Create element with vector data
        Quiver.create_element!(db, "Collection";
            label = "Item 1",
            some_integer = 10,
            value_int = [1, 2, 3],
            value_float = [0.1, 0.2, 0.3],
        )

        Quiver.create_element!(db, "Collection";
            label = "Item 2",
            some_integer = 20,
            value_int = [4, 5, 6],
        )

        # Verify elements exist
        ids = Quiver.read_element_ids(db, "Collection")
        @test length(ids) == 2

        # Delete element with id 1 (CASCADE should delete vector data)
        Quiver.delete_element!(db, "Collection", 1)

        # Verify element is deleted
        ids = Quiver.read_element_ids(db, "Collection")
        @test length(ids) == 1
        @test ids[1] == 2

        # Verify remaining element's data is intact
        values = Quiver.read_vector_integers(db, "Collection", "value_int")
        @test length(values) == 1
        @test values[1] == [4, 5, 6]

        Quiver.close!(db)
    end

    @testset "Element With Set Data (CASCADE)" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        # Create elements with set data
        Quiver.create_element!(db, "Collection"; label = "Item 1", tag = ["alpha", "beta"])
        Quiver.create_element!(db, "Collection"; label = "Item 2", tag = ["gamma", "delta"])

        # Verify elements exist
        ids = Quiver.read_element_ids(db, "Collection")
        @test length(ids) == 2

        # Delete element with id 1 (CASCADE should delete set data)
        Quiver.delete_element!(db, "Collection", 1)

        # Verify element is deleted
        ids = Quiver.read_element_ids(db, "Collection")
        @test length(ids) == 1
        @test ids[1] == 2

        # Verify remaining element's set data is intact
        sets = Quiver.read_set_strings(db, "Collection", "tag")
        @test length(sets) == 1
        @test sort(sets[1]) == ["delta", "gamma"]

        Quiver.close!(db)
    end

    @testset "Non-Existent Element (Idempotent)" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Config 1")

        # Delete non-existent element should succeed (idempotent)
        Quiver.delete_element!(db, "Configuration", Int64(999))

        # Verify original element still exists
        ids = Quiver.read_element_ids(db, "Configuration")
        @test length(ids) == 1
        @test ids[1] == 1

        Quiver.close!(db)
    end

    @testset "Does Not Affect Other Elements" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        # Create multiple elements with different values
        Quiver.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 100)
        Quiver.create_element!(db, "Configuration"; label = "Config 2", integer_attribute = 200)
        Quiver.create_element!(db, "Configuration"; label = "Config 3", integer_attribute = 300)

        # Delete middle element
        Quiver.delete_element!(db, "Configuration", Int64(2))

        # Verify other elements are unchanged
        labels = Quiver.read_scalar_strings(db, "Configuration", "label")
        @test length(labels) == 2
        @test "Config 1" ∈ labels
        @test "Config 3" ∈ labels
        @test "Config 2" ∉ labels

        values = Quiver.read_scalar_integers(db, "Configuration", "integer_attribute")
        @test length(values) == 2
        @test 100 ∈ values
        @test 300 ∈ values
        @test 200 ∉ values

        Quiver.close!(db)
    end
end

end
