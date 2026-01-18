module TestUpdate

using PSRDatabase
using Test

include("fixture.jl")

@testset "Update Element Single Scalar" begin
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

@testset "Update Element Multiple Scalars" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
    db = PSRDatabase.from_schema(":memory:", path_schema)

    # Create element
    PSRDatabase.create_element!(db, "Configuration"; label = "Config 1", integer_attribute = 100, float_attribute = 1.5)

    # Update multiple attributes at once
    PSRDatabase.update_element!(db, "Configuration", Int64(1); integer_attribute = 500, float_attribute = 9.9)

    # Verify updates
    int_value = PSRDatabase.read_scalar_integers_by_id(db, "Configuration", "integer_attribute", Int64(1))
    @test int_value == 500

    float_value = PSRDatabase.read_scalar_doubles_by_id(db, "Configuration", "float_attribute", Int64(1))
    @test float_value == 9.9

    # Verify label unchanged
    label = PSRDatabase.read_scalar_strings_by_id(db, "Configuration", "label", Int64(1))
    @test label == "Config 1"

    PSRDatabase.close!(db)
end

@testset "Update Element Other Elements Unchanged" begin
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

@testset "Update Element Arrays Ignored" begin
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

@testset "Update Element Using Element Builder" begin
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

@testset "Update Invalid Collection" begin
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

@testset "Update Invalid Element ID" begin
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

@testset "Update Invalid Attribute" begin
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

@testset "Update String Attribute" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
    db = PSRDatabase.from_schema(":memory:", path_schema)

    PSRDatabase.create_element!(db, "Configuration"; label = "Config 1", string_attribute = "original")

    PSRDatabase.update_element!(db, "Configuration", Int64(1); string_attribute = "updated")

    value = PSRDatabase.read_scalar_strings_by_id(db, "Configuration", "string_attribute", Int64(1))
    @test value == "updated"

    PSRDatabase.close!(db)
end

@testset "Update Float Attribute" begin
    path_schema = joinpath(tests_path(), "schemas", "valid", "basic.sql")
    db = PSRDatabase.from_schema(":memory:", path_schema)

    PSRDatabase.create_element!(db, "Configuration"; label = "Config 1", float_attribute = 1.5)

    PSRDatabase.update_element!(db, "Configuration", Int64(1); float_attribute = 99.99)

    value = PSRDatabase.read_scalar_doubles_by_id(db, "Configuration", "float_attribute", Int64(1))
    @test value == 99.99

    PSRDatabase.close!(db)
end

@testset "Update Multiple Elements Sequentially" begin
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

end
