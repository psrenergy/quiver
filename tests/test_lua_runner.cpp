#include "test_utils.h"

#include <gtest/gtest.h>
#include <quiver/database.h>
#include <quiver/element.h>
#include <quiver/lua_runner.h>

class LuaRunnerTest : public ::testing::Test {
protected:
    void SetUp() override { collections_schema = VALID_SCHEMA("collections.sql"); }
    std::string collections_schema;
};

TEST_F(LuaRunnerTest, CreateElementFromLua) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::LuaRunner lua(db);

    lua.run(R"(
        db:create_element("Configuration", { label = "Test Config" })
        db:create_element("Collection", { label = "Item 1", some_integer = 42, some_float = 3.14 })
    )");

    auto labels = db.read_scalar_strings("Collection", "label");
    EXPECT_EQ(labels.size(), 1);
    EXPECT_EQ(labels[0], "Item 1");

    auto integers = db.read_scalar_integers("Collection", "some_integer");
    EXPECT_EQ(integers[0], 42);
}

TEST_F(LuaRunnerTest, ReadScalarStringsFromLua) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Test Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1"));
    db.create_element("Collection", quiver::Element().set("label", "Item 2"));

    quiver::LuaRunner lua(db);

    // Read from Lua and verify count
    lua.run(R"(
        local labels = db:read_scalar_strings("Collection", "label")
        assert(#labels == 2, "Expected 2 labels")
        assert(labels[1] == "Item 1", "First label mismatch")
        assert(labels[2] == "Item 2", "Second label mismatch")
    )");
}

TEST_F(LuaRunnerTest, LuaScriptError) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Test Config"));

    quiver::LuaRunner lua(db);

    EXPECT_THROW({ lua.run("invalid lua syntax !!!"); }, std::runtime_error);
}

TEST_F(LuaRunnerTest, CreateElementWithArrays) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::LuaRunner lua(db);

    // Note: vector columns in the same table must have the same length
    lua.run(R"(
        db:create_element("Configuration", { label = "Test Config" })
        db:create_element("Collection", {
            label = "Item 1",
            value_int = {1, 2, 3},
            value_float = {1.5, 2.5, 3.5}
        })
    )");

    auto vectors = db.read_vector_integers("Collection", "value_int");
    EXPECT_EQ(vectors.size(), 1);
    EXPECT_EQ(vectors[0], (std::vector<int64_t>{1, 2, 3}));

    auto floats = db.read_vector_floats("Collection", "value_float");
    EXPECT_EQ(floats.size(), 1);
    EXPECT_EQ(floats[0], (std::vector<double>{1.5, 2.5, 3.5}));
}

TEST_F(LuaRunnerTest, ReuseRunner) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::LuaRunner lua(db);

    lua.run(R"(db:create_element("Configuration", { label = "Test Config" }))");
    lua.run(R"(db:create_element("Collection", { label = "Item 1" }))");
    lua.run(R"(db:create_element("Collection", { label = "Item 2" }))");

    auto labels = db.read_scalar_strings("Collection", "label");
    EXPECT_EQ(labels.size(), 2);
}

TEST_F(LuaRunnerTest, ReadScalarIntegersFromLua) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1").set("some_integer", int64_t{10}));
    db.create_element("Collection", quiver::Element().set("label", "Item 2").set("some_integer", int64_t{20}));
    db.create_element("Collection", quiver::Element().set("label", "Item 3").set("some_integer", int64_t{30}));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        local integers = db:read_scalar_integers("Collection", "some_integer")
        assert(#integers == 3, "Expected 3 integers, got " .. #integers)
        assert(integers[1] == 10, "First integer should be 10")
        assert(integers[2] == 20, "Second integer should be 20")
        assert(integers[3] == 30, "Third integer should be 30")

        -- Test sum
        local sum = 0
        for i, v in ipairs(integers) do
            sum = sum + v
        end
        assert(sum == 60, "Sum should be 60, got " .. sum)
    )");
}

