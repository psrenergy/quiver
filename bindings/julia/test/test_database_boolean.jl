module TestDatabaseBoolean

using Quiver
using Test

include("fixture.jl")

@testset "Boolean convenience methods" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "all_types.sql")
    db = Quiver.from_schema(":memory:", path_schema)

    @test Quiver.read_scalar_booleans(db, "AllTypes", "some_integer") isa Vector{Union{Nothing, Bool}}
    @test Quiver.read_vector_booleans(db, "AllTypes", "count_value") == Vector{Bool}[]
    @test Quiver.read_set_booleans(db, "AllTypes", "code") == Vector{Bool}[]

    id_false = Quiver.create_element!(db, "AllTypes";
        label = "False",
        some_integer = 0,
        count_value = [0, 1],
        code = [0, 1],
    )
    id_true = Quiver.create_element!(db, "AllTypes";
        label = "True",
        some_integer = 1,
        count_value = [1, 0],
        code = [1],
    )
    id_null = Quiver.create_element!(db, "AllTypes"; label = "Null")

    @test Quiver.read_scalar_booleans(db, "AllTypes", "some_integer") == [false, true, nothing]
    @test Quiver.read_scalar_boolean_by_id(db, "AllTypes", "some_integer", id_false) === false
    @test Quiver.read_scalar_boolean_by_id(db, "AllTypes", "some_integer", id_true) === true
    @test Quiver.read_scalar_boolean_by_id(db, "AllTypes", "some_integer", id_null) === nothing

    @test Quiver.read_vector_booleans(db, "AllTypes", "count_value") == [[false, true], [true, false]]
    @test Quiver.read_vector_booleans_by_id(db, "AllTypes", "count_value", id_false) == [false, true]
    @test Quiver.read_vector_booleans_by_id(db, "AllTypes", "count_value", id_null) == Bool[]

    @test Quiver.read_set_booleans(db, "AllTypes", "code") == [[false, true], [true]]
    @test Quiver.read_set_booleans_by_id(db, "AllTypes", "code", id_false) == [false, true]
    @test Quiver.read_set_booleans_by_id(db, "AllTypes", "code", id_null) == Bool[]

    @test Quiver.query_boolean(db, "SELECT 0") === false
    @test Quiver.query_boolean(db, "SELECT some_integer FROM AllTypes WHERE id = ?", [id_true]) === true
    @test Quiver.query_boolean(db, "SELECT some_integer FROM AllTypes WHERE id = -1") === nothing

    Quiver.create_element!(db, "AllTypes"; label = "Invalid", some_integer = 2)
    @test_throws InexactError Quiver.read_scalar_booleans(db, "AllTypes", "some_integer")
    @test_throws InexactError Quiver.query_boolean(db, "SELECT 2")

    Quiver.close!(db)
end

end
