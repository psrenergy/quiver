module TestDatabaseRead

using Dates
using Quiver
using Test

include("fixture.jl")

@testset "Read" begin
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

    @testset "Vector Attributes" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(
            db,
            "Collection";
            label = "Item 1",
            value_int = [1, 2, 3],
            value_float = [1.5, 2.5, 3.5],
        )
        Quiver.create_element!(
            db,
            "Collection";
            label = "Item 2",
            value_int = [10, 20],
            value_float = [10.5, 20.5],
        )

        @test Quiver.read_vector_integers(db, "Collection", "value_int") == [[1, 2, 3], [10, 20]]
        @test Quiver.read_vector_floats(db, "Collection", "value_float") == [[1.5, 2.5, 3.5], [10.5, 20.5]]

        Quiver.close!(db)
    end

    @testset "Vector Empty Result" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        # No Collection elements created
        @test Quiver.read_vector_integers(db, "Collection", "value_int") == Vector{Int64}[]
        @test Quiver.read_vector_floats(db, "Collection", "value_float") == Vector{Float64}[]

        Quiver.close!(db)
    end

    @testset "Vector Only Returns Elements With Data" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        # Create element with vectors
        Quiver.create_element!(db, "Collection"; label = "Item 1", value_int = [1, 2, 3])
        # Create element without vectors (no vector data inserted)
        Quiver.create_element!(db, "Collection"; label = "Item 2")
        # Create another element with vectors
        Quiver.create_element!(db, "Collection"; label = "Item 3", value_int = [4, 5])

        # Only elements with vector data are returned
        result = Quiver.read_vector_integers(db, "Collection", "value_int")
        @test length(result) == 2
        @test result[1] == [1, 2, 3]
        @test result[2] == [4, 5]

        Quiver.close!(db)
    end

    @testset "Set Attributes" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Collection"; label = "Item 1", tag = ["important", "urgent"])
        Quiver.create_element!(db, "Collection"; label = "Item 2", tag = ["review"])

        result = Quiver.read_set_strings(db, "Collection", "tag")
        @test length(result) == 2
        # Sets are unordered, so sort before comparison
        @test sort(result[1]) == ["important", "urgent"]
        @test result[2] == ["review"]

        Quiver.close!(db)
    end

    @testset "Set Empty Result" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        # No Collection elements created
        @test Quiver.read_set_strings(db, "Collection", "tag") == Vector{String}[]

        Quiver.close!(db)
    end

    @testset "Set Only Returns Elements With Data" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        # Create element with set data
        Quiver.create_element!(db, "Collection"; label = "Item 1", tag = ["important"])
        # Create element without set data
        Quiver.create_element!(db, "Collection"; label = "Item 2")
        # Create another element with set data
        Quiver.create_element!(db, "Collection"; label = "Item 3", tag = ["urgent", "review"])

        # Only elements with set data are returned
        result = Quiver.read_set_strings(db, "Collection", "tag")
        @test length(result) == 2

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
        date = Quiver.read_scalar_string_by_id(db, "Configuration", "date_attribute", Int64(1))
        @test date == "2024-01-15T10:30:00"

        Quiver.close!(db)
    end

    @testset "DateTime Native Objects via read_all_scalars_by_id" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration";
            label = "Config 1",
            date_attribute = "2024-01-15T10:30:00",
        )

        # Read all scalars for an element - date_attribute should be converted to DateTime
        scalars = Quiver.read_all_scalars_by_id(db, "Configuration", Int64(1))

        @test haskey(scalars, "date_attribute")
        @test scalars["date_attribute"] isa DateTime
        @test scalars["date_attribute"] == DateTime(2024, 1, 15, 10, 30, 0)

        Quiver.close!(db)
    end

    @testset "Vector Integers by ID" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Collection"; label = "Item 1", value_int = [1, 2, 3])
        Quiver.create_element!(db, "Collection"; label = "Item 2", value_int = [10, 20])

        @test Quiver.read_vector_integers_by_id(db, "Collection", "value_int", 1) == [1, 2, 3]
        @test Quiver.read_vector_integers_by_id(db, "Collection", "value_int", 2) == [10, 20]

        Quiver.close!(db)
    end

    @testset "Vector Floats by ID" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Collection"; label = "Item 1", value_float = [1.5, 2.5, 3.5])

        @test Quiver.read_vector_floats_by_id(db, "Collection", "value_float", 1) == [1.5, 2.5, 3.5]

        Quiver.close!(db)
    end

    @testset "Set Strings by ID" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Collection"; label = "Item 1", tag = ["important", "urgent"])
        Quiver.create_element!(db, "Collection"; label = "Item 2", tag = ["review"])

        result1 = Quiver.read_set_strings_by_id(db, "Collection", "tag", 1)
        @test sort(result1) == ["important", "urgent"]
        @test Quiver.read_set_strings_by_id(db, "Collection", "tag", 2) == ["review"]

        Quiver.close!(db)
    end

    @testset "Vector by ID Empty" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Collection"; label = "Item 1")  # No vector data

        @test Quiver.read_vector_integers_by_id(db, "Collection", "value_int", 1) == Int64[]

        Quiver.close!(db)
    end

    # Read element IDs tests

    @testset "Element IDs" begin
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

    @testset "Element IDs Empty" begin
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

    @testset "Vector Invalid Collection" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        @test_throws Quiver.DatabaseException Quiver.read_vector_integers(
            db,
            "NonexistentCollection",
            "value_int",
        )
        @test_throws Quiver.DatabaseException Quiver.read_vector_floats(
            db,
            "NonexistentCollection",
            "value_float",
        )

        Quiver.close!(db)
    end

    @testset "Set Invalid Collection" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        @test_throws Quiver.DatabaseException Quiver.read_set_strings(
            db,
            "NonexistentCollection",
            "tag",
        )

        Quiver.close!(db)
    end

    @testset "Element IDs Invalid Collection" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        @test_throws Quiver.DatabaseException Quiver.read_element_ids(db, "NonexistentCollection")

        Quiver.close!(db)
    end

    # ============================================================================
    # Gap-fill: String vector, integer set, float set reads (using all_types.sql)
    # ============================================================================

    @testset "Read Vector Strings Bulk" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "all_types.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "AllTypes"; label = "Item 1")
        Quiver.create_element!(db, "AllTypes"; label = "Item 2")

        Quiver.update_element!(db, "AllTypes", Int64(1); label_value = ["alpha", "beta"])
        Quiver.update_element!(db, "AllTypes", Int64(2); label_value = ["gamma"])

        result = Quiver.read_vector_strings(db, "AllTypes", "label_value")
        @test length(result) == 2
        @test result[1] == ["alpha", "beta"]
        @test result[2] == ["gamma"]

        Quiver.close!(db)
    end

    @testset "Read Vector Strings By ID" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "all_types.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "AllTypes"; label = "Item 1")
        Quiver.update_element!(db, "AllTypes", Int64(1); label_value = ["alpha", "beta", "gamma"])

        result = Quiver.read_vector_strings_by_id(db, "AllTypes", "label_value", Int64(1))
        @test result == ["alpha", "beta", "gamma"]

        Quiver.close!(db)
    end

    @testset "Read Set Integers Bulk" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "all_types.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "AllTypes"; label = "Item 1")
        Quiver.create_element!(db, "AllTypes"; label = "Item 2")

        Quiver.update_element!(db, "AllTypes", Int64(1); code = [10, 20, 30])
        Quiver.update_element!(db, "AllTypes", Int64(2); code = [40, 50])

        result = Quiver.read_set_integers(db, "AllTypes", "code")
        @test length(result) == 2
        @test sort(result[1]) == [10, 20, 30]
        @test sort(result[2]) == [40, 50]

        Quiver.close!(db)
    end

    @testset "Read Set Integers By ID" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "all_types.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "AllTypes"; label = "Item 1")
        Quiver.update_set_integers!(db, "AllTypes", "code", Int64(1), [10, 20, 30])

        result = Quiver.read_set_integers_by_id(db, "AllTypes", "code", Int64(1))
        @test sort(result) == [10, 20, 30]

        Quiver.close!(db)
    end

    @testset "Read Set Floats Bulk" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "all_types.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "AllTypes"; label = "Item 1")
        Quiver.create_element!(db, "AllTypes"; label = "Item 2")

        Quiver.update_set_floats!(db, "AllTypes", "weight", Int64(1), [1.1, 2.2])
        Quiver.update_set_floats!(db, "AllTypes", "weight", Int64(2), [3.3, 4.4, 5.5])

        result = Quiver.read_set_floats(db, "AllTypes", "weight")
        @test length(result) == 2
        @test sort(result[1]) == [1.1, 2.2]
        @test sort(result[2]) == [3.3, 4.4, 5.5]

        Quiver.close!(db)
    end

    @testset "Read Set Floats By ID" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "all_types.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "AllTypes"; label = "Item 1")
        Quiver.update_set_floats!(db, "AllTypes", "weight", Int64(1), [1.1, 2.2])

        result = Quiver.read_set_floats_by_id(db, "AllTypes", "weight", Int64(1))
        @test sort(result) == [1.1, 2.2]

        Quiver.close!(db)
    end

    @testset "Read Vector Strings By ID Empty" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "all_types.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "AllTypes"; label = "Item 1")

        result = Quiver.read_vector_strings_by_id(db, "AllTypes", "label_value", Int64(1))
        @test isempty(result)

        Quiver.close!(db)
    end

    @testset "Read Set Integers By ID Empty" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "all_types.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "AllTypes"; label = "Item 1")

        result = Quiver.read_set_integers_by_id(db, "AllTypes", "code", Int64(1))
        @test isempty(result)

        Quiver.close!(db)
    end

    @testset "Read Set Floats By ID Empty" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "all_types.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "AllTypes"; label = "Item 1")

        result = Quiver.read_set_floats_by_id(db, "AllTypes", "weight", Int64(1))
        @test isempty(result)

        Quiver.close!(db)
    end

    @testset "Vector Group by ID" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Collection";
            label = "Item 1",
            value_int = [1, 2, 3],
            value_float = [1.5, 2.5, 3.5],
        )

        rows = Quiver.read_vector_group_by_id(db, "Collection", "values", 1)
        @test length(rows) == 3
        @test rows[1]["value_int"] == 1
        @test rows[1]["value_float"] == 1.5
        @test rows[3]["value_int"] == 3
        @test rows[3]["value_float"] == 3.5

        Quiver.close!(db)
    end

    @testset "Vector Group by ID Empty" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        rows = Quiver.read_vector_group_by_id(db, "Collection", "values", 999)
        @test isempty(rows)

        Quiver.close!(db)
    end
end

end
