module TestUpdate

using PSRDatabase
using Test

include("fixture.jl")

@testset "Update" begin
    @testset "Element Single Scalar" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        # Create elements
        PSRDatabase.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 100)
        PSRDatabase.create_element!(db, "Configuration"; label = "Config 2", integer_attribute = 200)

        # Update single attribute
        PSRDatabase.update_element!(db, "Configuration", Int64(1); integer_attribute = 999)

        # Verify update
        value = PSRDatabase.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", Int64(1))
        @test value == 999

        # Verify label unchanged
        label = PSRDatabase.read_scalar_strings_by_id(db, "Configuration", "label", Int64(1))
        @test label == "Config 1"

        PSRDatabase.close!(db)
    end

    @testset "Element Multiple Scalars" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        # Create element
        PSRDatabase.create_element!(
            db,
            "Configuration";
            label = "Config 1",
            integer_attribute = 100,
            float_attribute = 1.5,
        )

        # Update multiple attributes at once
        PSRDatabase.update_element!(db, "Configuration", Int64(1); integer_attribute = 500, float_attribute = 9.9)

        # Verify updates
        int_value = PSRDatabase.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", Int64(1))
        @test int_value == 500

        float_value = PSRDatabase.read_scalar_floats_by_id(db, "Configuration", "float_attribute", Int64(1))
        @test float_value == 9.9

        # Verify label unchanged
        label = PSRDatabase.read_scalar_strings_by_id(db, "Configuration", "label", Int64(1))
        @test label == "Config 1"

        PSRDatabase.close!(db)
    end

    @testset "Element Other Elements Unchanged" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        # Create multiple elements
        PSRDatabase.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 100)
        PSRDatabase.create_element!(db, "Configuration"; label = "Config 2", integer_attribute = 200)
        PSRDatabase.create_element!(db, "Configuration"; label = "Config 3", integer_attribute = 300)

        # Update only element 2
        PSRDatabase.update_element!(db, "Configuration", Int64(2); integer_attribute = 999)

        # Verify element 2 updated
        value2 = PSRDatabase.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", Int64(2))
        @test value2 == 999

        # Verify elements 1 and 3 unchanged
        value1 = PSRDatabase.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", Int64(1))
        @test value1 == 100

        value3 = PSRDatabase.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", Int64(3))
        @test value3 == 300

        PSRDatabase.close!(db)
    end

    @testset "Element Arrays Ignored" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")

        # Create element with vector data
        PSRDatabase.create_element!(db, "Collection";
            label = "Item 1",
            some_integer = 10,
            value_int = [1, 2, 3],
        )

        # Try to update with array values (should be ignored)
        PSRDatabase.update_element!(db, "Collection", Int64(1); some_integer = 999, value_int = [7, 8, 9])

        # Verify scalar was updated
        int_value = PSRDatabase.read_scalar_integers_by_id(db, "Collection", "some_integer", Int64(1))
        @test int_value == 999

        # Verify vector was NOT updated (arrays ignored in update_element)
        vec_values = PSRDatabase.read_vector_integers_by_id(db, "Collection", "value_int", Int64(1))
        @test vec_values == [1, 2, 3]

        PSRDatabase.close!(db)
    end

    @testset "Element Using Element Builder" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        # Create element
        PSRDatabase.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 100)

        # Update using Element builder
        e = PSRDatabase.Element()
        e["integer_attribute"] = Int64(777)
        PSRDatabase.update_element!(db, "Configuration", Int64(1), e)

        # Verify update
        value = PSRDatabase.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", Int64(1))
        @test value == 777

        PSRDatabase.close!(db)
    end

    # Error handling tests

    @testset "Invalid Collection" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 100)

        @test_throws PSRDatabase.DatabaseException PSRDatabase.update_element!(
            db,
            "NonexistentCollection",
            Int64(1);
            integer_attribute = 999,
        )

        PSRDatabase.close!(db)
    end

    @testset "Invalid Element ID" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 100)

        # Update element that doesn't exist - should not throw but also should not affect anything
        PSRDatabase.update_element!(db, "Configuration", Int64(999); integer_attribute = 500)

        # Verify original element unchanged
        value = PSRDatabase.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", Int64(1))
        @test value == 100

        PSRDatabase.close!(db)
    end

    @testset "Invalid Attribute" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 100)

        @test_throws PSRDatabase.DatabaseException PSRDatabase.update_element!(
            db,
            "Configuration",
            Int64(1);
            nonexistent_attribute = 999,
        )

        PSRDatabase.close!(db)
    end

    @testset "String Attribute" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Config 1", string_attribute = "original")

        PSRDatabase.update_element!(db, "Configuration", Int64(1); string_attribute = "updated")

        value = PSRDatabase.read_scalar_strings_by_id(db, "Configuration", "string_attribute", Int64(1))
        @test value == "updated"

        PSRDatabase.close!(db)
    end

    @testset "Float Attribute" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Config 1", float_attribute = 1.5)

        PSRDatabase.update_element!(db, "Configuration", Int64(1); float_attribute = 99.99)

        value = PSRDatabase.read_scalar_floats_by_id(db, "Configuration", "float_attribute", Int64(1))
        @test value == 99.99

        PSRDatabase.close!(db)
    end

    @testset "Multiple Elements Sequentially" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 100)
        PSRDatabase.create_element!(db, "Configuration"; label = "Config 2", integer_attribute = 200)

        # Update both elements
        PSRDatabase.update_element!(db, "Configuration", Int64(1); integer_attribute = 111)
        PSRDatabase.update_element!(db, "Configuration", Int64(2); integer_attribute = 222)

        @test PSRDatabase.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", Int64(1)) == 111
        @test PSRDatabase.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", Int64(2)) == 222

        PSRDatabase.close!(db)
    end

    # ============================================================================
    # Scalar update functions tests
    # ============================================================================

    @testset "Scalar Integer" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 42)

        # Basic update
        PSRDatabase.update_scalar_integer!(db, "Configuration", "integer_attribute", Int64(1), 100)
        value = PSRDatabase.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", Int64(1))
        @test value == 100

        # Update to 0
        PSRDatabase.update_scalar_integer!(db, "Configuration", "integer_attribute", Int64(1), 0)
        value = PSRDatabase.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", Int64(1))
        @test value == 0

        # Update to negative
        PSRDatabase.update_scalar_integer!(db, "Configuration", "integer_attribute", Int64(1), -999)
        value = PSRDatabase.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", Int64(1))
        @test value == -999

        PSRDatabase.close!(db)
    end

    @testset "Scalar Integer Multiple Elements" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 42)
        PSRDatabase.create_element!(db, "Configuration"; label = "Config 2", integer_attribute = 100)

        # Update only first element
        PSRDatabase.update_scalar_integer!(db, "Configuration", "integer_attribute", Int64(1), 999)

        # Verify first element changed
        @test PSRDatabase.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", Int64(1)) == 999

        # Verify second element unchanged
        @test PSRDatabase.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", Int64(2)) == 100

        PSRDatabase.close!(db)
    end

    @testset "Scalar Double" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Config 1", float_attribute = 3.14)

        # Basic update
        PSRDatabase.update_scalar_double!(db, "Configuration", "float_attribute", Int64(1), 2.71)
        value = PSRDatabase.read_scalar_floats_by_id(db, "Configuration", "float_attribute", Int64(1))
        @test value == 2.71

        # Update to 0.0
        PSRDatabase.update_scalar_double!(db, "Configuration", "float_attribute", Int64(1), 0.0)
        value = PSRDatabase.read_scalar_floats_by_id(db, "Configuration", "float_attribute", Int64(1))
        @test value == 0.0

        # Precision test
        PSRDatabase.update_scalar_double!(db, "Configuration", "float_attribute", Int64(1), 1.23456789012345)
        value = PSRDatabase.read_scalar_floats_by_id(db, "Configuration", "float_attribute", Int64(1))
        @test value ≈ 1.23456789012345

        PSRDatabase.close!(db)
    end

    @testset "Scalar String" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Config 1", string_attribute = "hello")

        # Basic update
        PSRDatabase.update_scalar_string!(db, "Configuration", "string_attribute", Int64(1), "world")
        value = PSRDatabase.read_scalar_strings_by_id(db, "Configuration", "string_attribute", Int64(1))
        @test value == "world"

        # Update to empty string
        PSRDatabase.update_scalar_string!(db, "Configuration", "string_attribute", Int64(1), "")
        value = PSRDatabase.read_scalar_strings_by_id(db, "Configuration", "string_attribute", Int64(1))
        @test value == ""

        # Unicode support
        PSRDatabase.update_scalar_string!(db, "Configuration", "string_attribute", Int64(1), "日本語テスト")
        value = PSRDatabase.read_scalar_strings_by_id(db, "Configuration", "string_attribute", Int64(1))
        @test value == "日本語テスト"

        PSRDatabase.close!(db)
    end

    # ============================================================================
    # Vector update functions tests
    # ============================================================================

    @testset "Vector Integers" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")
        PSRDatabase.create_element!(db, "Collection"; label = "Item 1", value_int = [1, 2, 3])

        # Replace existing vector
        PSRDatabase.update_vector_integers!(db, "Collection", "value_int", Int64(1), [10, 20, 30, 40])
        values = PSRDatabase.read_vector_integers_by_id(db, "Collection", "value_int", Int64(1))
        @test values == [10, 20, 30, 40]

        # Update to smaller vector
        PSRDatabase.update_vector_integers!(db, "Collection", "value_int", Int64(1), [100])
        values = PSRDatabase.read_vector_integers_by_id(db, "Collection", "value_int", Int64(1))
        @test values == [100]

        # Update to empty vector
        PSRDatabase.update_vector_integers!(db, "Collection", "value_int", Int64(1), Int64[])
        values = PSRDatabase.read_vector_integers_by_id(db, "Collection", "value_int", Int64(1))
        @test isempty(values)

        PSRDatabase.close!(db)
    end

    @testset "Vector Integers From Empty" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")
        PSRDatabase.create_element!(db, "Collection"; label = "Item 1")

        # Verify initially empty
        values = PSRDatabase.read_vector_integers_by_id(db, "Collection", "value_int", Int64(1))
        @test isempty(values)

        # Update to non-empty
        PSRDatabase.update_vector_integers!(db, "Collection", "value_int", Int64(1), [1, 2, 3])
        values = PSRDatabase.read_vector_integers_by_id(db, "Collection", "value_int", Int64(1))
        @test values == [1, 2, 3]

        PSRDatabase.close!(db)
    end

    @testset "Vector Integers Multiple Elements" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")
        PSRDatabase.create_element!(db, "Collection"; label = "Item 1", value_int = [1, 2, 3])
        PSRDatabase.create_element!(db, "Collection"; label = "Item 2", value_int = [10, 20])

        # Update only first element
        PSRDatabase.update_vector_integers!(db, "Collection", "value_int", Int64(1), [100, 200])

        # Verify first element changed
        @test PSRDatabase.read_vector_integers_by_id(db, "Collection", "value_int", Int64(1)) == [100, 200]

        # Verify second element unchanged
        @test PSRDatabase.read_vector_integers_by_id(db, "Collection", "value_int", Int64(2)) == [10, 20]

        PSRDatabase.close!(db)
    end

    @testset "Vector Floats" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")
        PSRDatabase.create_element!(db, "Collection"; label = "Item 1", value_float = [1.5, 2.5, 3.5])

        # Replace existing vector
        PSRDatabase.update_vector_floats!(db, "Collection", "value_float", Int64(1), [10.5, 20.5])
        values = PSRDatabase.read_vector_floats_by_id(db, "Collection", "value_float", Int64(1))
        @test values == [10.5, 20.5]

        # Precision test
        PSRDatabase.update_vector_floats!(db, "Collection", "value_float", Int64(1), [1.23456789, 9.87654321])
        values = PSRDatabase.read_vector_floats_by_id(db, "Collection", "value_float", Int64(1))
        @test values ≈ [1.23456789, 9.87654321]

        # Update to empty vector
        PSRDatabase.update_vector_floats!(db, "Collection", "value_float", Int64(1), Float64[])
        values = PSRDatabase.read_vector_floats_by_id(db, "Collection", "value_float", Int64(1))
        @test isempty(values)

        PSRDatabase.close!(db)
    end

    # ============================================================================
    # Set update functions tests
    # ============================================================================

    @testset "Set Strings" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")
        PSRDatabase.create_element!(db, "Collection"; label = "Item 1", tag = ["important", "urgent"])

        # Replace existing set
        PSRDatabase.update_set_strings!(db, "Collection", "tag", Int64(1), ["new_tag1", "new_tag2", "new_tag3"])
        values = PSRDatabase.read_set_strings_by_id(db, "Collection", "tag", Int64(1))
        @test sort(values) == sort(["new_tag1", "new_tag2", "new_tag3"])

        # Update to single element
        PSRDatabase.update_set_strings!(db, "Collection", "tag", Int64(1), ["single_tag"])
        values = PSRDatabase.read_set_strings_by_id(db, "Collection", "tag", Int64(1))
        @test values == ["single_tag"]

        # Update to empty set
        PSRDatabase.update_set_strings!(db, "Collection", "tag", Int64(1), String[])
        values = PSRDatabase.read_set_strings_by_id(db, "Collection", "tag", Int64(1))
        @test isempty(values)

        PSRDatabase.close!(db)
    end

    @testset "Set Strings From Empty" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")
        PSRDatabase.create_element!(db, "Collection"; label = "Item 1")

        # Verify initially empty
        values = PSRDatabase.read_set_strings_by_id(db, "Collection", "tag", Int64(1))
        @test isempty(values)

        # Update to non-empty
        PSRDatabase.update_set_strings!(db, "Collection", "tag", Int64(1), ["important", "urgent"])
        values = PSRDatabase.read_set_strings_by_id(db, "Collection", "tag", Int64(1))
        @test sort(values) == sort(["important", "urgent"])

        PSRDatabase.close!(db)
    end

    @testset "Set Strings Multiple Elements" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")
        PSRDatabase.create_element!(db, "Collection"; label = "Item 1", tag = ["important"])
        PSRDatabase.create_element!(db, "Collection"; label = "Item 2", tag = ["urgent", "review"])

        # Update only first element
        PSRDatabase.update_set_strings!(db, "Collection", "tag", Int64(1), ["updated"])

        # Verify first element changed
        @test PSRDatabase.read_set_strings_by_id(db, "Collection", "tag", Int64(1)) == ["updated"]

        # Verify second element unchanged
        @test sort(PSRDatabase.read_set_strings_by_id(db, "Collection", "tag", Int64(2))) == sort(["urgent", "review"])

        PSRDatabase.close!(db)
    end

    @testset "Set Strings Unicode" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")
        PSRDatabase.create_element!(db, "Collection"; label = "Item 1", tag = ["tag1"])

        # Unicode support
        PSRDatabase.update_set_strings!(db, "Collection", "tag", Int64(1), ["日本語", "中文", "한국어"])
        values = PSRDatabase.read_set_strings_by_id(db, "Collection", "tag", Int64(1))
        @test sort(values) == sort(["日本語", "中文", "한국어"])

        PSRDatabase.close!(db)
    end

    # ============================================================================
    # Error handling tests for new update functions
    # ============================================================================

    @testset "Scalar Integer Invalid Collection" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        @test_throws PSRDatabase.DatabaseException PSRDatabase.update_scalar_integer!(
            db,
            "NonexistentCollection",
            "integer_attribute",
            Int64(1),
            42,
        )

        PSRDatabase.close!(db)
    end

    @testset "Scalar Integer Invalid Attribute" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 42)

        @test_throws PSRDatabase.DatabaseException PSRDatabase.update_scalar_integer!(
            db,
            "Configuration",
            "nonexistent_attribute",
            Int64(1),
            100,
        )

        PSRDatabase.close!(db)
    end

    @testset "Vector Integers Invalid Collection" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")

        @test_throws PSRDatabase.DatabaseException PSRDatabase.update_vector_integers!(
            db,
            "NonexistentCollection",
            "value_int",
            Int64(1),
            [1, 2, 3],
        )

        PSRDatabase.close!(db)
    end

    @testset "Set Strings Invalid Collection" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = PSRDatabase.from_schema(":memory:", path_schema)

        PSRDatabase.create_element!(db, "Configuration"; label = "Test Config")

        @test_throws PSRDatabase.DatabaseException PSRDatabase.update_set_strings!(
            db,
            "NonexistentCollection",
            "tag",
            Int64(1),
            ["tag1"],
        )

        PSRDatabase.close!(db)
    end
end

end
