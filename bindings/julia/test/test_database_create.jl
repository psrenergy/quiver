module TestDatabaseCreate

using Dates
using Quiver
using Test

include("fixture.jl")

@testset "Create" begin
    @testset "Scalar Attributes" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        # Create Configuration with various scalar types
        Quiver.create_element!(db, "Configuration";
            label = "Test Config",
            integer_attribute = 42,
            float_attribute = 3.14,
            string_attribute = "hello",
            date_attribute = "2024-01-01",
            boolean_attribute = 1,
        )

        # Test default value is used
        Quiver.create_element!(db, "Configuration"; label = "Config 2")

        Quiver.close!(db)
    end

    @testset "Collections with Vectors" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        # Create element with scalar and vector attributes
        Quiver.create_element!(db, "Collection";
            label = "Item 1",
            some_integer = 10,
            some_float = 1.5,
            value_int = [1, 2, 3],
            value_float = [0.1, 0.2, 0.3],
        )

        # Create element with only scalars
        Quiver.create_element!(db, "Collection"; label = "Item 2", some_integer = 20)

        # Reject empty vector
        @test_throws Quiver.DatabaseException Quiver.create_element!(
            db,
            "Collection";
            label = "Item 3",
            value_int = Int64[],
        )

        Quiver.close!(db)
    end

    @testset "Collections with Sets" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        # Create element with set attribute
        Quiver.create_element!(db, "Collection"; label = "Item 1", tag = ["alpha", "beta", "gamma"])

        Quiver.close!(db)
    end

    @testset "Relations" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        # Create parent elements
        Quiver.create_element!(db, "Parent"; label = "Parent 1")
        Quiver.create_element!(db, "Parent"; label = "Parent 2")

        # Create child with FK to parent
        Quiver.create_element!(db, "Child"; label = "Child 1", parent_id = 1)

        # Create child with self-reference
        Quiver.create_element!(db, "Child"; label = "Child 2", sibling_id = 1)

        # Create child with vector containing FK refs
        Quiver.create_element!(db, "Child"; label = "Child 3", parent_ref = [1, 2])

        Quiver.close!(db)
    end

    @testset "Collections with Time Series" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        Quiver.create_element!(db, "Collection";
            label = "Item 1",
            date_time = [DateTime(2024, 1, 1, 10), DateTime(2024, 1, 2, 10), DateTime(2024, 1, 3, 10)],
            value = [1.5, 2.5, 3.5],
        )

        # Verify time series data was persisted
        rows = Quiver.read_time_series_group(db, "Collection", "data", Int64(1))
        @test length(rows) == 3
        @test rows[1]["date_time"] == "2024-01-01T10:00:00"
        @test rows[2]["date_time"] == "2024-01-02T10:00:00"
        @test rows[3]["date_time"] == "2024-01-03T10:00:00"
        @test rows[1]["value"] == 1.5
        @test rows[2]["value"] == 2.5
        @test rows[3]["value"] == 3.5

        Quiver.close!(db)
    end

    @testset "Reject Invalid Element" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        # Missing required label
        @test_throws Quiver.DatabaseException Quiver.create_element!(db, "Configuration")

        # Wrong type for attribute
        @test_throws Quiver.DatabaseException Quiver.create_element!(
            db,
            "Configuration";
            label = "Test",
            integer_attribute = "not an int",
        )

        # Unknown attribute
        @test_throws Quiver.DatabaseException Quiver.create_element!(
            db,
            "Configuration";
            label = "Test",
            unknown_attr = 123,
        )

        Quiver.close!(db)
    end

    # Edge case tests

    @testset "Single Element Vector" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        # Create element with single element vector
        Quiver.create_element!(db, "Collection"; label = "Item 1", value_int = [42])

        result = Quiver.read_vector_integers_by_id(db, "Collection", "value_int", Int64(1))
        @test length(result) == 1
        @test result[1] == 42

        Quiver.close!(db)
    end

    @testset "Large Vector" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        # Create element with 100+ element vector
        large_vector = collect(1:150)
        Quiver.create_element!(db, "Collection"; label = "Item 1", value_int = large_vector)

        result = Quiver.read_vector_integers_by_id(db, "Collection", "value_int", Int64(1))
        @test length(result) == 150
        @test result == large_vector

        Quiver.close!(db)
    end

    @testset "Invalid Collection" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        @test_throws Quiver.DatabaseException Quiver.create_element!(
            db,
            "NonexistentCollection";
            label = "Test",
        )

        Quiver.close!(db)
    end

    @testset "Only Required Label" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        # Create with only required label, no optional attributes
        Quiver.create_element!(db, "Collection"; label = "Minimal Item")

        labels = Quiver.read_scalar_strings(db, "Collection", "label")
        @test length(labels) == 1
        @test labels[1] == "Minimal Item"

        Quiver.close!(db)
    end

    @testset "Multiple Sets" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        # Create element with set attribute containing multiple values
        Quiver.create_element!(db, "Collection"; label = "Item 1", tag = ["a", "b", "c", "d", "e"])

        result = Quiver.read_set_strings_by_id(db, "Collection", "tag", Int64(1))
        @test length(result) == 5
        @test sort(result) == ["a", "b", "c", "d", "e"]

        Quiver.close!(db)
    end

    @testset "Duplicate Label Rejected" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Config 1")

        # Trying to create with same label should fail
        @test_throws Quiver.DatabaseException Quiver.create_element!(
            db,
            "Configuration";
            label = "Config 1",
        )

        Quiver.close!(db)
    end

    @testset "Float Vector" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        Quiver.create_element!(db, "Collection"; label = "Item 1", value_float = [1.1, 2.2, 3.3])

        result = Quiver.read_vector_floats_by_id(db, "Collection", "value_float", Int64(1))
        @test length(result) == 3
        @test result == [1.1, 2.2, 3.3]

        Quiver.close!(db)
    end

    @testset "Special Characters" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        special_label = "ãçéóú\$/MWh"
        Quiver.create_element!(db, "Configuration"; label = special_label)

        labels = Quiver.read_scalar_strings(db, "Configuration", "label")
        @test length(labels) == 1
        @test labels[1] == special_label

        Quiver.close!(db)
    end

    @testset "Native DateTime Object" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        # Create element using native DateTime object
        dt = DateTime(2024, 3, 15, 14, 30, 45)
        Quiver.create_element!(db, "Configuration";
            label = "Config with DateTime",
            date_attribute = dt,
        )

        # Verify it was stored correctly as ISO 8601 string
        date_str = Quiver.read_scalar_string_by_id(db, "Configuration", "date_attribute", Int64(1))
        @test date_str == "2024-03-15T14:30:45"

        # Verify read_all_scalars_by_id returns native DateTime
        scalars = Quiver.read_all_scalars_by_id(db, "Configuration", Int64(1))
        @test scalars["date_attribute"] isa DateTime
        @test scalars["date_attribute"] == dt

        Quiver.close!(db)
    end
end

end