TEST_F(LuaRunnerTest, ReadScalarFloatsFromLua) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1").set("some_float", 1.5));
    db.create_element("Collection", quiver::Element().set("label", "Item 2").set("some_float", 2.5));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        local floats = db:read_scalar_floats("Collection", "some_float")
        assert(#floats == 2, "Expected 2 floats")
        assert(floats[1] == 1.5, "First float should be 1.5")
        assert(floats[2] == 2.5, "Second float should be 2.5")
    )");
}

TEST_F(LuaRunnerTest, ReadVectorIntegersFromLua) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection",
                      quiver::Element().set("label", "Item 1").set("value_int", std::vector<int64_t>{1, 2, 3}));
    db.create_element("Collection",
                      quiver::Element().set("label", "Item 2").set("value_int", std::vector<int64_t>{10, 20}));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        local vectors = db:read_vector_integers("Collection", "value_int")
        assert(#vectors == 2, "Expected 2 vectors")

        -- Check first vector
        assert(#vectors[1] == 3, "First vector should have 3 elements")
        assert(vectors[1][1] == 1, "First vector[1] should be 1")
        assert(vectors[1][2] == 2, "First vector[2] should be 2")
        assert(vectors[1][3] == 3, "First vector[3] should be 3")

        -- Check second vector
        assert(#vectors[2] == 2, "Second vector should have 2 elements")
        assert(vectors[2][1] == 10, "Second vector[1] should be 10")
        assert(vectors[2][2] == 20, "Second vector[2] should be 20")
    )");
}

TEST_F(LuaRunnerTest, ReadVectorFloatsFromLua) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection",
                      quiver::Element().set("label", "Item 1").set("value_float", std::vector<double>{1.1, 2.2, 3.3}));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        local vectors = db:read_vector_floats("Collection", "value_float")
        assert(#vectors == 1, "Expected 1 vector")
        assert(#vectors[1] == 3, "Vector should have 3 elements")
        assert(vectors[1][1] == 1.1, "vector[1] should be 1.1")
        assert(vectors[1][2] == 2.2, "vector[2] should be 2.2")
        assert(vectors[1][3] == 3.3, "vector[3] should be 3.3")
    )");
}

TEST_F(LuaRunnerTest, ReadEmptyVectorFromLua) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    // Create element without vector data
    db.create_element("Collection", quiver::Element().set("label", "Item 1"));

    quiver::LuaRunner lua(db);

    // Reading vectors from elements that don't have vector data should return empty
    lua.run(R"(
        local vectors = db:read_vector_integers("Collection", "value_int")
        assert(#vectors == 0, "Expected 0 vectors for elements without vector data")
    )");
}

TEST_F(LuaRunnerTest, LuaRuntimeError) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

    // Test runtime error (not syntax error)
    EXPECT_THROW({ lua.run("error('This is a runtime error')"); }, std::runtime_error);
}

TEST_F(LuaRunnerTest, LuaAssertionFailure) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

    EXPECT_THROW({ lua.run("assert(false, 'Assertion failed!')"); }, std::runtime_error);
}

TEST_F(LuaRunnerTest, ComplexLuaScript) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::LuaRunner lua(db);

    // A more complex script that uses multiple operations
    lua.run(R"lua(
        -- Create configuration
        db:create_element("Configuration", { label = "Main Config" })

        -- Create multiple elements with different data
        for i = 1, 5 do
            db:create_element("Collection", {
                label = "Item " .. i,
                some_integer = i * 10,
                some_float = i * 1.5
            })
        end

        -- Verify data
        local labels = db:read_scalar_strings("Collection", "label")
        assert(#labels == 5, "Should have 5 items")

        local integers = db:read_scalar_integers("Collection", "some_integer")
        local sum = 0
        for _, v in ipairs(integers) do
            sum = sum + v
        end
        assert(sum == 150, "Sum should be 150")
    )lua");

    // Verify from C++ side
    auto labels = db.read_scalar_strings("Collection", "label");
    EXPECT_EQ(labels.size(), 5);
    EXPECT_EQ(labels[0], "Item 1");
    EXPECT_EQ(labels[4], "Item 5");
}

// ============================================================================
// Read by ID tests
// ============================================================================

