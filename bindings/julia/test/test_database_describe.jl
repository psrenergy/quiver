module TestDatabaseDescribe

using Quiver
using Test

include("fixture.jl")

# The binding only verifies each method returns a string; the report content is
# covered by the C++ core tests (tests/test_database_describe.cpp).
function open_db()
    path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
    return Quiver.from_schema(":memory:", path_schema)
end

@testset "Describe" begin
    @testset "describe" begin
        db = open_db()
        @test Quiver.describe(db) isa String
        Quiver.close!(db)
    end

    @testset "describe_collection" begin
        db = open_db()
        @test Quiver.describe_collection(db, "Collection") isa String
        Quiver.close!(db)
    end

    @testset "summarize_collection" begin
        db = open_db()
        @test Quiver.summarize_collection(db, "Collection") isa String
        Quiver.close!(db)
    end
end

end
