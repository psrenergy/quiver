#include "test_utils.h"

#include <gtest/gtest.h>
#include <quiver/c/database.h>
#include <quiver/c/element.h>
#include <quiver/c/lua_runner.h>

class LuaRunnerCApiTest : public ::testing::Test {
protected:
    void SetUp() override { collections_schema = VALID_SCHEMA("collections.sql"); }
    std::string collections_schema;
};

TEST_F(LuaRunnerCApiTest, CreateAndDestroy) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", collections_schema.c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto lua = quiver_lua_runner_new(db);
    ASSERT_NE(lua, nullptr);

    quiver_lua_runner_free(lua);
    quiver_database_close(db);
}

TEST_F(LuaRunnerCApiTest, FreeNull) {
    quiver_lua_runner_free(nullptr);
}

TEST_F(LuaRunnerCApiTest, CreateWithNullDb) {
    auto lua = quiver_lua_runner_new(nullptr);
    EXPECT_EQ(lua, nullptr);
}

TEST_F(LuaRunnerCApiTest, RunSimpleScript) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", collections_schema.c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto lua = quiver_lua_runner_new(db);
    ASSERT_NE(lua, nullptr);

    auto result = quiver_lua_runner_run(lua, "local x = 1 + 1");
    EXPECT_EQ(result, QUIVER_OK);

    quiver_lua_runner_free(lua);
    quiver_database_close(db);
}

TEST_F(LuaRunnerCApiTest, RunNullScript) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", collections_schema.c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto lua = quiver_lua_runner_new(db);
    ASSERT_NE(lua, nullptr);

    auto result = quiver_lua_runner_run(lua, nullptr);
    EXPECT_EQ(result, QUIVER_ERROR_INVALID_ARGUMENT);

    quiver_lua_runner_free(lua);
    quiver_database_close(db);
}

TEST_F(LuaRunnerCApiTest, RunWithNullRunner) {
    auto result = quiver_lua_runner_run(nullptr, "local x = 1");
    EXPECT_EQ(result, QUIVER_ERROR_INVALID_ARGUMENT);
}

TEST_F(LuaRunnerCApiTest, CreateElement) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", collections_schema.c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto lua = quiver_lua_runner_new(db);
    ASSERT_NE(lua, nullptr);

    auto result = quiver_lua_runner_run(lua, R"(
        db:create_element("Configuration", { label = "Test Config" })
        db:create_element("Collection", { label = "Item 1", some_integer = 42 })
    )");
    EXPECT_EQ(result, QUIVER_OK);

    // Verify with C API read
    int64_t* values = nullptr;
    size_t count = 0;
    auto read_result = quiver_database_read_scalar_integers(db, "Collection", "some_integer", &values, &count);
    EXPECT_EQ(read_result, QUIVER_OK);
    EXPECT_EQ(count, 1);
    EXPECT_EQ(values[0], 42);
    quiver_free_integer_array(values);

    quiver_lua_runner_free(lua);
    quiver_database_close(db);
}

TEST_F(LuaRunnerCApiTest, SyntaxError) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", collections_schema.c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto lua = quiver_lua_runner_new(db);
    ASSERT_NE(lua, nullptr);

    auto result = quiver_lua_runner_run(lua, "invalid lua syntax !!!");
    EXPECT_NE(result, QUIVER_OK);

    auto error = quiver_lua_runner_get_error(lua);
    EXPECT_NE(error, nullptr);

    quiver_lua_runner_free(lua);
    quiver_database_close(db);
}

TEST_F(LuaRunnerCApiTest, RuntimeError) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", collections_schema.c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto lua = quiver_lua_runner_new(db);
    ASSERT_NE(lua, nullptr);

    auto result = quiver_lua_runner_run(lua, "error('This is a runtime error')");
    EXPECT_NE(result, QUIVER_OK);

    auto error = quiver_lua_runner_get_error(lua);
    EXPECT_NE(error, nullptr);

    quiver_lua_runner_free(lua);
    quiver_database_close(db);
}

TEST_F(LuaRunnerCApiTest, GetErrorNull) {
    auto error = quiver_lua_runner_get_error(nullptr);
    EXPECT_EQ(error, nullptr);
}

