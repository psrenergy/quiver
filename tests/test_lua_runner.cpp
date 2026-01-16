#include "database_fixture.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <psr/database.h>
#include <psr/element.h>
#include <psr/lua_runner.h>

namespace fs = std::filesystem;

namespace {
std::string schema_path(const std::string& filename) {
    return (fs::path(__FILE__).parent_path() / filename).string();
}
}  // namespace

class LuaRunnerTest : public ::testing::Test {
protected:
    void SetUp() override { collections_schema = schema_path("schemas/valid/collections.sql"); }
    std::string collections_schema;
};

TEST_F(LuaRunnerTest, CreateElementFromLua) {
    auto db = psr::Database::from_schema(":memory:", collections_schema);
    psr::LuaRunner lua(db);

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
    auto db = psr::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", psr::Element().set("label", "Test Config"));
    db.create_element("Collection", psr::Element().set("label", "Item 1"));
    db.create_element("Collection", psr::Element().set("label", "Item 2"));

    psr::LuaRunner lua(db);

    // Read from Lua and verify count
    lua.run(R"(
        local labels = db:read_scalar_strings("Collection", "label")
        assert(#labels == 2, "Expected 2 labels")
        assert(labels[1] == "Item 1", "First label mismatch")
        assert(labels[2] == "Item 2", "Second label mismatch")
    )");
}

TEST_F(LuaRunnerTest, LuaScriptError) {
    auto db = psr::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", psr::Element().set("label", "Test Config"));

    psr::LuaRunner lua(db);

    EXPECT_THROW({ lua.run("invalid lua syntax !!!"); }, std::runtime_error);
}

TEST_F(LuaRunnerTest, CreateElementWithArrays) {
    auto db = psr::Database::from_schema(":memory:", collections_schema);
    psr::LuaRunner lua(db);

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

    auto floats = db.read_vector_doubles("Collection", "value_float");
    EXPECT_EQ(floats.size(), 1);
    EXPECT_EQ(floats[0], (std::vector<double>{1.5, 2.5, 3.5}));
}

TEST_F(LuaRunnerTest, ReuseRunner) {
    auto db = psr::Database::from_schema(":memory:", collections_schema);
    psr::LuaRunner lua(db);

    lua.run(R"(db:create_element("Configuration", { label = "Test Config" }))");
    lua.run(R"(db:create_element("Collection", { label = "Item 1" }))");
    lua.run(R"(db:create_element("Collection", { label = "Item 2" }))");

    auto labels = db.read_scalar_strings("Collection", "label");
    EXPECT_EQ(labels.size(), 2);
}

TEST_F(LuaRunnerTest, ReadScalarIntegersFromLua) {
    auto db = psr::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", psr::Element().set("label", "Config"));
    db.create_element("Collection", psr::Element().set("label", "Item 1").set("some_integer", int64_t{10}));
    db.create_element("Collection", psr::Element().set("label", "Item 2").set("some_integer", int64_t{20}));
    db.create_element("Collection", psr::Element().set("label", "Item 3").set("some_integer", int64_t{30}));

    psr::LuaRunner lua(db);

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

TEST_F(LuaRunnerTest, ReadScalarDoublesFromLua) {
    auto db = psr::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", psr::Element().set("label", "Config"));
    db.create_element("Collection", psr::Element().set("label", "Item 1").set("some_float", 1.5));
    db.create_element("Collection", psr::Element().set("label", "Item 2").set("some_float", 2.5));

    psr::LuaRunner lua(db);

    lua.run(R"(
        local doubles = db:read_scalar_doubles("Collection", "some_float")
        assert(#doubles == 2, "Expected 2 doubles")
        assert(doubles[1] == 1.5, "First double should be 1.5")
        assert(doubles[2] == 2.5, "Second double should be 2.5")
    )");
}

TEST_F(LuaRunnerTest, ReadVectorIntegersFromLua) {
    auto db = psr::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", psr::Element().set("label", "Config"));
    db.create_element("Collection",
                      psr::Element().set("label", "Item 1").set("value_int", std::vector<int64_t>{1, 2, 3}));
    db.create_element("Collection",
                      psr::Element().set("label", "Item 2").set("value_int", std::vector<int64_t>{10, 20}));

    psr::LuaRunner lua(db);

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

TEST_F(LuaRunnerTest, ReadVectorDoublesFromLua) {
    auto db = psr::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", psr::Element().set("label", "Config"));
    db.create_element("Collection",
                      psr::Element().set("label", "Item 1").set("value_float", std::vector<double>{1.1, 2.2, 3.3}));

    psr::LuaRunner lua(db);

    lua.run(R"(
        local vectors = db:read_vector_doubles("Collection", "value_float")
        assert(#vectors == 1, "Expected 1 vector")
        assert(#vectors[1] == 3, "Vector should have 3 elements")
        assert(vectors[1][1] == 1.1, "vector[1] should be 1.1")
        assert(vectors[1][2] == 2.2, "vector[2] should be 2.2")
        assert(vectors[1][3] == 3.3, "vector[3] should be 3.3")
    )");
}

TEST_F(LuaRunnerTest, ReadEmptyVectorFromLua) {
    auto db = psr::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", psr::Element().set("label", "Config"));
    // Create element without vector data
    db.create_element("Collection", psr::Element().set("label", "Item 1"));

    psr::LuaRunner lua(db);

    // Reading vectors from elements that don't have vector data should return empty
    lua.run(R"(
        local vectors = db:read_vector_integers("Collection", "value_int")
        assert(#vectors == 0, "Expected 0 vectors for elements without vector data")
    )");
}

TEST_F(LuaRunnerTest, LuaRuntimeError) {
    auto db = psr::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", psr::Element().set("label", "Config"));

    psr::LuaRunner lua(db);

    // Test runtime error (not syntax error)
    EXPECT_THROW({ lua.run("error('This is a runtime error')"); }, std::runtime_error);
}

TEST_F(LuaRunnerTest, LuaAssertionFailure) {
    auto db = psr::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", psr::Element().set("label", "Config"));

    psr::LuaRunner lua(db);

    EXPECT_THROW({ lua.run("assert(false, 'Assertion failed!')"); }, std::runtime_error);
}

TEST_F(LuaRunnerTest, ComplexLuaScript) {
    auto db = psr::Database::from_schema(":memory:", collections_schema);
    psr::LuaRunner lua(db);

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
    auto db = psr::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", psr::Element().set("label", "Config"));
    int64_t id1 = db.create_element("Collection", psr::Element().set("label", "Item 1").set("some_integer", int64_t{42}));
    int64_t id2 =
        db.create_element("Collection", psr::Element().set("label", "Item 2").set("some_integer", int64_t{100}));

    psr::LuaRunner lua(db);

    std::string script = R"(
        local val1 = db:read_scalar_integers_by_id("Collection", "some_integer", )" +
                          std::to_string(id1) + R"()
        local val2 = db:read_scalar_integers_by_id("Collection", "some_integer", )" +
                          std::to_string(id2) + R"()
        assert(val1 == 42, "Expected 42, got " .. tostring(val1))
        assert(val2 == 100, "Expected 100, got " .. tostring(val2))
    )";
    lua.run(script);
}

TEST_F(LuaRunnerTest, ReadScalarDoubleByIdFromLua) {
    auto db = psr::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", psr::Element().set("label", "Config"));
    int64_t id1 = db.create_element("Collection", psr::Element().set("label", "Item 1").set("some_float", 3.14));

    psr::LuaRunner lua(db);

    std::string script = R"(
        local val1 = db:read_scalar_doubles_by_id("Collection", "some_float", )" +
                          std::to_string(id1) + R"()
        assert(val1 == 3.14, "Expected 3.14, got " .. tostring(val1))
    )";
    lua.run(script);
}

TEST_F(LuaRunnerTest, ReadScalarStringByIdFromLua) {
    auto db = psr::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", psr::Element().set("label", "Config"));
    int64_t id1 = db.create_element("Collection", psr::Element().set("label", "Item 1"));

    psr::LuaRunner lua(db);

    std::string script = R"(
        local val1 = db:read_scalar_strings_by_id("Collection", "label", )" +
                          std::to_string(id1) + R"()
        assert(val1 == "Item 1", "Expected 'Item 1', got " .. tostring(val1))
    )";
    lua.run(script);
}

TEST_F(LuaRunnerTest, ReadScalarByIdNotFoundFromLua) {
    auto db = psr::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", psr::Element().set("label", "Config"));
    db.create_element("Collection", psr::Element().set("label", "Item 1").set("some_integer", int64_t{42}));

    psr::LuaRunner lua(db);

    lua.run(R"(
        local val = db:read_scalar_integers_by_id("Collection", "some_integer", 999)
        assert(val == nil, "Expected nil for non-existent ID")
    )");
}

TEST_F(LuaRunnerTest, ReadVectorIntegerByIdFromLua) {
    auto db = psr::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", psr::Element().set("label", "Config"));
    int64_t id1 =
        db.create_element("Collection",
                          psr::Element().set("label", "Item 1").set("value_int", std::vector<int64_t>{1, 2, 3}));
    int64_t id2 =
        db.create_element("Collection",
                          psr::Element().set("label", "Item 2").set("value_int", std::vector<int64_t>{10, 20}));

    psr::LuaRunner lua(db);

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
    auto db = psr::Database::from_schema(":memory:", collections_schema);

    db.create_element("Configuration", psr::Element().set("label", "Config"));
    int64_t id1 = db.create_element(
        "Collection", psr::Element().set("label", "Item 1").set("tag", std::vector<std::string>{"important", "urgent"}));

    psr::LuaRunner lua(db);

    std::string script = R"(
        local tags = db:read_set_strings_by_id("Collection", "tag", )" +
                          std::to_string(id1) + R"()
        assert(#tags == 2, "Expected 2 tags, got " .. #tags)
    )";
    lua.run(script);
}
