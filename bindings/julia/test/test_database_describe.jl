module TestDatabaseDescribe

using Quiver
using Test

include("fixture.jl")

# Verify report forwarding; detailed formatting is covered by the C++ core tests.
function open_db()
    path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
    return Quiver.from_schema(":memory:", path_schema)
end

@testset "Describe" begin
    @testset "describe" begin
        db = open_db()
        @test Quiver.describe(db) ==
              "Database: :memory:\nVersion: 0\n\n" *
              Quiver.describe_collection(db, "Collection") * "\n" *
              Quiver.describe_collection(db, "Configuration")
        Quiver.close!(db)
    end

    @testset "describe_collection" begin
        db = open_db()
        Quiver.create_element!(db, "Collection"; label = "a", some_integer = Int64(1))
        report = Quiver.describe_collection(db, "Collection")
        @test occursin("some_integer (INTEGER); 1 non-null, 0 null; values {1: 1}\n", report)
        @test occursin("some_float (REAL); 0 non-null, 1 null\n", report)
        @test occursin("data: [date_time], value(REAL); 0/1 non-empty\n", report)
        Quiver.close!(db)
    end
end

end
