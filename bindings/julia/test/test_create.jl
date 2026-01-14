module TestCreate

using PSRDatabase
using Test

include("fixture.jl")

@testset "Create Scalar Attributes" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
    db = PSRDatabase.create_empty_db_from_schema(":memory:", path_schema; force = true)

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

@testset "Create Collections with Vectors" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
    db = PSRDatabase.create_empty_db_from_schema(":memory:", path_schema; force = true)

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
    PSRDatabase.create_element!(db, "Collection";
        label = "Item 2",
        some_integer = 20,
    )

    # Reject empty vector
    @test_throws PSRDatabase.DatabaseException PSRDatabase.create_element!(db, "Collection";
        label = "Item 3",
        value_int = Int64[],
    )

    PSRDatabase.close!(db)
end

@testset "Create Collections with Sets" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
    db = PSRDatabase.create_empty_db_from_schema(":memory:", path_schema; force = true)

    PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")

    # Create element with set attribute
    PSRDatabase.create_element!(db, "Collection";
        label = "Item 1",
        tag = ["alpha", "beta", "gamma"],
    )

    PSRDatabase.close!(db)
end

@testset "Create Relations" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
    db = PSRDatabase.create_empty_db_from_schema(":memory:", path_schema; force = true)

    PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")

    # Create parent elements
    PSRDatabase.create_element!(db, "Parent"; label = "Parent 1")
    PSRDatabase.create_element!(db, "Parent"; label = "Parent 2")

    # Create child with FK to parent
    PSRDatabase.create_element!(db, "Child";
        label = "Child 1",
        parent_id = 1,
    )

    # Create child with self-reference
    PSRDatabase.create_element!(db, "Child";
        label = "Child 2",
        sibling_id = 1,
    )

    # Create child with vector containing FK refs
    PSRDatabase.create_element!(db, "Child";
        label = "Child 3",
        parent_ref = [1, 2],
    )

    PSRDatabase.close!(db)
end

@testset "Reject Invalid Element" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
    db = PSRDatabase.create_empty_db_from_schema(":memory:", path_schema; force = true)

    # Missing required label
    @test_throws PSRDatabase.DatabaseException PSRDatabase.create_element!(db, "Configuration")

    # Wrong type for attribute
    @test_throws PSRDatabase.DatabaseException PSRDatabase.create_element!(db, "Configuration";
        label = "Test",
        integer_attribute = "not an int",
    )

    # Unknown attribute
    @test_throws PSRDatabase.DatabaseException PSRDatabase.create_element!(db, "Configuration";
        label = "Test",
        unknown_attr = 123,
    )

    PSRDatabase.close!(db)
end

end