TEST_F(LuaRunnerTest, ReadScalarIntegerByIdFromLua) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    int64_t id1 =
        db.create_element("Collection", quiver::Element().set("label", "Item 1").set("some_integer", int64_t{42}));
    int64_t id2 =
        db.create_element("Collection", quiver::Element().set("label", "Item 2").set("some_integer", int64_t{100}));

    quiver::LuaRunner lua(db);

    std::string script = R"(
        local val1 = db:read_scalar_integer_by_id("Collection", "some_integer", )" +
                         std::to_string(id1) + R"()
        local val2 = db:read_scalar_integer_by_id("Collection", "some_integer", )" +
                         std::to_string(id2) + R"()
        assert(val1 == 42, "Expected 42, got " .. tostring(val1))
        assert(val2 == 100, "Expected 100, got " .. tostring(val2))
    )";
    lua.run(script);
}

TEST_F(LuaRunnerTest, ReadScalarFloatByIdFromLua) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    int64_t id1 = db.create_element("Collection", quiver::Element().set("label", "Item 1").set("some_float", 3.14));

    quiver::LuaRunner lua(db);

    std::string script = R"(
        local val1 = db:read_scalar_float_by_id("Collection", "some_float", )" +
                         std::to_string(id1) + R"()
        assert(val1 == 3.14, "Expected 3.14, got " .. tostring(val1))
    )";
    lua.run(script);
}

TEST_F(LuaRunnerTest, ReadScalarStringByIdFromLua) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    int64_t id1 = db.create_element("Collection", quiver::Element().set("label", "Item 1"));

    quiver::LuaRunner lua(db);

    std::string script = R"(
        local val1 = db:read_scalar_string_by_id("Collection", "label", )" +
                         std::to_string(id1) + R"()
        assert(val1 == "Item 1", "Expected 'Item 1', got " .. tostring(val1))
    )";
    lua.run(script);
}

TEST_F(LuaRunnerTest, ReadScalarByIdNotFoundFromLua) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1").set("some_integer", int64_t{42}));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        local val = db:read_scalar_integer_by_id("Collection", "some_integer", 999)
        assert(val == nil, "Expected nil for non-existent ID")
    )");
}

TEST_F(LuaRunnerTest, ReadVectorIntegerByIdFromLua) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    int64_t id1 = db.create_element(
        "Collection", quiver::Element().set("label", "Item 1").set("value_int", std::vector<int64_t>{1, 2, 3}));
    int64_t id2 = db.create_element(
        "Collection", quiver::Element().set("label", "Item 2").set("value_int", std::vector<int64_t>{10, 20}));

    quiver::LuaRunner lua(db);

    std::string script = R"(
        local vec1 = db:read_vector_integers_by_id("Collection", "value_int", )" +
                         std::to_string(id1) + R"()
        assert(#vec1 == 3, "Expected 3 elements in vec1")
        assert(vec1[1] == 1, "vec1[1] should be 1")
        assert(vec1[2] == 2, "vec1[2] should be 2")
        assert(vec1[3] == 3, "vec1[3] should be 3")

        local vec2 = db:read_vector_integers_by_id("Collection", "value_int", )" +
                         std::to_string(id2) + R"()
        assert(#vec2 == 2, "Expected 2 elements in vec2")
        assert(vec2[1] == 10, "vec2[1] should be 10")
        assert(vec2[2] == 20, "vec2[2] should be 20")
    )";
    lua.run(script);
}

TEST_F(LuaRunnerTest, ReadSetStringsByIdFromLua) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    int64_t id1 = db.create_element(
        "Collection",
        quiver::Element().set("label", "Item 1").set("tag", std::vector<std::string>{"important", "urgent"}));

    quiver::LuaRunner lua(db);

    std::string script = R"(
        local tags = db:read_set_strings_by_id("Collection", "tag", )" +
                         std::to_string(id1) + R"()
        assert(#tags == 2, "Expected 2 tags, got " .. #tags)
    )";
    lua.run(script);
}

// ============================================================================
// Read element IDs tests
// ============================================================================

