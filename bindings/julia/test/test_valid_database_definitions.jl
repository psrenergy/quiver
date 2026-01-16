module TestValidDatabaseDefinitions

using PSRDatabase
using Test

include("fixture.jl")

@testset "Valid Schema - Basic" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
    db = PSRDatabase.from_schema(":memory:", path_schema; force = true)
    PSRDatabase.close!(db)
    @test true
end

@testset "Valid Schema - Collections" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
    db = PSRDatabase.from_schema(":memory:", path_schema; force = true)
    PSRDatabase.close!(db)
    @test true
end

@testset "Valid Schema - Relations" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
    db = PSRDatabase.from_schema(":memory:", path_schema; force = true)
    PSRDatabase.close!(db)
    @test true
end

@testset "Invalid - No Configuration" begin
    path_schema = joinpath(tests_path(), "schemas", "invalid", "no_configuration.sql")
    @test_throws PSRDatabase.DatabaseException PSRDatabase.from_schema(
        ":memory:",
        path_schema;
        force = true,
    )
end

@testset "Invalid - Label Not Null" begin
    path_schema = joinpath(tests_path(), "schemas", "invalid", "label_not_null.sql")
    @test_throws PSRDatabase.DatabaseException PSRDatabase.from_schema(
        ":memory:",
        path_schema;
        force = true,
    )
end

@testset "Invalid - Label Not Unique" begin
    path_schema = joinpath(tests_path(), "schemas", "invalid", "label_not_unique.sql")
    @test_throws PSRDatabase.DatabaseException PSRDatabase.from_schema(
        ":memory:",
        path_schema;
        force = true,
    )
end

@testset "Invalid - Label Wrong Type" begin
    path_schema = joinpath(tests_path(), "schemas", "invalid", "label_wrong_type.sql")
    @test_throws PSRDatabase.DatabaseException PSRDatabase.from_schema(
        ":memory:",
        path_schema;
        force = true,
    )
end

@testset "Invalid - Duplicate Attribute" begin
    path_schema = joinpath(tests_path(), "schemas", "invalid", "duplicate_attribute.sql")
    @test_throws PSRDatabase.DatabaseException PSRDatabase.from_schema(
        ":memory:",
        path_schema;
        force = true,
    )
end

@testset "Invalid - Vector No Index" begin
    path_schema = joinpath(tests_path(), "schemas", "invalid", "vector_no_index.sql")
    @test_throws PSRDatabase.DatabaseException PSRDatabase.from_schema(
        ":memory:",
        path_schema;
        force = true,
    )
end

@testset "Invalid - Set No Unique" begin
    path_schema = joinpath(tests_path(), "schemas", "invalid", "set_no_unique.sql")
    @test_throws PSRDatabase.DatabaseException PSRDatabase.from_schema(
        ":memory:",
        path_schema;
        force = true,
    )
end

@testset "Invalid - FK Not Null Set Null" begin
    path_schema = joinpath(tests_path(), "schemas", "invalid", "fk_not_null_set_null.sql")
    @test_throws PSRDatabase.DatabaseException PSRDatabase.from_schema(
        ":memory:",
        path_schema;
        force = true,
    )
end

@testset "Invalid - FK Actions" begin
    path_schema = joinpath(tests_path(), "schemas", "invalid", "fk_actions.sql")
    @test_throws PSRDatabase.DatabaseException PSRDatabase.from_schema(
        ":memory:",
        path_schema;
        force = true,
    )
end

end
