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

    @testset "Time Series Files - has_time_series_files" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        @test Quiver.has_time_series_files(db, "Collection") == true
        @test Quiver.has_time_series_files(db, "Configuration") == false

        Quiver.close!(db)
    end

    @testset "Time Series Files - list_time_series_files_columns" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        columns = Quiver.list_time_series_files_columns(db, "Collection")
        @test length(columns) == 2
        @test "data_file" in columns
        @test "metadata_file" in columns

        Quiver.close!(db)
    end

    @testset "Time Series Files - read empty" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        paths = Quiver.read_time_series_files(db, "Collection")
        @test length(paths) == 2
        @test paths["data_file"] === nothing
        @test paths["metadata_file"] === nothing

        Quiver.close!(db)
    end

    @testset "Time Series Files - update and read" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.update_time_series_files!(
            db,
            "Collection",
            Dict{String, Union{String, Nothing}}(
                "data_file" => "/path/to/data.csv",
                "metadata_file" => "/path/to/meta.json",
            ),
        )

        paths = Quiver.read_time_series_files(db, "Collection")
        @test paths["data_file"] == "/path/to/data.csv"
        @test paths["metadata_file"] == "/path/to/meta.json"

        Quiver.close!(db)
    end

    @testset "Time Series Files - update with nulls" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.update_time_series_files!(
            db,
            "Collection",
            Dict{String, Union{String, Nothing}}(
                "data_file" => "/path/to/data.csv",
                "metadata_file" => nothing,
            ),
        )

        paths = Quiver.read_time_series_files(db, "Collection")
        @test paths["data_file"] == "/path/to/data.csv"
        @test paths["metadata_file"] === nothing

        Quiver.close!(db)
    end

    @testset "Time Series Files - replace" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        # First update
        Quiver.update_time_series_files!(
            db,
            "Collection",
            Dict{String, Union{String, Nothing}}(
                "data_file" => "/old/data.csv",
                "metadata_file" => "/old/meta.json",
            ),
        )

        # Second update replaces
        Quiver.update_time_series_files!(
            db,
            "Collection",
            Dict{String, Union{String, Nothing}}(
                "data_file" => "/new/data.csv",
                "metadata_file" => "/new/meta.json",
            ),
        )

        paths = Quiver.read_time_series_files(db, "Collection")
        @test paths["data_file"] == "/new/data.csv"
        @test paths["metadata_file"] == "/new/meta.json"

        Quiver.close!(db)
    end

    @testset "Time Series Files - not found" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        @test_throws Quiver.DatabaseException Quiver.read_time_series_files(db, "Configuration")
        @test_throws Quiver.DatabaseException Quiver.list_time_series_files_columns(db, "Configuration")

        Quiver.close!(db)
    end
end

end  # module
