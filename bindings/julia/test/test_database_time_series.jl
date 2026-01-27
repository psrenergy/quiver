module TestDatabaseTimeSeries

using Dates
using Quiver
using Test

include("fixture.jl")

@testset "Time Series" begin
    @testset "Add and Read Single Row" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "time_series.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        # Create a resource element
        id = Quiver.create_element!(db, "Resource"; label = "Resource 1")
        @test id == 1

        # Add a time series row
        Quiver.add_time_series_row!(db, "Resource", "value1", id, 42.5, "2024-01-01T00:00:00")

        # Read back
        rows = Quiver.read_time_series_table(db, "Resource", "group1", id)
        @test length(rows) == 1
        @test rows[1]["date_time"] == "2024-01-01T00:00:00"
        @test rows[1]["value1"] == 42.5

        Quiver.close!(db)
    end

    @testset "Add Multiple Rows" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "time_series.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        id = Quiver.create_element!(db, "Resource"; label = "Resource 1")

        # Add multiple rows
        Quiver.add_time_series_row!(db, "Resource", "value1", id, 10.0, "2024-01-01T00:00:00")
        Quiver.add_time_series_row!(db, "Resource", "value1", id, 20.0, "2024-01-02T00:00:00")
        Quiver.add_time_series_row!(db, "Resource", "value1", id, 30.0, "2024-01-03T00:00:00")

        rows = Quiver.read_time_series_table(db, "Resource", "group1", id)
        @test length(rows) == 3

        # Should be ordered by date_time
        @test rows[1]["date_time"] == "2024-01-01T00:00:00"
        @test rows[2]["date_time"] == "2024-01-02T00:00:00"
        @test rows[3]["date_time"] == "2024-01-03T00:00:00"

        @test rows[1]["value1"] == 10.0
        @test rows[2]["value1"] == 20.0
        @test rows[3]["value1"] == 30.0

        Quiver.close!(db)
    end

    @testset "Multi-Dimensional With Block" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "time_series.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        id = Quiver.create_element!(db, "Resource"; label = "Resource 1")

        # Add rows with block dimension
        Quiver.add_time_series_row!(db, "Resource", "value3", id, 100.0, "2024-01-01T00:00:00";
            dimensions = Dict("block" => Int64(1)))
        Quiver.add_time_series_row!(db, "Resource", "value3", id, 200.0, "2024-01-01T00:00:00";
            dimensions = Dict("block" => Int64(2)))
        Quiver.add_time_series_row!(db, "Resource", "value3", id, 150.0, "2024-01-02T00:00:00";
            dimensions = Dict("block" => Int64(1)))

        rows = Quiver.read_time_series_table(db, "Resource", "group2", id)
        @test length(rows) == 3

        # Verify dimensions are correct (ordered by date_time, then block)
        @test rows[1]["date_time"] == "2024-01-01T00:00:00"
        @test rows[1]["block"] == 1
        @test rows[1]["value3"] == 100.0

        @test rows[2]["date_time"] == "2024-01-01T00:00:00"
        @test rows[2]["block"] == 2
        @test rows[2]["value3"] == 200.0

        Quiver.close!(db)
    end

    @testset "Update Existing Row" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "time_series.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        id = Quiver.create_element!(db, "Resource"; label = "Resource 1")

        # Add initial row
        Quiver.add_time_series_row!(db, "Resource", "value1", id, 42.5, "2024-01-01T00:00:00")

        # Update it
        Quiver.update_time_series_row!(db, "Resource", "value1", id, 99.9, "2024-01-01T00:00:00")

        rows = Quiver.read_time_series_table(db, "Resource", "group1", id)
        @test length(rows) == 1
        @test rows[1]["value1"] == 99.9

        Quiver.close!(db)
    end

    @testset "Delete All For Element" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "time_series.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        id = Quiver.create_element!(db, "Resource"; label = "Resource 1")

        # Add multiple rows
        Quiver.add_time_series_row!(db, "Resource", "value1", id, 10.0, "2024-01-01T00:00:00")
        Quiver.add_time_series_row!(db, "Resource", "value1", id, 20.0, "2024-01-02T00:00:00")

        # Delete all
        Quiver.delete_time_series!(db, "Resource", "group1", id)

        rows = Quiver.read_time_series_table(db, "Resource", "group1", id)
        @test isempty(rows)

        Quiver.close!(db)
    end

    @testset "Get Metadata" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "time_series.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        meta = Quiver.get_time_series_metadata(db, "Resource", "group1")
        @test meta.group_name == "group1"
        @test length(meta.dimension_columns) == 1
        @test "date_time" in meta.dimension_columns
        @test length(meta.value_columns) == 2  # value1 and value2

        Quiver.close!(db)
    end

    @testset "List Time Series Groups" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "time_series.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        groups = Quiver.list_time_series_groups(db, "Resource")
        @test length(groups) == 3  # group1, group2, group3

        Quiver.close!(db)
    end

    @testset "With DateTime Object" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "time_series.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        id = Quiver.create_element!(db, "Resource"; label = "Resource 1")

        # Use DateTime object
        dt = DateTime(2024, 1, 15, 12, 30, 0)
        Quiver.add_time_series_row!(db, "Resource", "value1", id, 42.5, dt)

        rows = Quiver.read_time_series_table(db, "Resource", "group1", id)
        @test length(rows) == 1
        @test rows[1]["date_time"] == "2024-01-15T12:30:00"

        Quiver.close!(db)
    end
end

end # module
