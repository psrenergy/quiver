module TestDatabaseTransaction

using Quiver
using Test

include("fixture.jl")

@testset "Transaction" begin
    @testset "Begin and commit persists" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Config")
        Quiver.begin_transaction!(db)
        Quiver.create_element!(db, "Collection"; label = "Item 1", some_integer = 10)
        Quiver.commit!(db)

        labels = Quiver.read_scalar_strings(db, "Collection", "label")
        @test length(labels) == 1
        @test labels[1] == "Item 1"

        Quiver.close!(db)
    end

    @testset "Begin and rollback discards" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Config")
        Quiver.begin_transaction!(db)
        Quiver.create_element!(db, "Collection"; label = "Item 1", some_integer = 10)
        Quiver.rollback!(db)

        labels = Quiver.read_scalar_strings(db, "Collection", "label")
        @test length(labels) == 0

        Quiver.close!(db)
    end

    @testset "Double begin error" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Config")
        Quiver.begin_transaction!(db)
        @test_throws DatabaseException Quiver.begin_transaction!(db)

        # Clean up: rollback the first transaction
        Quiver.rollback!(db)
        Quiver.close!(db)
    end

    @testset "Commit without begin error" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Config")
        @test_throws DatabaseException Quiver.commit!(db)

        Quiver.close!(db)
    end

    @testset "Rollback without begin error" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Config")
        @test_throws DatabaseException Quiver.rollback!(db)

        Quiver.close!(db)
    end

    @testset "in_transaction state" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Config")

        @test Quiver.in_transaction(db) == false
        Quiver.begin_transaction!(db)
        @test Quiver.in_transaction(db) == true
        Quiver.commit!(db)
        @test Quiver.in_transaction(db) == false

        Quiver.close!(db)
    end

    @testset "Transaction block auto-commits" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Config")

        result = Quiver.transaction(db) do db
            Quiver.create_element!(db, "Collection"; label = "Item 1", some_integer = 42)
            42
        end

        @test result == 42
        labels = Quiver.read_scalar_strings(db, "Collection", "label")
        @test length(labels) == 1
        @test labels[1] == "Item 1"

        Quiver.close!(db)
    end

    @testset "Transaction block rollback on exception" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Config")

        @test_throws ErrorException begin
            Quiver.transaction(db) do db
                Quiver.create_element!(db, "Collection"; label = "Item 1", some_integer = 10)
                error("intentional error")
            end
        end

        labels = Quiver.read_scalar_strings(db, "Collection", "label")
        @test length(labels) == 0

        Quiver.close!(db)
    end

    @testset "Multi-operation batch" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Config")

        Quiver.transaction(db) do db
            Quiver.create_element!(db, "Collection"; label = "Item 1", some_integer = 10)
            Quiver.create_element!(db, "Collection"; label = "Item 2", some_integer = 20)
            Quiver.update_scalar_integer!(db, "Collection", "some_integer", Int64(1), Int64(100))
        end

        labels = Quiver.read_scalar_strings(db, "Collection", "label")
        @test length(labels) == 2
        @test "Item 1" in labels
        @test "Item 2" in labels

        integers = Quiver.read_scalar_integers(db, "Collection", "some_integer")
        @test 100 in integers
        @test 20 in integers

        Quiver.close!(db)
    end
end

end
