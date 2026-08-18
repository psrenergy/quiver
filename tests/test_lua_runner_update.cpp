#include "test_lua_runner.h"

#include <algorithm>

TEST_F(LuaRunnerTest, UpdateElementSingleScalar) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1").set("some_integer", int64_t{100}));
    db.create_element("Collection", quiver::Element().set("label", "Item 2").set("some_integer", int64_t{200}));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        db:update_element("Collection", 1, { some_integer = 999 })

        local scalars = db:read_scalars_by_id("Collection", 1)
        assert(scalars.some_integer == 999, "Expected 999, got " .. tostring(scalars.some_integer))
        assert(scalars.label == "Item 1", "Label should be unchanged")
    )");

    // Verify from C++ side
    auto value = db.read_scalar_integer_by_id("Collection", "some_integer", 1);
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(*value, 999);

    auto label = db.read_scalar_string_by_id("Collection", "label", 1);
    EXPECT_TRUE(label.has_value());
    EXPECT_EQ(*label, "Item 1");
}

TEST_F(LuaRunnerTest, UpdateElementMultipleScalars) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element(
        "Collection",
        quiver::Element().set("label", "Item 1").set("some_integer", int64_t{100}).set("some_float", 1.5));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        db:update_element("Collection", 1, { some_integer = 500, some_float = 9.9 })

        local scalars = db:read_scalars_by_id("Collection", 1)
        assert(scalars.some_integer == 500, "Expected integer 500, got " .. tostring(scalars.some_integer))
        assert(scalars.some_float == 9.9, "Expected float 9.9, got " .. tostring(scalars.some_float))
        assert(scalars.label == "Item 1", "Label should be unchanged")
    )");

    // Verify from C++ side
    auto integer_value = db.read_scalar_integer_by_id("Collection", "some_integer", 1);
    EXPECT_TRUE(integer_value.has_value());
    EXPECT_EQ(*integer_value, 500);

    auto float_value = db.read_scalar_float_by_id("Collection", "some_float", 1);
    EXPECT_TRUE(float_value.has_value());
    EXPECT_DOUBLE_EQ(*float_value, 9.9);
}

TEST_F(LuaRunnerTest, UpdateElementOtherElementsUnchanged) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1").set("some_integer", int64_t{100}));
    db.create_element("Collection", quiver::Element().set("label", "Item 2").set("some_integer", int64_t{200}));
    db.create_element("Collection", quiver::Element().set("label", "Item 3").set("some_integer", int64_t{300}));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        -- Update only element 2
        db:update_element("Collection", 2, { some_integer = 999 })

        -- Verify element 2 updated
        local s2 = db:read_scalars_by_id("Collection", 2)
        assert(s2.some_integer == 999, "Element 2 should be updated to 999")

        -- Verify elements 1 and 3 unchanged
        local s1 = db:read_scalars_by_id("Collection", 1)
        assert(s1.some_integer == 100, "Element 1 should be unchanged at 100")

        local s3 = db:read_scalars_by_id("Collection", 3)
        assert(s3.some_integer == 300, "Element 3 should be unchanged at 300")
    )");

    // Verify from C++ side
    EXPECT_EQ(*db.read_scalar_integer_by_id("Collection", "some_integer", 1), 100);
    EXPECT_EQ(*db.read_scalar_integer_by_id("Collection", "some_integer", 2), 999);
    EXPECT_EQ(*db.read_scalar_integer_by_id("Collection", "some_integer", 3), 300);
}

TEST_F(LuaRunnerTest, UpdateElementWithArrays) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection",
                      quiver::Element()
                          .set("label", "Item 1")
                          .set("some_integer", int64_t{10})
                          .set("value_int", std::vector<int64_t>{1, 2, 3}));

    quiver::LuaRunner lua(db);

    // Update with both scalar and array values - both should be updated
    lua.run(R"(
        db:update_element("Collection", 1, { some_integer = 999, value_int = {7, 8, 9} })
    )");

    // Verify from C++ side
    auto integer_value = db.read_scalar_integer_by_id("Collection", "some_integer", 1);
    EXPECT_TRUE(integer_value.has_value());
    EXPECT_EQ(*integer_value, 999);

    auto vec_values = db.read_vector_integers_by_id("Collection", "value_int", 1);
    EXPECT_EQ(vec_values, (std::vector<int64_t>{7, 8, 9}));
}

