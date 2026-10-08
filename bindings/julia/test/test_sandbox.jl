module TestSandbox

using Quiver
using Test

include("fixture.jl")

@testset "Sandbox" begin
    @testset "Element REAL Arrays Preserve Lua Cell Types" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)
        sandbox = Quiver.Sandbox(db)
        try
            Quiver.run!(
                sandbox,
                """db:create_element("Collection", { label = "Mixed", value_float = {1, 2.5, true} })""",
            )
            @test Quiver.read_vector_floats(db, "Collection", "value_float") == [[1, 2.5, 1]]

            Quiver.run!(
                sandbox,
                """db:update_element_by_label("Collection", "Mixed", { value_float = {false, 3.5, 2} })""",
            )
            @test Quiver.read_vector_floats(db, "Collection", "value_float") == [[0, 3.5, 2]]
        finally
            Quiver.close!(sandbox)
            Quiver.close!(db)
        end
    end

    @testset "Create Element" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        sandbox = Quiver.Sandbox(db)

        Quiver.run!(
            sandbox,
            """
        db:create_element("Configuration", { label = "Test Config" })
        db:create_element("Collection", { label = "Item 1", some_integer = 42 })
    """,
        )

        labels = Quiver.read_scalar_strings(db, "Collection", "label")
        @test length(labels) == 1
        @test labels[1] == "Item 1"

        integers = Quiver.read_scalar_integers(db, "Collection", "some_integer")
        @test length(integers) == 1
        @test integers[1] == 42

        Quiver.close!(sandbox)
        Quiver.close!(db)
    end

    @testset "Read from Sandbox" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Config")
        Quiver.create_element!(db, "Collection"; label = "Item 1", some_integer = 10)
        Quiver.create_element!(db, "Collection"; label = "Item 2", some_integer = 20)

        sandbox = Quiver.Sandbox(db)

        Quiver.run!(
            sandbox,
            """
        local labels = db:read_scalar_strings("Collection", "label")
        assert(#labels == 2, "Expected 2 labels")
        assert(labels[1] == "Item 1", "First label mismatch")
        assert(labels[2] == "Item 2", "Second label mismatch")
    """,
        )

        Quiver.close!(sandbox)
        Quiver.close!(db)
    end

    @testset "Script Error" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        sandbox = Quiver.Sandbox(db)

        @test_throws Quiver.DatabaseException Quiver.run!(sandbox, "invalid syntax !!!")

        Quiver.close!(sandbox)
        Quiver.close!(db)
    end

    @testset "Reuse Runner" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        sandbox = Quiver.Sandbox(db)

        Quiver.run!(sandbox, """db:create_element("Configuration", { label = "Config" })""")
        Quiver.run!(sandbox, """db:create_element("Collection", { label = "Item 1" })""")
        Quiver.run!(sandbox, """db:create_element("Collection", { label = "Item 2" })""")

        labels = Quiver.read_scalar_strings(db, "Collection", "label")
        @test length(labels) == 2
        @test labels[1] == "Item 1"
        @test labels[2] == "Item 2"

        Quiver.close!(sandbox)
        Quiver.close!(db)
    end

    # Error handling tests

    @testset "Undefined Variable" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        sandbox = Quiver.Sandbox(db)

        # Script that references undefined variable
        @test_throws Quiver.DatabaseException Quiver.run!(sandbox, "print(undefined_variable.field)")

        Quiver.close!(sandbox)
        Quiver.close!(db)
    end

    @testset "Create Invalid Collection" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        sandbox = Quiver.Sandbox(db)

        Quiver.run!(sandbox, """db:create_element("Configuration", { label = "Test Config" })""")

        # Script that creates element in nonexistent collection
        @test_throws Quiver.DatabaseException Quiver.run!(
            sandbox,
            """db:create_element("NonexistentCollection", { label = "Item" })""",
        )

        Quiver.close!(sandbox)
        Quiver.close!(db)
    end

    @testset "Empty Script" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        sandbox = Quiver.Sandbox(db)

        # Empty script should succeed without error
        Quiver.run!(sandbox, "")
        @test true  # If we get here, the empty script ran without error

        Quiver.close!(sandbox)
        Quiver.close!(db)
    end

    @testset "Comment Only Script" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        sandbox = Quiver.Sandbox(db)

        # Comment-only script should succeed
        Quiver.run!(sandbox, "-- this is just a comment")
        @test true

        Quiver.close!(sandbox)
        Quiver.close!(db)
    end

    @testset "Read Integers" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Config")
        Quiver.create_element!(db, "Collection"; label = "Item 1", some_integer = 100)
        Quiver.create_element!(db, "Collection"; label = "Item 2", some_integer = 200)

        sandbox = Quiver.Sandbox(db)

        Quiver.run!(
            sandbox,
            """
        local ints = db:read_scalar_integers("Collection", "some_integer")
        assert(#ints == 2, "Expected 2 integers")
        assert(ints[1] == 100, "First integer mismatch")
        assert(ints[2] == 200, "Second integer mismatch")
    """,
        )

        Quiver.close!(sandbox)
        Quiver.close!(db)
    end

    @testset "Read Floats" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Config")
        Quiver.create_element!(db, "Collection"; label = "Item 1", some_float = 1.5)
        Quiver.create_element!(db, "Collection"; label = "Item 2", some_float = 2.5)

        sandbox = Quiver.Sandbox(db)

        Quiver.run!(
            sandbox,
            """
        local floats = db:read_scalar_floats("Collection", "some_float")
        assert(#floats == 2, "Expected 2 floats")
        assert(floats[1] == 1.5, "First float mismatch")
        assert(floats[2] == 2.5, "Second float mismatch")
    """,
        )

        Quiver.close!(sandbox)
        Quiver.close!(db)
    end

    @testset "Read Vectors" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Config")
        Quiver.create_element!(db, "Collection"; label = "Item 1", value_int = [1, 2, 3])

        sandbox = Quiver.Sandbox(db)

        Quiver.run!(
            sandbox,
            """
        local vectors = db:read_vector_integers("Collection", "value_int")
        assert(#vectors == 1, "Expected 1 vector")
        assert(#vectors[1] == 3, "Expected 3 elements in vector")
        assert(vectors[1][1] == 1, "First element mismatch")
        assert(vectors[1][2] == 2, "Second element mismatch")
        assert(vectors[1][3] == 3, "Third element mismatch")
    """,
        )

        Quiver.close!(sandbox)
        Quiver.close!(db)
    end

    @testset "Create With Vector" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        sandbox = Quiver.Sandbox(db)

        Quiver.run!(
            sandbox,
            """
        db:create_element("Configuration", { label = "Config" })
        db:create_element("Collection", { label = "Item 1", value_int = {10, 20, 30} })
    """,
        )

        result = Quiver.read_vector_integers(db, "Collection", "value_int")
        @test length(result) == 1
        @test result[1] == [10, 20, 30]

        Quiver.close!(sandbox)
        Quiver.close!(db)
    end

    @testset "Return Values" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        sandbox = Quiver.Sandbox(db)

        # A script hands one value back as JSON.
        @test Quiver.run!(sandbox, "return { a = 1, b = { 2, 3 } }") == """{"a":1,"b":[2,3]}"""
        @test Quiver.run!(sandbox, """return db:read_element_ids("Collection")""") == "[]"
        # Returning nothing is an empty string, distinct from returning nil.
        @test Quiver.run!(sandbox, "local x = 1") == ""
        @test Quiver.run!(sandbox, "return nil") == "null"

        Quiver.close!(sandbox)
        Quiver.close!(db)
    end

    @testset "Dry Run" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        sandbox = Quiver.Sandbox(db)
        Quiver.run!(sandbox, """db:create_element("Configuration", { label = "Config" })""")

        @test Quiver.in_dry_run(db) == false
        result = Quiver.dry_run(db) do db
            @test Quiver.in_dry_run(db) == true
            # db:transaction composes: the dry run absorbs the nested BEGIN/COMMIT.
            Quiver.run!(
                sandbox,
                """
            db:transaction(function(db)
                db:create_element("Collection", { label = "Preview" })
            end)
            return db:read_scalar_strings("Collection", "label")
        """,
            )
        end

        @test result == """["Preview"]"""
        @test Quiver.in_dry_run(db) == false
        @test isempty(Quiver.read_scalar_strings(db, "Collection", "label"))

        # Ending one that was never started is an error.
        @test_throws Quiver.DatabaseException Quiver.end_dry_run!(db)

        Quiver.close!(sandbox)
        Quiver.close!(db)
    end
end

end
