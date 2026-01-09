module TestValidDatabaseDefinitions

using PSRDatabase
using Test

@testset "invalid_database_without_configuration_table" begin
    path_schema =
        joinpath(@__DIR__, "test_invalid_database_without_configuration_table.sql")
    @test_throws PSRDatabase.DatabaseException PSRDatabase.create_empty_db_from_schema(
        ":memory:",
        path_schema;
        force = true,
    )
end

@testset "invalid_database_with_duplicated_attributes" begin
    path_schema = joinpath(@__DIR__, "test_invalid_database_with_duplicated_attributes.sql")
    @test_throws PSRDatabase.DatabaseException PSRDatabase.create_empty_db_from_schema(
        ":memory:",
        path_schema;
        force = true,
    )

    path_schema =
        joinpath(@__DIR__, "test_invalid_database_with_duplicated_attributes_2.sql")
    @test_throws PSRDatabase.DatabaseException PSRDatabase.create_empty_db_from_schema(
        ":memory:",
        path_schema;
        force = true,
    )
end

@testset "invalid_database_with_invalid_collection_id" begin
    path_schema =
        joinpath(@__DIR__, "test_invalid_database_with_invalid_collection_id.sql")
    @test_throws PSRDatabase.DatabaseException PSRDatabase.create_empty_db_from_schema(
        ":memory:",
        path_schema;
        force = true,
    )
end

@testset "invalid_vector_table" begin
    path_schema =
        joinpath(@__DIR__, "test_invalid_vector_table.sql")
    @test_throws PSRDatabase.DatabaseException PSRDatabase.create_empty_db_from_schema(
        ":memory:",
        path_schema;
        force = true,
    )
end

@testset "invalid_database_foreign_key_actions" begin
    path_schema =
        joinpath(@__DIR__, "test_invalid_database_foreign_key_actions.sql")
    @test_throws PSRDatabase.DatabaseException PSRDatabase.create_empty_db_from_schema(
        ":memory:",
        path_schema;
        force = true,
    )
end

@testset "invalid_database_vector_table_without_vector_index" begin
    path_schema =
        joinpath(@__DIR__, "test_invalid_database_vector_table_without_vector_index.sql")
    @test_throws PSRDatabase.DatabaseException PSRDatabase.create_empty_db_from_schema(
        ":memory:",
        path_schema;
        force = true,
    )
end

@testset "invalid_duplicated_collection_definition" begin
    path_schema = joinpath(@__DIR__, "test_invalid_duplicated_collection_definition.sql")
    @test_throws SQLite.SQLiteException PSRDatabase.create_empty_db_from_schema(
        ":memory:",
        path_schema;
        force = true,
    )
end

@testset "valid_database" begin
    path_schema = joinpath(@__DIR__, "test_valid_database.sql")
    db = PSRDatabase.create_empty_db_from_schema(":memory:", path_schema; force = true)
    PSRDatabase.close!(db)
    @test true
end

@testset "invalid_foreign_key_has_not_null_constraint" begin
    path_schema = joinpath(@__DIR__, "test_invalid_foreign_key_has_not_null_constraint.sql")
    @test_throws PSRDatabase.DatabaseException PSRDatabase.create_empty_db_from_schema(
        ":memory:",
        path_schema;
        force = true,
    )
end

@testset "invalid_relation_attribute" begin
    path_schema = joinpath(@__DIR__, "test_invalid_relation_attribute.sql")
    @test_throws PSRDatabase.DatabaseException PSRDatabase.create_empty_db_from_schema(
        ":memory:",
        path_schema;
        force = true,
    )
end

@testset "invalid_database_without_label_constraints" begin
    path_schema = joinpath(@__DIR__, "test_invalid_database_without_label_constraints.sql")
    @test_throws PSRDatabase.DatabaseException PSRDatabase.create_empty_db_from_schema(
        ":memory:",
        path_schema;
        force = true,
    )
end

@testset "invalid_set_table_not_all_unique" begin
    path_schema = joinpath(@__DIR__, "test_invalid_set_table_not_all_unique.sql")
    @test_throws PSRDatabase.DatabaseException PSRDatabase.create_empty_db_from_schema(
        ":memory:",
        path_schema;
        force = true,
    )
end

@testset "invalid_set_table_without_unique_constraint" begin
    path_schema = joinpath(@__DIR__, "test_invalid_set_table_without_unique_constraint.sql")
    @test_throws PSRDatabase.DatabaseException PSRDatabase.create_empty_db_from_schema(
        ":memory:",
        path_schema;
        force = true,
    )
end

end
