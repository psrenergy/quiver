module TestUpdate

using Quiver
using Test

include("fixture.jl")

@testset "Update" begin
    @testset "Element Single Scalar" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        # Create elements
        Quiver.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 100)
        Quiver.create_element!(db, "Configuration"; label = "Config 2", integer_attribute = 200)

        # Update single attribute
        Quiver.update_element!(db, "Configuration", Int64(1); integer_attribute = 999)

        # Verify update
        value = Quiver.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", Int64(1))
        @test value == 999

        # Verify label unchanged
        label = Quiver.read_scalar_strings_by_id(db, "Configuration", "label", Int64(1))
        @test label == "Config 1"

        Quiver.close!(db)
    end

    @testset "Element Multiple Scalars" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        # Create element
        Quiver.create_element!(
            db,
            "Configuration";
            label = "Config 1",
            integer_attribute = 100,
            float_attribute = 1.5,
        )

        # Update multiple attributes at once
        Quiver.update_element!(db, "Configuration", Int64(1); integer_attribute = 500, float_attribute = 9.9)

        # Verify updates
        integer_value = Quiver.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", Int64(1))
        @test integer_value == 500

        float_value = Quiver.read_scalar_floats_by_id(db, "Configuration", "float_attribute", Int64(1))
        @test float_value == 9.9

        # Verify label unchanged
        label = Quiver.read_scalar_strings_by_id(db, "Configuration", "label", Int64(1))
        @test label == "Config 1"

        Quiver.close!(db)
    end

    @testset "Element Other Elements Unchanged" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        # Create multiple elements
        Quiver.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 100)
        Quiver.create_element!(db, "Configuration"; label = "Config 2", integer_attribute = 200)
        Quiver.create_element!(db, "Configuration"; label = "Config 3", integer_attribute = 300)

        # Update only element 2
        Quiver.update_element!(db, "Configuration", Int64(2); integer_attribute = 999)

        # Verify element 2 updated
        value2 = Quiver.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", Int64(2))
        @test value2 == 999

        # Verify elements 1 and 3 unchanged
        value1 = Quiver.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", Int64(1))
        @test value1 == 100

        value3 = Quiver.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", Int64(3))
        @test value3 == 300

        Quiver.close!(db)
    end

    @testset "Element Arrays Ignored" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        # Create element with vector data
        Quiver.create_element!(db, "Collection";
            label = "Item 1",
            some_integer = 10,
            value_int = [1, 2, 3],
        )

        # Try to update with array values (should be ignored)
        Quiver.update_element!(db, "Collection", Int64(1); some_integer = 999, value_int = [7, 8, 9])

        # Verify scalar was updated
        integer_value = Quiver.read_scalar_integers_by_id(db, "Collection", "some_integer", Int64(1))
        @test integer_value == 999

        # Verify vector was NOT updated (arrays ignored in update_element)
        vec_values = Quiver.read_vector_integers_by_id(db, "Collection", "value_int", Int64(1))
        @test vec_values == [1, 2, 3]

        Quiver.close!(db)
    end

    @testset "Element Using Element Builder" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        # Create element
        Quiver.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 100)

        # Update using Element builder
        e = Quiver.Element()
        e["integer_attribute"] = Int64(777)
        Quiver.update_element!(db, "Configuration", Int64(1), e)

        # Verify update
        value = Quiver.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", Int64(1))
        @test value == 777

        Quiver.close!(db)
    end

    # Error handling tests

    @testset "Invalid Collection" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 100)

        @test_throws Quiver.DatabaseException Quiver.update_element!(
            db,
            "NonexistentCollection",
            Int64(1);
            integer_attribute = 999,
        )

        Quiver.close!(db)
    end

    @testset "Invalid Element ID" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 100)

        # Update element that doesn't exist - should not throw but also should not affect anything
        Quiver.update_element!(db, "Configuration", Int64(999); integer_attribute = 500)

        # Verify original element unchanged
        value = Quiver.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", Int64(1))
        @test value == 100

        Quiver.close!(db)
    end

    @testset "Invalid Attribute" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 100)

        @test_throws Quiver.DatabaseException Quiver.update_element!(
            db,
            "Configuration",
            Int64(1);
            nonexistent_attribute = 999,
        )

        Quiver.close!(db)
    end

    @testset "String Attribute" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Config 1", string_attribute = "original")

        Quiver.update_element!(db, "Configuration", Int64(1); string_attribute = "updated")

        value = Quiver.read_scalar_strings_by_id(db, "Configuration", "string_attribute", Int64(1))
        @test value == "updated"

        Quiver.close!(db)
    end

    @testset "Float Attribute" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Config 1", float_attribute = 1.5)

        Quiver.update_element!(db, "Configuration", Int64(1); float_attribute = 99.99)

        value = Quiver.read_scalar_floats_by_id(db, "Configuration", "float_attribute", Int64(1))
        @test value == 99.99

        Quiver.close!(db)
    end

    @testset "Multiple Elements Sequentially" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 100)
        Quiver.create_element!(db, "Configuration"; label = "Config 2", integer_attribute = 200)

        # Update both elements
        Quiver.update_element!(db, "Configuration", Int64(1); integer_attribute = 111)
        Quiver.update_element!(db, "Configuration", Int64(2); integer_attribute = 222)

        @test Quiver.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", Int64(1)) == 111
        @test Quiver.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", Int64(2)) == 222

        Quiver.close!(db)
    end

    # ============================================================================
    # Scalar update functions tests
    # ============================================================================

    @testset "Scalar Integer" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 42)

        # Basic update
        Quiver.update_scalar_integer!(db, "Configuration", "integer_attribute", Int64(1), 100)
        value = Quiver.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", Int64(1))
        @test value == 100

        # Update to 0
        Quiver.update_scalar_integer!(db, "Configuration", "integer_attribute", Int64(1), 0)
        value = Quiver.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", Int64(1))
        @test value == 0

        # Update to negative
        Quiver.update_scalar_integer!(db, "Configuration", "integer_attribute", Int64(1), -999)
        value = Quiver.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", Int64(1))
        @test value == -999

        Quiver.close!(db)
    end

    @testset "Scalar Integer Multiple Elements" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 42)
        Quiver.create_element!(db, "Configuration"; label = "Config 2", integer_attribute = 100)

        # Update only first element
        Quiver.update_scalar_integer!(db, "Configuration", "integer_attribute", Int64(1), 999)

        # Verify first element changed
        @test Quiver.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", Int64(1)) == 999

        # Verify second element unchanged
        @test Quiver.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", Int64(2)) == 100

        Quiver.close!(db)
    end

    @testset "Scalar Float" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Config 1", float_attribute = 3.14)

        # Basic update
        Quiver.update_scalar_float!(db, "Configuration", "float_attribute", Int64(1), 2.71)
        value = Quiver.read_scalar_floats_by_id(db, "Configuration", "float_attribute", Int64(1))
        @test value == 2.71

        # Update to 0.0
        Quiver.update_scalar_float!(db, "Configuration", "float_attribute", Int64(1), 0.0)
        value = Quiver.read_scalar_floats_by_id(db, "Configuration", "float_attribute", Int64(1))
        @test value == 0.0

        # Precision test
        Quiver.update_scalar_float!(db, "Configuration", "float_attribute", Int64(1), 1.23456789012345)
        value = Quiver.read_scalar_floats_by_id(db, "Configuration", "float_attribute", Int64(1))
        @test value ≈ 1.23456789012345

        Quiver.close!(db)
    end

    @testset "Scalar String" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Config 1", string_attribute = "hello")

        # Basic update
        Quiver.update_scalar_string!(db, "Configuration", "string_attribute", Int64(1), "world")
        value = Quiver.read_scalar_strings_by_id(db, "Configuration", "string_attribute", Int64(1))
        @test value == "world"

        # Update to empty string
        Quiver.update_scalar_string!(db, "Configuration", "string_attribute", Int64(1), "")
        value = Quiver.read_scalar_strings_by_id(db, "Configuration", "string_attribute", Int64(1))
        @test value == ""

        # Unicode support
        Quiver.update_scalar_string!(db, "Configuration", "string_attribute", Int64(1), "日本語テスト")
        value = Quiver.read_scalar_strings_by_id(db, "Configuration", "string_attribute", Int64(1))
        @test value == "日本語テスト"

        Quiver.close!(db)
    end

    # ============================================================================
    # Vector update functions tests
    # ============================================================================

    @testset "Vector Integers" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Collection"; label = "Item 1", value_int = [1, 2, 3])

        # Replace existing vector
        Quiver.update_vector_integers!(db, "Collection", "value_int", Int64(1), [10, 20, 30, 40])
        values = Quiver.read_vector_integers_by_id(db, "Collection", "value_int", Int64(1))
        @test values == [10, 20, 30, 40]

        # Update to smaller vector
        Quiver.update_vector_integers!(db, "Collection", "value_int", Int64(1), [100])
        values = Quiver.read_vector_integers_by_id(db, "Collection", "value_int", Int64(1))
        @test values == [100]

        # Update to empty vector
        Quiver.update_vector_integers!(db, "Collection", "value_int", Int64(1), Int64[])
        values = Quiver.read_vector_integers_by_id(db, "Collection", "value_int", Int64(1))
        @test isempty(values)

        Quiver.close!(db)
    end

    @testset "Vector Integers From Empty" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Collection"; label = "Item 1")

        # Verify initially empty
        values = Quiver.read_vector_integers_by_id(db, "Collection", "value_int", Int64(1))
        @test isempty(values)

        # Update to non-empty
        Quiver.update_vector_integers!(db, "Collection", "value_int", Int64(1), [1, 2, 3])
        values = Quiver.read_vector_integers_by_id(db, "Collection", "value_int", Int64(1))
        @test values == [1, 2, 3]

        Quiver.close!(db)
    end

    @testset "Vector Integers Multiple Elements" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Collection"; label = "Item 1", value_int = [1, 2, 3])
        Quiver.create_element!(db, "Collection"; label = "Item 2", value_int = [10, 20])

        # Update only first element
        Quiver.update_vector_integers!(db, "Collection", "value_int", Int64(1), [100, 200])

        # Verify first element changed
        @test Quiver.read_vector_integers_by_id(db, "Collection", "value_int", Int64(1)) == [100, 200]

        # Verify second element unchanged
        @test Quiver.read_vector_integers_by_id(db, "Collection", "value_int", Int64(2)) == [10, 20]

        Quiver.close!(db)
    end

    @testset "Vector Floats" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Collection"; label = "Item 1", value_float = [1.5, 2.5, 3.5])

        # Replace existing vector
        Quiver.update_vector_floats!(db, "Collection", "value_float", Int64(1), [10.5, 20.5])
        values = Quiver.read_vector_floats_by_id(db, "Collection", "value_float", Int64(1))
        @test values == [10.5, 20.5]

        # Precision test
        Quiver.update_vector_floats!(db, "Collection", "value_float", Int64(1), [1.23456789, 9.87654321])
        values = Quiver.read_vector_floats_by_id(db, "Collection", "value_float", Int64(1))
        @test values ≈ [1.23456789, 9.87654321]

        # Update to empty vector
        Quiver.update_vector_floats!(db, "Collection", "value_float", Int64(1), Float64[])
        values = Quiver.read_vector_floats_by_id(db, "Collection", "value_float", Int64(1))
        @test isempty(values)

        Quiver.close!(db)
    end

    # ============================================================================
    # Set update functions tests
    # ============================================================================

    @testset "Set Strings" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Collection"; label = "Item 1", tag = ["important", "urgent"])

        # Replace existing set
        Quiver.update_set_strings!(db, "Collection", "tag", Int64(1), ["new_tag1", "new_tag2", "new_tag3"])
        values = Quiver.read_set_strings_by_id(db, "Collection", "tag", Int64(1))
        @test sort(values) == sort(["new_tag1", "new_tag2", "new_tag3"])

        # Update to single element
        Quiver.update_set_strings!(db, "Collection", "tag", Int64(1), ["single_tag"])
        values = Quiver.read_set_strings_by_id(db, "Collection", "tag", Int64(1))
        @test values == ["single_tag"]

        # Update to empty set
        Quiver.update_set_strings!(db, "Collection", "tag", Int64(1), String[])
        values = Quiver.read_set_strings_by_id(db, "Collection", "tag", Int64(1))
        @test isempty(values)

        Quiver.close!(db)
    end

    @testset "Set Strings From Empty" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Collection"; label = "Item 1")

        # Verify initially empty
        values = Quiver.read_set_strings_by_id(db, "Collection", "tag", Int64(1))
        @test isempty(values)

        # Update to non-empty
        Quiver.update_set_strings!(db, "Collection", "tag", Int64(1), ["important", "urgent"])
        values = Quiver.read_set_strings_by_id(db, "Collection", "tag", Int64(1))
        @test sort(values) == sort(["important", "urgent"])

        Quiver.close!(db)
    end

    @testset "Set Strings Multiple Elements" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Collection"; label = "Item 1", tag = ["important"])
        Quiver.create_element!(db, "Collection"; label = "Item 2", tag = ["urgent", "review"])

        # Update only first element
        Quiver.update_set_strings!(db, "Collection", "tag", Int64(1), ["updated"])

        # Verify first element changed
        @test Quiver.read_set_strings_by_id(db, "Collection", "tag", Int64(1)) == ["updated"]

        # Verify second element unchanged
        @test sort(Quiver.read_set_strings_by_id(db, "Collection", "tag", Int64(2))) ==
              sort(["urgent", "review"])

        Quiver.close!(db)
    end

    @testset "Set Strings Unicode" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Collection"; label = "Item 1", tag = ["tag1"])

        # Unicode support
        Quiver.update_set_strings!(db, "Collection", "tag", Int64(1), ["日本語", "中文", "한국어"])
        values = Quiver.read_set_strings_by_id(db, "Collection", "tag", Int64(1))
        @test sort(values) == sort(["日本語", "中文", "한국어"])

        Quiver.close!(db)
    end

    # ============================================================================
    # Error handling tests for new update functions
    # ============================================================================

    @testset "Scalar Integer Invalid Collection" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        @test_throws Quiver.DatabaseException Quiver.update_scalar_integer!(
            db,
            "NonexistentCollection",
            "integer_attribute",
            Int64(1),
            42,
        )

        Quiver.close!(db)
    end

    @testset "Scalar Integer Invalid Attribute" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 42)

        @test_throws Quiver.DatabaseException Quiver.update_scalar_integer!(
            db,
            "Configuration",
            "nonexistent_attribute",
            Int64(1),
            100,
        )

        Quiver.close!(db)
    end

    @testset "Vector Integers Invalid Collection" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        @test_throws Quiver.DatabaseException Quiver.update_vector_integers!(
            db,
            "NonexistentCollection",
            "value_int",
            Int64(1),
            [1, 2, 3],
        )

        Quiver.close!(db)
    end

    @testset "Set Strings Invalid Collection" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        @test_throws Quiver.DatabaseException Quiver.update_set_strings!(
            db,
            "NonexistentCollection",
            "tag",
            Int64(1),
            ["tag1"],
        )

        Quiver.close!(db)
    end

    # ============================================================================
    # update_element_vectors_sets! tests
    # ============================================================================

    @testset "Element Vectors Sets Single Vector" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Collection"; label = "Item 1", value_int = [1, 2, 3])

        # Update vector using update_element_vectors_sets!
        Quiver.update_element_vectors_sets!(db, "Collection", Int64(1); value_int = [10, 20, 30, 40])

        values = Quiver.read_vector_integers_by_id(db, "Collection", "value_int", Int64(1))
        @test values == [10, 20, 30, 40]

        Quiver.close!(db)
    end

    @testset "Element Vectors Sets Single Set" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Collection"; label = "Item 1", tag = ["important", "urgent"])

        # Update set using update_element_vectors_sets!
        Quiver.update_element_vectors_sets!(db, "Collection", Int64(1); tag = ["new_tag1", "new_tag2"])

        values = Quiver.read_set_strings_by_id(db, "Collection", "tag", Int64(1))
        @test sort(values) == sort(["new_tag1", "new_tag2"])

        Quiver.close!(db)
    end

    @testset "Element Vectors Sets Multiple Attributes" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Collection";
            label = "Item 1",
            value_int = [1, 2, 3],
            tag = ["old_tag"],
        )

        # Update both vector and set in one call
        Quiver.update_element_vectors_sets!(db, "Collection", Int64(1);
            value_int = [100, 200],
            tag = ["new_tag1", "new_tag2"],
        )

        vec = Quiver.read_vector_integers_by_id(db, "Collection", "value_int", Int64(1))
        @test vec == [100, 200]

        set_values = Quiver.read_set_strings_by_id(db, "Collection", "tag", Int64(1))
        @test sort(set_values) == sort(["new_tag1", "new_tag2"])

        Quiver.close!(db)
    end

    @testset "Element Vectors Sets Unchanged Attributes Remain Intact" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        # Note: value_int and value_float must have same length (same vector table)
        Quiver.create_element!(db, "Collection";
            label = "Item 1",
            some_integer = 42,
            value_int = [1, 2, 3],
            value_float = [1.5, 2.5, 3.5],
            tag = ["tag1"],
        )

        # Only update tag, leave vectors unchanged
        Quiver.update_element_vectors_sets!(db, "Collection", Int64(1); tag = ["updated_tag"])

        # Verify tag was updated
        tag_values = Quiver.read_set_strings_by_id(db, "Collection", "tag", Int64(1))
        @test tag_values == ["updated_tag"]

        # Verify value_int is unchanged
        vec_int = Quiver.read_vector_integers_by_id(db, "Collection", "value_int", Int64(1))
        @test vec_int == [1, 2, 3]

        # Verify value_float is unchanged
        vec_float = Quiver.read_vector_floats_by_id(db, "Collection", "value_float", Int64(1))
        @test vec_float == [1.5, 2.5, 3.5]

        # Verify scalar is unchanged
        scalar = Quiver.read_scalar_integers_by_id(db, "Collection", "some_integer", Int64(1))
        @test scalar == 42

        Quiver.close!(db)
    end

    @testset "Element Vectors Sets Empty Element Is NoOp" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Collection"; label = "Item 1", value_int = [1, 2, 3])

        # Empty element update should be a no-op
        e = Quiver.Element()
        Quiver.update_element_vectors_sets!(db, "Collection", Int64(1), e)

        # Verify data is unchanged
        values = Quiver.read_vector_integers_by_id(db, "Collection", "value_int", Int64(1))
        @test values == [1, 2, 3]

        Quiver.close!(db)
    end

    @testset "Element Vectors Sets Ignores Scalars" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Collection";
            label = "Item 1",
            some_integer = 42,
            value_int = [1, 2, 3],
        )

        # Element with both scalars and arrays - scalars should be ignored
        Quiver.update_element_vectors_sets!(db, "Collection", Int64(1);
            some_integer = 999,  # scalar - should be ignored
            value_int = [100, 200],  # vector - should be updated
        )

        # Verify vector was updated
        vec = Quiver.read_vector_integers_by_id(db, "Collection", "value_int", Int64(1))
        @test vec == [100, 200]

        # Verify scalar was NOT updated (scalars should be ignored)
        scalar = Quiver.read_scalar_integers_by_id(db, "Collection", "some_integer", Int64(1))
        @test scalar == 42

        Quiver.close!(db)
    end

    @testset "Element Vectors Sets Invalid Attribute" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Collection"; label = "Item 1")

        # Try to update non-existent attribute
        @test_throws Quiver.DatabaseException Quiver.update_element_vectors_sets!(
            db,
            "Collection",
            Int64(1);
            nonexistent_attr = [1, 2, 3],
        )

        Quiver.close!(db)
    end

    @testset "Element Vectors Sets Other Elements Unchanged" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Collection"; label = "Item 1", value_int = [1, 2, 3])
        Quiver.create_element!(db, "Collection"; label = "Item 2", value_int = [10, 20])

        # Update only first element
        Quiver.update_element_vectors_sets!(db, "Collection", Int64(1); value_int = [100, 200])

        # Verify first element was updated
        @test Quiver.read_vector_integers_by_id(db, "Collection", "value_int", Int64(1)) == [100, 200]

        # Verify second element is unchanged
        @test Quiver.read_vector_integers_by_id(db, "Collection", "value_int", Int64(2)) == [10, 20]

        Quiver.close!(db)
    end
end

end
