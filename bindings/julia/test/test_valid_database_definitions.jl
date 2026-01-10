module TestValidDatabaseDefinitions

using PSRDatabase
using Test

include("fixture.jl")

@testset "Invalid Database Without Configuration Table" begin
    path_schema =
        joinpath(tests_path(), "test_valid_database_definitions", "test_invalid_database_without_configuration_table.sql")
    @test_throws PSRDatabase.DatabaseException PSRDatabase.create_empty_db_from_schema(
        ":memory:",
        path_schema;
        force = true,
    )
end

@testset "Invalid Database With Duplicated Attributes" begin
    path_schema = joinpath(tests_path(), "test_valid_database_definitions", "test_invalid_database_with_duplicated_attributes.sql")
    @test_throws PSRDatabase.DatabaseException PSRDatabase.create_empty_db_from_schema(
        ":memory:",
        path_schema;
        force = true,
    )

    path_schema =
        joinpath(tests_path(), "test_valid_database_definitions", "test_invalid_database_with_duplicated_attributes_2.sql")
    @test_throws PSRDatabase.DatabaseException PSRDatabase.create_empty_db_from_schema(
        ":memory:",
        path_schema;
        force = true,
    )
end

@testset "Invalid Database With Invalid Collection Id" begin
    path_schema =
        joinpath(tests_path(), "test_valid_database_definitions", "test_invalid_database_with_invalid_collection_id.sql")
    @test_throws PSRDatabase.DatabaseException PSRDatabase.create_empty_db_from_schema(
        ":memory:",
        path_schema;
        force = true,
    )
end

@testset "Invalid Vector Table" begin
    path_schema =
        joinpath(tests_path(), "test_valid_database_definitions", "test_invalid_vector_table.sql")
    @test_throws PSRDatabase.DatabaseException PSRDatabase.create_empty_db_from_schema(
        ":memory:",
        path_schema;
        force = true,
    )
end

@testset "Invalid Database Foreign Key Actions" begin
    path_schema =
        joinpath(tests_path(), "test_valid_database_definitions", "test_invalid_database_foreign_key_actions.sql")
    @test_throws PSRDatabase.DatabaseException PSRDatabase.create_empty_db_from_schema(
        ":memory:",
        path_schema;
        force = true,
    )
end

@testset "Invalid Database Vector Table Without Vector Index" begin
    path_schema =
        joinpath(tests_path(), "test_valid_database_definitions", "test_invalid_database_vector_table_without_vector_index.sql")
    @test_throws PSRDatabase.DatabaseException PSRDatabase.create_empty_db_from_schema(
        ":memory:",
        path_schema;
        force = true,
    )
end

@testset "Invalid Duplicated Collection Definition" begin
    path_schema = joinpath(tests_path(), "test_valid_database_definitions", "test_invalid_duplicated_collection_definition.sql")
    @test_throws PSRDatabase.DatabaseException PSRDatabase.create_empty_db_from_schema(
        ":memory:",
        path_schema;
        force = true,
    )
end

@testset "Valid Database" begin
    path_schema = joinpath(tests_path(), "test_valid_database_definitions", "test_valid_database.sql")
    db = PSRDatabase.create_empty_db_from_schema(":memory:", path_schema; force = true)
    PSRDatabase.close!(db)
    @test true
end

@testset "Invalid Foreign Key Has Not Null Constraint" begin
    path_schema = joinpath(tests_path(), "test_valid_database_definitions", "test_invalid_foreign_key_has_not_null_constraint.sql")
    @test_throws PSRDatabase.DatabaseException PSRDatabase.create_empty_db_from_schema(
        ":memory:",
        path_schema;
        force = true,
    )
end

@testset "Invalid Relation Attribute" begin
    path_schema = joinpath(tests_path(), "test_valid_database_definitions", "test_invalid_relation_attribute.sql")
    @test_throws PSRDatabase.DatabaseException PSRDatabase.create_empty_db_from_schema(
        ":memory:",
        path_schema;
        force = true,
    )
end

@testset "Invalid Database Without Label Constraints" begin
    path_schema = joinpath(tests_path(), "test_valid_database_definitions", "test_invalid_database_without_label_constraints.sql")
    @test_throws PSRDatabase.DatabaseException PSRDatabase.create_empty_db_from_schema(
        ":memory:",
        path_schema;
        force = true,
    )
end

@testset "Invalid Set Table Not All Unique" begin
    path_schema = joinpath(tests_path(), "test_valid_database_definitions", "test_invalid_set_table_not_all_unique.sql")
    @test_throws PSRDatabase.DatabaseException PSRDatabase.create_empty_db_from_schema(
        ":memory:",
        path_schema;
        force = true,
    )
end

@testset "Invalid Set Table Without Unique Constraint" begin
    path_schema = joinpath(tests_path(), "test_valid_database_definitions", "test_invalid_set_table_without_unique_constraint.sql")
    @test_throws PSRDatabase.DatabaseException PSRDatabase.create_empty_db_from_schema(
        ":memory:",
        path_schema;
        force = true,
    )
end

end
