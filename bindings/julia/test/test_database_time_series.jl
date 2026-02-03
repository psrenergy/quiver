module TestDatabaseTimeSeries

using Quiver
using Test

include("fixture.jl")

@testset "Time Series" begin
    @testset "Metadata" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        # get_time_series_metadata
        meta = Quiver.get_time_series_metadata(db, "Collection", "data")
        @test meta.group_name == "data"
        @test length(meta.value_columns) == 1
        @test meta.value_columns[1].name == "value"

        # list_time_series_groups
        groups = Quiver.list_time_series_groups(db, "Collection")
        @test length(groups) == 1
        @test groups[1].group_name == "data"

        Quiver.close!(db)
    end

    @testset "Metadata Empty" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        # Configuration has no time series tables
        groups = Quiver.list_time_series_groups(db, "Configuration")
        @test isempty(groups)

        Quiver.close!(db)
    end

    @testset "Read" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        id = Quiver.create_element!(db, "Collection"; label = "Item 1")

        # Insert time series data
        Quiver.update_time_series_group!(
            db,
            "Collection",
            "data",
            id,
            [
                Dict{String, Any}("date_time" => "2024-01-01T10:00:00", "value" => 1.5),
                Dict{String, Any}("date_time" => "2024-01-01T11:00:00", "value" => 2.5),
                Dict{String, Any}("date_time" => "2024-01-01T12:00:00", "value" => 3.5),
            ],
        )

        # Read back
        rows = Quiver.read_time_series_group_by_id(db, "Collection", "data", id)
        @test length(rows) == 3
        @test rows[1]["date_time"] == "2024-01-01T10:00:00"
        @test rows[1]["value"] == 1.5
        @test rows[2]["date_time"] == "2024-01-01T11:00:00"
        @test rows[2]["value"] == 2.5
        @test rows[3]["date_time"] == "2024-01-01T12:00:00"
        @test rows[3]["value"] == 3.5

        Quiver.close!(db)
    end

    @testset "Read Empty" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        id = Quiver.create_element!(db, "Collection"; label = "Item 1")

        # No time series data inserted
        rows = Quiver.read_time_series_group_by_id(db, "Collection", "data", id)
        @test isempty(rows)

        Quiver.close!(db)
    end

    @testset "Update" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        id = Quiver.create_element!(db, "Collection"; label = "Item 1")

        # Insert initial data
        Quiver.update_time_series_group!(
            db,
            "Collection",
            "data",
            id,
            [
                Dict{String, Any}("date_time" => "2024-01-01T10:00:00", "value" => 1.0),
            ],
        )

        rows = Quiver.read_time_series_group_by_id(db, "Collection", "data", id)
        @test length(rows) == 1

        # Replace with new data
        Quiver.update_time_series_group!(
            db,
            "Collection",
            "data",
            id,
            [
                Dict{String, Any}("date_time" => "2024-02-01T10:00:00", "value" => 10.0),
                Dict{String, Any}("date_time" => "2024-02-01T11:00:00", "value" => 20.0),
            ],
        )

        rows = Quiver.read_time_series_group_by_id(db, "Collection", "data", id)
        @test length(rows) == 2
        @test rows[1]["date_time"] == "2024-02-01T10:00:00"
        @test rows[1]["value"] == 10.0

        Quiver.close!(db)
    end

    @testset "Update Clear" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        id = Quiver.create_element!(db, "Collection"; label = "Item 1")

        # Insert data
        Quiver.update_time_series_group!(
            db,
            "Collection",
            "data",
            id,
            [
                Dict{String, Any}("date_time" => "2024-01-01T10:00:00", "value" => 1.0),
            ],
        )

        # Clear by updating with empty
        Quiver.update_time_series_group!(db, "Collection", "data", id, Dict{String, Any}[])

        rows = Quiver.read_time_series_group_by_id(db, "Collection", "data", id)
        @test isempty(rows)

        Quiver.close!(db)
    end

    @testset "Ordering" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        id = Quiver.create_element!(db, "Collection"; label = "Item 1")

        # Insert out of order
        Quiver.update_time_series_group!(
            db,
            "Collection",
            "data",
            id,
            [
                Dict{String, Any}("date_time" => "2024-01-03T10:00:00", "value" => 3.0),
                Dict{String, Any}("date_time" => "2024-01-01T10:00:00", "value" => 1.0),
                Dict{String, Any}("date_time" => "2024-01-02T10:00:00", "value" => 2.0),
            ],
        )

        # Should be returned ordered by date_time
        rows = Quiver.read_time_series_group_by_id(db, "Collection", "data", id)
        @test length(rows) == 3
        @test rows[1]["date_time"] == "2024-01-01T10:00:00"
        @test rows[2]["date_time"] == "2024-01-02T10:00:00"
        @test rows[3]["date_time"] == "2024-01-03T10:00:00"

        Quiver.close!(db)
    end
end

end  # module
