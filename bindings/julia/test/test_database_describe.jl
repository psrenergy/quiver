module TestDatabaseDescribe

using Quiver
using Test

include("fixture.jl")

function seeded()
    path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
    db = Quiver.from_schema(":memory:", path_schema)
    Quiver.create_element!(db, "Collection"; label = "a", some_integer = 1, value_int = [10, 20])
    Quiver.create_element!(db, "Collection"; label = "b", some_integer = 1)
    Quiver.create_element!(db, "Collection"; label = "c", some_integer = 5)
    return db
end

@testset "Describe" begin
    @testset "describe" begin
        db = seeded()

        report = Quiver.describe(db)
        @test report isa String
        @test occursin("Collection: Configuration", report)
        @test occursin("Collection: Collection (3 elements)", report)

        Quiver.close!(db)
    end

    @testset "describe_collection" begin
        db = seeded()

        report = Quiver.describe_collection(db, "Collection")
        @test report isa String
        @test occursin("Collection: Collection", report)
        @test occursin("[date_time]", report)

        Quiver.close!(db)
    end

    @testset "summarize_collection" begin
        db = seeded()

        report = Quiver.summarize_collection(db, "Collection")
        @test report isa String
        @test occursin("some_integer: 3 non-null, 0 null; values {1: 2, 5: 1}", report)
        @test occursin("values: 1/3 non-empty", report)

        Quiver.close!(db)
    end
end

end
