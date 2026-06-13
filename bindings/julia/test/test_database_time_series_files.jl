module TestDatabaseTimeSeriesFiles

using Quiver
using Test
using Dates

include("fixture.jl")

@testset "Time Series Files" begin
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
            Dict{String, Quiver.Optional{String}}(
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
            Dict{String, Quiver.Optional{String}}(
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
            Dict{String, Quiver.Optional{String}}(
                "data_file" => "/old/data.csv",
                "metadata_file" => "/old/meta.json",
            ),
        )

        # Second update replaces
        Quiver.update_time_series_files!(
            db,
            "Collection",
            Dict{String, Quiver.Optional{String}}(
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