TEST_F(LuaRunnerTest, ReadElementIdsFromLua) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    int64_t id1 = db.create_element("Collection", quiver::Element().set("label", "Item 1"));
    int64_t id2 = db.create_element("Collection", quiver::Element().set("label", "Item 2"));
    int64_t id3 = db.create_element("Collection", quiver::Element().set("label", "Item 3"));

    quiver::LuaRunner lua(db);

    std::string script = R"(
        local ids = db:read_element_ids("Collection")
        assert(#ids == 3, "Expected 3 IDs, got " .. #ids)
        assert(ids[1] == )" +
                         std::to_string(id1) + R"(, "First ID mismatch")
        assert(ids[2] == )" +
                         std::to_string(id2) + R"(, "Second ID mismatch")
        assert(ids[3] == )" +
                         std::to_string(id3) + R"(, "Third ID mismatch")
    )";
    lua.run(script);
}

TEST_F(LuaRunnerTest, ReadElementIdsEmptyFromLua) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        local ids = db:read_element_ids("Collection")
        assert(#ids == 0, "Expected 0 IDs for empty collection, got " .. #ids)
    )");
}

// ============================================================================
// Delete element by ID tests
// ============================================================================

TEST_F(LuaRunnerTest, DeleteElementByIdFromLua) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1"));
    db.create_element("Collection", quiver::Element().set("label", "Item 2"));
    db.create_element("Collection", quiver::Element().set("label", "Item 3"));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        local ids = db:read_element_ids("Collection")
        assert(#ids == 3, "Expected 3 elements before delete")

        db:delete_element_by_id("Collection", 2)

        ids = db:read_element_ids("Collection")
        assert(#ids == 2, "Expected 2 elements after delete")
    )");

    // Verify from C++ side
    auto ids = db.read_element_ids("Collection");
    EXPECT_EQ(ids.size(), 2);
    EXPECT_EQ(ids[0], 1);
    EXPECT_EQ(ids[1], 3);
}

TEST_F(LuaRunnerTest, DeleteElementByIdWithVectorDataFromLua) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection",
                      quiver::Element().set("label", "Item 1").set("value_int", std::vector<int64_t>{1, 2, 3}));
    db.create_element("Collection",
                      quiver::Element().set("label", "Item 2").set("value_int", std::vector<int64_t>{4, 5, 6}));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        db:delete_element_by_id("Collection", 1)

        local ids = db:read_element_ids("Collection")
        assert(#ids == 1, "Expected 1 element after delete")
        assert(ids[1] == 2, "Remaining element should have ID 2")
    )");

    // Verify vector data of remaining element is intact
    auto vectors = db.read_vector_integers("Collection", "value_int");
    EXPECT_EQ(vectors.size(), 1);
    EXPECT_EQ(vectors[0], (std::vector<int64_t>{4, 5, 6}));
}

TEST_F(LuaRunnerTest, DeleteElementByIdNonExistentFromLua) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1"));

    quiver::LuaRunner lua(db);

    // Deleting non-existent element should succeed silently (idempotent)
    lua.run(R"(
        db:delete_element_by_id("Collection", 999)

        local ids = db:read_element_ids("Collection")
        assert(#ids == 1, "Original element should still exist")
    )");
}

TEST_F(LuaRunnerTest, DeleteElementByIdOtherElementsUnchangedFromLua) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1").set("some_integer", int64_t{100}));
    db.create_element("Collection", quiver::Element().set("label", "Item 2").set("some_integer", int64_t{200}));
    db.create_element("Collection", quiver::Element().set("label", "Item 3").set("some_integer", int64_t{300}));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        db:delete_element_by_id("Collection", 2)

        local labels = db:read_scalar_strings("Collection", "label")
        assert(#labels == 2, "Expected 2 labels after delete")

        local integers = db:read_scalar_integers("Collection", "some_integer")
        assert(#integers == 2, "Expected 2 integers after delete")

        local sum = integers[1] + integers[2]
        assert(sum == 400, "Sum should be 400 (100 + 300), got " .. sum)
    )");

    // Verify from C++ side
    auto labels = db.read_scalar_strings("Collection", "label");
    EXPECT_EQ(labels.size(), 2);
    EXPECT_TRUE(std::find(labels.begin(), labels.end(), "Item 1") != labels.end());
    EXPECT_TRUE(std::find(labels.begin(), labels.end(), "Item 3") != labels.end());
    EXPECT_TRUE(std::find(labels.begin(), labels.end(), "Item 2") == labels.end());
}

