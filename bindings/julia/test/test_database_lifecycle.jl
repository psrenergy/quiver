module TestDatabaseLifecycle

using Quiver
using Test

include("fixture.jl")

@testset "Valid Schema" begin
    @testset "Basic" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)
        Quiver.close!(db)
        @test true
    end

    @testset "Collections" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)
        Quiver.close!(db)
        @test true
    end

    @testset "Relations" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
        db = Quiver.from_schema(":memory:", path_schema)
        Quiver.close!(db)
        @test true
    end
end

@testset "Open Existing" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
    mktempdir() do dir
        db_path = joinpath(dir, "test.db")
        db = Quiver.from_schema(db_path, path_schema)
        Quiver.close!(db)

        reopened = Quiver.open(db_path)
        @test Quiver.is_healthy(reopened) == true
        Quiver.close!(reopened)
        return nothing
    end
end

@testset "Describe" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
    db = Quiver.from_schema(":memory:", path_schema)

    # Just verify describe runs without error
    Quiver.describe(db)
    @test true

    Quiver.close!(db)
end

@testset "Current Version" begin
    @testset "Schema returns 0" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)
        @test Quiver.current_version(db) == 0
        Quiver.close!(db)
    end

    @testset "Migrations returns count" begin
        # schemas/ holds the shared 3-migration fixture under migrations/ (no ui/ -> empty UI).
        db = Quiver.from_hub(":memory:", joinpath(tests_path(), "schemas"))
        @test Quiver.current_version(db) == 3
        Quiver.close!(db)
    end
end

@testset "from_hub" begin
    dir = joinpath(tests_path(), "schemas", "from_hub")
    mktempdir() do tmp
        db_path = joinpath(tmp, "test.db")
        db = Quiver.from_hub(db_path, dir)
        @test Quiver.current_version(db) == 1
        @test Quiver.is_healthy(db) == true
        @test Quiver.get_model_name(db) == "demo_model"
        @test Quiver.get_attribute_unit(db, "Material", "demand") == "unit/year"
        @test Quiver.get_attribute_unit(db, "Material", "label") == ""
        @test Quiver.get_attribute_unit(db, "Nonexistent", "x") == ""
        Quiver.close!(db)
        return nothing
    end
end

@testset "UI metadata empty without from_hub" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
    db = Quiver.from_schema(":memory:", path_schema)
    @test Quiver.get_model_name(db) == ""
    @test Quiver.get_attribute_unit(db, "Material", "demand") == ""
    Quiver.close!(db)
end

@testset "is_healthy" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
    db = Quiver.from_schema(":memory:", path_schema)
    @test Quiver.is_healthy(db) == true
    Quiver.close!(db)
end

@testset "path" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
    mktempdir() do dir
        db_path = joinpath(dir, "test.db")
        db = Quiver.from_schema(db_path, path_schema)
        result = Quiver.path(db)
        @test result isa String
        @test occursin("test.db", result)
        Quiver.close!(db)
        return nothing
    end
end

end
