module TestDatabaseUpdate

using Dates
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
        value = Quiver.read_scalar_integer_by_id(db, "Configuration", "integer_attribute", Int64(1))
        @test value == 999

        # Verify label unchanged
        label = Quiver.read_scalar_string_by_id(db, "Configuration", "label", Int64(1))
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
        integer_value = Quiver.read_scalar_integer_by_id(db, "Configuration", "integer_attribute", Int64(1))
        @test integer_value == 500

        float_value = Quiver.read_scalar_float_by_id(db, "Configuration", "float_attribute", Int64(1))
        @test float_value == 9.9

        # Verify label unchanged
        label = Quiver.read_scalar_string_by_id(db, "Configuration", "label", Int64(1))
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
        value2 = Quiver.read_scalar_integer_by_id(db, "Configuration", "integer_attribute", Int64(2))
        @test value2 == 999

        # Verify elements 1 and 3 unchanged
        value1 = Quiver.read_scalar_integer_by_id(db, "Configuration", "integer_attribute", Int64(1))
        @test value1 == 100

        value3 = Quiver.read_scalar_integer_by_id(db, "Configuration", "integer_attribute", Int64(3))
        @test value3 == 300

        Quiver.close!(db)
    end

    @testset "Element With Arrays" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        # Create element with vector data
        Quiver.create_element!(db, "Collection";
            label = "Item 1",
            some_integer = 10,
            value_int = [1, 2, 3],
        )

        # Update with both scalar and array values - both should be updated
        Quiver.update_element!(db, "Collection", Int64(1); some_integer = 999, value_int = [7, 8, 9])

        # Verify scalar was updated
        integer_value = Quiver.read_scalar_integer_by_id(db, "Collection", "some_integer", Int64(1))
        @test integer_value == 999

        # Verify vector was also updated
        vec_values = Quiver.read_vector_integers_by_id(db, "Collection", "value_int", Int64(1))
        @test vec_values == [7, 8, 9]

        Quiver.close!(db)
    end

    @testset "Element With Set Only" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        Quiver.create_element!(db, "Collection";
            label = "Item 1",
            tag = ["important", "urgent"],
        )

        # Update with only set attribute
        Quiver.update_element!(db, "Collection", Int64(1); tag = ["new_tag1", "new_tag2"])

        # Verify set was updated
        tag_values = Quiver.read_set_strings_by_id(db, "Collection", "tag", Int64(1))
        @test sort(tag_values) == sort(["new_tag1", "new_tag2"])

        # Verify label unchanged
        label = Quiver.read_scalar_string_by_id(db, "Collection", "label", Int64(1))
        @test label == "Item 1"

        Quiver.close!(db)
    end

    @testset "Element With Vector And Set" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        Quiver.create_element!(db, "Collection";
            label = "Item 1",
            value_int = [1, 2, 3],
            tag = ["old_tag"],
        )

        # Update both vector and set atomically
        Quiver.update_element!(db, "Collection", Int64(1);
            value_int = [100, 200],
            tag = ["new_tag1", "new_tag2"],
        )

        # Verify vector was updated
        vec = Quiver.read_vector_integers_by_id(db, "Collection", "value_int", Int64(1))
        @test vec == [100, 200]

        # Verify set was updated
        set_values = Quiver.read_set_strings_by_id(db, "Collection", "tag", Int64(1))
        @test sort(set_values) == sort(["new_tag1", "new_tag2"])

        Quiver.close!(db)
    end

    @testset "Element Invalid Array Attribute" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Collection"; label = "Item 1")

        # Try to update non-existent array attribute
        @test_throws Quiver.DatabaseException Quiver.update_element!(
            db,
            "Collection",
            Int64(1);
            nonexistent_attr = [1, 2, 3],
        )

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
        value = Quiver.read_scalar_integer_by_id(db, "Configuration", "integer_attribute", Int64(1))
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
        value = Quiver.read_scalar_integer_by_id(db, "Configuration", "integer_attribute", Int64(1))
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

        value = Quiver.read_scalar_string_by_id(db, "Configuration", "string_attribute", Int64(1))
        @test value == "updated"

        Quiver.close!(db)
    end

    @testset "Float Attribute" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Config 1", float_attribute = 1.5)

        Quiver.update_element!(db, "Configuration", Int64(1); float_attribute = 99.99)

        value = Quiver.read_scalar_float_by_id(db, "Configuration", "float_attribute", Int64(1))
        @test value == 99.99

        Quiver.close!(db)
    end

    @testset "DateTime Attribute" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration";
            label = "Config 1",
            date_attribute = "2024-01-01T00:00:00",
        )

        # Update DateTime value
        Quiver.update_element!(db, "Configuration", Int64(1); date_attribute = "2025-12-31T23:59:59")

        # Verify update
        date = Quiver.read_scalar_string_by_id(db, "Configuration", "date_attribute", Int64(1))
        @test date == "2025-12-31T23:59:59"

        Quiver.close!(db)
    end

    @testset "Native DateTime Object Update" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration";
            label = "Config 1",
            date_attribute = "2024-01-01T00:00:00",
        )

        # Update using native DateTime object via Element builder
        e = Quiver.Element()
        e["date_attribute"] = DateTime(2025, 6, 15, 12, 30, 45)
        Quiver.update_element!(db, "Configuration", Int64(1), e)

        # Verify it was stored correctly
        date_str = Quiver.read_scalar_string_by_id(db, "Configuration", "date_attribute", Int64(1))
        @test date_str == "2025-06-15T12:30:45"

        # Verify read_all_scalars_by_id returns native DateTime
        scalars = Quiver.read_all_scalars_by_id(db, "Configuration", Int64(1))
        @test scalars["date_attribute"] isa DateTime
        @test scalars["date_attribute"] == DateTime(2025, 6, 15, 12, 30, 45)

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

        @test Quiver.read_scalar_integer_by_id(db, "Configuration", "integer_attribute", Int64(1)) == 111
        @test Quiver.read_scalar_integer_by_id(db, "Configuration", "integer_attribute", Int64(2)) == 222

        Quiver.close!(db)
    end

    # ============================================================================
    # Whitespace trimming tests
    # ============================================================================

    @testset "Scalar String Trims Whitespace" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Config 1", string_attribute = "hello")

        Quiver.update_element!(db, "Configuration", Int64(1); string_attribute = "  world  ")

        value = Quiver.read_scalar_string_by_id(db, "Configuration", "string_attribute", Int64(1))
        @test value == "world"

        Quiver.close!(db)
    end

    @testset "Set Strings Trims Whitespace" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Collection"; label = "Item 1", tag = ["old"])

        Quiver.update_set_strings!(db, "Collection", "tag", Int64(1), ["  alpha  ", "\turgent\n", " gamma "])

        values = Quiver.read_set_strings_by_id(db, "Collection", "tag", Int64(1))
        @test sort(values) == ["alpha", "gamma", "urgent"]

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
        Quiver.update_element!(db, "Collection", Int64(1); value_int = [10, 20, 30, 40])
        values = Quiver.read_vector_integers_by_id(db, "Collection", "value_int", Int64(1))
        @test values == [10, 20, 30, 40]

        # Update to smaller vector
        Quiver.update_element!(db, "Collection", Int64(1); value_int = [100])
        values = Quiver.read_vector_integers_by_id(db, "Collection", "value_int", Int64(1))
        @test values == [100]

        # Update to empty vector
        Quiver.update_element!(db, "Collection", Int64(1); value_int = Int64[])
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
        Quiver.update_element!(db, "Collection", Int64(1); value_int = [1, 2, 3])

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
        Quiver.update_element!(db, "Collection", Int64(1); value_int = [100, 200])

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
    # Gap-fill: String vector, integer set, float set updates (using all_types.sql)
    # ============================================================================

    @testset "Update Vector Strings" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "all_types.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "AllTypes"; label = "Item 1")

        Quiver.update_vector_strings!(db, "AllTypes", "label_value", Int64(1), ["alpha", "beta"])
        result = Quiver.read_vector_strings_by_id(db, "AllTypes", "label_value", Int64(1))
        @test result == ["alpha", "beta"]

        Quiver.close!(db)
    end

    @testset "Update Set Integers" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "all_types.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "AllTypes"; label = "Item 1")

        Quiver.update_set_integers!(db, "AllTypes", "code", Int64(1), [10, 20, 30])
        result = Quiver.read_set_integers_by_id(db, "AllTypes", "code", Int64(1))
        @test sort(result) == [10, 20, 30]

        Quiver.close!(db)
    end

    @testset "Update Set Floats" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "all_types.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "AllTypes"; label = "Item 1")

        Quiver.update_set_floats!(db, "AllTypes", "weight", Int64(1), [1.1, 2.2])
        result = Quiver.read_set_floats_by_id(db, "AllTypes", "weight", Int64(1))
        @test sort(result) == [1.1, 2.2]

        Quiver.close!(db)
    end

    # ============================================================================
    # Error handling tests for new update functions
    # ============================================================================

    @testset "Vector Integers Invalid Collection" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        @test_throws Quiver.DatabaseException Quiver.update_element!(db, "NonexistentCollection", Int64(1); value_int = [1, 2, 3])

        Quiver.close!(db)
    end

    @testset "Set Strings Invalid Collection" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "collections.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")

        @test_throws Quiver.DatabaseException Quiver.update_element!(
            db,
            "NonexistentCollection",
            Int64(1);
            tag = ["tag1"],
        )

        Quiver.close!(db)
    end

    # ============================================================================
    # Update element FK label resolution tests
    # ============================================================================

    @testset "Update Element Scalar FK Label" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Parent"; label = "Parent 1")
        Quiver.create_element!(db, "Parent"; label = "Parent 2")
        Quiver.create_element!(db, "Child"; label = "Child 1", parent_id = "Parent 1")

        # Update child: change parent_id to Parent 2 using string label
        Quiver.update_element!(db, "Child", Int64(1); parent_id = "Parent 2")

        # Verify: parent_id resolved to Parent 2's ID (2)
        parent_ids = Quiver.read_scalar_integers(db, "Child", "parent_id")
        @test parent_ids == [2]

        Quiver.close!(db)
    end

    @testset "Update Element Scalar FK Integer" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Parent"; label = "Parent 1")
        Quiver.create_element!(db, "Parent"; label = "Parent 2")
        Quiver.create_element!(db, "Child"; label = "Child 1", parent_id = 1)

        # Update child: change parent_id to 2 using integer ID directly
        Quiver.update_element!(db, "Child", Int64(1); parent_id = 2)

        # Verify: parent_id updated to 2
        parent_ids = Quiver.read_scalar_integers(db, "Child", "parent_id")
        @test parent_ids == [2]

        Quiver.close!(db)
    end

    @testset "Update Element Vector FK Labels" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Parent"; label = "Parent 1")
        Quiver.create_element!(db, "Parent"; label = "Parent 2")
        Quiver.create_element!(db, "Child"; label = "Child 1", parent_ref = ["Parent 1"])

        # Update child: change vector FK to [Parent 2, Parent 1]
        Quiver.update_element!(db, "Child", Int64(1); parent_ref = ["Parent 2", "Parent 1"])

        # Verify: vector resolved to [2, 1] (order preserved)
        refs = Quiver.read_vector_integers_by_id(db, "Child", "parent_ref", Int64(1))
        @test refs == [2, 1]

        Quiver.close!(db)
    end

    @testset "Update Element Set FK Labels" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Parent"; label = "Parent 1")
        Quiver.create_element!(db, "Parent"; label = "Parent 2")
        Quiver.create_element!(db, "Child"; label = "Child 1", mentor_id = ["Parent 1"])

        # Update child: change set FK to [Parent 2]
        Quiver.update_element!(db, "Child", Int64(1); mentor_id = ["Parent 2"])

        # Verify: set resolved to [2]
        mentors = Quiver.read_set_integers_by_id(db, "Child", "mentor_id", Int64(1))
        @test mentors == [2]

        Quiver.close!(db)
    end

    @testset "Update Element Time Series FK Labels" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Parent"; label = "Parent 1")
        Quiver.create_element!(db, "Parent"; label = "Parent 2")
        Quiver.create_element!(db, "Child";
            label = "Child 1",
            date_time = ["2024-01-01"],
            sponsor_id = ["Parent 1"],
        )

        # Update child: change time series FK to [Parent 2, Parent 1]
        Quiver.update_element!(db, "Child", Int64(1);
            date_time = ["2024-06-01", "2024-06-02"],
            sponsor_id = ["Parent 2", "Parent 1"],
        )

        # Verify: time series resolved to [2, 1]
        result = Quiver.read_time_series_group(db, "Child", "events", Int64(1))
        @test result["sponsor_id"] == [2, 1]
        @test length(result["date_time"]) == 2

        Quiver.close!(db)
    end

    @testset "Update Element All FK Types In One Call" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Parent"; label = "Parent 1")
        Quiver.create_element!(db, "Parent"; label = "Parent 2")

        # Create child with all FK types pointing to Parent 1
        Quiver.create_element!(db, "Child";
            label = "Child 1",
            parent_id = "Parent 1",
            mentor_id = ["Parent 1"],
            parent_ref = ["Parent 1"],
            date_time = ["2024-01-01"],
            sponsor_id = ["Parent 1"],
        )

        # Update child: change all FK types to Parent 2
        Quiver.update_element!(db, "Child", Int64(1);
            parent_id = "Parent 2",
            mentor_id = ["Parent 2"],
            parent_ref = ["Parent 2"],
            date_time = ["2025-01-01"],
            sponsor_id = ["Parent 2"],
        )

        # Verify scalar FK
        parent_ids = Quiver.read_scalar_integers(db, "Child", "parent_id")
        @test parent_ids == [2]

        # Verify set FK
        mentors = Quiver.read_set_integers_by_id(db, "Child", "mentor_id", Int64(1))
        @test mentors == [2]

        # Verify vector FK
        refs = Quiver.read_vector_integers_by_id(db, "Child", "parent_ref", Int64(1))
        @test refs == [2]

        # Verify time series FK
        ts = Quiver.read_time_series_group(db, "Child", "events", Int64(1))
        @test ts["sponsor_id"] == [2]

        Quiver.close!(db)
    end

    @testset "Update Element No FK Columns Unchanged" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration";
            label = "Config 1",
            integer_attribute = 42,
            float_attribute = 3.14,
            string_attribute = "hello",
        )

        # Update scalar attributes in a non-FK schema
        Quiver.update_element!(db, "Configuration", Int64(1);
            integer_attribute = 100,
            float_attribute = 2.71,
            string_attribute = "world",
        )

        # Verify values updated correctly (pre-resolve passthrough safe for non-FK schemas)
        @test Quiver.read_scalar_integer_by_id(db, "Configuration", "integer_attribute", Int64(1)) == 100
        @test Quiver.read_scalar_float_by_id(db, "Configuration", "float_attribute", Int64(1)) == 2.71
        @test Quiver.read_scalar_string_by_id(db, "Configuration", "string_attribute", Int64(1)) == "world"

        Quiver.close!(db)
    end

    @testset "Update Element FK Resolution Failure Preserves Existing" begin
        path_schema = joinpath(tests_path(), "schemas", "valid", "relations.sql")
        db = Quiver.from_schema(":memory:", path_schema)

        Quiver.create_element!(db, "Configuration"; label = "Test Config")
        Quiver.create_element!(db, "Parent"; label = "Parent 1")
        Quiver.create_element!(db, "Child"; label = "Child 1", parent_id = "Parent 1")

        # Attempt update with nonexistent FK label
        @test_throws Quiver.DatabaseException Quiver.update_element!(
            db,
            "Child",
            Int64(1);
            parent_id = "Nonexistent Parent",
        )

        # Verify original value preserved
        parent_ids = Quiver.read_scalar_integers(db, "Child", "parent_id")
        @test parent_ids == [1]

        Quiver.close!(db)
    end
end

end
