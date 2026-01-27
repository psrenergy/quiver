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

@testset "Current Version" begin
    @testset "Schema returns 0" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)
        @test Quiver.current_version(db) == 0
        Quiver.close!(db)
    end

    @testset "Migrations returns count" begin
        path_migrations = joinpath(tests_path(), "schemas", "migrations")
        db = Quiver.from_migrations(":memory:", path_migrations)
        @test Quiver.current_version(db) == 3
        Quiver.close!(db)
    end
end

end
