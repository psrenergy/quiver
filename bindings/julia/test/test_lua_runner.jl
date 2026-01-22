module TestLuaRunner

using QUIVERDatabase
using Test

include("fixture.jl")

@testset "LuaRunner" begin
    @testset "Create Element" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = QUIVERDatabase.from_schema(":memory:", path_schema)

        lua = QUIVERDatabase.LuaRunner(db)

        QUIVERDatabase.run!(
            lua,
            """
        db:create_element("Configuration", { label = "Test Config" })
        db:create_element("Collection", { label = "Item 1", some_integer = 42 })
    """,
        )

        labels = QUIVERDatabase.read_scalar_strings(db, "Collection", "label")
        @test length(labels) == 1
        @test labels[1] == "Item 1"

        integers = QUIVERDatabase.read_scalar_integers(db, "Collection", "some_integer")
        @test length(integers) == 1
        @test integers[1] == 42

        QUIVERDatabase.close!(lua)
        QUIVERDatabase.close!(db)
    end

    @testset "Read from Lua" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = QUIVERDatabase.from_schema(":memory:", path_schema)

        QUIVERDatabase.create_element!(db, "Configuration"; label = "Config")
        QUIVERDatabase.create_element!(db, "Collection"; label = "Item 1", some_integer = 10)
        QUIVERDatabase.create_element!(db, "Collection"; label = "Item 2", some_integer = 20)

        lua = QUIVERDatabase.LuaRunner(db)

        QUIVERDatabase.run!(
            lua,
            """
        local labels = db:read_scalar_strings("Collection", "label")
        assert(#labels == 2, "Expected 2 labels")
        assert(labels[1] == "Item 1", "First label mismatch")
        assert(labels[2] == "Item 2", "Second label mismatch")
    """,
        )

        QUIVERDatabase.close!(lua)
        QUIVERDatabase.close!(db)
    end

    @testset "Script Error" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = QUIVERDatabase.from_schema(":memory:", path_schema)

        lua = QUIVERDatabase.LuaRunner(db)

        @test_throws QUIVERDatabase.DatabaseException QUIVERDatabase.run!(lua, "invalid syntax !!!")

        QUIVERDatabase.close!(lua)
        QUIVERDatabase.close!(db)
    end

    @testset "Reuse Runner" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = QUIVERDatabase.from_schema(":memory:", path_schema)

        lua = QUIVERDatabase.LuaRunner(db)

        QUIVERDatabase.run!(lua, """db:create_element("Configuration", { label = "Config" })""")
        QUIVERDatabase.run!(lua, """db:create_element("Collection", { label = "Item 1" })""")
        QUIVERDatabase.run!(lua, """db:create_element("Collection", { label = "Item 2" })""")

        labels = QUIVERDatabase.read_scalar_strings(db, "Collection", "label")
        @test length(labels) == 2
        @test labels[1] == "Item 1"
        @test labels[2] == "Item 2"

        QUIVERDatabase.close!(lua)
        QUIVERDatabase.close!(db)
    end

    # Error handling tests

    @testset "Undefined Variable" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = QUIVERDatabase.from_schema(":memory:", path_schema)

        lua = QUIVERDatabase.LuaRunner(db)

        # Script that references undefined variable
        @test_throws QUIVERDatabase.DatabaseException QUIVERDatabase.run!(lua, "print(undefined_variable.field)")

        QUIVERDatabase.close!(lua)
        QUIVERDatabase.close!(db)
    end

    @testset "Create Invalid Collection" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = QUIVERDatabase.from_schema(":memory:", path_schema)

        lua = QUIVERDatabase.LuaRunner(db)

        QUIVERDatabase.run!(lua, """db:create_element("Configuration", { label = "Test Config" })""")

        # Script that creates element in nonexistent collection
        @test_throws QUIVERDatabase.DatabaseException QUIVERDatabase.run!(
            lua,
            """db:create_element("NonexistentCollection", { label = "Item" })""",
        )

        QUIVERDatabase.close!(lua)
        QUIVERDatabase.close!(db)
    end

    @testset "Empty Script" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = QUIVERDatabase.from_schema(":memory:", path_schema)

        lua = QUIVERDatabase.LuaRunner(db)

        # Empty script should succeed without error
        QUIVERDatabase.run!(lua, "")
        @test true  # If we get here, the empty script ran without error

        QUIVERDatabase.close!(lua)
        QUIVERDatabase.close!(db)
    end

    @testset "Comment Only Script" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = QUIVERDatabase.from_schema(":memory:", path_schema)

        lua = QUIVERDatabase.LuaRunner(db)

        # Comment-only script should succeed
        QUIVERDatabase.run!(lua, "-- this is just a comment")
        @test true

        QUIVERDatabase.close!(lua)
        QUIVERDatabase.close!(db)
    end

    @testset "Read Integers" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = QUIVERDatabase.from_schema(":memory:", path_schema)

        QUIVERDatabase.create_element!(db, "Configuration"; label = "Config")
        QUIVERDatabase.create_element!(db, "Collection"; label = "Item 1", some_integer = 100)
        QUIVERDatabase.create_element!(db, "Collection"; label = "Item 2", some_integer = 200)

        lua = QUIVERDatabase.LuaRunner(db)

        QUIVERDatabase.run!(
            lua,
            """
        local ints = db:read_scalar_integers("Collection", "some_integer")
        assert(#ints == 2, "Expected 2 integers")
        assert(ints[1] == 100, "First integer mismatch")
        assert(ints[2] == 200, "Second integer mismatch")
    """,
        )

        QUIVERDatabase.close!(lua)
        QUIVERDatabase.close!(db)
    end

    @testset "Read Floats" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = QUIVERDatabase.from_schema(":memory:", path_schema)

        QUIVERDatabase.create_element!(db, "Configuration"; label = "Config")
        QUIVERDatabase.create_element!(db, "Collection"; label = "Item 1", some_float = 1.5)
        QUIVERDatabase.create_element!(db, "Collection"; label = "Item 2", some_float = 2.5)

        lua = QUIVERDatabase.LuaRunner(db)

        QUIVERDatabase.run!(
            lua,
            """
        local floats = db:read_scalar_floats("Collection", "some_float")
        assert(#floats == 2, "Expected 2 floats")
        assert(floats[1] == 1.5, "First float mismatch")
        assert(floats[2] == 2.5, "Second float mismatch")
    """,
        )

        QUIVERDatabase.close!(lua)
        QUIVERDatabase.close!(db)
    end

    @testset "Read Vectors" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = QUIVERDatabase.from_schema(":memory:", path_schema)

        QUIVERDatabase.create_element!(db, "Configuration"; label = "Config")
        QUIVERDatabase.create_element!(db, "Collection"; label = "Item 1", value_int = [1, 2, 3])

        lua = QUIVERDatabase.LuaRunner(db)

        QUIVERDatabase.run!(
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

        QUIVERDatabase.close!(lua)
        QUIVERDatabase.close!(db)
    end

    @testset "Create With Vector" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = QUIVERDatabase.from_schema(":memory:", path_schema)

        lua = QUIVERDatabase.LuaRunner(db)

        QUIVERDatabase.run!(
            lua,
            """
        db:create_element("Configuration", { label = "Config" })
        db:create_element("Collection", { label = "Item 1", value_int = {10, 20, 30} })
    """,
        )

        result = QUIVERDatabase.read_vector_integers(db, "Collection", "value_int")
        @test length(result) == 1
        @test result[1] == [10, 20, 30]

        QUIVERDatabase.close!(lua)
        QUIVERDatabase.close!(db)
    end
end

end
