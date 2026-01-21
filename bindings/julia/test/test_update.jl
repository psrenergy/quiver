module TestUpdate

using Margaux
using Test

include("fixture.jl")

@testset "Update" begin
    @testset "Element Single Scalar" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        # Create elements
        Margaux.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 100)
        Margaux.create_element!(db, "Configuration"; label = "Config 2", integer_attribute = 200)

        # Update single attribute
        Margaux.update_element!(db, "Configuration", Int64(1); integer_attribute = 999)

        # Verify update
        value = Margaux.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", Int64(1))
        @test value == 999

        # Verify label unchanged
        label = Margaux.read_scalar_strings_by_id(db, "Configuration", "label", Int64(1))
        @test label == "Config 1"

        Margaux.close!(db)
    end

    @testset "Element Multiple Scalars" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        # Create element
        Margaux.create_element!(
            db,
            "Configuration";
            label = "Config 1",
            integer_attribute = 100,
            float_attribute = 1.5,
        )

        # Update multiple attributes at once
        Margaux.update_element!(db, "Configuration", Int64(1); integer_attribute = 500, float_attribute = 9.9)

        # Verify updates
        integer_value = Margaux.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", Int64(1))
        @test integer_value == 500

        float_value = Margaux.read_scalar_floats_by_id(db, "Configuration", "float_attribute", Int64(1))
        @test float_value == 9.9

        # Verify label unchanged
        label = Margaux.read_scalar_strings_by_id(db, "Configuration", "label", Int64(1))
        @test label == "Config 1"

        Margaux.close!(db)
    end

    @testset "Element Other Elements Unchanged" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        # Create multiple elements
        Margaux.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 100)
        Margaux.create_element!(db, "Configuration"; label = "Config 2", integer_attribute = 200)
        Margaux.create_element!(db, "Configuration"; label = "Config 3", integer_attribute = 300)

        # Update only element 2
        Margaux.update_element!(db, "Configuration", Int64(2); integer_attribute = 999)

        # Verify element 2 updated
        value2 = Margaux.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", Int64(2))
        @test value2 == 999

        # Verify elements 1 and 3 unchanged
        value1 = Margaux.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", Int64(1))
        @test value1 == 100

        value3 = Margaux.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", Int64(3))
        @test value3 == 300

        Margaux.close!(db)
    end

    @testset "Element Arrays Ignored" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Test Config")

        # Create element with vector data
        Margaux.create_element!(db, "Collection";
            label = "Item 1",
            some_integer = 10,
            value_int = [1, 2, 3],
        )

        # Try to update with array values (should be ignored)
        Margaux.update_element!(db, "Collection", Int64(1); some_integer = 999, value_int = [7, 8, 9])

        # Verify scalar was updated
        integer_value = Margaux.read_scalar_integers_by_id(db, "Collection", "some_integer", Int64(1))
        @test integer_value == 999

        # Verify vector was NOT updated (arrays ignored in update_element)
        vec_values = Margaux.read_vector_integers_by_id(db, "Collection", "value_int", Int64(1))
        @test vec_values == [1, 2, 3]

        Margaux.close!(db)
    end

    @testset "Element Using Element Builder" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        # Create element
        Margaux.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 100)

        # Update using Element builder
        e = Margaux.Element()
        e["integer_attribute"] = Int64(777)
        Margaux.update_element!(db, "Configuration", Int64(1), e)

        # Verify update
        value = Margaux.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", Int64(1))
        @test value == 777

        Margaux.close!(db)
    end

    # Error handling tests

    @testset "Invalid Collection" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 100)

        @test_throws Margaux.DatabaseException Margaux.update_element!(
            db,
            "NonexistentCollection",
            Int64(1);
            integer_attribute = 999,
        )

        Margaux.close!(db)
    end

    @testset "Invalid Element ID" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 100)

        # Update element that doesn't exist - should not throw but also should not affect anything
        Margaux.update_element!(db, "Configuration", Int64(999); integer_attribute = 500)

        # Verify original element unchanged
        value = Margaux.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", Int64(1))
        @test value == 100

        Margaux.close!(db)
    end

    @testset "Invalid Attribute" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 100)

        @test_throws Margaux.DatabaseException Margaux.update_element!(
            db,
            "Configuration",
            Int64(1);
            nonexistent_attribute = 999,
        )

        Margaux.close!(db)
    end

    @testset "String Attribute" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Config 1", string_attribute = "original")

        Margaux.update_element!(db, "Configuration", Int64(1); string_attribute = "updated")

        value = Margaux.read_scalar_strings_by_id(db, "Configuration", "string_attribute", Int64(1))
        @test value == "updated"

        Margaux.close!(db)
    end

    @testset "Float Attribute" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Config 1", float_attribute = 1.5)

        Margaux.update_element!(db, "Configuration", Int64(1); float_attribute = 99.99)

        value = Margaux.read_scalar_floats_by_id(db, "Configuration", "float_attribute", Int64(1))
        @test value == 99.99

        Margaux.close!(db)
    end

    @testset "Multiple Elements Sequentially" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 100)
        Margaux.create_element!(db, "Configuration"; label = "Config 2", integer_attribute = 200)

        # Update both elements
        Margaux.update_element!(db, "Configuration", Int64(1); integer_attribute = 111)
        Margaux.update_element!(db, "Configuration", Int64(2); integer_attribute = 222)

        @test Margaux.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", Int64(1)) == 111
        @test Margaux.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", Int64(2)) == 222

        Margaux.close!(db)
    end

    # ============================================================================
    # Scalar update functions tests
    # ============================================================================

    @testset "Scalar Integer" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 42)

        # Basic update
        Margaux.update_scalar_integer!(db, "Configuration", "integer_attribute", Int64(1), 100)
        value = Margaux.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", Int64(1))
        @test value == 100

        # Update to 0
        Margaux.update_scalar_integer!(db, "Configuration", "integer_attribute", Int64(1), 0)
        value = Margaux.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", Int64(1))
        @test value == 0

        # Update to negative
        Margaux.update_scalar_integer!(db, "Configuration", "integer_attribute", Int64(1), -999)
        value = Margaux.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", Int64(1))
        @test value == -999

        Margaux.close!(db)
    end

    @testset "Scalar Integer Multiple Elements" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 42)
        Margaux.create_element!(db, "Configuration"; label = "Config 2", integer_attribute = 100)

        # Update only first element
        Margaux.update_scalar_integer!(db, "Configuration", "integer_attribute", Int64(1), 999)

        # Verify first element changed
        @test Margaux.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", Int64(1)) == 999

        # Verify second element unchanged
        @test Margaux.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", Int64(2)) == 100

        Margaux.close!(db)
    end

    @testset "Scalar Float" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Config 1", float_attribute = 3.14)

        # Basic update
        Margaux.update_scalar_float!(db, "Configuration", "float_attribute", Int64(1), 2.71)
        value = Margaux.read_scalar_floats_by_id(db, "Configuration", "float_attribute", Int64(1))
        @test value == 2.71

        # Update to 0.0
        Margaux.update_scalar_float!(db, "Configuration", "float_attribute", Int64(1), 0.0)
        value = Margaux.read_scalar_floats_by_id(db, "Configuration", "float_attribute", Int64(1))
        @test value == 0.0

        # Precision test
        Margaux.update_scalar_float!(db, "Configuration", "float_attribute", Int64(1), 1.23456789012345)
        value = Margaux.read_scalar_floats_by_id(db, "Configuration", "float_attribute", Int64(1))
        @test value ≈ 1.23456789012345

        Margaux.close!(db)
    end

    @testset "Scalar String" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Config 1", string_attribute = "hello")

        # Basic update
        Margaux.update_scalar_string!(db, "Configuration", "string_attribute", Int64(1), "world")
        value = Margaux.read_scalar_strings_by_id(db, "Configuration", "string_attribute", Int64(1))
        @test value == "world"

        # Update to empty string
        Margaux.update_scalar_string!(db, "Configuration", "string_attribute", Int64(1), "")
        value = Margaux.read_scalar_strings_by_id(db, "Configuration", "string_attribute", Int64(1))
        @test value == ""

        # Unicode support
        Margaux.update_scalar_string!(db, "Configuration", "string_attribute", Int64(1), "日本語テスト")
        value = Margaux.read_scalar_strings_by_id(db, "Configuration", "string_attribute", Int64(1))
        @test value == "日本語テスト"

        Margaux.close!(db)
    end

    # ============================================================================
    # Vector update functions tests
    # ============================================================================

    @testset "Vector Integers" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Test Config")
        Margaux.create_element!(db, "Collection"; label = "Item 1", value_int = [1, 2, 3])

        # Replace existing vector
        Margaux.update_vector_integers!(db, "Collection", "value_int", Int64(1), [10, 20, 30, 40])
        values = Margaux.read_vector_integers_by_id(db, "Collection", "value_int", Int64(1))
        @test values == [10, 20, 30, 40]

        # Update to smaller vector
        Margaux.update_vector_integers!(db, "Collection", "value_int", Int64(1), [100])
        values = Margaux.read_vector_integers_by_id(db, "Collection", "value_int", Int64(1))
        @test values == [100]

        # Update to empty vector
        Margaux.update_vector_integers!(db, "Collection", "value_int", Int64(1), Int64[])
        values = Margaux.read_vector_integers_by_id(db, "Collection", "value_int", Int64(1))
        @test isempty(values)

        Margaux.close!(db)
    end

    @testset "Vector Integers From Empty" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Test Config")
        Margaux.create_element!(db, "Collection"; label = "Item 1")

        # Verify initially empty
        values = Margaux.read_vector_integers_by_id(db, "Collection", "value_int", Int64(1))
        @test isempty(values)

        # Update to non-empty
        Margaux.update_vector_integers!(db, "Collection", "value_int", Int64(1), [1, 2, 3])
        values = Margaux.read_vector_integers_by_id(db, "Collection", "value_int", Int64(1))
        @test values == [1, 2, 3]

        Margaux.close!(db)
    end

    @testset "Vector Integers Multiple Elements" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Test Config")
        Margaux.create_element!(db, "Collection"; label = "Item 1", value_int = [1, 2, 3])
        Margaux.create_element!(db, "Collection"; label = "Item 2", value_int = [10, 20])

        # Update only first element
        Margaux.update_vector_integers!(db, "Collection", "value_int", Int64(1), [100, 200])

        # Verify first element changed
        @test Margaux.read_vector_integers_by_id(db, "Collection", "value_int", Int64(1)) == [100, 200]

        # Verify second element unchanged
        @test Margaux.read_vector_integers_by_id(db, "Collection", "value_int", Int64(2)) == [10, 20]

        Margaux.close!(db)
    end

    @testset "Vector Floats" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Test Config")
        Margaux.create_element!(db, "Collection"; label = "Item 1", value_float = [1.5, 2.5, 3.5])

        # Replace existing vector
        Margaux.update_vector_floats!(db, "Collection", "value_float", Int64(1), [10.5, 20.5])
        values = Margaux.read_vector_floats_by_id(db, "Collection", "value_float", Int64(1))
        @test values == [10.5, 20.5]

        # Precision test
        Margaux.update_vector_floats!(db, "Collection", "value_float", Int64(1), [1.23456789, 9.87654321])
        values = Margaux.read_vector_floats_by_id(db, "Collection", "value_float", Int64(1))
        @test values ≈ [1.23456789, 9.87654321]

        # Update to empty vector
        Margaux.update_vector_floats!(db, "Collection", "value_float", Int64(1), Float64[])
        values = Margaux.read_vector_floats_by_id(db, "Collection", "value_float", Int64(1))
        @test isempty(values)

        Margaux.close!(db)
    end

    # ============================================================================
    # Set update functions tests
    # ============================================================================

    @testset "Set Strings" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Test Config")
        Margaux.create_element!(db, "Collection"; label = "Item 1", tag = ["important", "urgent"])

        # Replace existing set
        Margaux.update_set_strings!(db, "Collection", "tag", Int64(1), ["new_tag1", "new_tag2", "new_tag3"])
        values = Margaux.read_set_strings_by_id(db, "Collection", "tag", Int64(1))
        @test sort(values) == sort(["new_tag1", "new_tag2", "new_tag3"])

        # Update to single element
        Margaux.update_set_strings!(db, "Collection", "tag", Int64(1), ["single_tag"])
        values = Margaux.read_set_strings_by_id(db, "Collection", "tag", Int64(1))
        @test values == ["single_tag"]

        # Update to empty set
        Margaux.update_set_strings!(db, "Collection", "tag", Int64(1), String[])
        values = Margaux.read_set_strings_by_id(db, "Collection", "tag", Int64(1))
        @test isempty(values)

        Margaux.close!(db)
    end

    @testset "Set Strings From Empty" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Test Config")
        Margaux.create_element!(db, "Collection"; label = "Item 1")

        # Verify initially empty
        values = Margaux.read_set_strings_by_id(db, "Collection", "tag", Int64(1))
        @test isempty(values)

        # Update to non-empty
        Margaux.update_set_strings!(db, "Collection", "tag", Int64(1), ["important", "urgent"])
        values = Margaux.read_set_strings_by_id(db, "Collection", "tag", Int64(1))
        @test sort(values) == sort(["important", "urgent"])

        Margaux.close!(db)
    end

    @testset "Set Strings Multiple Elements" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Test Config")
        Margaux.create_element!(db, "Collection"; label = "Item 1", tag = ["important"])
        Margaux.create_element!(db, "Collection"; label = "Item 2", tag = ["urgent", "review"])

        # Update only first element
        Margaux.update_set_strings!(db, "Collection", "tag", Int64(1), ["updated"])

        # Verify first element changed
        @test Margaux.read_set_strings_by_id(db, "Collection", "tag", Int64(1)) == ["updated"]

        # Verify second element unchanged
        @test sort(Margaux.read_set_strings_by_id(db, "Collection", "tag", Int64(2))) == sort(["urgent", "review"])

        Margaux.close!(db)
    end

    @testset "Set Strings Unicode" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Test Config")
        Margaux.create_element!(db, "Collection"; label = "Item 1", tag = ["tag1"])

        # Unicode support
        Margaux.update_set_strings!(db, "Collection", "tag", Int64(1), ["日本語", "中文", "한국어"])
        values = Margaux.read_set_strings_by_id(db, "Collection", "tag", Int64(1))
        @test sort(values) == sort(["日本語", "中文", "한국어"])

        Margaux.close!(db)
    end

    # ============================================================================
    # Error handling tests for new update functions
    # ============================================================================

    @testset "Scalar Integer Invalid Collection" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        @test_throws Margaux.DatabaseException Margaux.update_scalar_integer!(
            db,
            "NonexistentCollection",
            "integer_attribute",
            Int64(1),
            42,
        )

        Margaux.close!(db)
    end

    @testset "Scalar Integer Invalid Attribute" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 42)

        @test_throws Margaux.DatabaseException Margaux.update_scalar_integer!(
            db,
            "Configuration",
            "nonexistent_attribute",
            Int64(1),
            100,
        )

        Margaux.close!(db)
    end

    @testset "Vector Integers Invalid Collection" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Test Config")

        @test_throws Margaux.DatabaseException Margaux.update_vector_integers!(
            db,
            "NonexistentCollection",
            "value_int",
            Int64(1),
            [1, 2, 3],
        )

        Margaux.close!(db)
    end

    @testset "Set Strings Invalid Collection" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Margaux.from_schema(":memory:", path_schema)

        Margaux.create_element!(db, "Configuration"; label = "Test Config")

        @test_throws Margaux.DatabaseException Margaux.update_set_strings!(
            db,
            "NonexistentCollection",
            "tag",
            Int64(1),
            ["tag1"],
        )

        Margaux.close!(db)
    end
end

end
