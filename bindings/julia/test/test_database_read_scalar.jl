module TestDatabaseReadScalar

using Dates
using Quiver
using Test

include("fixture.jl")

@testset "Read Scalar" begin
    @testset "Scalar Attributes" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration";
            label = "Config 1",
            integer_attribute = 42,
            float_attribute = 3.14,
            string_attribute = "hello",
        )
        Quiver.create_element!(db, "Configuration";
            label = "Config 2",
            integer_attribute = 100,
            float_attribute = 2.71,
            string_attribute = "world",
        )

        @test Quiver.read_scalar_strings(db, "Configuration", "label") == ["Config 1", "Config 2"]
        @test Quiver.read_scalar_integers(db, "Configuration", "integer_attribute") == [42, 100]
        @test Quiver.read_scalar_floats(db, "Configuration", "float_attribute") == [3.14, 2.71]
        @test Quiver.read_scalar_strings(db, "Configuration", "string_attribute") == ["hello", "world"]

        Quiver.close!(db)
    end

    @testset "Collections" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Collection"; label = "Item 1", some_integer = 10, some_float = 1.5)
        Quiver.create_element!(db, "Collection"; label = "Item 2", some_integer = 20, some_float = 2.5)

        @test Quiver.read_scalar_strings(db, "Collection", "label") == ["Item 1", "Item 2"]
        @test Quiver.read_scalar_integers(db, "Collection", "some_integer") == [10, 20]
        @test Quiver.read_scalar_floats(db, "Collection", "some_float") == [1.5, 2.5]

        Quiver.close!(db)
    end

    @testset "Null Values Preserve Position" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        # float_attribute has no default, so an unset value is SQL NULL.
        Quiver.create_element!(db, "Configuration"; label = "Config 1", float_attribute = 10.0)
        Quiver.create_element!(db, "Configuration"; label = "Config 2")  # NULL float
        Quiver.create_element!(db, "Configuration"; label = "Config 3", float_attribute = 30.0)
        Quiver.create_element!(db, "Configuration"; label = "Config 4", float_attribute = 40.0)

        # One entry per element; the NULL must occupy a slot (nothing), not be dropped.
        @test Quiver.read_scalar_floats(db, "Configuration", "float_attribute") == [10.0, nothing, 30.0, 40.0]

        Quiver.close!(db)
    end

    @testset "Integer Null Preserves Position" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "all_types.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        # AllTypes.some_integer has no default, so an unset value is SQL NULL.
        Quiver.create_element!(db, "AllTypes"; label = "a", some_integer = 10)
        Quiver.create_element!(db, "AllTypes"; label = "b")  # NULL integer
        Quiver.create_element!(db, "AllTypes"; label = "c", some_integer = 30)

        @test Quiver.read_scalar_integers(db, "AllTypes", "some_integer") == [10, nothing, 30]

        Quiver.close!(db)
    end

    @testset "String Null Preserves Position" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Config 1", string_attribute = "hello")
        Quiver.create_element!(db, "Configuration"; label = "Config 2")  # NULL string
        Quiver.create_element!(db, "Configuration"; label = "Config 3", string_attribute = "world")
        # Empty string is a present value, not a NULL — it must stay distinct from `nothing`.
        Quiver.create_element!(db, "Configuration"; label = "Config 4", string_attribute = "")

        @test Quiver.read_scalar_strings(db, "Configuration", "string_attribute") ==
              ["hello", nothing, "world", ""]

        Quiver.close!(db)
    end

    @testset "All Null Column" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Config 1")
        Quiver.create_element!(db, "Configuration"; label = "Config 2")

        # Full-length result, every slot nothing (not an empty vector).
        @test Quiver.read_scalar_floats(db, "Configuration", "float_attribute") == [nothing, nothing]

        Quiver.close!(db)
    end

    @testset "Empty Result" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        # No Collection elements created
        @test Quiver.read_scalar_strings(db, "Collection", "label") == String[]
        @test Quiver.read_scalar_integers(db, "Collection", "some_integer") == Int64[]
        @test Quiver.read_scalar_floats(db, "Collection", "some_float") == Float64[]

        Quiver.close!(db)
    end

    @testset "Concrete vs Optional element types" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration";
            label = "Config 1",
            integer_attribute = 42,
            float_attribute = 3.14,
            string_attribute = "hello",
        )

        # NOT NULL column (label is TEXT UNIQUE NOT NULL) -> concrete element type.
        @test Quiver.read_scalar_strings(db, "Configuration", "label") isa Vector{String}

        # Nullable columns -> Optional element type, even when this read happens to have no NULLs.
        @test Quiver.read_scalar_integers(db, "Configuration", "integer_attribute") isa
              Vector{Union{Int64, Nothing}}
        @test Quiver.read_scalar_floats(db, "Configuration", "float_attribute") isa
              Vector{Union{Float64, Nothing}}
        @test Quiver.read_scalar_strings(db, "Configuration", "string_attribute") isa
              Vector{Union{String, Nothing}}

        Quiver.close!(db)
    end

    @testset "Element type stable across empty and populated reads" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        # Empty Collection: type is decided by the schema, not the data.
        @test Quiver.read_scalar_strings(db, "Collection", "label") isa Vector{String}
        @test Quiver.read_scalar_integers(db, "Collection", "some_integer") isa
              Vector{Union{Int64, Nothing}}
        @test Quiver.read_scalar_floats(db, "Collection", "some_float") isa
              Vector{Union{Float64, Nothing}}

        Quiver.create_element!(db, "Collection"; label = "Item 1", some_integer = 10, some_float = 1.5)

        # Populated read of the same columns yields the identical element types.
        @test Quiver.read_scalar_strings(db, "Collection", "label") isa Vector{String}
        @test Quiver.read_scalar_integers(db, "Collection", "some_integer") isa
              Vector{Union{Int64, Nothing}}
        @test Quiver.read_scalar_floats(db, "Collection", "some_float") isa
              Vector{Union{Float64, Nothing}}

        Quiver.close!(db)
    end

    @testset "All-NULL nullable column stays full-length Optional" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Config 1")
        Quiver.create_element!(db, "Configuration"; label = "Config 2")

        result = Quiver.read_scalar_floats(db, "Configuration", "float_attribute")
        @test result isa Vector{Union{Float64, Nothing}}
        @test result == [nothing, nothing]

        Quiver.close!(db)
    end
    # Read by ID tests

    @testset "Scalar Integers by ID" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 42)
        Quiver.create_element!(db, "Configuration"; label = "Config 2", integer_attribute = 100)

        @test Quiver.read_scalar_integer_by_id(db, "Configuration", "integer_attribute", 1) == 42
        @test Quiver.read_scalar_integer_by_id(db, "Configuration", "integer_attribute", 2) == 100
        @test Quiver.read_scalar_integer_by_id(db, "Configuration", "integer_attribute", 999) === nothing

        Quiver.close!(db)
    end

    @testset "Scalar Floats by ID" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Config 1", float_attribute = 3.14)
        Quiver.create_element!(db, "Configuration"; label = "Config 2", float_attribute = 2.71)

        @test Quiver.read_scalar_float_by_id(db, "Configuration", "float_attribute", 1) == 3.14
        @test Quiver.read_scalar_float_by_id(db, "Configuration", "float_attribute", 2) == 2.71

        Quiver.close!(db)
    end

    @testset "Scalar Strings by ID" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Config 1", string_attribute = "hello")
        Quiver.create_element!(db, "Configuration"; label = "Config 2", string_attribute = "world")

        @test Quiver.read_scalar_string_by_id(db, "Configuration", "string_attribute", 1) == "hello"
        @test Quiver.read_scalar_string_by_id(db, "Configuration", "string_attribute", 2) == "world"

        Quiver.close!(db)
    end

    @testset "DateTime Attributes" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration";
            label = "Config 1",
            date_attribute = "2024-01-15T10:30:00",
        )
        Quiver.create_element!(db, "Configuration";
            label = "Config 2",
            date_attribute = "2024-06-20T14:45:30",
        )

        # Read all DateTime values
        dates = Quiver.read_scalar_strings(db, "Configuration", "date_attribute")
        @test length(dates) == 2
        @test dates[1] == "2024-01-15T10:30:00"
        @test dates[2] == "2024-06-20T14:45:30"

        # Read DateTime by ID
        date = Quiver.read_scalar_string_by_id(db, "Configuration", "date_attribute", 1)
        @test date == "2024-01-15T10:30:00"

        Quiver.close!(db)
    end

    @testset "DateTime Native Objects via read_scalars_by_id" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration";
            label = "Config 1",
            date_attribute = "2024-01-15T10:30:00",
        )

        # Read all scalars for an element - date_attribute should be converted to DateTime
        scalars = Quiver.read_scalars_by_id(db, "Configuration", 1)

        @test haskey(scalars, "date_attribute")
        @test scalars["date_attribute"] isa DateTime
        @test scalars["date_attribute"] == DateTime(2024, 1, 15, 10, 30, 0)

        Quiver.close!(db)
    end
    # Read element Ids tests

    @testset "Element Ids" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 42)
        Quiver.create_element!(db, "Configuration"; label = "Config 2", integer_attribute = 100)
        Quiver.create_element!(db, "Configuration"; label = "Config 3", integer_attribute = 200)

        ids = Quiver.read_element_ids(db, "Configuration")
        @test length(ids) == 3
        @test ids[1] == 1
        @test ids[2] == 2
        @test ids[3] == 3

        Quiver.close!(db)
    end

    @testset "Element Ids Empty" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        # No Collection elements created
        ids = Quiver.read_element_ids(db, "Collection")
        @test ids == Int64[]

        Quiver.close!(db)
    end

    # Error handling tests

    @testset "Invalid Collection" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        @test_throws Quiver.DatabaseException Quiver.read_scalar_strings(
            db,
            "NonexistentCollection",
            "label",
        )
        @test_throws Quiver.DatabaseException Quiver.read_scalar_integers(
            db,
            "NonexistentCollection",
            "value",
        )
        @test_throws Quiver.DatabaseException Quiver.read_scalar_floats(
            db,
            "NonexistentCollection",
            "value",
        )

        Quiver.close!(db)
    end

    @testset "Invalid Attribute" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        @test_throws Quiver.DatabaseException Quiver.read_scalar_strings(
            db,
            "Configuration",
            "nonexistent_attr",
        )
        @test_throws Quiver.DatabaseException Quiver.read_scalar_integers(
            db,
            "Configuration",
            "nonexistent_attr",
        )
        @test_throws Quiver.DatabaseException Quiver.read_scalar_floats(
            db,
            "Configuration",
            "nonexistent_attr",
        )

        Quiver.close!(db)
    end

    @testset "ID Not Found" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 42)

        # Read by ID that doesn't exist returns nothing
        @test Quiver.read_scalar_integer_by_id(db, "Configuration", "integer_attribute", 999) === nothing
        @test Quiver.read_scalar_float_by_id(db, "Configuration", "float_attribute", 999) === nothing
        @test Quiver.read_scalar_string_by_id(db, "Configuration", "string_attribute", 999) === nothing

        Quiver.close!(db)
    end
    @testset "Element Ids Invalid Collection" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        @test_throws Quiver.DatabaseException Quiver.read_element_ids(db, "NonexistentCollection")

        Quiver.close!(db)
    end
    @testset "read_element_by_id" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        Quiver.create_element!(db, "Collection";
            label = "Item 1",
            some_integer = 10,
            some_float = 1.5,
            value_int = [1, 2, 3],
            value_float = [0.1, 0.2, 0.3],
        )

        element = Quiver.read_element_by_id(db, "Collection", 1)

        @test element["label"] == "Item 1"
        @test element["some_integer"] == 10
        @test element["some_float"] == 1.5
        @test element["value_int"] == [1, 2, 3]
        @test element["value_float"] == [0.1, 0.2, 0.3]

        Quiver.close!(db)
    end

    @testset "Read DateTime By Id" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration";
            label = "With Date",
            date_attribute = DateTime(2024, 6, 15, 10, 30, 0),
        )
        Quiver.create_element!(db, "Configuration"; label = "Without Date")

        @test Quiver.read_scalar_date_time_by_id(db, "Configuration", "date_attribute", 1) ==
              DateTime(2024, 6, 15, 10, 30, 0)

        # NULL must come back as nothing, not crash
        @test Quiver.read_scalar_date_time_by_id(db, "Configuration", "date_attribute", 2) === nothing

        Quiver.close!(db)
    end
end

end