// ============================================================================
// Update element tests
// ============================================================================

TEST_F(LuaRunnerTest, UpdateElementSingleScalarFromLua) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1").set("some_integer", int64_t{100}));
    db.create_element("Collection", quiver::Element().set("label", "Item 2").set("some_integer", int64_t{200}));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        db:update_element("Collection", 1, { some_integer = 999 })

        local val = db:read_scalar_integer_by_id("Collection", "some_integer", 1)
        assert(val == 999, "Expected 999, got " .. tostring(val))

        -- Verify label unchanged
        local label = db:read_scalar_string_by_id("Collection", "label", 1)
        assert(label == "Item 1", "Label should be unchanged")
    )");

    // Verify from C++ side
    auto value = db.read_scalar_integer_by_id("Collection", "some_integer", 1);
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(*value, 999);

    auto label = db.read_scalar_string_by_id("Collection", "label", 1);
    EXPECT_TRUE(label.has_value());
    EXPECT_EQ(*label, "Item 1");
}

TEST_F(LuaRunnerTest, UpdateElementMultipleScalarsFromLua) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element(
        "Collection",
        quiver::Element().set("label", "Item 1").set("some_integer", int64_t{100}).set("some_float", 1.5));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        db:update_element("Collection", 1, { some_integer = 500, some_float = 9.9 })

        local integer_val = db:read_scalar_integer_by_id("Collection", "some_integer", 1)
        assert(integer_val == 500, "Expected integer 500, got " .. tostring(integer_val))

        local float_val = db:read_scalar_float_by_id("Collection", "some_float", 1)
        assert(float_val == 9.9, "Expected float 9.9, got " .. tostring(float_val))

        -- Verify label unchanged
        local label = db:read_scalar_string_by_id("Collection", "label", 1)
        assert(label == "Item 1", "Label should be unchanged")
    )");

    // Verify from C++ side
    auto integer_value = db.read_scalar_integer_by_id("Collection", "some_integer", 1);
    EXPECT_TRUE(integer_value.has_value());
    EXPECT_EQ(*integer_value, 500);

    auto float_value = db.read_scalar_float_by_id("Collection", "some_float", 1);
    EXPECT_TRUE(float_value.has_value());
    EXPECT_DOUBLE_EQ(*float_value, 9.9);
}

TEST_F(LuaRunnerTest, UpdateElementOtherElementsUnchangedFromLua) {
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
        local val2 = db:read_scalar_integer_by_id("Collection", "some_integer", 2)
        assert(val2 == 999, "Element 2 should be updated to 999")

        -- Verify elements 1 and 3 unchanged
        local val1 = db:read_scalar_integer_by_id("Collection", "some_integer", 1)
        assert(val1 == 100, "Element 1 should be unchanged at 100")

        local val3 = db:read_scalar_integer_by_id("Collection", "some_integer", 3)
        assert(val3 == 300, "Element 3 should be unchanged at 300")
    )");

    // Verify from C++ side
    EXPECT_EQ(*db.read_scalar_integer_by_id("Collection", "some_integer", 1), 100);
    EXPECT_EQ(*db.read_scalar_integer_by_id("Collection", "some_integer", 2), 999);
    EXPECT_EQ(*db.read_scalar_integer_by_id("Collection", "some_integer", 3), 300);
}

TEST_F(LuaRunnerTest, UpdateElementWithArraysFromLua) {
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

        -- Verify scalar was updated
        local integer_val = db:read_scalar_integer_by_id("Collection", "some_integer", 1)
        assert(integer_val == 999, "Scalar should be updated to 999")

        -- Verify vector was also updated
        local vec = db:read_vector_integers_by_id("Collection", "value_int", 1)
        assert(#vec == 3, "Vector should have 3 elements")
        assert(vec[1] == 7, "Vector[1] should be 7")
        assert(vec[2] == 8, "Vector[2] should be 8")
        assert(vec[3] == 9, "Vector[3] should be 9")
    )");

    // Verify from C++ side
    auto integer_value = db.read_scalar_integer_by_id("Collection", "some_integer", 1);
    EXPECT_TRUE(integer_value.has_value());
    EXPECT_EQ(*integer_value, 999);

    auto vec_values = db.read_vector_integers_by_id("Collection", "value_int", 1);
    EXPECT_EQ(vec_values, (std::vector<int64_t>{7, 8, 9}));
}

