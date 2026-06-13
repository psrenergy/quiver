module TestDatabaseTimeSeriesRow

using Quiver
using Test
using Dates

include("fixture.jl")

@testset "Time Series Row" begin
    @testset "Add Row" begin
        # Test 1 — insert: create one element, add a row, read back and assert presence
        @testset "Insert" begin
            path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
            db = Quiver.from_schema(":memory:", path_schema)

            Quiver.create_element!(db, "Configuration"; label = "Test Config")
            id = Quiver.create_element!(db, "Collection"; label = "Item 1")

            Quiver.add_time_series_row!(db, "Collection", "data", id;
                date_time = DateTime(2024, 1, 1),
                value = 10.0,
            )

            result = Quiver.read_time_series_group(db, "Collection", "data", id)
            @test length(result["date_time"]) == 1
            @test result["date_time"][1] == DateTime(2024, 1, 1)
            @test result["value"][1] == 10.0

            Quiver.close!(db)
        end

        # Test 2 — upsert: same key overwrites value
        @testset "Upsert" begin
            path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
            db = Quiver.from_schema(":memory:", path_schema)

            Quiver.create_element!(db, "Configuration"; label = "Test Config")
            id = Quiver.create_element!(db, "Collection"; label = "Item 1")

            Quiver.add_time_series_row!(db, "Collection", "data", id;
                date_time = DateTime(2024, 1, 1),
                value = 10.0,
            )

            # Same date_time PK → should overwrite value
            Quiver.add_time_series_row!(db, "Collection", "data", id;
                date_time = DateTime(2024, 1, 1),
                value = 99.0,
            )

            result = Quiver.read_time_series_group(db, "Collection", "data", id)
            @test length(result["date_time"]) == 1  # still one row, not two
            @test result["value"][1] == 99.0

            Quiver.close!(db)
        end

        # Test 3 — multi-dim happy path: date_time + block composite PK
        @testset "Multi-Dim" begin
            path_schema = joinpath(tests_path(), "schemas", "valid", "multi_dim_time_series.sql")
            db = Quiver.from_schema(":memory:", path_schema)

            Quiver.create_element!(db, "Configuration"; label = "Test Config")
            id = Quiver.create_element!(db, "Resource"; label = "Resource 1")

            # Insert row with both dimension columns (date_time + block) and
            # all value columns (load REAL, flag INTEGER) — mirrors the Dart
            # suite which also passes `flag` so both bindings exercise the
            # multi-value-column path.
            Quiver.add_time_series_row!(db, "Resource", "load", id;
                date_time = DateTime(2024, 1, 1),
                block = 1,
                load = 100.0,
                flag = 0,
            )

            result = Quiver.read_time_series_group(db, "Resource", "load", id)
            @test length(result["date_time"]) == 1
            @test result["date_time"][1] == DateTime(2024, 1, 1)
            @test result["load"][1] == 100.0
            @test result["flag"][1] == 0

            Quiver.close!(db)
        end

        # Test 4 — missing-dim error: omitting required date_time kwarg
        @testset "Missing Dim Error" begin
            path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
            db = Quiver.from_schema(":memory:", path_schema)

            Quiver.create_element!(db, "Configuration"; label = "Test Config")
            id = Quiver.create_element!(db, "Collection"; label = "Item 1")

            # Omit date_time — C++ layer should surface "Cannot add_time_series_row: ..."
            exc = @test_throws Quiver.DatabaseException Quiver.add_time_series_row!(
                db, "Collection", "data", id;
                value = 10.0,
            )
            @test occursin("Cannot add_time_series_row", exc.value.msg)

            Quiver.close!(db)
        end
    end
    @testset "Read Time Series Row" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        id1 = Quiver.create_element!(db, "Collection"; label = "Item 1")
        id2 = Quiver.create_element!(db, "Collection"; label = "Item 2")

        Quiver.update_time_series_group!(db, "Collection", "data", id1;
            date_time = ["2024-01-01T00:00:00", "2024-01-02T00:00:00", "2024-01-03T00:00:00"],
            value = [1.0, 2.0, 3.0],
        )
        Quiver.update_time_series_group!(db, "Collection", "data", id2;
            date_time = ["2024-01-01T00:00:00", "2024-01-02T00:00:00"],
            value = [10.0, 20.0],
        )

        # Query at 2024-01-02: Item 1 -> 2.0, Item 2 -> 20.0
        result = Quiver.read_time_series_row(db, "Collection", "data", "value"; date_time = DateTime(2024, 1, 2))
        @test result isa Vector{Float64}
        @test length(result) == 2
        @test result[1] == 2.0
        @test result[2] == 20.0

        # Query at 2024-01-03: Item 1 -> 3.0, Item 2 -> 20.0 (last at or before)
        result2 = Quiver.read_time_series_row(db, "Collection", "data", "value"; date_time = DateTime(2024, 1, 3))
        @test result2[1] == 3.0
        @test result2[2] == 20.0

        Quiver.close!(db)
    end

    @testset "Read Time Series Row With Missing Elements" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        id1 = Quiver.create_element!(db, "Collection"; label = "Item 1")
        id2 = Quiver.create_element!(db, "Collection"; label = "Item 2")

        Quiver.update_time_series_group!(db, "Collection", "data", id1;
            date_time = ["2024-01-01T00:00:00", "2024-01-02T00:00:00", "2024-01-03T00:00:00"],
            value = [1.0, 2.0, 3.0],
        )
        Quiver.update_time_series_group!(db, "Collection", "data", id2;
            date_time = ["2024-01-01T00:00:00", "2024-01-04T00:00:00"],
            value = [10.0, 20.0],
        )

        # Query at 2024-01-02: Item 1 -> 2.0, Item 2 -> 10.0
        result = Quiver.read_time_series_row(db, "Collection", "data", "value"; date_time = DateTime(2024, 1, 2))
        @test result isa Vector{Float64}
        @test length(result) == 2
        @test result[1] == 2.0
        @test result[2] == 10.0

        # Query at 2024-01-03: Item 1 -> 3.0, Item 2 -> 10.0 (last at or before)
        result2 = Quiver.read_time_series_row(db, "Collection", "data", "value"; date_time = DateTime(2024, 1, 3))
        @test result2[1] == 3.0
        @test result2[2] == 10.0

        Quiver.close!(db)
    end

    @testset "Read Time Series Row - Before All Data" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        id = Quiver.create_element!(db, "Collection"; label = "Item 1")

        Quiver.update_time_series_group!(db, "Collection", "data", id;
            date_time = ["2024-01-02T00:00:00"],
            value = [1.0],
        )

        # Query before any data: should return NaN for the float attribute
        result = Quiver.read_time_series_row(db, "Collection", "data", "value"; date_time = DateTime(2024, 1, 1))
        @test length(result) == 1
        @test isnan(result[1])

        Quiver.close!(db)
    end

    @testset "Read Time Series Row - Empty Collection" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        result = Quiver.read_time_series_row(db, "Collection", "data", "value"; date_time = DateTime(2024, 1, 1))
        @test isempty(result)
        @test result isa Vector{Float64}

        Quiver.close!(db)
    end

    @testset "Read Time Series Row - Mixed Elements" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        id1 = Quiver.create_element!(db, "Collection"; label = "Item 1")
        Quiver.create_element!(db, "Collection"; label = "Item 2")  # no time series data

        Quiver.update_time_series_group!(db, "Collection", "data", id1;
            date_time = ["2024-01-01T00:00:00"],
            value = [5.0],
        )

        # Item 1 has data, Item 2 doesn't (NaN sentinel)
        result = Quiver.read_time_series_row(db, "Collection", "data", "value"; date_time = DateTime(2024, 1, 1))
        @test length(result) == 2
        @test result[1] == 5.0
        @test isnan(result[2])

        Quiver.close!(db)
    end

    @testset "Read Time Series Row - Multi-Column Integer" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "mixed_time_series.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        id = Quiver.create_element!(db, "Sensor"; label = "Sensor 1")

        Quiver.update_time_series_group!(db, "Sensor", "readings", id;
            date_time = ["2024-01-01T00:00:00", "2024-01-02T00:00:00"],
            temperature = [20.5, 21.0],
            humidity = [65, 70],
            status = ["ok", "warn"],
        )

        # Read humidity (INTEGER type)
        humids = Quiver.read_time_series_row(db, "Sensor", "readings", "humidity"; date_time = DateTime(2024, 1, 2))
        @test humids isa Vector{Int64}
        @test humids[1] == 70

        # Read status (STRING type)
        stats = Quiver.read_time_series_row(db, "Sensor", "readings", "status"; date_time = DateTime(2024, 1, 1))
        @test stats isa Vector{Quiver.Optional{String}}
        @test stats[1] == "ok"

        # Read temperature (FLOAT type)
        temps = Quiver.read_time_series_row(db, "Sensor", "readings", "temperature"; date_time = DateTime(2024, 1, 2))
        @test temps isa Vector{Float64}
        @test temps[1] == 21.0

        Quiver.close!(db)
    end

    @testset "Read Time Series Row - Attribute Not Found" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        @test_throws Quiver.DatabaseException Quiver.read_time_series_row(
            db, "Collection", "data", "nonexistent"; date_time = DateTime(2024, 1, 1),
        )

        Quiver.close!(db)
    end

    @testset "Read Time Series Row - Group Not Found" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        @test_throws Quiver.DatabaseException Quiver.read_time_series_row(
            db, "Collection", "nonexistent", "value"; date_time = DateTime(2024, 1, 1),
        )

        Quiver.close!(db)
    end

    @testset "Read Time Series Row - String Null Distinguished From Empty" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "mixed_time_series.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        id1 = Quiver.create_element!(db, "Sensor"; label = "Sensor 1")
        Quiver.create_element!(db, "Sensor"; label = "Sensor 2")  # no data

        Quiver.update_time_series_group!(db, "Sensor", "readings", id1;
            date_time = ["2024-01-01T00:00:00"],
            temperature = [20.0],
            humidity = [50],
            status = [""],  # empty string, not null
        )

        # Sensor 1 has "" (real value), Sensor 2 has no row (nothing)
        result = Quiver.read_time_series_row(db, "Sensor", "readings", "status"; date_time = DateTime(2024, 1, 1))
        @test result isa Vector{Quiver.Optional{String}}
        @test length(result) == 2
        @test result[1] == ""
        @test result[2] === nothing

        Quiver.close!(db)
    end
end

end  # module
