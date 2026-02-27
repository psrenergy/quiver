module TestElement

using Dates
using Quiver
using Test

include("fixture.jl")

@testset "Element" begin
    @testset "Set Integer" begin
        el = Quiver.Element()
        el["value"] = 42
        el["label"] = "Test"
        Quiver.destroy!(el)
        @test true
    end

    @testset "Set Float" begin
        el = Quiver.Element()
        el["value"] = 3.14
        el["label"] = "Test"
        Quiver.destroy!(el)
        @test true
    end

    @testset "Set String" begin
        el = Quiver.Element()
        el["value"] = "hello world"
        el["label"] = "Test"
        Quiver.destroy!(el)
        @test true
    end

    @testset "Set DateTime" begin
        el = Quiver.Element()
        el["value"] = DateTime(2024, 1, 15, 10, 30, 0)
        el["label"] = "Test"
        Quiver.destroy!(el)
        @test true
    end

    @testset "Set Array Integer" begin
        el = Quiver.Element()
        el["values"] = [1, 2, 3, 4, 5]
        el["label"] = "Test"
        Quiver.destroy!(el)
        @test true
    end

    @testset "Set Array Float" begin
        el = Quiver.Element()
        el["values"] = [1.1, 2.2, 3.3]
        el["label"] = "Test"
        Quiver.destroy!(el)
        @test true
    end

    @testset "Set Array String" begin
        el = Quiver.Element()
        el["values"] = ["alpha", "beta", "gamma"]
        el["label"] = "Test"
        Quiver.destroy!(el)
        @test true
    end

    @testset "Empty Array Rejected" begin
        el = Quiver.Element()
        @test_throws ArgumentError el["values"] = Any[]
        Quiver.destroy!(el)
    end

    @testset "Clear Element" begin
        el = Quiver.Element()
        el["label"] = "Test"
        el["value"] = 42
        Quiver.clear!(el)
        # After clear, element can be reused with new values
        el["label"] = "Reused"
        Quiver.destroy!(el)
        @test true
    end

    @testset "Create Element with Builder" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        el = Quiver.Element()
        el["label"] = "Config via Builder"
        el["integer_attribute"] = 42
        el["float_attribute"] = 3.14
        el["string_attribute"] = "hello"

        id = Quiver.create_element!(db, "Configuration", el)
        Quiver.destroy!(el)

        @test id > 0

        # Verify the element was created correctly
        @test Quiver.read_scalar_string_by_id(db, "Configuration", "label", id) == "Config via Builder"
        @test Quiver.read_scalar_integer_by_id(db, "Configuration", "integer_attribute", id) == 42
        @test Quiver.read_scalar_float_by_id(db, "Configuration", "float_attribute", id) == 3.14
        @test Quiver.read_scalar_string_by_id(db, "Configuration", "string_attribute", id) == "hello"

        Quiver.close!(db)
    end

    @testset "Create Element with Vector via Builder" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        el = Quiver.Element()
        el["label"] = "Item via Builder"
        el["value_int"] = [1, 2, 3]
        el["value_float"] = [1.5, 2.5, 3.5]

        id = Quiver.create_element!(db, "Collection", el)
        Quiver.destroy!(el)

        @test id > 0

        # Verify vectors were created correctly
        @test Quiver.read_vector_integers_by_id(db, "Collection", "value_int", id) == [1, 2, 3]
        @test Quiver.read_vector_floats_by_id(db, "Collection", "value_float", id) == [1.5, 2.5, 3.5]

        Quiver.close!(db)
    end

    @testset "Create Element with Set via Builder" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        el = Quiver.Element()
        el["label"] = "Item with Set"
        el["tag"] = ["alpha", "beta", "gamma"]

        id = Quiver.create_element!(db, "Collection", el)
        Quiver.destroy!(el)

        @test id > 0

        # Verify set was created correctly
        result = Quiver.read_set_strings_by_id(db, "Collection", "tag", id)
        @test sort(result) == ["alpha", "beta", "gamma"]

        Quiver.close!(db)
    end

    @testset "Show Element" begin
        el = Quiver.Element()
        el["label"] = "Test"
        el["value"] = 42

        # Test that show/display doesn't error
        io = IOBuffer()
        show(io, el)
        str = String(take!(io))
        @test length(str) > 0

        Quiver.destroy!(el)
    end
end

end