// ============================================================================
// LuaRunner error path tests
// ============================================================================

TEST_F(LuaRunnerTest, UndefinedVariableError) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

    EXPECT_THROW({ lua.run("local x = undefined_variable + 1"); }, std::runtime_error);
}

TEST_F(LuaRunnerTest, NilFunctionCallError) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

    EXPECT_THROW({ lua.run("local f = nil; f()"); }, std::runtime_error);
}

TEST_F(LuaRunnerTest, TableIndexError) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

    EXPECT_THROW({ lua.run("local t = nil; local x = t.field"); }, std::runtime_error);
}

TEST_F(LuaRunnerTest, CreateElementInvalidCollection) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

    EXPECT_THROW({ lua.run(R"(db:create_element("NonexistentCollection", { label = "Test" }))"); }, std::runtime_error);
}

TEST_F(LuaRunnerTest, MultipleScriptExecutions) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::LuaRunner lua(db);

    lua.run(R"(db:create_element("Configuration", { label = "Config" }))");
    lua.run(R"(db:create_element("Collection", { label = "Item 1" }))");
    lua.run(R"(db:create_element("Collection", { label = "Item 2" }))");
    lua.run(R"(db:create_element("Collection", { label = "Item 3" }))");

    auto labels = db.read_scalar_strings("Collection", "label");
    EXPECT_EQ(labels.size(), 3);
}

TEST_F(LuaRunnerTest, EmptyScriptSucceeds) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

    // Empty script should succeed
    lua.run("");
}

TEST_F(LuaRunnerTest, WhitespaceOnlyScriptSucceeds) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

    // Whitespace only script should succeed
    lua.run("   \n\t\n   ");
}

TEST_F(LuaRunnerTest, CommentOnlyScriptSucceeds) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

    // Comment only script should succeed
    lua.run("-- this is a comment\n-- another comment");
}

TEST_F(LuaRunnerTest, ReadFromNonExistentCollection) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

    EXPECT_THROW(
        { lua.run(R"(local x = db:read_scalar_strings("NonexistentCollection", "label"))"); }, std::runtime_error);
}

// ============================================================================
// Additional edge case tests
// ============================================================================

TEST_F(LuaRunnerTest, ReadScalarStringsEmpty) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

    // No Collection elements created, should return empty table
    lua.run(R"(
        local labels = db:read_scalar_strings("Collection", "label")
        assert(#labels == 0, "Expected empty table, got " .. #labels .. " items")
    )");
}

TEST_F(LuaRunnerTest, ReadScalarIntegersEmpty) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        local integers = db:read_scalar_integers("Collection", "some_integer")
        assert(#integers == 0, "Expected empty table, got " .. #integers .. " items")
    )");
}

TEST_F(LuaRunnerTest, ReadVectorIntegersEmpty) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        local vectors = db:read_vector_integers("Collection", "value_int")
        assert(#vectors == 0, "Expected empty table, got " .. #vectors .. " items")
    )");
}

TEST_F(LuaRunnerTest, ReadSetStringsByIdEmpty) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    // Create a collection element without any set values
    int64_t id = db.create_element("Collection", quiver::Element().set("label", "Item 1"));

    quiver::LuaRunner lua(db);

    std::string script = R"(
        local set = db:read_set_strings_by_id("Collection", "tag", )" +
                         std::to_string(id) + R"()
        assert(#set == 0, "Expected empty set, got " .. #set .. " items")
    )";
    lua.run(script);
}

