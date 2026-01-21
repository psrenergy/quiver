module TestRead

using Margaux
using Test

include("fixture.jl")

@testset "Read" begin
    @testset "Scalar Attributes" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration";
            label = "Config 1",
            integer_attribute = 42,
            float_attribute = 3.14,
            string_attribute = "hello",
        )
        Margaux.create_element!(db, "Configuration";
            label = "Config 2",
            integer_attribute = 100,
            float_attribute = 2.71,
            string_attribute = "world",
        )

        @test Margaux.read_scalar_strings(db, "Configuration", "label") == ["Config 1", "Config 2"]
        @test Margaux.read_scalar_integers(db, "Configuration", "integer_attribute") == [42, 100]
        @test Margaux.read_scalar_floats(db, "Configuration", "float_attribute") == [3.14, 2.71]
        @test Margaux.read_scalar_strings(db, "Configuration", "string_attribute") == ["hello", "world"]

        Margaux.close!(db)
    end

    @testset "Collections" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Test Config")
        Margaux.create_element!(db, "Collection"; label = "Item 1", some_integer = 10, some_float = 1.5)
        Margaux.create_element!(db, "Collection"; label = "Item 2", some_integer = 20, some_float = 2.5)

        @test Margaux.read_scalar_strings(db, "Collection", "label") == ["Item 1", "Item 2"]
        @test Margaux.read_scalar_integers(db, "Collection", "some_integer") == [10, 20]
        @test Margaux.read_scalar_floats(db, "Collection", "some_float") == [1.5, 2.5]

        Margaux.close!(db)
    end

    @testset "Empty Result" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Test Config")

        # No Collection elements created
        @test Margaux.read_scalar_strings(db, "Collection", "label") == String[]
        @test Margaux.read_scalar_integers(db, "Collection", "some_integer") == Int64[]
        @test Margaux.read_scalar_floats(db, "Collection", "some_float") == Float64[]

        Margaux.close!(db)
    end

    @testset "Vector Attributes" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Test Config")
        Margaux.create_element!(
            db,
            "Collection";
            label = "Item 1",
            value_int = [1, 2, 3],
            value_float = [1.5, 2.5, 3.5],
        )
        Margaux.create_element!(
            db,
            "Collection";
            label = "Item 2",
            value_int = [10, 20],
            value_float = [10.5, 20.5],
        )

        @test Margaux.read_vector_integers(db, "Collection", "value_int") == [[1, 2, 3], [10, 20]]
        @test Margaux.read_vector_floats(db, "Collection", "value_float") == [[1.5, 2.5, 3.5], [10.5, 20.5]]

        Margaux.close!(db)
    end

    @testset "Vector Empty Result" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Test Config")

        # No Collection elements created
        @test Margaux.read_vector_integers(db, "Collection", "value_int") == Vector{Int64}[]
        @test Margaux.read_vector_floats(db, "Collection", "value_float") == Vector{Float64}[]

        Margaux.close!(db)
    end

    @testset "Vector Only Returns Elements With Data" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Test Config")

        # Create element with vectors
        Margaux.create_element!(db, "Collection"; label = "Item 1", value_int = [1, 2, 3])
        # Create element without vectors (no vector data inserted)
        Margaux.create_element!(db, "Collection"; label = "Item 2")
        # Create another element with vectors
        Margaux.create_element!(db, "Collection"; label = "Item 3", value_int = [4, 5])

        # Only elements with vector data are returned
        result = Margaux.read_vector_integers(db, "Collection", "value_int")
        @test length(result) == 2
        @test result[1] == [1, 2, 3]
        @test result[2] == [4, 5]

        Margaux.close!(db)
    end

    @testset "Set Attributes" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Test Config")
        Margaux.create_element!(db, "Collection"; label = "Item 1", tag = ["important", "urgent"])
        Margaux.create_element!(db, "Collection"; label = "Item 2", tag = ["review"])

        result = Margaux.read_set_strings(db, "Collection", "tag")
        @test length(result) == 2
        # Sets are unordered, so sort before comparison
        @test sort(result[1]) == ["important", "urgent"]
        @test result[2] == ["review"]

        Margaux.close!(db)
    end

    @testset "Set Empty Result" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Test Config")

        # No Collection elements created
        @test Margaux.read_set_strings(db, "Collection", "tag") == Vector{String}[]

        Margaux.close!(db)
    end

    @testset "Set Only Returns Elements With Data" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Test Config")

        # Create element with set data
        Margaux.create_element!(db, "Collection"; label = "Item 1", tag = ["important"])
        # Create element without set data
        Margaux.create_element!(db, "Collection"; label = "Item 2")
        # Create another element with set data
        Margaux.create_element!(db, "Collection"; label = "Item 3", tag = ["urgent", "review"])

        # Only elements with set data are returned
        result = Margaux.read_set_strings(db, "Collection", "tag")
        @test length(result) == 2

        Margaux.close!(db)
    end

    @testset "Set and Read Scalar Relations" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Test Config")

        # Create parents
        Margaux.create_element!(db, "Parent"; label = "Parent 1")
        Margaux.create_element!(db, "Parent"; label = "Parent 2")

        # Create children
        Margaux.create_element!(db, "Child"; label = "Child 1")
        Margaux.create_element!(db, "Child"; label = "Child 2")
        Margaux.create_element!(db, "Child"; label = "Child 3")

        # Set relations
        Margaux.set_scalar_relation!(db, "Child", "parent_id", "Child 1", "Parent 1")
        Margaux.set_scalar_relation!(db, "Child", "parent_id", "Child 3", "Parent 2")

        # Read relations
        labels = Margaux.read_scalar_relation(db, "Child", "parent_id")
        @test length(labels) == 3
        @test labels[1] == "Parent 1"
        @test labels[2] === nothing  # Child 2 has no parent
        @test labels[3] == "Parent 2"

        Margaux.close!(db)
    end

    @testset "Scalar Relations Self-Reference" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Test Config")

        # Create children (Child references itself via sibling_id)
        Margaux.create_element!(db, "Child"; label = "Child 1")
        Margaux.create_element!(db, "Child"; label = "Child 2")

        # Set sibling relation (self-reference)
        Margaux.set_scalar_relation!(db, "Child", "sibling_id", "Child 1", "Child 2")

        # Read sibling relations
        labels = Margaux.read_scalar_relation(db, "Child", "sibling_id")
        @test length(labels) == 2
        @test labels[1] == "Child 2"  # Child 1's sibling is Child 2
        @test labels[2] === nothing   # Child 2 has no sibling set

        Margaux.close!(db)
    end

    @testset "Scalar Relations Empty Result" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Test Config")

        # No Child elements created
        labels = Margaux.read_scalar_relation(db, "Child", "parent_id")
        @test labels == Union{String, Nothing}[]

        Margaux.close!(db)
    end

    # Read by ID tests

    @testset "Scalar Integers by ID" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 42)
        Margaux.create_element!(db, "Configuration"; label = "Config 2", integer_attribute = 100)

        @test Margaux.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", 1) == 42
        @test Margaux.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", 2) == 100
        @test Margaux.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", 999) === nothing

        Margaux.close!(db)
    end

    @testset "Scalar Floats by ID" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Config 1", float_attribute = 3.14)
        Margaux.create_element!(db, "Configuration"; label = "Config 2", float_attribute = 2.71)

        @test Margaux.read_scalar_floats_by_id(db, "Configuration", "float_attribute", 1) == 3.14
        @test Margaux.read_scalar_floats_by_id(db, "Configuration", "float_attribute", 2) == 2.71

        Margaux.close!(db)
    end

    @testset "Scalar Strings by ID" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Config 1", string_attribute = "hello")
        Margaux.create_element!(db, "Configuration"; label = "Config 2", string_attribute = "world")

        @test Margaux.read_scalar_strings_by_id(db, "Configuration", "string_attribute", 1) == "hello"
        @test Margaux.read_scalar_strings_by_id(db, "Configuration", "string_attribute", 2) == "world"

        Margaux.close!(db)
    end

    @testset "Vector Integers by ID" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Test Config")
        Margaux.create_element!(db, "Collection"; label = "Item 1", value_int = [1, 2, 3])
        Margaux.create_element!(db, "Collection"; label = "Item 2", value_int = [10, 20])

        @test Margaux.read_vector_integers_by_id(db, "Collection", "value_int", 1) == [1, 2, 3]
        @test Margaux.read_vector_integers_by_id(db, "Collection", "value_int", 2) == [10, 20]

        Margaux.close!(db)
    end

    @testset "Vector Floats by ID" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Test Config")
        Margaux.create_element!(db, "Collection"; label = "Item 1", value_float = [1.5, 2.5, 3.5])

        @test Margaux.read_vector_floats_by_id(db, "Collection", "value_float", 1) == [1.5, 2.5, 3.5]

        Margaux.close!(db)
    end

    @testset "Set Strings by ID" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Test Config")
        Margaux.create_element!(db, "Collection"; label = "Item 1", tag = ["important", "urgent"])
        Margaux.create_element!(db, "Collection"; label = "Item 2", tag = ["review"])

        result1 = Margaux.read_set_strings_by_id(db, "Collection", "tag", 1)
        @test sort(result1) == ["important", "urgent"]
        @test Margaux.read_set_strings_by_id(db, "Collection", "tag", 2) == ["review"]

        Margaux.close!(db)
    end

    @testset "Vector by ID Empty" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Test Config")
        Margaux.create_element!(db, "Collection"; label = "Item 1")  # No vector data

        @test Margaux.read_vector_integers_by_id(db, "Collection", "value_int", 1) == Int64[]

        Margaux.close!(db)
    end

    # Read element IDs tests

    @testset "Element IDs" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 42)
        Margaux.create_element!(db, "Configuration"; label = "Config 2", integer_attribute = 100)
        Margaux.create_element!(db, "Configuration"; label = "Config 3", integer_attribute = 200)

        ids = Margaux.read_element_ids(db, "Configuration")
        @test length(ids) == 3
        @test ids[1] == 1
        @test ids[2] == 2
        @test ids[3] == 3

        Margaux.close!(db)
    end

    @testset "Element IDs Empty" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Test Config")

        # No Collection elements created
        ids = Margaux.read_element_ids(db, "Collection")
        @test ids == Int64[]

        Margaux.close!(db)
    end

    # Get attribute type tests

    @testset "Get Attribute Type - Scalar Integer" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        result = Margaux.get_attribute_type(db, "Configuration", "integer_attribute")
        @test result.data_structure == Margaux.MARGAUX_DATA_STRUCTURE_SCALAR
        @test result.data_type == Margaux.MARGAUX_DATA_TYPE_INTEGER

        Margaux.close!(db)
    end

    @testset "Get Attribute Type - Scalar Real" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        result = Margaux.get_attribute_type(db, "Configuration", "float_attribute")
        @test result.data_structure == Margaux.MARGAUX_DATA_STRUCTURE_SCALAR
        @test result.data_type == Margaux.MARGAUX_DATA_TYPE_FLOAT

        Margaux.close!(db)
    end

    @testset "Get Attribute Type - Scalar Text" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        result = Margaux.get_attribute_type(db, "Configuration", "string_attribute")
        @test result.data_structure == Margaux.MARGAUX_DATA_STRUCTURE_SCALAR
        @test result.data_type == Margaux.MARGAUX_DATA_TYPE_STRING

        Margaux.close!(db)
    end

    @testset "Get Attribute Type - Vector Integer" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        result = Margaux.get_attribute_type(db, "Collection", "value_int")
        @test result.data_structure == Margaux.MARGAUX_DATA_STRUCTURE_VECTOR
        @test result.data_type == Margaux.MARGAUX_DATA_TYPE_INTEGER

        Margaux.close!(db)
    end

    @testset "Get Attribute Type - Vector Real" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        result = Margaux.get_attribute_type(db, "Collection", "value_float")
        @test result.data_structure == Margaux.MARGAUX_DATA_STRUCTURE_VECTOR
        @test result.data_type == Margaux.MARGAUX_DATA_TYPE_FLOAT

        Margaux.close!(db)
    end

    @testset "Get Attribute Type - Set Text" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        result = Margaux.get_attribute_type(db, "Collection", "tag")
        @test result.data_structure == Margaux.MARGAUX_DATA_STRUCTURE_SET
        @test result.data_type == Margaux.MARGAUX_DATA_TYPE_STRING

        Margaux.close!(db)
    end

    @testset "Get Attribute Type - Error on Nonexistent" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        @test_throws Margaux.DatabaseException Margaux.get_attribute_type(db, "Configuration", "nonexistent")

        Margaux.close!(db)
    end

    # Error handling tests

    @testset "Invalid Collection" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        @test_throws Margaux.DatabaseException Margaux.read_scalar_strings(db, "NonexistentCollection", "label")
        @test_throws Margaux.DatabaseException Margaux.read_scalar_integers(
            db,
            "NonexistentCollection",
            "value",
        )
        @test_throws Margaux.DatabaseException Margaux.read_scalar_floats(db, "NonexistentCollection", "value")

        Margaux.close!(db)
    end

    @testset "Invalid Attribute" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        @test_throws Margaux.DatabaseException Margaux.read_scalar_strings(
            db,
            "Configuration",
            "nonexistent_attr",
        )
        @test_throws Margaux.DatabaseException Margaux.read_scalar_integers(
            db,
            "Configuration",
            "nonexistent_attr",
        )
        @test_throws Margaux.DatabaseException Margaux.read_scalar_floats(
            db,
            "Configuration",
            "nonexistent_attr",
        )

        Margaux.close!(db)
    end

    @testset "ID Not Found" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 42)

        # Read by ID that doesn't exist returns nothing
        @test Margaux.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", 999) === nothing
        @test Margaux.read_scalar_floats_by_id(db, "Configuration", "float_attribute", 999) === nothing
        @test Margaux.read_scalar_strings_by_id(db, "Configuration", "string_attribute", 999) === nothing

        Margaux.close!(db)
    end

    @testset "Vector Invalid Collection" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Test Config")

        @test_throws Margaux.DatabaseException Margaux.read_vector_integers(
            db,
            "NonexistentCollection",
            "value_int",
        )
        @test_throws Margaux.DatabaseException Margaux.read_vector_floats(
            db,
            "NonexistentCollection",
            "value_float",
        )

        Margaux.close!(db)
    end

    @testset "Set Invalid Collection" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Test Config")

        @test_throws Margaux.DatabaseException Margaux.read_set_strings(db, "NonexistentCollection", "tag")

        Margaux.close!(db)
    end

    @testset "Element IDs Invalid Collection" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        @test_throws Margaux.DatabaseException Margaux.read_element_ids(db, "NonexistentCollection")

        Margaux.close!(db)
    end

    @testset "Scalar Relation Invalid Collection" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Test Config")

        @test_throws Margaux.DatabaseException Margaux.read_scalar_relation(
            db,
            "NonexistentCollection",
            "parent_id",
        )

        Margaux.close!(db)
    end

    # ============================================================================
    # Generic read function tests
    # ============================================================================

    @testset "Generic Read - Scalar Integer" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 42)
        Margaux.create_element!(db, "Configuration"; label = "Config 2", integer_attribute = 100)

        result = Margaux.read(db, "Configuration", "integer_attribute")
        @test result == [42, 100]
        @test eltype(result) == Int64

        Margaux.close!(db)
    end

    @testset "Generic Read - Scalar Float" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Config 1", float_attribute = 3.14)
        Margaux.create_element!(db, "Configuration"; label = "Config 2", float_attribute = 2.71)

        result = Margaux.read(db, "Configuration", "float_attribute")
        @test result == [3.14, 2.71]
        @test eltype(result) == Float64

        Margaux.close!(db)
    end

    @testset "Generic Read - Scalar String" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Config 1", string_attribute = "hello")
        Margaux.create_element!(db, "Configuration"; label = "Config 2", string_attribute = "world")

        result = Margaux.read(db, "Configuration", "string_attribute")
        @test result == ["hello", "world"]
        @test eltype(result) == String

        Margaux.close!(db)
    end

    @testset "Generic Read - Vector Integer" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Test Config")
        Margaux.create_element!(db, "Collection"; label = "Item 1", value_int = [1, 2, 3])
        Margaux.create_element!(db, "Collection"; label = "Item 2", value_int = [10, 20])

        result = Margaux.read(db, "Collection", "value_int")
        @test result == [[1, 2, 3], [10, 20]]

        Margaux.close!(db)
    end

    @testset "Generic Read - Vector Float" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Test Config")
        Margaux.create_element!(db, "Collection"; label = "Item 1", value_float = [1.5, 2.5, 3.5])
        Margaux.create_element!(db, "Collection"; label = "Item 2", value_float = [10.5, 20.5])

        result = Margaux.read(db, "Collection", "value_float")
        @test result == [[1.5, 2.5, 3.5], [10.5, 20.5]]

        Margaux.close!(db)
    end

    @testset "Generic Read - Set String" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Test Config")
        Margaux.create_element!(db, "Collection"; label = "Item 1", tag = ["important", "urgent"])
        Margaux.create_element!(db, "Collection"; label = "Item 2", tag = ["review"])

        result = Margaux.read(db, "Collection", "tag")
        @test length(result) == 2
        @test sort(result[1]) == ["important", "urgent"]
        @test result[2] == ["review"]

        Margaux.close!(db)
    end

    @testset "Generic Read - Empty Result" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Test Config")

        # No Collection elements, scalar returns empty array
        result = Margaux.read(db, "Collection", "some_integer")
        @test result == Int64[]

        Margaux.close!(db)
    end

    @testset "Generic Read - Invalid Collection" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        @test_throws Margaux.DatabaseException Margaux.read(db, "NonexistentCollection", "label")

        Margaux.close!(db)
    end

    @testset "Generic Read - Invalid Attribute" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        @test_throws Margaux.DatabaseException Margaux.read(db, "Configuration", "nonexistent_attribute")

        Margaux.close!(db)
    end

    # ============================================================================
    # Generic read_by_id function tests
    # ============================================================================

    @testset "Generic Read by ID - Scalar Integer" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 42)
        Margaux.create_element!(db, "Configuration"; label = "Config 2", integer_attribute = 100)

        @test Margaux.read_by_id(db, "Configuration", "integer_attribute", 1) == 42
        @test Margaux.read_by_id(db, "Configuration", "integer_attribute", 2) == 100

        Margaux.close!(db)
    end

    @testset "Generic Read by ID - Scalar Float" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Config 1", float_attribute = 3.14)
        Margaux.create_element!(db, "Configuration"; label = "Config 2", float_attribute = 2.71)

        @test Margaux.read_by_id(db, "Configuration", "float_attribute", 1) == 3.14
        @test Margaux.read_by_id(db, "Configuration", "float_attribute", 2) == 2.71

        Margaux.close!(db)
    end

    @testset "Generic Read by ID - Scalar String" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Config 1", string_attribute = "hello")
        Margaux.create_element!(db, "Configuration"; label = "Config 2", string_attribute = "world")

        @test Margaux.read_by_id(db, "Configuration", "string_attribute", 1) == "hello"
        @test Margaux.read_by_id(db, "Configuration", "string_attribute", 2) == "world"

        Margaux.close!(db)
    end

    @testset "Generic Read by ID - Vector Integer" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Test Config")
        Margaux.create_element!(db, "Collection"; label = "Item 1", value_int = [1, 2, 3])
        Margaux.create_element!(db, "Collection"; label = "Item 2", value_int = [10, 20])

        @test Margaux.read_by_id(db, "Collection", "value_int", 1) == [1, 2, 3]
        @test Margaux.read_by_id(db, "Collection", "value_int", 2) == [10, 20]

        Margaux.close!(db)
    end

    @testset "Generic Read by ID - Vector Float" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Test Config")
        Margaux.create_element!(db, "Collection"; label = "Item 1", value_float = [1.5, 2.5, 3.5])
        Margaux.create_element!(db, "Collection"; label = "Item 2", value_float = [10.5, 20.5])

        @test Margaux.read_by_id(db, "Collection", "value_float", 1) == [1.5, 2.5, 3.5]
        @test Margaux.read_by_id(db, "Collection", "value_float", 2) == [10.5, 20.5]

        Margaux.close!(db)
    end

    @testset "Generic Read by ID - Set String" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Test Config")
        Margaux.create_element!(db, "Collection"; label = "Item 1", tag = ["important", "urgent"])
        Margaux.create_element!(db, "Collection"; label = "Item 2", tag = ["review"])

        result1 = Margaux.read_by_id(db, "Collection", "tag", 1)
        @test sort(result1) == ["important", "urgent"]
        @test Margaux.read_by_id(db, "Collection", "tag", 2) == ["review"]

        Margaux.close!(db)
    end

    @testset "Generic Read by ID - Scalar Not Found" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 42)

        # Scalar returns nothing for non-existent ID
        @test Margaux.read_by_id(db, "Configuration", "integer_attribute", 999) === nothing
        @test Margaux.read_by_id(db, "Configuration", "float_attribute", 999) === nothing
        @test Margaux.read_by_id(db, "Configuration", "string_attribute", 999) === nothing

        Margaux.close!(db)
    end

    @testset "Generic Read by ID - Vector Empty" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Test Config")
        Margaux.create_element!(db, "Collection"; label = "Item 1")  # No vector data

        # Vector returns empty array for element without data
        @test Margaux.read_by_id(db, "Collection", "value_int", 1) == Int64[]

        Margaux.close!(db)
    end

    @testset "Generic Read by ID - Invalid Collection" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        @test_throws Margaux.DatabaseException Margaux.read_by_id(
            db,
            "NonexistentCollection",
            "label",
            1,
        )

        Margaux.close!(db)
    end

    @testset "Generic Read by ID - Invalid Attribute" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        @test_throws Margaux.DatabaseException Margaux.read_by_id(
            db,
            "Configuration",
            "nonexistent_attribute",
            1,
        )

        Margaux.close!(db)
    end
end

end