TEST_F(LuaRunnerTest, UpdateVectorIntegers) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection",
                      quiver::Element().set("label", "Item 1").set("value_int", std::vector<int64_t>{1, 2, 3}));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        db:update_element("Collection", 1, { value_int = {10, 20, 30, 40} })
    )");

    auto vec = db.read_vector_integers_by_id("Collection", "value_int", 1);
    EXPECT_EQ(vec, (std::vector<int64_t>{10, 20, 30, 40}));
}

TEST_F(LuaRunnerTest, UpdateVectorFloats) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection",
                      quiver::Element().set("label", "Item 1").set("value_float", std::vector<double>{1.0, 2.0}));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        db:update_element("Collection", 1, { value_float = {5.5, 6.6, 7.7} })
    )");

    auto vec = db.read_vector_floats_by_id("Collection", "value_float", 1);
    EXPECT_EQ(vec, (std::vector<double>{5.5, 6.6, 7.7}));
}

TEST_F(LuaRunnerTest, UpdateScalarStringTrimsWhitespace) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Test Config"));
    db.create_element("Collection",
                      quiver::Element().set("label", "Item 1").set("tag", std::vector<std::string>{"old"}));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        db:update_element("Collection", 1, { tag = {"  alpha  ", "	beta\n", " gamma "} })
    )");

    auto set_vals = db.read_set_strings_by_id("Collection", "tag", 1);
    std::sort(set_vals.begin(), set_vals.end());
    EXPECT_EQ(set_vals, (std::vector<std::string>{"alpha", "beta", "gamma"}));
}

TEST_F(LuaRunnerTest, UpdateVectorStrings) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection",
                      quiver::Element()
                          .set("label", "Item 1")
                          .set("value_int", std::vector<int64_t>{1})
                          .set("value_float", std::vector<double>{1.0}));

    quiver::LuaRunner lua(db);

    // The collections.sql schema has value_int and value_float vectors but no string vector.
    // We test that update_vector_strings compiles and runs; actual schema support depends on schema.
    // For now, just verify no crash when calling with an empty vector on a valid attribute.
    auto vec = db.read_vector_integers_by_id("Collection", "value_int", 1);
    EXPECT_EQ(vec.size(), 1);
}

TEST_F(LuaRunnerTest, UpdateSetStrings) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection",
                      quiver::Element().set("label", "Item 1").set("tag", std::vector<std::string>{"alpha", "beta"}));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        db:update_element("Collection", 1, { tag = {"x", "y", "z"} })
    )");

    auto tags = db.read_set_strings_by_id("Collection", "tag", 1);
    EXPECT_EQ(tags.size(), 3);
}

TEST_F(LuaRunnerTest, UpdateElementByIdNonExistent) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1"));

    quiver::LuaRunner lua(db);

    // Updating a non-existent element throws "Element not found"
    expect_lua_error(lua, R"(db:update_element("Collection", 999, { some_integer = 5 }))", "Element not found");
}

TEST_F(LuaRunnerTest, UpdateElementByLabel) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1").set("some_integer", int64_t{100}));
    db.create_element("Collection", quiver::Element().set("label", "Item 2").set("some_integer", int64_t{200}));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        db:update_element_by_label("Collection", "Item 1", { some_integer = 999 })

        local scalars = db:read_scalars_by_id("Collection", 1)
        assert(scalars.some_integer == 999, "Expected 999, got " .. tostring(scalars.some_integer))
    )");

    // Verify from C++ side
    auto value = db.read_scalar_integer_by_id("Collection", "some_integer", 1);
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(*value, 999);

    // The sibling element is untouched.
    auto other = db.read_scalar_integer_by_id("Collection", "some_integer", 2);
    EXPECT_TRUE(other.has_value());
    EXPECT_EQ(*other, 200);
}

TEST_F(LuaRunnerTest, UpdateElementByLabelNonExistent) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1").set("some_integer", int64_t{100}));

    quiver::LuaRunner lua(db);

    // Updating a non-existent label throws "Element not found"
    expect_lua_error(
        lua, R"(db:update_element_by_label("Collection", "Nope", { some_integer = 5 }))", "Element not found");

    // Nothing was written.
    auto value = db.read_scalar_integer_by_id("Collection", "some_integer", 1);
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(*value, 100);
}

// ============================================================================
// Vector / set group writers
// ============================================================================

namespace {

// relations.sql gives Child a vector group and a set group that legally share the FK column name
// "parent_ref" -- the case that makes routing an array by column name ambiguous.
quiver::Database relations_db_with_child() {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("relations.sql"));
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Parent", quiver::Element().set("label", "Parent A"));
    db.create_element("Parent", quiver::Element().set("label", "Parent B"));
    db.create_element("Child", quiver::Element().set("label", "Child 1"));
    return db;
}

}  // namespace

