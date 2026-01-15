module TestRead

using PSRDatabase
using Test

include("fixture.jl")

@testset "Read Scalar Attributes" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
    db = PSRDatabase.create_empty_db_from_schema(":memory:", path_schema; force = true)

    PSRDatabase.create_element!(db, "Configuration";
        label = "Config 1",
        integer_attribute = 42,
        float_attribute = 3.14,
        string_attribute = "hello",
    )
    PSRDatabase.create_element!(db, "Configuration";
        label = "Config 2",
        integer_attribute = 100,
        float_attribute = 2.71,
        string_attribute = "world",
    )

    @test PSRDatabase.read_scalar_strings(db, "Configuration", "label") == ["Config 1", "Config 2"]
    @test PSRDatabase.read_scalar_ints(db, "Configuration", "integer_attribute") == [42, 100]
    @test PSRDatabase.read_scalar_doubles(db, "Configuration", "float_attribute") == [3.14, 2.71]
    @test PSRDatabase.read_scalar_strings(db, "Configuration", "string_attribute") == ["hello", "world"]

    PSRDatabase.close!(db)
end

@testset "Read From Collections" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
    db = PSRDatabase.create_empty_db_from_schema(":memory:", path_schema; force = true)

    PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")

    PSRDatabase.create_element!(db, "Collection";
        label = "Item 1",
        some_integer = 10,
        some_float = 1.5,
    )
    PSRDatabase.create_element!(db, "Collection";
        label = "Item 2",
        some_integer = 20,
        some_float = 2.5,
    )

    @test PSRDatabase.read_scalar_strings(db, "Collection", "label") == ["Item 1", "Item 2"]
    @test PSRDatabase.read_scalar_ints(db, "Collection", "some_integer") == [10, 20]
    @test PSRDatabase.read_scalar_doubles(db, "Collection", "some_float") == [1.5, 2.5]

    PSRDatabase.close!(db)
end

@testset "Read Empty Result" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
    db = PSRDatabase.create_empty_db_from_schema(":memory:", path_schema; force = true)

    PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")

    # No Collection elements created
    @test PSRDatabase.read_scalar_strings(db, "Collection", "label") == String[]
    @test PSRDatabase.read_scalar_ints(db, "Collection", "some_integer") == Int64[]
    @test PSRDatabase.read_scalar_doubles(db, "Collection", "some_float") == Float64[]

    PSRDatabase.close!(db)
end

@testset "Read Vector Attributes" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
    db = PSRDatabase.create_empty_db_from_schema(":memory:", path_schema; force = true)

    PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")

    PSRDatabase.create_element!(db, "Collection";
        label = "Item 1",
        value_int = [1, 2, 3],
        value_float = [1.5, 2.5, 3.5],
    )
    PSRDatabase.create_element!(db, "Collection";
        label = "Item 2",
        value_int = [10, 20],
        value_float = [10.5, 20.5],
    )

    @test PSRDatabase.read_vector_ints(db, "Collection", "value_int") == [[1, 2, 3], [10, 20]]
    @test PSRDatabase.read_vector_doubles(db, "Collection", "value_float") == [[1.5, 2.5, 3.5], [10.5, 20.5]]

    PSRDatabase.close!(db)
end

@testset "Read Vector Empty Result" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
    db = PSRDatabase.create_empty_db_from_schema(":memory:", path_schema; force = true)

    PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")

    # No Collection elements created
    @test PSRDatabase.read_vector_ints(db, "Collection", "value_int") == Vector{Int64}[]
    @test PSRDatabase.read_vector_doubles(db, "Collection", "value_float") == Vector{Float64}[]

    PSRDatabase.close!(db)
end

@testset "Read Vector Only Returns Elements With Data" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
    db = PSRDatabase.create_empty_db_from_schema(":memory:", path_schema; force = true)

    PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")

    # Create element with vectors
    PSRDatabase.create_element!(db, "Collection";
        label = "Item 1",
        value_int = [1, 2, 3],
    )
    # Create element without vectors (no vector data inserted)
    PSRDatabase.create_element!(db, "Collection";
        label = "Item 2",
    )
    # Create another element with vectors
    PSRDatabase.create_element!(db, "Collection";
        label = "Item 3",
        value_int = [4, 5],
    )

    # Only elements with vector data are returned
    result = PSRDatabase.read_vector_ints(db, "Collection", "value_int")
    @test length(result) == 2
    @test result[1] == [1, 2, 3]
    @test result[2] == [4, 5]

    PSRDatabase.close!(db)
end

end
