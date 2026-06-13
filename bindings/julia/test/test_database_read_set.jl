module TestDatabaseReadSet

using Dates
using Quiver
using Test

include("fixture.jl")

@testset "Read Set" begin
    @testset "Set Attributes" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Collection"; label = "Item 1", tag = ["important", "urgent"])
        Quiver.create_element!(db, "Collection"; label = "Item 2", tag = ["review"])

        result = Quiver.read_set_strings(db, "Collection", "tag")
        @test length(result) == 2
        # Sets are unordered, so sort before comparison
        @test sort(result[1]) == ["important", "urgent"]
        @test result[2] == ["review"]

        Quiver.close!(db)
    end

    @testset "Set Empty Result" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        # No Collection elements created
        @test Quiver.read_set_strings(db, "Collection", "tag") == Vector{String}[]

        Quiver.close!(db)
    end

    @testset "Set Only Returns Elements With Data" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        # Create element with set data
        Quiver.create_element!(db, "Collection"; label = "Item 1", tag = ["important"])
        # Create element without set data
        Quiver.create_element!(db, "Collection"; label = "Item 2")
        # Create another element with set data
        Quiver.create_element!(db, "Collection"; label = "Item 3", tag = ["urgent", "review"])

        # Only elements with set data are returned
        result = Quiver.read_set_strings(db, "Collection", "tag")
        @test length(result) == 2

        Quiver.close!(db)
    end
    @testset "Set Strings by ID" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Collection"; label = "Item 1", tag = ["important", "urgent"])
        Quiver.create_element!(db, "Collection"; label = "Item 2", tag = ["review"])

        result1 = Quiver.read_set_strings_by_id(db, "Collection", "tag", 1)
        @test sort(result1) == ["important", "urgent"]
        @test Quiver.read_set_strings_by_id(db, "Collection", "tag", 2) == ["review"]

        Quiver.close!(db)
    end
    @testset "Set Invalid Collection" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        @test_throws Quiver.DatabaseException Quiver.read_set_strings(
            db,
            "NonexistentCollection",
            "tag",
        )

        Quiver.close!(db)
    end
    @testset "Read Set Integers Bulk" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "all_types.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "AllTypes"; label = "Item 1")
        Quiver.create_element!(db, "AllTypes"; label = "Item 2")

        Quiver.update_element!(db, "AllTypes", 1; code = [10, 20, 30])
        Quiver.update_element!(db, "AllTypes", 2; code = [40, 50])

        result = Quiver.read_set_integers(db, "AllTypes", "code")
        @test length(result) == 2
        @test sort(result[1]) == [10, 20, 30]
        @test sort(result[2]) == [40, 50]

        Quiver.close!(db)
    end

    @testset "Read Set Integers By ID" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "all_types.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "AllTypes"; label = "Item 1")
        Quiver.update_element!(db, "AllTypes", 1; code = [10, 20, 30])

        result = Quiver.read_set_integers_by_id(db, "AllTypes", "code", 1)
        @test sort(result) == [10, 20, 30]

        Quiver.close!(db)
    end

    @testset "Read Set Floats Bulk" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "all_types.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "AllTypes"; label = "Item 1")
        Quiver.create_element!(db, "AllTypes"; label = "Item 2")

        Quiver.update_element!(db, "AllTypes", 1; weight = [1.1, 2.2])
        Quiver.update_element!(db, "AllTypes", 2; weight = [3.3, 4.4, 5.5])

        result = Quiver.read_set_floats(db, "AllTypes", "weight")
        @test length(result) == 2
        @test sort(result[1]) == [1.1, 2.2]
        @test sort(result[2]) == [3.3, 4.4, 5.5]

        Quiver.close!(db)
    end

    @testset "Read Set Floats By ID" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "all_types.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "AllTypes"; label = "Item 1")
        Quiver.update_element!(db, "AllTypes", 1; weight = [1.1, 2.2])

        result = Quiver.read_set_floats_by_id(db, "AllTypes", "weight", 1)
        @test sort(result) == [1.1, 2.2]

        Quiver.close!(db)
    end
    @testset "Read Set Integers By ID Empty" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "all_types.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "AllTypes"; label = "Item 1")

        result = Quiver.read_set_integers_by_id(db, "AllTypes", "code", 1)
        @test isempty(result)

        Quiver.close!(db)
    end

    @testset "Read Set Floats By ID Empty" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "all_types.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "AllTypes"; label = "Item 1")

        result = Quiver.read_set_floats_by_id(db, "AllTypes", "weight", 1)
        @test isempty(result)

        Quiver.close!(db)
    end
    @testset "read_sets_by_id" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "composite_helpers.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        id = Quiver.create_element!(db, "Items";
            label = "Item 1",
            code = [10, 20, 30],
            weight = [1.1, 2.2],
            tag = ["alpha", "beta"],
        )

        result = Quiver.read_sets_by_id(db, "Items", id)

        @test length(result) == 3
        @test haskey(result, "code")
        @test haskey(result, "weight")
        @test haskey(result, "tag")
        @test sort(result["code"]) == [10, 20, 30]
        @test sort(result["weight"]) == [1.1, 2.2]
        @test sort(result["tag"]) == ["alpha", "beta"]

        # Verify element types (Dict values are Vector{Any}, check individual elements)
        @test all(v -> v isa Int64, result["code"])
        @test all(v -> v isa Float64, result["weight"])
        @test all(v -> v isa String, result["tag"])

        Quiver.close!(db)
    end
end

end
