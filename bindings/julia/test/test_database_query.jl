module TestDatabaseQuery

using Quiver
using Test
using Dates

include("fixture.jl")

@testset "Query" begin
    @testset "Query String" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration";
            label = "Test Label",
            string_attribute = "hello world",
        )

        result = Quiver.query_string(db, "SELECT string_attribute FROM Configuration WHERE label = 'Test Label'")
        @test result == "hello world"

        # Test empty result returns nothing
        result = Quiver.query_string(db, "SELECT string_attribute FROM Configuration WHERE 1 = 0")
        @test result === nothing

        Quiver.close!(db)
    end

    @testset "Query String First Row" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration";
            label = "First",
            string_attribute = "first value",
        )
        Quiver.create_element!(db, "Configuration";
            label = "Second",
            string_attribute = "second value",
        )

        result = Quiver.query_string(db, "SELECT string_attribute FROM Configuration ORDER BY label")
        @test result == "first value"

        Quiver.close!(db)
    end

    @testset "Query Integer" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration";
            label = "Test",
            integer_attribute = 42,
        )

        result = Quiver.query_integer(db, "SELECT integer_attribute FROM Configuration WHERE label = 'Test'")
        @test result == 42

        # Test empty result returns nothing
        result = Quiver.query_integer(db, "SELECT integer_attribute FROM Configuration WHERE 1 = 0")
        @test result === nothing

        Quiver.close!(db)
    end

    @testset "Query Integer Count" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "A")
        Quiver.create_element!(db, "Configuration"; label = "B")
        Quiver.create_element!(db, "Configuration"; label = "C")

        result = Quiver.query_integer(db, "SELECT COUNT(*) FROM Configuration")
        @test result == 3

        Quiver.close!(db)
    end

    @testset "Query Float" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration";
            label = "Test",
            float_attribute = 3.14159,
        )

        result = Quiver.query_float(db, "SELECT float_attribute FROM Configuration WHERE label = 'Test'")
        @test result ≈ 3.14159

        # Test empty result returns nothing
        result = Quiver.query_float(db, "SELECT float_attribute FROM Configuration WHERE 1 = 0")
        @test result === nothing

        Quiver.close!(db)
    end

    @testset "Query Float Average" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration";
            label = "A",
            float_attribute = 10.0,
        )
        Quiver.create_element!(db, "Configuration";
            label = "B",
            float_attribute = 20.0,
        )

        result = Quiver.query_float(db, "SELECT AVG(float_attribute) FROM Configuration")
        @test result ≈ 15.0

        Quiver.close!(db)
    end

    @testset "Query String with Params" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration";
            label = "Test Label",
            string_attribute = "hello world",
        )

        result =
            Quiver.query_string(db, "SELECT string_attribute FROM Configuration WHERE label = ?", Any["Test Label"])
        @test result == "hello world"

        result = Quiver.query_string(db, "SELECT string_attribute FROM Configuration WHERE label = ?", Any["NoMatch"])
        @test result === nothing

        Quiver.close!(db)
    end

    @testset "Query Integer with Params" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration";
            label = "Test",
            integer_attribute = 42,
        )

        result = Quiver.query_integer(db, "SELECT integer_attribute FROM Configuration WHERE label = ?", Any["Test"])
        @test result == 42

        Quiver.close!(db)
    end

    @testset "Query Float with Params" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration";
            label = "Test",
            float_attribute = 3.14159,
        )

        result = Quiver.query_float(db, "SELECT float_attribute FROM Configuration WHERE label = ?", Any["Test"])
        @test result ≈ 3.14159

        Quiver.close!(db)
    end

    @testset "Query with Integer Param" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration";
            label = "A",
            integer_attribute = 10,
        )
        Quiver.create_element!(db, "Configuration";
            label = "B",
            integer_attribute = 20,
        )

        result = Quiver.query_integer(
            db,
            "SELECT integer_attribute FROM Configuration WHERE integer_attribute > ? ORDER BY integer_attribute",
            Any[15],
        )
        @test result == 20

        Quiver.close!(db)
    end

    @testset "Query with Multiple Params" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration";
            label = "A",
            integer_attribute = 10,
        )
        Quiver.create_element!(db, "Configuration";
            label = "B",
            integer_attribute = 20,
        )

        result = Quiver.query_integer(
            db,
            "SELECT integer_attribute FROM Configuration WHERE label = ? AND integer_attribute > ?",
            Any["B", 5],
        )
        @test result == 20

        Quiver.close!(db)
    end

    @testset "Query DateTime with Params" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration";
            label = "Test",
            date_attribute = DateTime(2024, 6, 15, 10, 30, 0),
        )

        result = Quiver.query_date_time(db, "SELECT date_attribute FROM Configuration WHERE label = ?", Any["Test"])
        @test result == DateTime(2024, 6, 15, 10, 30, 0)

        Quiver.close!(db)
    end

    @testset "Query DateTime" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration";
            label = "Test",
            date_attribute = DateTime(2024, 6, 15, 10, 30, 0),
        )

        result = Quiver.query_date_time(db, "SELECT date_attribute FROM Configuration WHERE label = 'Test'")
        @test result == DateTime(2024, 6, 15, 10, 30, 0)

        # Test empty result returns nothing
        result = Quiver.query_date_time(db, "SELECT date_attribute FROM Configuration WHERE 1 = 0")
        @test result === nothing

        Quiver.close!(db)
    end
end

end