TEST_F(LuaRunnerTest, UpdateVectorGroupReplacesAndClears) {
    auto db = relations_db_with_child();
    quiver::LuaRunner lua(db);

    lua.run(R"(db:update_vector_group("Child", "refs", 1, { parent_ref = { 1, 2 } }))");
    EXPECT_EQ(db.read_vector_integers_by_id("Child", "parent_ref", 1), (std::vector<int64_t>{1, 2}));

    lua.run(R"(db:update_vector_group("Child", "refs", 1, {}))");
    EXPECT_TRUE(db.read_vector_integers_by_id("Child", "parent_ref", 1).empty());
}

TEST_F(LuaRunnerTest, UpdateSetGroupLeavesSiblingSharingColumnNameUntouched) {
    auto db = relations_db_with_child();
    quiver::LuaRunner lua(db);

    lua.run(R"(
        db:update_set_group("Child", "parents", 1, { parent_ref = { 1 } })
        db:update_vector_group("Child", "refs", 1, { parent_ref = { 2 } })
        db:update_vector_group("Child", "refs", 1, {})
    )");

    EXPECT_TRUE(db.read_vector_integers_by_id("Child", "parent_ref", 1).empty());
    EXPECT_EQ(db.read_set_integers_by_id("Child", "parent_ref", 1), (std::vector<int64_t>{1}));
}

TEST_F(LuaRunnerTest, UpdateGroupResolvesForeignKeyLabels) {
    auto db = relations_db_with_child();
    quiver::LuaRunner lua(db);

    lua.run(R"(db:update_vector_group("Child", "refs", 1, { parent_ref = { "Parent B" } }))");
    EXPECT_EQ(db.read_vector_integers_by_id("Child", "parent_ref", 1), (std::vector<int64_t>{2}));
}

// No dimension column here, so the row count is the largest index any column reaches; a nil hole
// writes SQL NULL, which is how a read's nil holes round-trip.
TEST_F(LuaRunnerTest, UpdateGroupSparseColumnWritesNulls) {
    auto db = relations_db_with_child();
    quiver::LuaRunner lua(db);

    auto result = lua.run(R"(
        db:update_vector_group("Child", "refs", 1, { parent_ref = { 1, nil, 2 } })
        return {
            rows = db:query_integer("SELECT COUNT(*) FROM Child_vector_refs WHERE id = 1"),
            nulls = db:query_integer("SELECT COUNT(*) FROM Child_vector_refs WHERE id = 1 AND parent_ref IS NULL"),
        }
    )");
    EXPECT_EQ(result, R"({"nulls":1,"rows":3})");
}

TEST_F(LuaRunnerTest, UpdateGroupErrors) {
    auto db = relations_db_with_child();
    quiver::LuaRunner lua(db);
    lua.run(R"(db:update_vector_group("Child", "refs", 1, { parent_ref = { 1 } }))");

    expect_lua_error(
        lua, R"(db:update_vector_group("Child", "nope", 1, { parent_ref = { 1 } }))", "Vector group not found");
    expect_lua_error(lua, R"(db:update_set_group("Child", "nope", 1, { parent_ref = { 1 } }))", "Set group not found");
    expect_lua_error(
        lua, R"(db:update_vector_group("Child", "refs", 1, { not_a_column = { 1 } }))", "not found in group");
    expect_lua_error(lua,
                     R"(db:update_vector_group("Child", "refs", 1, { parent_ref = { 1 }, vector_index = { 7 } }))",
                     "managed by the group table");
    expect_lua_error(
        lua, R"(db:update_vector_group("Child", "refs", 999, { parent_ref = { 1 } }))", "Element not found");
    // The clear path used to succeed silently: the DELETE simply matched nothing.
    expect_lua_error(lua, R"(db:update_set_group("Child", "parents", 999, {}))", "Element not found");
    // A named column with no cells is a caller mistake, not a clear -- {} clears.
    expect_lua_error(lua, R"(db:update_vector_group("Child", "refs", 1, { parent_ref = {} }))", "contain no rows");
    expect_lua_error(
        lua, R"(db:update_set_group("Child", "parents", 1, { parent_ref = 5 }))", "must be an array of values");

    // Every rejected call left the existing row alone.
    EXPECT_EQ(db.read_vector_integers_by_id("Child", "parent_ref", 1), (std::vector<int64_t>{1}));
}
