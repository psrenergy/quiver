module TestValidDatabaseDefinitions

using Margaux
using Test

include("fixture.jl")

@testset "Valid Schema" begin
    @testset "Basic" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Margaux.from_schema(":memory:", path_schema)
        Margaux.close!(db)
        @test true
    end

    @testset "Collections" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Margaux.from_schema(":memory:", path_schema)
        Margaux.close!(db)
        @test true
    end

    @testset "Relations" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
        db = Margaux.from_schema(":memory:", path_schema)
        Margaux.close!(db)
        @test true
    end
end

end
