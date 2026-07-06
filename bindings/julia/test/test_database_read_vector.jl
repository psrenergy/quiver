module TestDatabaseReadVector

using Dates
using Quiver
using Test

include("fixture.jl")

@testset "Read Vector" begin
    @testset "Vector Attributes" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(
            db,
            "Collection";
            label = "Item 1",
            value_int = [1, 2, 3],
            value_float = [1.5, 2.5, 3.5],
        )
        Quiver.create_element!(
            db,
            "Collection";
            label = "Item 2",
            value_int = [10, 20],
            value_float = [10.5, 20.5],
        )

        @test Quiver.read_vector_integers(db, "Collection", "value_int") == [[1, 2, 3], [10, 20]]
        @test Quiver.read_vector_floats(db, "Collection", "value_float") == [[1.5, 2.5, 3.5], [10.5, 20.5]]

        Quiver.close!(db)
    end

    @testset "Vector Empty Result" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        # No Collection elements created
        @test Quiver.read_vector_integers(db, "Collection", "value_int") == Vector{Int64}[]
        @test Quiver.read_vector_floats(db, "Collection", "value_float") == Vector{Float64}[]

        Quiver.close!(db)
    end

    @testset "Vector Only Returns Elements With Data" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        # Create element with vectors
        Quiver.create_element!(db, "Collection"; label = "Item 1", value_int = [1, 2, 3])
        # Create element without vectors (no vector data inserted)
        Quiver.create_element!(db, "Collection"; label = "Item 2")
        # Create another element with vectors
        Quiver.create_element!(db, "Collection"; label = "Item 3", value_int = [4, 5])

        # Only elements with vector data are returned
        result = Quiver.read_vector_integers(db, "Collection", "value_int")
        @test length(result) == 2
        @test result[1] == [1, 2, 3]
        @test result[2] == [4, 5]

        Quiver.close!(db)
    end
    @testset "Vector Integers by ID" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Collection"; label = "Item 1", value_int = [1, 2, 3])
        Quiver.create_element!(db, "Collection"; label = "Item 2", value_int = [10, 20])

        @test Quiver.read_vector_integers_by_id(db, "Collection", "value_int", 1) == [1, 2, 3]
        @test Quiver.read_vector_integers_by_id(db, "Collection", "value_int", 2) == [10, 20]

        Quiver.close!(db)
    end

    @testset "Vector Floats by ID" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Collection"; label = "Item 1", value_float = [1.5, 2.5, 3.5])

        @test Quiver.read_vector_floats_by_id(db, "Collection", "value_float", 1) == [1.5, 2.5, 3.5]

        Quiver.close!(db)
    end
    @testset "Vector by ID Empty" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Collection"; label = "Item 1")  # No vector data

        @test Quiver.read_vector_integers_by_id(db, "Collection", "value_int", 1) == Int64[]

        Quiver.close!(db)
    end
    @testset "Vector Invalid Collection" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        @test_throws Quiver.DatabaseException Quiver.read_vector_integers(
            db,
            "NonexistentCollection",
            "value_int",
        )
        @test_throws Quiver.DatabaseException Quiver.read_vector_floats(
            db,
            "NonexistentCollection",
            "value_float",
        )

        Quiver.close!(db)
    end
    @testset "Read Vector Strings Bulk" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "all_types.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "AllTypes"; label = "Item 1")
        Quiver.create_element!(db, "AllTypes"; label = "Item 2")

        Quiver.update_element!(db, "AllTypes", 1; label_value = ["alpha", "beta"])
        Quiver.update_element!(db, "AllTypes", 2; label_value = ["gamma"])

        result = Quiver.read_vector_strings(db, "AllTypes", "label_value")
        @test length(result) == 2
        @test result[1] == ["alpha", "beta"]
        @test result[2] == ["gamma"]

        Quiver.close!(db)
    end

    @testset "Read Vector Strings By ID" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "all_types.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "AllTypes"; label = "Item 1")
        Quiver.update_element!(db, "AllTypes", 1; label_value = ["alpha", "beta", "gamma"])

        result = Quiver.read_vector_strings_by_id(db, "AllTypes", "label_value", 1)
        @test result == ["alpha", "beta", "gamma"]

        Quiver.close!(db)
    end
    @testset "Read Vector Strings By ID Empty" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "all_types.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "AllTypes"; label = "Item 1")

        result = Quiver.read_vector_strings_by_id(db, "AllTypes", "label_value", 1)
        @test isempty(result)

        Quiver.close!(db)
    end
    @testset "Vector Group by ID" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Collection";
            label = "Item 1",
            value_int = [1, 2, 3],
            value_float = [1.5, 2.5, 3.5],
        )

        rows = Quiver.read_vector_group_by_id(db, "Collection", "values", 1)
        @test length(rows) == 3
        @test rows[1]["value_int"] == 1
        @test rows[1]["value_float"] == 1.5
        @test rows[3]["value_int"] == 3
        @test rows[3]["value_float"] == 3.5

        Quiver.close!(db)
    end

    @testset "Vector Group by ID Empty" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        rows = Quiver.read_vector_group_by_id(db, "Collection", "values", 999)
        @test isempty(rows)

        Quiver.close!(db)
    end

    @testset "read_vectors_by_id" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "composite_helpers.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        id = Quiver.create_element!(db, "Items";
            label = "Item 1",
            amount = [10, 20, 30],
            score = [1.1, 2.2],
            note = ["hello", "world"],
        )

        result = Quiver.read_vectors_by_id(db, "Items", id)

        @test length(result) == 3
        @test haskey(result, "amount")
        @test haskey(result, "score")
        @test haskey(result, "note")
        @test result["amount"] == [10, 20, 30]
        @test result["score"] == [1.1, 2.2]
        @test result["note"] == ["hello", "world"]

        # Verify element types (Dict values are Vector{Any}, check individual elements)
        @test all(v -> v isa Int64, result["amount"])
        @test all(v -> v isa Float64, result["score"])
        @test all(v -> v isa String, result["note"])

        Quiver.close!(db)
    end
end

end
