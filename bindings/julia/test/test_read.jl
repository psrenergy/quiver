module TestRead

using PSRDatabase
using Test

include("fixture.jl")

@testset "Read" begin
    @testset "Scalar Attributes" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

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
        @test PSRDatabase.read_scalar_integers(db, "Configuration", "integer_attribute") == [42, 100]
        @test PSRDatabase.read_scalar_floats(db, "Configuration", "float_attribute") == [3.14, 2.71]
        @test PSRDatabase.read_scalar_strings(db, "Configuration", "string_attribute") == ["hello", "world"]

        PSRDatabase.close!(db)
    end

    @testset "Collections" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")
        PSRDatabase.create_element!(db, "Collection"; label = "Item 1", some_integer = 10, some_float = 1.5)
        PSRDatabase.create_element!(db, "Collection"; label = "Item 2", some_integer = 20, some_float = 2.5)

        @test PSRDatabase.read_scalar_strings(db, "Collection", "label") == ["Item 1", "Item 2"]
        @test PSRDatabase.read_scalar_integers(db, "Collection", "some_integer") == [10, 20]
        @test PSRDatabase.read_scalar_floats(db, "Collection", "some_float") == [1.5, 2.5]

        PSRDatabase.close!(db)
    end

    @testset "Empty Result" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")

        # No Collection elements created
        @test PSRDatabase.read_scalar_strings(db, "Collection", "label") == String[]
        @test PSRDatabase.read_scalar_integers(db, "Collection", "some_integer") == Int64[]
        @test PSRDatabase.read_scalar_floats(db, "Collection", "some_float") == Float64[]

        PSRDatabase.close!(db)
    end

    @testset "Vector Attributes" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")
        PSRDatabase.create_element!(
            db,
            "Collection";
            label = "Item 1",
            value_int = [1, 2, 3],
            value_float = [1.5, 2.5, 3.5],
        )
        PSRDatabase.create_element!(
            db,
            "Collection";
            label = "Item 2",
            value_int = [10, 20],
            value_float = [10.5, 20.5],
        )

        @test PSRDatabase.read_vector_integers(db, "Collection", "value_int") == [[1, 2, 3], [10, 20]]
        @test PSRDatabase.read_vector_floats(db, "Collection", "value_float") == [[1.5, 2.5, 3.5], [10.5, 20.5]]

        PSRDatabase.close!(db)
    end

    @testset "Vector Empty Result" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")

        # No Collection elements created
        @test PSRDatabase.read_vector_integers(db, "Collection", "value_int") == Vector{Int64}[]
        @test PSRDatabase.read_vector_floats(db, "Collection", "value_float") == Vector{Float64}[]

        PSRDatabase.close!(db)
    end

    @testset "Vector Only Returns Elements With Data" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")

        # Create element with vectors
        PSRDatabase.create_element!(db, "Collection"; label = "Item 1", value_int = [1, 2, 3])
        # Create element without vectors (no vector data inserted)
        PSRDatabase.create_element!(db, "Collection"; label = "Item 2")
        # Create another element with vectors
        PSRDatabase.create_element!(db, "Collection"; label = "Item 3", value_int = [4, 5])

        # Only elements with vector data are returned
        result = PSRDatabase.read_vector_integers(db, "Collection", "value_int")
        @test length(result) == 2
        @test result[1] == [1, 2, 3]
        @test result[2] == [4, 5]

        PSRDatabase.close!(db)
    end

    @testset "Set Attributes" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")
        PSRDatabase.create_element!(db, "Collection"; label = "Item 1", tag = ["important", "urgent"])
        PSRDatabase.create_element!(db, "Collection"; label = "Item 2", tag = ["review"])

        result = PSRDatabase.read_set_strings(db, "Collection", "tag")
        @test length(result) == 2
        # Sets are unordered, so sort before comparison
        @test sort(result[1]) == ["important", "urgent"]
        @test result[2] == ["review"]

        PSRDatabase.close!(db)
    end

    @testset "Set Empty Result" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")

        # No Collection elements created
        @test PSRDatabase.read_set_strings(db, "Collection", "tag") == Vector{String}[]

        PSRDatabase.close!(db)
    end

    @testset "Set Only Returns Elements With Data" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")

        # Create element with set data
        PSRDatabase.create_element!(db, "Collection"; label = "Item 1", tag = ["important"])
        # Create element without set data
        PSRDatabase.create_element!(db, "Collection"; label = "Item 2")
        # Create another element with set data
        PSRDatabase.create_element!(db, "Collection"; label = "Item 3", tag = ["urgent", "review"])

        # Only elements with set data are returned
        result = PSRDatabase.read_set_strings(db, "Collection", "tag")
        @test length(result) == 2

        PSRDatabase.close!(db)
    end

    @testset "Set and Read Scalar Relations" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")

        # Create parents
        PSRDatabase.create_element!(db, "Parent"; label = "Parent 1")
        PSRDatabase.create_element!(db, "Parent"; label = "Parent 2")

        # Create children
        PSRDatabase.create_element!(db, "Child"; label = "Child 1")
        PSRDatabase.create_element!(db, "Child"; label = "Child 2")
        PSRDatabase.create_element!(db, "Child"; label = "Child 3")

        # Set relations
        PSRDatabase.set_scalar_relation!(db, "Child", "parent_id", "Child 1", "Parent 1")
        PSRDatabase.set_scalar_relation!(db, "Child", "parent_id", "Child 3", "Parent 2")

        # Read relations
        labels = PSRDatabase.read_scalar_relation(db, "Child", "parent_id")
        @test length(labels) == 3
        @test labels[1] == "Parent 1"
        @test labels[2] === nothing  # Child 2 has no parent
        @test labels[3] == "Parent 2"

        PSRDatabase.close!(db)
    end

    @testset "Scalar Relations Self-Reference" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")

        # Create children (Child references itself via sibling_id)
        PSRDatabase.create_element!(db, "Child"; label = "Child 1")
        PSRDatabase.create_element!(db, "Child"; label = "Child 2")

        # Set sibling relation (self-reference)
        PSRDatabase.set_scalar_relation!(db, "Child", "sibling_id", "Child 1", "Child 2")

        # Read sibling relations
        labels = PSRDatabase.read_scalar_relation(db, "Child", "sibling_id")
        @test length(labels) == 2
        @test labels[1] == "Child 2"  # Child 1's sibling is Child 2
        @test labels[2] === nothing   # Child 2 has no sibling set

        PSRDatabase.close!(db)
    end

    @testset "Scalar Relations Empty Result" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")

        # No Child elements created
        labels = PSRDatabase.read_scalar_relation(db, "Child", "parent_id")
        @test labels == Union{String, Nothing}[]

        PSRDatabase.close!(db)
    end

    # Read by ID tests

    @testset "Scalar Integers by ID" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 42)
        PSRDatabase.create_element!(db, "Configuration"; label = "Config 2", integer_attribute = 100)

        @test PSRDatabase.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", 1) == 42
        @test PSRDatabase.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", 2) == 100
        @test PSRDatabase.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", 999) === nothing

        PSRDatabase.close!(db)
    end

    @testset "Scalar Floats by ID" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Config 1", float_attribute = 3.14)
        PSRDatabase.create_element!(db, "Configuration"; label = "Config 2", float_attribute = 2.71)

        @test PSRDatabase.read_scalar_floats_by_id(db, "Configuration", "float_attribute", 1) == 3.14
        @test PSRDatabase.read_scalar_floats_by_id(db, "Configuration", "float_attribute", 2) == 2.71

        PSRDatabase.close!(db)
    end

    @testset "Scalar Strings by ID" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Config 1", string_attribute = "hello")
        PSRDatabase.create_element!(db, "Configuration"; label = "Config 2", string_attribute = "world")

        @test PSRDatabase.read_scalar_strings_by_id(db, "Configuration", "string_attribute", 1) == "hello"
        @test PSRDatabase.read_scalar_strings_by_id(db, "Configuration", "string_attribute", 2) == "world"

        PSRDatabase.close!(db)
    end

    @testset "Vector Integers by ID" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")
        PSRDatabase.create_element!(db, "Collection"; label = "Item 1", value_int = [1, 2, 3])
        PSRDatabase.create_element!(db, "Collection"; label = "Item 2", value_int = [10, 20])

        @test PSRDatabase.read_vector_integers_by_id(db, "Collection", "value_int", 1) == [1, 2, 3]
        @test PSRDatabase.read_vector_integers_by_id(db, "Collection", "value_int", 2) == [10, 20]

        PSRDatabase.close!(db)
    end

    @testset "Vector Floats by ID" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")
        PSRDatabase.create_element!(db, "Collection"; label = "Item 1", value_float = [1.5, 2.5, 3.5])

        @test PSRDatabase.read_vector_floats_by_id(db, "Collection", "value_float", 1) == [1.5, 2.5, 3.5]

        PSRDatabase.close!(db)
    end

    @testset "Set Strings by ID" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")
        PSRDatabase.create_element!(db, "Collection"; label = "Item 1", tag = ["important", "urgent"])
        PSRDatabase.create_element!(db, "Collection"; label = "Item 2", tag = ["review"])

        result1 = PSRDatabase.read_set_strings_by_id(db, "Collection", "tag", 1)
        @test sort(result1) == ["important", "urgent"]
        @test PSRDatabase.read_set_strings_by_id(db, "Collection", "tag", 2) == ["review"]

        PSRDatabase.close!(db)
    end

    @testset "Vector by ID Empty" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")
        PSRDatabase.create_element!(db, "Collection"; label = "Item 1")  # No vector data

        @test PSRDatabase.read_vector_integers_by_id(db, "Collection", "value_int", 1) == Int64[]

        PSRDatabase.close!(db)
    end

    # Read element IDs tests

    @testset "Element IDs" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 42)
        PSRDatabase.create_element!(db, "Configuration"; label = "Config 2", integer_attribute = 100)
        PSRDatabase.create_element!(db, "Configuration"; label = "Config 3", integer_attribute = 200)

        ids = PSRDatabase.read_element_ids(db, "Configuration")
        @test length(ids) == 3
        @test ids[1] == 1
        @test ids[2] == 2
        @test ids[3] == 3

        PSRDatabase.close!(db)
    end

    @testset "Element IDs Empty" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")

        # No Collection elements created
        ids = PSRDatabase.read_element_ids(db, "Collection")
        @test ids == Int64[]

        PSRDatabase.close!(db)
    end

    # Get attribute type tests

    @testset "Get Attribute Type - Scalar Integer" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        result = PSRDatabase.get_attribute_type(db, "Configuration", "integer_attribute")
        @test result.data_structure == PSRDatabase.PSR_DATA_STRUCTURE_SCALAR
        @test result.data_type == PSRDatabase.PSR_DATA_TYPE_INTEGER

        PSRDatabase.close!(db)
    end

    @testset "Get Attribute Type - Scalar Real" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        result = PSRDatabase.get_attribute_type(db, "Configuration", "float_attribute")
        @test result.data_structure == PSRDatabase.PSR_DATA_STRUCTURE_SCALAR
        @test result.data_type == PSRDatabase.PSR_DATA_TYPE_FLOAT

        PSRDatabase.close!(db)
    end

    @testset "Get Attribute Type - Scalar Text" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        result = PSRDatabase.get_attribute_type(db, "Configuration", "string_attribute")
        @test result.data_structure == PSRDatabase.PSR_DATA_STRUCTURE_SCALAR
        @test result.data_type == PSRDatabase.PSR_DATA_TYPE_STRING

        PSRDatabase.close!(db)
    end

    @testset "Get Attribute Type - Vector Integer" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        result = PSRDatabase.get_attribute_type(db, "Collection", "value_int")
        @test result.data_structure == PSRDatabase.PSR_DATA_STRUCTURE_VECTOR
        @test result.data_type == PSRDatabase.PSR_DATA_TYPE_INTEGER

        PSRDatabase.close!(db)
    end

    @testset "Get Attribute Type - Vector Real" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        result = PSRDatabase.get_attribute_type(db, "Collection", "value_float")
        @test result.data_structure == PSRDatabase.PSR_DATA_STRUCTURE_VECTOR
        @test result.data_type == PSRDatabase.PSR_DATA_TYPE_FLOAT

        PSRDatabase.close!(db)
    end

    @testset "Get Attribute Type - Set Text" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        result = PSRDatabase.get_attribute_type(db, "Collection", "tag")
        @test result.data_structure == PSRDatabase.PSR_DATA_STRUCTURE_SET
        @test result.data_type == PSRDatabase.PSR_DATA_TYPE_STRING

        PSRDatabase.close!(db)
    end

    @testset "Get Attribute Type - Error on Nonexistent" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        @test_throws PSRDatabase.DatabaseException PSRDatabase.get_attribute_type(db, "Configuration", "nonexistent")

        PSRDatabase.close!(db)
    end

    # Error handling tests

    @testset "Invalid Collection" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        @test_throws PSRDatabase.DatabaseException PSRDatabase.read_scalar_strings(db, "NonexistentCollection", "label")
        @test_throws PSRDatabase.DatabaseException PSRDatabase.read_scalar_integers(
            db,
            "NonexistentCollection",
            "value",
        )
        @test_throws PSRDatabase.DatabaseException PSRDatabase.read_scalar_floats(db, "NonexistentCollection", "value")

        PSRDatabase.close!(db)
    end

    @testset "Invalid Attribute" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        @test_throws PSRDatabase.DatabaseException PSRDatabase.read_scalar_strings(
            db,
            "Configuration",
            "nonexistent_attr",
        )
        @test_throws PSRDatabase.DatabaseException PSRDatabase.read_scalar_integers(
            db,
            "Configuration",
            "nonexistent_attr",
        )
        @test_throws PSRDatabase.DatabaseException PSRDatabase.read_scalar_floats(
            db,
            "Configuration",
            "nonexistent_attr",
        )

        PSRDatabase.close!(db)
    end

    @testset "ID Not Found" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 42)

        # Read by ID that doesn't exist returns nothing
        @test PSRDatabase.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", 999) === nothing
        @test PSRDatabase.read_scalar_floats_by_id(db, "Configuration", "float_attribute", 999) === nothing
        @test PSRDatabase.read_scalar_strings_by_id(db, "Configuration", "string_attribute", 999) === nothing

        PSRDatabase.close!(db)
    end

    @testset "Vector Invalid Collection" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")

        @test_throws PSRDatabase.DatabaseException PSRDatabase.read_vector_integers(
            db,
            "NonexistentCollection",
            "value_int",
        )
        @test_throws PSRDatabase.DatabaseException PSRDatabase.read_vector_floats(
            db,
            "NonexistentCollection",
            "value_float",
        )

        PSRDatabase.close!(db)
    end

    @testset "Set Invalid Collection" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")

        @test_throws PSRDatabase.DatabaseException PSRDatabase.read_set_strings(db, "NonexistentCollection", "tag")

        PSRDatabase.close!(db)
    end

    @testset "Element IDs Invalid Collection" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        @test_throws PSRDatabase.DatabaseException PSRDatabase.read_element_ids(db, "NonexistentCollection")

        PSRDatabase.close!(db)
    end

    @testset "Scalar Relation Invalid Collection" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")

        @test_throws PSRDatabase.DatabaseException PSRDatabase.read_scalar_relation(
            db,
            "NonexistentCollection",
            "parent_id",
        )

        PSRDatabase.close!(db)
    end

    # ============================================================================
    # Generic read function tests
    # ============================================================================

    @testset "Generic Read - Scalar Integer" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 42)
        PSRDatabase.create_element!(db, "Configuration"; label = "Config 2", integer_attribute = 100)

        result = PSRDatabase.read(db, "Configuration", "integer_attribute")
        @test result == [42, 100]
        @test eltype(result) == Int64

        PSRDatabase.close!(db)
    end

    @testset "Generic Read - Scalar Float" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Config 1", float_attribute = 3.14)
        PSRDatabase.create_element!(db, "Configuration"; label = "Config 2", float_attribute = 2.71)

        result = PSRDatabase.read(db, "Configuration", "float_attribute")
        @test result == [3.14, 2.71]
        @test eltype(result) == Float64

        PSRDatabase.close!(db)
    end

    @testset "Generic Read - Scalar String" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Config 1", string_attribute = "hello")
        PSRDatabase.create_element!(db, "Configuration"; label = "Config 2", string_attribute = "world")

        result = PSRDatabase.read(db, "Configuration", "string_attribute")
        @test result == ["hello", "world"]
        @test eltype(result) == String

        PSRDatabase.close!(db)
    end

    @testset "Generic Read - Vector Integer" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")
        PSRDatabase.create_element!(db, "Collection"; label = "Item 1", value_int = [1, 2, 3])
        PSRDatabase.create_element!(db, "Collection"; label = "Item 2", value_int = [10, 20])

        result = PSRDatabase.read(db, "Collection", "value_int")
        @test result == [[1, 2, 3], [10, 20]]

        PSRDatabase.close!(db)
    end

    @testset "Generic Read - Vector Float" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")
        PSRDatabase.create_element!(db, "Collection"; label = "Item 1", value_float = [1.5, 2.5, 3.5])
        PSRDatabase.create_element!(db, "Collection"; label = "Item 2", value_float = [10.5, 20.5])

        result = PSRDatabase.read(db, "Collection", "value_float")
        @test result == [[1.5, 2.5, 3.5], [10.5, 20.5]]

        PSRDatabase.close!(db)
    end

    @testset "Generic Read - Set String" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")
        PSRDatabase.create_element!(db, "Collection"; label = "Item 1", tag = ["important", "urgent"])
        PSRDatabase.create_element!(db, "Collection"; label = "Item 2", tag = ["review"])

        result = PSRDatabase.read(db, "Collection", "tag")
        @test length(result) == 2
        @test sort(result[1]) == ["important", "urgent"]
        @test result[2] == ["review"]

        PSRDatabase.close!(db)
    end

    @testset "Generic Read - Empty Result" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")

        # No Collection elements, scalar returns empty array
        result = PSRDatabase.read(db, "Collection", "some_integer")
        @test result == Int64[]

        PSRDatabase.close!(db)
    end

    @testset "Generic Read - Invalid Collection" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        @test_throws PSRDatabase.DatabaseException PSRDatabase.read(db, "NonexistentCollection", "label")

        PSRDatabase.close!(db)
    end

    @testset "Generic Read - Invalid Attribute" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        @test_throws PSRDatabase.DatabaseException PSRDatabase.read(db, "Configuration", "nonexistent_attribute")

        PSRDatabase.close!(db)
    end

    # ============================================================================
    # Generic read_by_id function tests
    # ============================================================================

    @testset "Generic Read by ID - Scalar Integer" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 42)
        PSRDatabase.create_element!(db, "Configuration"; label = "Config 2", integer_attribute = 100)

        @test PSRDatabase.read_by_id(db, "Configuration", "integer_attribute", 1) == 42
        @test PSRDatabase.read_by_id(db, "Configuration", "integer_attribute", 2) == 100

        PSRDatabase.close!(db)
    end

    @testset "Generic Read by ID - Scalar Float" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Config 1", float_attribute = 3.14)
        PSRDatabase.create_element!(db, "Configuration"; label = "Config 2", float_attribute = 2.71)

        @test PSRDatabase.read_by_id(db, "Configuration", "float_attribute", 1) == 3.14
        @test PSRDatabase.read_by_id(db, "Configuration", "float_attribute", 2) == 2.71

        PSRDatabase.close!(db)
    end

    @testset "Generic Read by ID - Scalar String" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Config 1", string_attribute = "hello")
        PSRDatabase.create_element!(db, "Configuration"; label = "Config 2", string_attribute = "world")

        @test PSRDatabase.read_by_id(db, "Configuration", "string_attribute", 1) == "hello"
        @test PSRDatabase.read_by_id(db, "Configuration", "string_attribute", 2) == "world"

        PSRDatabase.close!(db)
    end

    @testset "Generic Read by ID - Vector Integer" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")
        PSRDatabase.create_element!(db, "Collection"; label = "Item 1", value_int = [1, 2, 3])
        PSRDatabase.create_element!(db, "Collection"; label = "Item 2", value_int = [10, 20])

        @test PSRDatabase.read_by_id(db, "Collection", "value_int", 1) == [1, 2, 3]
        @test PSRDatabase.read_by_id(db, "Collection", "value_int", 2) == [10, 20]

        PSRDatabase.close!(db)
    end

    @testset "Generic Read by ID - Vector Float" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")
        PSRDatabase.create_element!(db, "Collection"; label = "Item 1", value_float = [1.5, 2.5, 3.5])
        PSRDatabase.create_element!(db, "Collection"; label = "Item 2", value_float = [10.5, 20.5])

        @test PSRDatabase.read_by_id(db, "Collection", "value_float", 1) == [1.5, 2.5, 3.5]
        @test PSRDatabase.read_by_id(db, "Collection", "value_float", 2) == [10.5, 20.5]

        PSRDatabase.close!(db)
    end

    @testset "Generic Read by ID - Set String" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")
        PSRDatabase.create_element!(db, "Collection"; label = "Item 1", tag = ["important", "urgent"])
        PSRDatabase.create_element!(db, "Collection"; label = "Item 2", tag = ["review"])

        result1 = PSRDatabase.read_by_id(db, "Collection", "tag", 1)
        @test sort(result1) == ["important", "urgent"]
        @test PSRDatabase.read_by_id(db, "Collection", "tag", 2) == ["review"]

        PSRDatabase.close!(db)
    end

    @testset "Generic Read by ID - Scalar Not Found" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 42)

        # Scalar returns nothing for non-existent ID
        @test PSRDatabase.read_by_id(db, "Configuration", "integer_attribute", 999) === nothing
        @test PSRDatabase.read_by_id(db, "Configuration", "float_attribute", 999) === nothing
        @test PSRDatabase.read_by_id(db, "Configuration", "string_attribute", 999) === nothing

        PSRDatabase.close!(db)
    end

    @testset "Generic Read by ID - Vector Empty" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")
        PSRDatabase.create_element!(db, "Collection"; label = "Item 1")  # No vector data

        # Vector returns empty array for element without data
        @test PSRDatabase.read_by_id(db, "Collection", "value_int", 1) == Int64[]

        PSRDatabase.close!(db)
    end

    @testset "Generic Read by ID - Invalid Collection" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        @test_throws PSRDatabase.DatabaseException PSRDatabase.read_by_id(
            db,
            "NonexistentCollection",
            "label",
            1,
        )

        PSRDatabase.close!(db)
    end

    @testset "Generic Read by ID - Invalid Attribute" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        @test_throws PSRDatabase.DatabaseException PSRDatabase.read_by_id(
            db,
            "Configuration",
            "nonexistent_attribute",
            1,
        )

        PSRDatabase.close!(db)
    end
end

end
