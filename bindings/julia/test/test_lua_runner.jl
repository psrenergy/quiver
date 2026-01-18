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

# Error handling tests

@testset "LuaRunner Undefined Variable" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
    db = PSRDatabase.from_schema(":memory:", path_schema)

    lua = PSRDatabase.LuaRunner(db)

    # Script that references undefined variable
    @test_throws PSRDatabase.DatabaseException PSRDatabase.run!(lua, "print(undefined_variable.field)")

    PSRDatabase.close!(lua)
    PSRDatabase.close!(db)
end

@testset "LuaRunner Create Invalid Collection" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
    db = PSRDatabase.from_schema(":memory:", path_schema)

    lua = PSRDatabase.LuaRunner(db)

    PSRDatabase.run!(lua, """db:create_element("Configuration", { label = "Test Config" })""")

    # Script that creates element in nonexistent collection
    @test_throws PSRDatabase.DatabaseException PSRDatabase.run!(lua, """db:create_element("NonexistentCollection", { label = "Item" })""")

    PSRDatabase.close!(lua)
    PSRDatabase.close!(db)
end

@testset "LuaRunner Empty Script" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
    db = PSRDatabase.from_schema(":memory:", path_schema)

    lua = PSRDatabase.LuaRunner(db)

    # Empty script should succeed without error
    PSRDatabase.run!(lua, "")
    @test true  # If we get here, the empty script ran without error

    PSRDatabase.close!(lua)
    PSRDatabase.close!(db)
end

@testset "LuaRunner Comment Only Script" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
    db = PSRDatabase.from_schema(":memory:", path_schema)

    lua = PSRDatabase.LuaRunner(db)

    # Comment-only script should succeed
    PSRDatabase.run!(lua, "-- this is just a comment")
    @test true

    PSRDatabase.close!(lua)
    PSRDatabase.close!(db)
end

@testset "LuaRunner Read Integers" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
    db = PSRDatabase.from_schema(":memory:", path_schema)

    PSRDatabase.create_element!(db, "Configuration"; label = "Config")
    PSRDatabase.create_element!(db, "Collection"; label = "Item 1", some_integer = 100)
    PSRDatabase.create_element!(db, "Collection"; label = "Item 2", some_integer = 200)

    lua = PSRDatabase.LuaRunner(db)

    PSRDatabase.run!(
        lua,
        """
    local ints = db:read_scalar_integers("Collection", "some_integer")
    assert(#ints == 2, "Expected 2 integers")
    assert(ints[1] == 100, "First integer mismatch")
    assert(ints[2] == 200, "Second integer mismatch")
""",
    )

    PSRDatabase.close!(lua)
    PSRDatabase.close!(db)
end

@testset "LuaRunner Read Doubles" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
    db = PSRDatabase.from_schema(":memory:", path_schema)

    PSRDatabase.create_element!(db, "Configuration"; label = "Config")
    PSRDatabase.create_element!(db, "Collection"; label = "Item 1", some_float = 1.5)
    PSRDatabase.create_element!(db, "Collection"; label = "Item 2", some_float = 2.5)

    lua = PSRDatabase.LuaRunner(db)

    PSRDatabase.run!(
        lua,
        """
    local floats = db:read_scalar_doubles("Collection", "some_float")
    assert(#floats == 2, "Expected 2 doubles")
    assert(floats[1] == 1.5, "First double mismatch")
    assert(floats[2] == 2.5, "Second double mismatch")
""",
    )

    PSRDatabase.close!(lua)
    PSRDatabase.close!(db)
end

@testset "LuaRunner Read Vectors" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
    db = PSRDatabase.from_schema(":memory:", path_schema)

    PSRDatabase.create_element!(db, "Configuration"; label = "Config")
    PSRDatabase.create_element!(db, "Collection"; label = "Item 1", value_int = [1, 2, 3])

    lua = PSRDatabase.LuaRunner(db)

    PSRDatabase.run!(
        lua,
        """
    local vectors = db:read_vector_integers("Collection", "value_int")
    assert(#vectors == 1, "Expected 1 vector")
    assert(#vectors[1] == 3, "Expected 3 elements in vector")
    assert(vectors[1][1] == 1, "First element mismatch")
    assert(vectors[1][2] == 2, "Second element mismatch")
    assert(vectors[1][3] == 3, "Third element mismatch")
""",
    )

    PSRDatabase.close!(lua)
    PSRDatabase.close!(db)
end

@testset "LuaRunner Create With Vector" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
    db = PSRDatabase.from_schema(":memory:", path_schema)

    lua = PSRDatabase.LuaRunner(db)

    PSRDatabase.run!(
        lua,
        """
    db:create_element("Configuration", { label = "Config" })
    db:create_element("Collection", { label = "Item 1", value_int = {10, 20, 30} })
""",
    )

    result = PSRDatabase.read_vector_integers(db, "Collection", "value_int")
    @test length(result) == 1
    @test result[1] == [10, 20, 30]

    PSRDatabase.close!(lua)
    PSRDatabase.close!(db)
end

end
