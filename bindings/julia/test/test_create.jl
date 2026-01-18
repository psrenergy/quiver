module TestCreate

using PSRDatabase
using Test

include("fixture.jl")

@testset "Create" begin
    @testset "Scalar Attributes" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        # Create Configuration with various scalar types
        PSRDatabase.create_element!(db, "Configuration";
            label = "Test Config",
            integer_attribute = 42,
            float_attribute = 3.14,
            string_attribute = "hello",
            date_attribute = "2024-01-01",
            boolean_attribute = 1,
        )

        # Test default value is used
        PSRDatabase.create_element!(db, "Configuration"; label = "Config 2")

        PSRDatabase.close!(db)
    end

    @testset "Collections with Vectors" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")

        # Create element with scalar and vector attributes
        PSRDatabase.create_element!(db, "Collection";
            label = "Item 1",
            some_integer = 10,
            some_float = 1.5,
            value_int = [1, 2, 3],
            value_float = [0.1, 0.2, 0.3],
        )

        # Create element with only scalars
        PSRDatabase.create_element!(db, "Collection"; label = "Item 2", some_integer = 20)

        # Reject empty vector
        @test_throws PSRDatabase.DatabaseException PSRDatabase.create_element!(
            db,
            "Collection";
            label = "Item 3",
            value_int = Int64[],
        )

        PSRDatabase.close!(db)
    end

    @testset "Collections with Sets" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")

        # Create element with set attribute
        PSRDatabase.create_element!(db, "Collection"; label = "Item 1", tag = ["alpha", "beta", "gamma"])

        PSRDatabase.close!(db)
    end

    @testset "Relations" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")

        # Create parent elements
        PSRDatabase.create_element!(db, "Parent"; label = "Parent 1")
        PSRDatabase.create_element!(db, "Parent"; label = "Parent 2")

        # Create child with FK to parent
        PSRDatabase.create_element!(db, "Child"; label = "Child 1", parent_id = 1)

        # Create child with self-reference
        PSRDatabase.create_element!(db, "Child"; label = "Child 2", sibling_id = 1)

        # Create child with vector containing FK refs
        PSRDatabase.create_element!(db, "Child"; label = "Child 3", parent_ref = [1, 2])

        PSRDatabase.close!(db)
    end

    @testset "Reject Invalid Element" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        # Missing required label
        @test_throws PSRDatabase.DatabaseException PSRDatabase.create_element!(db, "Configuration")

        # Wrong type for attribute
        @test_throws PSRDatabase.DatabaseException PSRDatabase.create_element!(
            db,
            "Configuration";
            label = "Test",
            integer_attribute = "not an int",
        )

        # Unknown attribute
        @test_throws PSRDatabase.DatabaseException PSRDatabase.create_element!(
            db,
            "Configuration";
            label = "Test",
            unknown_attr = 123,
        )

        PSRDatabase.close!(db)
    end

    # Edge case tests

    @testset "Single Element Vector" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")

        # Create element with single element vector
        PSRDatabase.create_element!(db, "Collection"; label = "Item 1", value_int = [42])

        result = PSRDatabase.read_vector_integers_by_id(db, "Collection", "value_int", Int64(1))
        @test length(result) == 1
        @test result[1] == 42

        PSRDatabase.close!(db)
    end

    @testset "Large Vector" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")

        # Create element with 100+ element vector
        large_vector = collect(1:150)
        PSRDatabase.create_element!(db, "Collection"; label = "Item 1", value_int = large_vector)

        result = PSRDatabase.read_vector_integers_by_id(db, "Collection", "value_int", Int64(1))
        @test length(result) == 150
        @test result == large_vector

        PSRDatabase.close!(db)
    end

    @testset "Invalid Collection" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        @test_throws PSRDatabase.DatabaseException PSRDatabase.create_element!(
            db,
            "NonexistentCollection";
            label = "Test",
        )

        PSRDatabase.close!(db)
    end

    @testset "Only Required Label" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")

        # Create with only required label, no optional attributes
        PSRDatabase.create_element!(db, "Collection"; label = "Minimal Item")

        labels = PSRDatabase.read_scalar_strings(db, "Collection", "label")
        @test length(labels) == 1
        @test labels[1] == "Minimal Item"

        PSRDatabase.close!(db)
    end

    @testset "Multiple Sets" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")

        # Create element with set attribute containing multiple values
        PSRDatabase.create_element!(db, "Collection"; label = "Item 1", tag = ["a", "b", "c", "d", "e"])

        result = PSRDatabase.read_set_strings_by_id(db, "Collection", "tag", Int64(1))
        @test length(result) == 5
        @test sort(result) == ["a", "b", "c", "d", "e"]

        PSRDatabase.close!(db)
    end

    @testset "Duplicate Label Rejected" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Config 1")

        # Trying to create with same label should fail
        @test_throws PSRDatabase.DatabaseException PSRDatabase.create_element!(db, "Configuration"; label = "Config 1")

        PSRDatabase.close!(db)
    end

    @testset "Float Vector" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")

        PSRDatabase.create_element!(db, "Collection"; label = "Item 1", value_float = [1.1, 2.2, 3.3])

        result = PSRDatabase.read_vector_doubles_by_id(db, "Collection", "value_float", Int64(1))
        @test length(result) == 3
        @test result == [1.1, 2.2, 3.3]

        PSRDatabase.close!(db)
    end
end

end
