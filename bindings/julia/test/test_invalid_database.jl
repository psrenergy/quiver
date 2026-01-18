module TestValidDatabaseDefinitions

using PSRDatabase
using Test

include("fixture.jl")

@testset "Invalid Schema" begin
    @testset "No Configuration" begin
        path_schema = joinpath(tests_path(), "schemas", "invalid", "no_configuration.sql")
        @test_throws PSRDatabase.DatabaseException PSRDatabase.from_schema(":memory:", path_schema)
    end

    @testset "Label Not Null" begin
        path_schema = joinpath(tests_path(), "schemas", "invalid", "label_not_null.sql")
        @test_throws PSRDatabase.DatabaseException PSRDatabase.from_schema(":memory:", path_schema)
    end

    @testset "Label Not Unique" begin
        path_schema = joinpath(tests_path(), "schemas", "invalid", "label_not_unique.sql")
        @test_throws PSRDatabase.DatabaseException PSRDatabase.from_schema(":memory:", path_schema)
    end

    @testset "Label Wrong Type" begin
        path_schema = joinpath(tests_path(), "schemas", "invalid", "label_wrong_type.sql")
        @test_throws PSRDatabase.DatabaseException PSRDatabase.from_schema(":memory:", path_schema)
    end

    @testset "Duplicate Attribute" begin
        path_schema = joinpath(tests_path(), "schemas", "invalid", "duplicate_attribute.sql")
        @test_throws PSRDatabase.DatabaseException PSRDatabase.from_schema(":memory:", path_schema)
    end

    @testset "Vector No Index" begin
        path_schema = joinpath(tests_path(), "schemas", "invalid", "vector_no_index.sql")
        @test_throws PSRDatabase.DatabaseException PSRDatabase.from_schema(":memory:", path_schema)
    end

    @testset "Set No Unique" begin
        path_schema = joinpath(tests_path(), "schemas", "invalid", "set_no_unique.sql")
        @test_throws PSRDatabase.DatabaseException PSRDatabase.from_schema(":memory:", path_schema)
    end

    @testset "FK Not Null Set Null" begin
        path_schema = joinpath(tests_path(), "schemas", "invalid", "fk_not_null_set_null.sql")
        @test_throws PSRDatabase.DatabaseException PSRDatabase.from_schema(":memory:", path_schema)
    end

    @testset "FK Actions" begin
        path_schema = joinpath(tests_path(), "schemas", "invalid", "fk_actions.sql")
        @test_throws PSRDatabase.DatabaseException PSRDatabase.from_schema(":memory:", path_schema)
    end
end

end
