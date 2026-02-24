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
        result = Quiver.read_time_series_group(db, "Collection", "data", Int64(1))
        @test length(result) == 2  # 2 columns: date_time, value
        @test length(result["date_time"]) == 3
        @test result["date_time"][1] == DateTime(2024, 1, 1, 10, 0, 0)
        @test result["date_time"][2] == DateTime(2024, 1, 2, 10, 0, 0)
        @test result["date_time"][3] == DateTime(2024, 1, 3, 10, 0, 0)
        @test result["value"][1] == 1.5
        @test result["value"][2] == 2.5
        @test result["value"][3] == 3.5

        Quiver.close!(db)
    end

    @testset "Multiple Time Series Groups with Shared Dimension" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "multi_time_series.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        Quiver.create_element!(db, "Sensor";
            label = "Sensor 1",
            date_time = [DateTime(2024, 1, 1, 10), DateTime(2024, 1, 2, 10), DateTime(2024, 1, 3, 10)],
            temperature = [20.0, 21.5, 22.0],
            humidity = [45.0, 50.0, 55.0],
        )

        # Verify temperature group
        temp_result = Quiver.read_time_series_group(db, "Sensor", "temperature", Int64(1))
        @test length(temp_result) == 2  # 2 columns: date_time, temperature
        @test length(temp_result["date_time"]) == 3
        @test temp_result["date_time"][1] == DateTime(2024, 1, 1, 10, 0, 0)
        @test temp_result["date_time"][2] == DateTime(2024, 1, 2, 10, 0, 0)
        @test temp_result["date_time"][3] == DateTime(2024, 1, 3, 10, 0, 0)
        @test temp_result["temperature"][1] == 20.0
        @test temp_result["temperature"][2] == 21.5
        @test temp_result["temperature"][3] == 22.0

        # Verify humidity group
        hum_result = Quiver.read_time_series_group(db, "Sensor", "humidity", Int64(1))
        @test length(hum_result) == 2  # 2 columns: date_time, humidity
        @test length(hum_result["date_time"]) == 3
        @test hum_result["date_time"][1] == DateTime(2024, 1, 1, 10, 0, 0)
        @test hum_result["date_time"][2] == DateTime(2024, 1, 2, 10, 0, 0)
        @test hum_result["date_time"][3] == DateTime(2024, 1, 3, 10, 0, 0)
        @test hum_result["humidity"][1] == 45.0
        @test hum_result["humidity"][2] == 50.0
        @test hum_result["humidity"][3] == 55.0

        Quiver.close!(db)
    end

    @testset "Multiple Time Series Groups Mismatched Lengths" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "multi_time_series.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        @test_throws Quiver.DatabaseException Quiver.create_element!(db, "Sensor";
            label = "Sensor 1",
            date_time = [DateTime(2024, 1, 1, 10), DateTime(2024, 1, 2, 10), DateTime(2024, 1, 3, 10)],
            temperature = [20.0, 21.5, 22.0],
            humidity = [45.0, 50.0],
        )

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

    # ============================================================================
    # FK label resolution in create_element!
    # ============================================================================

    @testset "Resolve FK Label In Set Create" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Parent"; label = "Parent 1")
        Quiver.create_element!(db, "Parent"; label = "Parent 2")

        # Create child with set FK using string labels (mentor_id is unique to set table)
        Quiver.create_element!(db, "Child"; label = "Child 1", mentor_id = ["Parent 1", "Parent 2"])

        # Read back resolved integer IDs
        result = Quiver.read_set_integers_by_id(db, "Child", "mentor_id", Int64(1))
        @test sort(result) == [1, 2]

        Quiver.close!(db)
    end

    @testset "Resolve FK Label Missing Target" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        @test_throws Quiver.DatabaseException Quiver.create_element!(db, "Child";
            label = "Child 1",
            mentor_id = ["Nonexistent Parent"],
        )

        Quiver.close!(db)
    end

    @testset "Reject String For Non-FK Integer Column" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Parent"; label = "Parent 1")

        @test_throws Quiver.DatabaseException Quiver.create_element!(db, "Child";
            label = "Child 1",
            parent_id = 1,
            score = ["not_a_label"],
        )

        Quiver.close!(db)
    end

    @testset "Create Element Scalar FK Label" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Parent"; label = "Parent 1")

        # Create child with scalar FK using string label (not integer)
        Quiver.create_element!(db, "Child"; label = "Child 1", parent_id = "Parent 1")

        # Verify: read back as integer, should be resolved ID
        result = Quiver.read_scalar_integers(db, "Child", "parent_id")
        @test result == [1]

        Quiver.close!(db)
    end

    @testset "Create Element Scalar FK Integer" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Parent"; label = "Parent 1")

        # Create child with scalar FK using integer ID directly
        Quiver.create_element!(db, "Child"; label = "Child 1", parent_id = 1)

        # Verify: read back integer, should be stored as-is
        result = Quiver.read_scalar_integers(db, "Child", "parent_id")
        @test result == [1]

        Quiver.close!(db)
    end

    @testset "Create Element Vector FK Labels" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Parent"; label = "Parent 1")
        Quiver.create_element!(db, "Parent"; label = "Parent 2")

        # Create child with vector FK using string labels
        Quiver.create_element!(db, "Child"; label = "Child 1", parent_ref = ["Parent 1", "Parent 2"])

        # Verify: read back vector integers
        result = Quiver.read_vector_integers_by_id(db, "Child", "parent_ref", Int64(1))
        @test result == [1, 2]

        Quiver.close!(db)
    end

    @testset "Create Element Time Series FK Labels" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Parent"; label = "Parent 1")
        Quiver.create_element!(db, "Parent"; label = "Parent 2")

        # Create child with time series FK using string labels
        Quiver.create_element!(db, "Child";
            label = "Child 1",
            date_time = ["2024-01-01", "2024-01-02"],
            sponsor_id = ["Parent 1", "Parent 2"],
        )

        # Verify: read time series group and check sponsor_id values
        result = Quiver.read_time_series_group(db, "Child", "events", Int64(1))
        @test result["sponsor_id"] == [1, 2]

        Quiver.close!(db)
    end

    @testset "Create Element All FK Types In One Call" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Parent"; label = "Parent 1")
        Quiver.create_element!(db, "Parent"; label = "Parent 2")

        # Create child with ALL FK types in one call
        Quiver.create_element!(db, "Child";
            label = "Child 1",
            parent_id = "Parent 1",
            mentor_id = ["Parent 2"],
            parent_ref = ["Parent 1"],
            date_time = ["2024-01-01"],
            sponsor_id = ["Parent 2"],
        )

        # Verify scalar FK
        @test Quiver.read_scalar_integers(db, "Child", "parent_id") == [1]

        # Verify set FK (mentor_id)
        @test sort(Quiver.read_set_integers_by_id(db, "Child", "mentor_id", Int64(1))) == [2]

        # Verify vector FK (parent_ref)
        @test Quiver.read_vector_integers_by_id(db, "Child", "parent_ref", Int64(1)) == [1]

        # Verify time series FK (sponsor_id)
        ts = Quiver.read_time_series_group(db, "Child", "events", Int64(1))
        @test ts["sponsor_id"] == [2]

        Quiver.close!(db)
    end

    @testset "Create Element No FK Columns Unchanged" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        # basic.sql has no FK columns -- pre-resolve pass is a no-op
        Quiver.create_element!(db, "Configuration";
            label = "Config 1",
            integer_attribute = 42,
            float_attribute = 3.14,
        )

        @test Quiver.read_scalar_strings(db, "Configuration", "label") == ["Config 1"]
        @test Quiver.read_scalar_integers(db, "Configuration", "integer_attribute") == [42]
        @test Quiver.read_scalar_floats(db, "Configuration", "float_attribute") == [3.14]

        Quiver.close!(db)
    end

    @testset "Scalar FK Resolution Failure Causes No Partial Writes" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        # Attempt to create child with scalar FK referencing nonexistent parent
        @test_throws Quiver.DatabaseException Quiver.create_element!(db, "Child";
            label = "Orphan Child",
            parent_id = "Nonexistent Parent",
        )

        # Verify: no child was created (zero partial writes)
        labels = Quiver.read_scalar_strings(db, "Child", "label")
        @test length(labels) == 0

        Quiver.close!(db)
    end
end

end