TEST_F(LuaRunnerTest, CreateElementWithOnlyLabel) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::LuaRunner lua(db);

    lua.run(R"(
        db:create_element("Configuration", { label = "Test Config" })
        db:create_element("Collection", { label = "Item 1" })
    )");

    auto labels = db.read_scalar_strings("Collection", "label");
    EXPECT_EQ(labels.size(), 1);
    EXPECT_EQ(labels[0], "Item 1");
}

TEST_F(LuaRunnerTest, CreateElementMixedTypes) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::LuaRunner lua(db);

    lua.run(R"(
        db:create_element("Configuration", { label = "Test Config" })
        db:create_element("Collection", {
            label = "Item 1",
            some_integer = 42,
            some_float = 3.14
        })
    )");

    auto integers = db.read_scalar_integers("Collection", "some_integer");
    EXPECT_EQ(integers.size(), 1);
    EXPECT_EQ(integers[0], 42);

    auto floats = db.read_scalar_floats("Collection", "some_float");
    EXPECT_EQ(floats.size(), 1);
    EXPECT_DOUBLE_EQ(floats[0], 3.14);
}

TEST_F(LuaRunnerTest, ReadVectorIntegersByIdFromLua) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    int64_t id1 = db.create_element(
        "Collection", quiver::Element().set("label", "Item 1").set("value_int", std::vector<int64_t>{10, 20, 30}));

    quiver::LuaRunner lua(db);

    std::string script = R"(
        local vec = db:read_vector_integers_by_id("Collection", "value_int", )" +
                         std::to_string(id1) + R"()
        assert(#vec == 3, "Expected 3 elements, got " .. #vec)
        assert(vec[1] == 10, "First element should be 10")
        assert(vec[2] == 20, "Second element should be 20")
        assert(vec[3] == 30, "Third element should be 30")
    )";
    lua.run(script);
}

TEST_F(LuaRunnerTest, ReadVectorFloatsByIdFromLua) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    int64_t id1 = db.create_element(
        "Collection", quiver::Element().set("label", "Item 1").set("value_float", std::vector<double>{1.1, 2.2, 3.3}));

    quiver::LuaRunner lua(db);

    std::string script = R"(
        local vec = db:read_vector_floats_by_id("Collection", "value_float", )" +
                         std::to_string(id1) + R"()
        assert(#vec == 3, "Expected 3 elements, got " .. #vec)
        assert(vec[1] == 1.1, "First element should be 1.1")
        assert(vec[2] == 2.2, "Second element should be 2.2")
        assert(vec[3] == 3.3, "Third element should be 3.3")
    )";
    lua.run(script);
}

// ============================================================================
// Additional LuaRunner error handling tests
// ============================================================================

TEST_F(LuaRunnerTest, CreateElementMissingLabel) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

    // Attempting to create element without required label should fail
    EXPECT_THROW({ lua.run(R"(db:create_element("Collection", { some_integer = 42 }))"); }, std::runtime_error);
}

TEST_F(LuaRunnerTest, ReadNonExistentAttribute) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1"));

    quiver::LuaRunner lua(db);

    EXPECT_THROW({ lua.run(R"(local x = db:read_scalar_strings("Collection", "nonexistent"))"); }, std::runtime_error);
}

TEST_F(LuaRunnerTest, UpdateElementNonExistentCollection) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

    EXPECT_THROW(
        { lua.run(R"(db:update_element("NonexistentCollection", 1, { label = "Test" }))"); }, std::runtime_error);
}

TEST_F(LuaRunnerTest, DeleteFromNonExistentCollection) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

    EXPECT_THROW({ lua.run(R"(db:delete_element_by_id("NonexistentCollection", 1))"); }, std::runtime_error);
}

TEST_F(LuaRunnerTest, MultipleOperationsPartialFailure) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

    // First operation succeeds, second should fail
    EXPECT_THROW(
        {
            lua.run(R"(
            db:create_element("Collection", { label = "Item 1" })
            db:create_element("NonexistentCollection", { label = "Bad" })
        )");
        },
        std::runtime_error);

    // Verify first element was created before failure
    auto labels = db.read_scalar_strings("Collection", "label");
    EXPECT_EQ(labels.size(), 1);
    EXPECT_EQ(labels[0], "Item 1");
}