TEST_F(LuaRunnerCApiTest, ReuseRunner) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", collections_schema.c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto lua = quiver_lua_runner_new(db);
    ASSERT_NE(lua, nullptr);

    // Run multiple scripts
    EXPECT_EQ(quiver_lua_runner_run(lua, R"(db:create_element("Configuration", { label = "Config" }))"), QUIVER_OK);
    EXPECT_EQ(quiver_lua_runner_run(lua, R"(db:create_element("Collection", { label = "Item 1" }))"), QUIVER_OK);
    EXPECT_EQ(quiver_lua_runner_run(lua, R"(db:create_element("Collection", { label = "Item 2" }))"), QUIVER_OK);

    // Verify count
    char** labels = nullptr;
    size_t count = 0;
    auto read_result = quiver_database_read_scalar_strings(db, "Collection", "label", &labels, &count);
    EXPECT_EQ(read_result, QUIVER_OK);
    EXPECT_EQ(count, 2);
    quiver_free_string_array(labels, count);

    quiver_lua_runner_free(lua);
    quiver_database_close(db);
}

TEST_F(LuaRunnerCApiTest, ReadScalarIntegers) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", collections_schema.c_str(), &options);
    ASSERT_NE(db, nullptr);

    // Create elements with C API
    auto config = quiver_element_create();
    quiver_element_set_string(config, "label", "Config");
    quiver_database_create_element(db, "Configuration", config);
    quiver_element_destroy(config);

    auto elem = quiver_element_create();
    quiver_element_set_string(elem, "label", "Item 1");
    quiver_element_set_integer(elem, "some_integer", 100);
    quiver_database_create_element(db, "Collection", elem);
    quiver_element_destroy(elem);

    auto lua = quiver_lua_runner_new(db);
    ASSERT_NE(lua, nullptr);

    // Read and verify from Lua
    auto result = quiver_lua_runner_run(lua, R"(
        local integers = db:read_scalar_integers("Collection", "some_integer")
        assert(#integers == 1, "Expected 1 integer")
        assert(integers[1] == 100, "Expected 100")
    )");
    EXPECT_EQ(result, QUIVER_OK);

    quiver_lua_runner_free(lua);
    quiver_database_close(db);
}

TEST_F(LuaRunnerCApiTest, CreateElementWithVectors) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", collections_schema.c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto lua = quiver_lua_runner_new(db);
    ASSERT_NE(lua, nullptr);

    auto result = quiver_lua_runner_run(lua, R"(
        db:create_element("Configuration", { label = "Config" })
        db:create_element("Collection", {
            label = "Item 1",
            value_int = {1, 2, 3},
            value_float = {1.5, 2.5, 3.5}
        })
    )");
    EXPECT_EQ(result, QUIVER_OK);

    // Verify with C API read
    int64_t** vectors = nullptr;
    size_t* sizes = nullptr;
    size_t count = 0;
    auto read_result = quiver_database_read_vector_integers(db, "Collection", "value_int", &vectors, &sizes, &count);
    EXPECT_EQ(read_result, QUIVER_OK);
    EXPECT_EQ(count, 1);
    EXPECT_EQ(sizes[0], 3);
    EXPECT_EQ(vectors[0][0], 1);
    EXPECT_EQ(vectors[0][1], 2);
    EXPECT_EQ(vectors[0][2], 3);
    quiver_free_integer_vectors(vectors, sizes, count);

    quiver_lua_runner_free(lua);
    quiver_database_close(db);
}

TEST_F(LuaRunnerCApiTest, DeleteElement) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", collections_schema.c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto lua = quiver_lua_runner_new(db);
    ASSERT_NE(lua, nullptr);

    // Create and delete elements
    auto result = quiver_lua_runner_run(lua, R"(
        db:create_element("Configuration", { label = "Config" })
        db:create_element("Collection", { label = "Item 1" })
        db:create_element("Collection", { label = "Item 2" })

        local ids = db:read_element_ids("Collection")
        assert(#ids == 2, "Expected 2 elements before delete")

        db:delete_element_by_id("Collection", 1)

        ids = db:read_element_ids("Collection")
        assert(#ids == 1, "Expected 1 element after delete")
    )");
    EXPECT_EQ(result, QUIVER_OK);

    quiver_lua_runner_free(lua);
    quiver_database_close(db);
}

TEST_F(LuaRunnerCApiTest, UpdateElement) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", collections_schema.c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto lua = quiver_lua_runner_new(db);
    ASSERT_NE(lua, nullptr);

    auto result = quiver_lua_runner_run(lua, R"(
        db:create_element("Configuration", { label = "Config" })
        db:create_element("Collection", { label = "Item 1", some_integer = 100 })

        db:update_element("Collection", 1, { some_integer = 999 })

        local val = db:read_scalar_integers_by_id("Collection", "some_integer", 1)
        assert(val == 999, "Expected 999 after update")
    )");
    EXPECT_EQ(result, QUIVER_OK);

    // Verify with C API
    int64_t value = 0;
    int has_value = 0;
    auto read_result = quiver_database_read_scalar_integers_by_id(db, "Collection", "some_integer", 1, &value, &has_value);
    EXPECT_EQ(read_result, QUIVER_OK);
    EXPECT_EQ(has_value, 1);
    EXPECT_EQ(value, 999);

    quiver_lua_runner_free(lua);
    quiver_database_close(db);
}

