module TestDatabaseTimeSeries

using Quiver
using Test
using Dates

include("fixture.jl")

@testset "Time Series" begin
    @testset "Metadata" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        # get_time_series_metadata
        metadata = Quiver.get_time_series_metadata(db, "Collection", "data")
        @test metadata.group_name == "data"
        @test length(metadata.value_columns) == 1
        @test metadata.value_columns[1].name == "value"

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

        # Insert time series data using kwargs
        Quiver.update_time_series_group!(db, "Collection", "data", id;
            date_time = ["2024-01-01T10:00:00", "2024-01-01T11:00:00", "2024-01-01T12:00:00"],
            value = [1.5, 2.5, 3.5],
        )

        # Read back as Dict{String, Vector}
        result = Quiver.read_time_series_group(db, "Collection", "data", id)
        @test length(result) == 2
        @test length(result["date_time"]) == 3
        @test length(result["value"]) == 3

        # Dimension column values are parsed to DateTime
        @test result["date_time"][1] == DateTime(2024, 1, 1, 10, 0, 0)
        @test result["date_time"][2] == DateTime(2024, 1, 1, 11, 0, 0)
        @test result["date_time"][3] == DateTime(2024, 1, 1, 12, 0, 0)
        @test result["date_time"] isa Vector{DateTime}

        # Value column values are Float64
        @test result["value"][1] == 1.5
        @test result["value"][2] == 2.5
        @test result["value"][3] == 3.5
        @test result["value"] isa Vector{Float64}

        Quiver.close!(db)
    end

    @testset "Read Empty" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        id = Quiver.create_element!(db, "Collection"; label = "Item 1")

        # No time series data inserted
        result = Quiver.read_time_series_group(db, "Collection", "data", id)
        @test isempty(result)
        @test result isa Dict{String, Vector}

        Quiver.close!(db)
    end

    @testset "Update" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        id = Quiver.create_element!(db, "Collection"; label = "Item 1")

        # Insert initial data using kwargs
        Quiver.update_time_series_group!(db, "Collection", "data", id;
            date_time = ["2024-01-01T10:00:00"],
            value = [1.0],
        )

        result = Quiver.read_time_series_group(db, "Collection", "data", id)
        @test length(result["date_time"]) == 1

        # Replace with new data using kwargs
        Quiver.update_time_series_group!(db, "Collection", "data", id;
            date_time = ["2024-02-01T10:00:00", "2024-02-01T11:00:00"],
            value = [10.0, 20.0],
        )

        result = Quiver.read_time_series_group(db, "Collection", "data", id)
        @test length(result["date_time"]) == 2
        @test result["date_time"][1] == DateTime(2024, 2, 1, 10, 0, 0)
        @test result["value"][1] == 10.0

        Quiver.close!(db)
    end

    @testset "Update Clear" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        id = Quiver.create_element!(db, "Collection"; label = "Item 1")

        # Insert data using kwargs
        Quiver.update_time_series_group!(db, "Collection", "data", id;
            date_time = ["2024-01-01T10:00:00"],
            value = [1.0],
        )

        # Clear by calling with no kwargs
        Quiver.update_time_series_group!(db, "Collection", "data", id)

        result = Quiver.read_time_series_group(db, "Collection", "data", id)
        @test isempty(result)

        Quiver.close!(db)
    end

    @testset "Ordering" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        id = Quiver.create_element!(db, "Collection"; label = "Item 1")

        # Insert out of order using kwargs
        Quiver.update_time_series_group!(db, "Collection", "data", id;
            date_time = ["2024-01-03T10:00:00", "2024-01-01T10:00:00", "2024-01-02T10:00:00"],
            value = [3.0, 1.0, 2.0],
        )

        # Should be returned ordered by date_time (dimension column)
        result = Quiver.read_time_series_group(db, "Collection", "data", id)
        @test length(result["date_time"]) == 3
        @test result["date_time"][1] == DateTime(2024, 1, 1, 10, 0, 0)
        @test result["date_time"][2] == DateTime(2024, 1, 2, 10, 0, 0)
        @test result["date_time"][3] == DateTime(2024, 1, 3, 10, 0, 0)

        Quiver.close!(db)
    end

    # Multi-column mixed-type tests using mixed_time_series.sql schema
    # Schema: Sensor_time_series_readings with date_time TEXT, temperature REAL, humidity INTEGER, status TEXT

    @testset "Multi-Column Update and Read" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "mixed_time_series.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        id = Quiver.create_element!(db, "Sensor"; label = "Sensor 1")

        Quiver.update_time_series_group!(db, "Sensor", "readings", id;
            date_time = [DateTime(2024, 1, 1, 10, 0, 0), DateTime(2024, 1, 1, 11, 0, 0)],
            temperature = [20.5, 21.3],
            humidity = [45, 50],
            status = ["normal", "high"],
        )

        result = Quiver.read_time_series_group(db, "Sensor", "readings", id)
        @test length(result) == 4  # 4 columns

        # Dimension column: DateTime
        @test result["date_time"] isa Vector{DateTime}
        @test result["date_time"][1] == DateTime(2024, 1, 1, 10, 0, 0)
        @test result["date_time"][2] == DateTime(2024, 1, 1, 11, 0, 0)

        # REAL column: Float64
        @test result["temperature"] isa Vector{Float64}
        @test result["temperature"] == [20.5, 21.3]

        # INTEGER column: Int64
        @test result["humidity"] isa Vector{Int64}
        @test result["humidity"] == [45, 50]

        # TEXT column: String
        @test result["status"] isa Vector{String}
        @test result["status"] == ["normal", "high"]

        Quiver.close!(db)
    end

    @testset "Multi-Column Auto-Coercion Int to Float" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "mixed_time_series.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        id = Quiver.create_element!(db, "Sensor"; label = "Sensor 1")

        # Pass Int for temperature (REAL column) -- should auto-coerce to Float64
        Quiver.update_time_series_group!(db, "Sensor", "readings", id;
            date_time = ["2024-01-01T10:00:00"],
            temperature = [20],
            humidity = [45],
            status = ["normal"],
        )

        result = Quiver.read_time_series_group(db, "Sensor", "readings", id)
        @test result["temperature"] == [20.0]
        @test result["temperature"] isa Vector{Float64}

        Quiver.close!(db)
    end

    @testset "Multi-Column DateTime on Dimension" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "mixed_time_series.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        id = Quiver.create_element!(db, "Sensor"; label = "Sensor 1")

        # Pass DateTime objects for dimension column
        dt = DateTime(2024, 6, 15, 14, 30, 0)
        Quiver.update_time_series_group!(db, "Sensor", "readings", id;
            date_time = [dt],
            temperature = [25.0],
            humidity = [60],
            status = ["ok"],
        )

        result = Quiver.read_time_series_group(db, "Sensor", "readings", id)
        @test result["date_time"][1] == dt
        @test result["date_time"] isa Vector{DateTime}

        Quiver.close!(db)
    end

    @testset "Multi-Column String Dimension" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "mixed_time_series.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        id = Quiver.create_element!(db, "Sensor"; label = "Sensor 1")

        # Pass string values for dimension column (not DateTime)
        Quiver.update_time_series_group!(db, "Sensor", "readings", id;
            date_time = ["2024-03-20T08:15:00"],
            temperature = [18.0],
            humidity = [70],
            status = ["damp"],
        )

        # Should come back parsed to DateTime on read
        result = Quiver.read_time_series_group(db, "Sensor", "readings", id)
        @test result["date_time"][1] == DateTime(2024, 3, 20, 8, 15, 0)
        @test result["date_time"] isa Vector{DateTime}

        Quiver.close!(db)
    end

    @testset "Multi-Column Clear" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "mixed_time_series.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        id = Quiver.create_element!(db, "Sensor"; label = "Sensor 1")

        # Insert data
        Quiver.update_time_series_group!(db, "Sensor", "readings", id;
            date_time = ["2024-01-01T10:00:00"],
            temperature = [20.0],
            humidity = [45],
            status = ["normal"],
        )

        # Clear with no kwargs
        Quiver.update_time_series_group!(db, "Sensor", "readings", id)

        result = Quiver.read_time_series_group(db, "Sensor", "readings", id)
        @test isempty(result)

        Quiver.close!(db)
    end

    @testset "Multi-Column Replace" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "mixed_time_series.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        id = Quiver.create_element!(db, "Sensor"; label = "Sensor 1")

        # First update
        Quiver.update_time_series_group!(db, "Sensor", "readings", id;
            date_time = ["2024-01-01T10:00:00"],
            temperature = [20.0],
            humidity = [45],
            status = ["normal"],
        )

        # Second update fully replaces first
        Quiver.update_time_series_group!(db, "Sensor", "readings", id;
            date_time = ["2024-06-01T12:00:00", "2024-06-01T13:00:00"],
            temperature = [30.0, 31.5],
            humidity = [35, 40],
            status = ["hot", "very_hot"],
        )

        result = Quiver.read_time_series_group(db, "Sensor", "readings", id)
        @test length(result["date_time"]) == 2
        @test result["temperature"] == [30.0, 31.5]
        @test result["humidity"] == [35, 40]
        @test result["status"] == ["hot", "very_hot"]

        Quiver.close!(db)
    end

    @testset "Multi-Column Read Empty" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "mixed_time_series.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        id = Quiver.create_element!(db, "Sensor"; label = "Sensor 1")

        # Read without any update
        result = Quiver.read_time_series_group(db, "Sensor", "readings", id)
        @test isempty(result)
        @test result isa Dict{String, Vector}

        Quiver.close!(db)
    end

    @testset "Multi-Column Row Count Mismatch" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "mixed_time_series.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        id = Quiver.create_element!(db, "Sensor"; label = "Sensor 1")

        # Vectors of different lengths should throw ArgumentError
        @test_throws ArgumentError Quiver.update_time_series_group!(db, "Sensor", "readings", id;
            date_time = ["2024-01-01T10:00:00", "2024-01-01T11:00:00"],
            temperature = [20.5],  # length 1 vs length 2
            humidity = [45, 50],
            status = ["normal", "high"],
        )

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
