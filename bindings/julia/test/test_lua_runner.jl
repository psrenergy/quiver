module TestLuaRunner

using PSRDatabase
using Test

include("fixture.jl")

@testset "LuaRunner Create Element" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
    db = PSRDatabase.from_schema(":memory:", path_schema)

    lua = PSRDatabase.LuaRunner(db)

    PSRDatabase.run!(
        lua,
        """
    db:create_element("Configuration", { label = "Test Config" })
    db:create_element("Collection", { label = "Item 1", some_integer = 42 })
""",
    )

    labels = PSRDatabase.read_scalar_strings(db, "Collection", "label")
    @test length(labels) == 1
    @test labels[1] == "Item 1"

    integers = PSRDatabase.read_scalar_integers(db, "Collection", "some_integer")
    @test length(integers) == 1
    @test integers[1] == 42

    PSRDatabase.close!(lua)
    PSRDatabase.close!(db)
end

@testset "LuaRunner Read from Lua" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
    db = PSRDatabase.from_schema(":memory:", path_schema)

    PSRDatabase.create_element!(db, "Configuration"; label = "Config")
    PSRDatabase.create_element!(db, "Collection"; label = "Item 1", some_integer = 10)
    PSRDatabase.create_element!(db, "Collection"; label = "Item 2", some_integer = 20)

    lua = PSRDatabase.LuaRunner(db)

    PSRDatabase.run!(
        lua,
        """
    local labels = db:read_scalar_strings("Collection", "label")
    assert(#labels == 2, "Expected 2 labels")
    assert(labels[1] == "Item 1", "First label mismatch")
    assert(labels[2] == "Item 2", "Second label mismatch")
""",
    )

    PSRDatabase.close!(lua)
    PSRDatabase.close!(db)
end

@testset "LuaRunner Script Error" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
    db = PSRDatabase.from_schema(":memory:", path_schema)

    lua = PSRDatabase.LuaRunner(db)

    @test_throws PSRDatabase.DatabaseException PSRDatabase.run!(lua, "invalid syntax !!!")

    PSRDatabase.close!(lua)
    PSRDatabase.close!(db)
end

@testset "LuaRunner Reuse Runner" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
    db = PSRDatabase.from_schema(":memory:", path_schema)

    lua = PSRDatabase.LuaRunner(db)

    PSRDatabase.run!(lua, """db:create_element("Configuration", { label = "Config" })""")
    PSRDatabase.run!(lua, """db:create_element("Collection", { label = "Item 1" })""")
    PSRDatabase.run!(lua, """db:create_element("Collection", { label = "Item 2" })""")

    labels = PSRDatabase.read_scalar_strings(db, "Collection", "label")
    @test length(labels) == 2
    @test labels[1] == "Item 1"
    @test labels[2] == "Item 2"

    PSRDatabase.close!(lua)
    PSRDatabase.close!(db)
end

end
