#include "test_lua_runner.h"

TEST_F(LuaRunnerTest, LuaScriptError) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Test Config"));

    quiver::LuaRunner lua(db);

    EXPECT_THROW({ lua.run("invalid lua syntax !!!"); }, std::runtime_error);
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

    EXPECT_THROW({ lua.run(R"(db:delete_element("NonexistentCollection", 1))"); }, std::runtime_error);
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

TEST_F(LuaRunnerTest, QueryParameterUnsupportedTypeThrows) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

    // A silently dropped parameter would shift the remaining ones and return wrong results
    try {
        lua.run(R"(db:query_integer("SELECT id FROM Configuration WHERE label = ?", { true }))");
        FAIL() << "expected unsupported query parameter type to throw";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find("Cannot lua_table_to_values: parameter #1"), std::string::npos)
            << e.what();
    }
}