// ============================================================================
// Additional C API LuaRunner error tests
// ============================================================================

TEST_F(LuaRunnerCApiTest, EmptyScript) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", collections_schema.c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto lua = quiver_lua_runner_new(db);
    ASSERT_NE(lua, nullptr);

    auto result = quiver_lua_runner_run(lua, "");
    EXPECT_EQ(result, QUIVER_OK);

    quiver_lua_runner_free(lua);
    quiver_database_close(db);
}

TEST_F(LuaRunnerCApiTest, CommentOnlyScript) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", collections_schema.c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto lua = quiver_lua_runner_new(db);
    ASSERT_NE(lua, nullptr);

    auto result = quiver_lua_runner_run(lua, "-- this is a comment\n-- another comment");
    EXPECT_EQ(result, QUIVER_OK);

    quiver_lua_runner_free(lua);
    quiver_database_close(db);
}

TEST_F(LuaRunnerCApiTest, AssertionFailure) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", collections_schema.c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto lua = quiver_lua_runner_new(db);
    ASSERT_NE(lua, nullptr);

    auto result = quiver_lua_runner_run(lua, "assert(false, 'Test assertion failure')");
    EXPECT_NE(result, QUIVER_OK);

    auto error = quiver_lua_runner_get_error(lua);
    EXPECT_NE(error, nullptr);
    EXPECT_NE(std::string(error).find("assertion"), std::string::npos);

    quiver_lua_runner_free(lua);
    quiver_database_close(db);
}

TEST_F(LuaRunnerCApiTest, UndefinedVariableError) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", collections_schema.c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto lua = quiver_lua_runner_new(db);
    ASSERT_NE(lua, nullptr);

    auto result = quiver_lua_runner_run(lua, "local x = undefined_variable + 1");
    EXPECT_NE(result, QUIVER_OK);

    auto error = quiver_lua_runner_get_error(lua);
    EXPECT_NE(error, nullptr);

    quiver_lua_runner_free(lua);
    quiver_database_close(db);
}

TEST_F(LuaRunnerCApiTest, ErrorClearedAfterSuccessfulRun) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", collections_schema.c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto lua = quiver_lua_runner_new(db);
    ASSERT_NE(lua, nullptr);

    // First, run a failing script
    auto result = quiver_lua_runner_run(lua, "invalid lua syntax !!!");
    EXPECT_NE(result, QUIVER_OK);
    auto error1 = quiver_lua_runner_get_error(lua);
    EXPECT_NE(error1, nullptr);

    // Now run a successful script
    result = quiver_lua_runner_run(lua, "local x = 1 + 1");
    EXPECT_EQ(result, QUIVER_OK);

    quiver_lua_runner_free(lua);
    quiver_database_close(db);
}

TEST_F(LuaRunnerCApiTest, ReadVectorIntegersFromLua) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    auto db = quiver_database_from_schema(":memory:", collections_schema.c_str(), &options);
    ASSERT_NE(db, nullptr);

    auto lua = quiver_lua_runner_new(db);
    ASSERT_NE(lua, nullptr);

    auto result = quiver_lua_runner_run(lua, R"(
        db:create_element("Configuration", { label = "Config" })
        db:create_element("Collection", {
            label = "Item 1",
            value_int = {1, 2, 3}
        })

        local vectors = db:read_vector_integers("Collection", "value_int")
        assert(#vectors == 1, "Expected 1 vector")
        assert(#vectors[1] == 3, "Expected 3 values")
        assert(vectors[1][1] == 1, "Expected first value to be 1")
    )");
    EXPECT_EQ(result, QUIVER_OK);

    quiver_lua_runner_free(lua);
    quiver_database_close(db);
}
