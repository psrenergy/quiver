module TestDatabaseRunModel

using Quiver
using Test

include("fixture.jl")

@testset "Run model" begin
    # The happy path (a real exit code from CarbSteeler) is not testable hermetically because
    # the model directory is a hardcoded constant in the native layer; it is verified manually.
    # The in-memory precondition fires before the model directory is consulted, so it is
    # deterministic on every machine/CI runner.
    @testset "Throws on in-memory database" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)
        @test_throws Quiver.DatabaseException Quiver.run_model!(db)
        Quiver.close!(db)
    end
end

end
