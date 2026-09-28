module TestSchemaValidator

using Quiver
using Test

include("fixture.jl")

@testset "Invalid Schema" begin
    @testset "No Configuration" begin
        path_schema = joinpath(tests_path(), "schemas", "invalid", "no_configuration.sql")
        @test_throws Quiver.DatabaseException Quiver.from_schema(":memory:", path_schema)
    end

    @testset "Label Not Null" begin
        path_schema = joinpath(tests_path(), "schemas", "invalid", "label_not_null.sql")
        @test_throws Quiver.DatabaseException Quiver.from_schema(":memory:", path_schema)
    end

    @testset "Label Not Unique" begin
        path_schema = joinpath(tests_path(), "schemas", "invalid", "label_not_unique.sql")
        @test_throws Quiver.DatabaseException Quiver.from_schema(":memory:", path_schema)
    end

    @testset "Label Wrong Type" begin
        path_schema = joinpath(tests_path(), "schemas", "invalid", "label_wrong_type.sql")
        @test_throws Quiver.DatabaseException Quiver.from_schema(":memory:", path_schema)
    end

    @testset "Duplicate Attribute" begin
        path_schema = joinpath(tests_path(), "schemas", "invalid", "duplicate_attribute_vector.sql")
        @test_throws Quiver.DatabaseException Quiver.from_schema(":memory:", path_schema)
    end

    @testset "Duplicate Attribute Time Series" begin
        path_schema = joinpath(tests_path(), "schemas", "invalid", "duplicate_attribute_time_series.sql")
        @test_throws Quiver.DatabaseException Quiver.from_schema(":memory:", path_schema)
    end

    @testset "Vector No Index" begin
        path_schema = joinpath(tests_path(), "schemas", "invalid", "vector_no_index.sql")
        @test_throws Quiver.DatabaseException Quiver.from_schema(":memory:", path_schema)
    end

    @testset "Set No Unique" begin
        path_schema = joinpath(tests_path(), "schemas", "invalid", "set_no_unique.sql")
        @test_throws Quiver.DatabaseException Quiver.from_schema(":memory:", path_schema)
    end

    @testset "FK Not Null Set Null" begin
        path_schema = joinpath(tests_path(), "schemas", "invalid", "fk_not_null_set_null.sql")
        @test_throws Quiver.DatabaseException Quiver.from_schema(":memory:", path_schema)
    end

    @testset "FK Actions" begin
        path_schema = joinpath(tests_path(), "schemas", "invalid", "fk_actions.sql")
        @test_throws Quiver.DatabaseException Quiver.from_schema(":memory:", path_schema)
    end

    @testset "Set No Parent FK" begin
        path_schema = joinpath(tests_path(), "schemas", "invalid", "set_no_parent_fk.sql")
        exc = @test_throws Quiver.DatabaseException Quiver.from_schema(":memory:", path_schema)
        @test occursin("Set table 'Collection_set_tags' must have foreign key to parent collection", exc.value.msg)
    end

    @testset "Set Unknown Parent" begin
        path_schema = joinpath(tests_path(), "schemas", "invalid", "set_unknown_parent.sql")
        exc = @test_throws Quiver.DatabaseException Quiver.from_schema(":memory:", path_schema)
        @test occursin("Set table 'Ghost_set_tags' references non-existent collection 'Ghost'", exc.value.msg)
    end

    @testset "Time Series FK Actions" begin
        path_schema = joinpath(tests_path(), "schemas", "invalid", "time_series_fk_actions.sql")
        exc = @test_throws Quiver.DatabaseException Quiver.from_schema(":memory:", path_schema)
        @test occursin("FK to parent must use ON DELETE CASCADE ON UPDATE CASCADE", exc.value.msg)
    end

    @testset "Time Series Relation FK Actions" begin
        path_schema = joinpath(tests_path(), "schemas", "invalid", "time_series_relation_fk_actions.sql")
        exc = @test_throws Quiver.DatabaseException Quiver.from_schema(":memory:", path_schema)
        @test occursin("Foreign key 'parent_id' in table 'Collection_time_series_events'", exc.value.msg)
    end
end

end