TEST_F(LuaRunnerTest, LuaTypeCoercionInteger) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::LuaRunner lua(db);

    lua.run(R"(
        db:create_element("Configuration", { label = "Config" })
        db:create_element("Collection", {
            label = "Item 1",
            some_integer = 42.0  -- Lua number that happens to be whole
        })
    )");

    auto integers = db.read_scalar_integers("Collection", "some_integer");
    EXPECT_EQ(integers.size(), 1);
    EXPECT_EQ(integers[0], 42);
}

TEST_F(LuaRunnerTest, ReadElementIdsFromNonExistentCollection) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

    EXPECT_THROW({ lua.run(R"(local ids = db:read_element_ids("NonexistentCollection"))"); }, std::runtime_error);
}

TEST_F(LuaRunnerTest, CreateElementWithSpecialCharactersInLabel) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::LuaRunner lua(db);

    lua.run(R"(
        db:create_element("Configuration", { label = "Config" })
        db:create_element("Collection", { label = "Test's \"special\" chars: <>&" })
    )");

    auto labels = db.read_scalar_strings("Collection", "label");
    EXPECT_EQ(labels.size(), 1);
    EXPECT_EQ(labels[0], "Test's \"special\" chars: <>&");
}

// ============================================================================
// Query method tests
// ============================================================================

TEST_F(LuaRunnerTest, QueryStringFromLua) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1").set("some_integer", int64_t{42}));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        local label = db:query_string("SELECT label FROM Collection WHERE label = ?", {"Item 1"})
        assert(label == "Item 1", "Expected 'Item 1', got " .. tostring(label))
    )");
}

TEST_F(LuaRunnerTest, QueryStringNoParamsFromLua) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1"));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        local label = db:query_string("SELECT label FROM Collection")
        assert(label == "Item 1", "Expected 'Item 1', got " .. tostring(label))
    )");
}

TEST_F(LuaRunnerTest, QueryStringReturnsNilFromLua) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        local result = db:query_string("SELECT label FROM Collection WHERE 1 = 0")
        assert(result == nil, "Expected nil, got " .. tostring(result))
    )");
}

TEST_F(LuaRunnerTest, QueryIntegerFromLua) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1").set("some_integer", int64_t{42}));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        local val = db:query_integer("SELECT some_integer FROM Collection WHERE label = ?", {"Item 1"})
        assert(val == 42, "Expected 42, got " .. tostring(val))
    )");
}

TEST_F(LuaRunnerTest, QueryIntegerCountFromLua) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1"));
    db.create_element("Collection", quiver::Element().set("label", "Item 2"));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        local count = db:query_integer("SELECT COUNT(*) FROM Collection")
        assert(count == 2, "Expected 2, got " .. tostring(count))
    )");
}

TEST_F(LuaRunnerTest, QueryFloatFromLua) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1").set("some_float", 3.14));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        local val = db:query_float("SELECT some_float FROM Collection WHERE label = ?", {"Item 1"})
        assert(val == 3.14, "Expected 3.14, got " .. tostring(val))
    )");
}

TEST_F(LuaRunnerTest, QueryWithMultipleParamsFromLua) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1").set("some_integer", int64_t{10}));
    db.create_element("Collection", quiver::Element().set("label", "Item 2").set("some_integer", int64_t{20}));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        local val = db:query_integer(
            "SELECT some_integer FROM Collection WHERE label = ? AND some_integer > ?",
            {"Item 2", 5}
        )
        assert(val == 20, "Expected 20, got " .. tostring(val))
    )");
}

TEST_F(LuaRunnerTest, LuaScriptWithUnicodeCharacters) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::LuaRunner lua(db);

    lua.run(R"(
        db:create_element("Configuration", { label = "配置" })
        db:create_element("Collection", { label = "项目 αβγ 🎉" })
    )");

    auto config_labels = db.read_scalar_strings("Configuration", "label");
    auto collection_labels = db.read_scalar_strings("Collection", "label");

    EXPECT_EQ(config_labels.size(), 1);
    EXPECT_EQ(collection_labels.size(), 1);
}
