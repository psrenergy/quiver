#include "test_lua_runner.h"

#include <algorithm>

TEST_F(LuaRunnerTest, CreateElement) {
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
    EXPECT_DOUBLE_EQ(*floats[0], 3.14);
}

TEST_F(LuaRunnerTest, CreateElementMissingLabel) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

    // Attempting to create element without required label should fail
    EXPECT_THROW({ lua.run(R"(db:create_element("Collection", { some_integer = 42 }))"); }, std::runtime_error);
}

TEST_F(LuaRunnerTest, CreateElementTrimsWhitespace) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::LuaRunner lua(db);

    lua.run(R"(
        db:create_element("Configuration", { label = "Test Config" })
        db:create_element("Collection", {
            label = "  Item 1  ",
            tag = {"  important  ", "	urgent\n", " review "}
        })
    )");

    auto labels = db.read_scalar_strings("Collection", "label");
    EXPECT_EQ(labels.size(), 1);
    EXPECT_EQ(labels[0], "Item 1");

    auto sets = db.read_set_strings("Collection", "tag");
    EXPECT_EQ(sets.size(), 1);
    auto tags = sets[0];
    std::sort(tags.begin(), tags.end());
    EXPECT_EQ(tags, (std::vector<std::string>{"important", "review", "urgent"}));
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

TEST_F(LuaRunnerTest, CreateElementInvalidCollection) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

    EXPECT_THROW({ lua.run(R"(db:create_element("NonexistentCollection", { label = "Test" }))"); }, std::runtime_error);
}

TEST_F(LuaRunnerTest, CreateElementUnsupportedAttributeTypeThrows) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    quiver::LuaRunner lua(db);

    try {
        lua.run(R"(db:create_element("Configuration", { label = "Item", enabled = true }))");
        FAIL() << "expected unsupported attribute type to throw";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find("Cannot table_to_element: attribute 'enabled'"), std::string::npos)
            << e.what();
    }
}

TEST_F(LuaRunnerTest, CreateElementUnsupportedArrayElementTypeThrows) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    quiver::LuaRunner lua(db);

    try {
        lua.run(R"(db:create_element("Configuration", { label = "Item", tags = { true, false } }))");
        FAIL() << "expected unsupported array element type to throw";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find("Cannot table_to_element: array 'tags'"), std::string::npos) << e.what();
    }
}
