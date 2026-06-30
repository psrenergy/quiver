module TestDatabaseTimeSeriesNulls

using Quiver
using Test
using Dates

include("fixture.jl")

@testset "Time Series NULLs" begin
    @testset "NULL cells round-trip" begin
        # nullable_time_series.sql: Sensor.readings with date_time TEXT NOT NULL,
        # temperature REAL, counter INTEGER, status TEXT (nullable value columns).
        path_schema = joinpath(tests_path(), "schemas", "valid", "nullable_time_series.sql")
        db = Quiver.from_schema(":memory:", path_schema)
        Quiver.create_element!(db, "Configuration"; label = "Config")
        id = Quiver.create_element!(db, "Sensor"; label = "Sensor 1")

        Quiver.update_time_series_group!(
            db, "Sensor", "readings", id;
            date_time = [DateTime(2024, 1, 1), DateTime(2024, 1, 2)],
            temperature = [10.5, nothing],
            counter = [nothing, 7],
            status = ["ok", nothing],
        )

        result = Quiver.read_time_series_group(db, "Sensor", "readings", id)
        @test result["temperature"] == [10.5, nothing]
        @test result["counter"] == [nothing, 7]
        @test result["status"] == ["ok", nothing]

        Quiver.close!(db)
    end

    @testset "NULL all-nothing column" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "nullable_time_series.sql")
        db = Quiver.from_schema(":memory:", path_schema)
        Quiver.create_element!(db, "Configuration"; label = "Config")
        id = Quiver.create_element!(db, "Sensor"; label = "Sensor 1")

        Quiver.update_time_series_group!(
            db, "Sensor", "readings", id;
            date_time = [DateTime(2024, 1, 1), DateTime(2024, 1, 2)],
            status = [nothing, nothing],
        )

        result = Quiver.read_time_series_group(db, "Sensor", "readings", id)
        @test result["status"] == [nothing, nothing]

        Quiver.close!(db)
    end

    @testset "NULL read-modify-write" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "nullable_time_series.sql")
        db = Quiver.from_schema(":memory:", path_schema)
        Quiver.create_element!(db, "Configuration"; label = "Config")
        id = Quiver.create_element!(db, "Sensor"; label = "Sensor 1")

        Quiver.update_time_series_group!(
            db, "Sensor", "readings", id;
            date_time = [DateTime(2024, 1, 1), DateTime(2024, 1, 2)],
            temperature = [10.5, nothing],
        )

        ts = Quiver.read_time_series_group(db, "Sensor", "readings", id)
        ts["temperature"][1] = 99.0
        Quiver.update_time_series_group!(
            db, "Sensor", "readings", id;
            date_time = ts["date_time"], temperature = ts["temperature"],
        )

        result = Quiver.read_time_series_group(db, "Sensor", "readings", id)
        @test result["temperature"] == [99.0, nothing]

        Quiver.close!(db)
    end
end

end
