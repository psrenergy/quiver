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
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", collections_schema.c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_lua_runner_t* lua = nullptr;
    ASSERT_EQ(quiver_lua_runner_new(db, &lua), QUIVER_OK);
    ASSERT_NE(lua, nullptr);

    quiver_lua_runner_free(lua);
    quiver_database_close(db);
}

TEST_F(LuaRunnerCApiTest, FreeNull) {
    EXPECT_EQ(quiver_lua_runner_free(nullptr), QUIVER_OK);
}

TEST_F(LuaRunnerCApiTest, CreateWithNullDb) {
    quiver_lua_runner_t* lua = nullptr;
    EXPECT_EQ(quiver_lua_runner_new(nullptr, &lua), QUIVER_ERROR);
}

TEST_F(LuaRunnerCApiTest, RunSimpleScript) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", collections_schema.c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_lua_runner_t* lua = nullptr;
    ASSERT_EQ(quiver_lua_runner_new(db, &lua), QUIVER_OK);
    ASSERT_NE(lua, nullptr);

    auto result = quiver_lua_runner_run(lua, "local x = 1 + 1");
    EXPECT_EQ(result, QUIVER_OK);

    quiver_lua_runner_free(lua);
    quiver_database_close(db);
}

TEST_F(LuaRunnerCApiTest, RunNullScript) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", collections_schema.c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_lua_runner_t* lua = nullptr;
    ASSERT_EQ(quiver_lua_runner_new(db, &lua), QUIVER_OK);
    ASSERT_NE(lua, nullptr);

    auto result = quiver_lua_runner_run(lua, nullptr);
    EXPECT_EQ(result, QUIVER_ERROR);

    quiver_lua_runner_free(lua);
    quiver_database_close(db);
}

TEST_F(LuaRunnerCApiTest, RunWithNullRunner) {
    auto result = quiver_lua_runner_run(nullptr, "local x = 1");
    EXPECT_EQ(result, QUIVER_ERROR);
}

TEST_F(LuaRunnerCApiTest, CreateElement) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", collections_schema.c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_lua_runner_t* lua = nullptr;
    ASSERT_EQ(quiver_lua_runner_new(db, &lua), QUIVER_OK);
    ASSERT_NE(lua, nullptr);

    auto result = quiver_lua_runner_run(lua, R"(
        db:create_element("Configuration", { label = "Test Config" })
        db:create_element("Collection", { label = "Item 1", some_integer = 42 })
    )");
    EXPECT_EQ(result, QUIVER_OK);

    // Verify with C API read
    int64_t* values = nullptr;
    uint8_t* mask = nullptr;
    size_t count = 0;
    auto read_result = quiver_database_read_scalar_integers(db, "Collection", "some_integer", &values, &mask, &count);
    EXPECT_EQ(read_result, QUIVER_OK);
    EXPECT_EQ(count, 1);
    EXPECT_EQ(values[0], 42);
    quiver_database_free_integer_array(values);
    quiver_database_free_mask(mask);

    quiver_lua_runner_free(lua);
    quiver_database_close(db);
}

TEST_F(LuaRunnerCApiTest, SyntaxError) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", collections_schema.c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_lua_runner_t* lua = nullptr;
    ASSERT_EQ(quiver_lua_runner_new(db, &lua), QUIVER_OK);
    ASSERT_NE(lua, nullptr);

    auto result = quiver_lua_runner_run(lua, "invalid lua syntax !!!");
    EXPECT_NE(result, QUIVER_OK);

    const char* error = quiver_get_last_error();
    EXPECT_NE(error, nullptr);

    quiver_lua_runner_free(lua);
    quiver_database_close(db);
}

TEST_F(LuaRunnerCApiTest, RuntimeError) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", collections_schema.c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_lua_runner_t* lua = nullptr;
    ASSERT_EQ(quiver_lua_runner_new(db, &lua), QUIVER_OK);
    ASSERT_NE(lua, nullptr);

    auto result = quiver_lua_runner_run(lua, "error('This is a runtime error')");
    EXPECT_NE(result, QUIVER_OK);

    const char* error = quiver_get_last_error();
    EXPECT_NE(error, nullptr);

    quiver_lua_runner_free(lua);
    quiver_database_close(db);
}

TEST_F(LuaRunnerCApiTest, SandboxViolationError) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", collections_schema.c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_lua_runner_t* lua = nullptr;
    ASSERT_EQ(quiver_lua_runner_new(db, &lua), QUIVER_OK);
    ASSERT_NE(lua, nullptr);

    // File operations are db-scoped and sandboxed; the violation surfaces through the single
    // error channel with the script-failure prefix wrapping the Pattern 1 message.
    auto result = quiver_lua_runner_run(lua, "db:open_file('x', 'r')");
    EXPECT_NE(result, QUIVER_OK);

    const char* error = quiver_get_last_error();
    ASSERT_NE(error, nullptr);
    EXPECT_NE(std::string(error).find("Failed to run Lua script:"), std::string::npos) << error;
    EXPECT_NE(std::string(error).find("Cannot open_file: database is in-memory"), std::string::npos) << error;

    quiver_lua_runner_free(lua);
    quiver_database_close(db);
}

TEST_F(LuaRunnerCApiTest, ReuseRunner) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", collections_schema.c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_lua_runner_t* lua = nullptr;
    ASSERT_EQ(quiver_lua_runner_new(db, &lua), QUIVER_OK);
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
    quiver_database_free_string_array(labels, count);

    quiver_lua_runner_free(lua);
    quiver_database_close(db);
}

TEST_F(LuaRunnerCApiTest, ReadScalarIntegers) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", collections_schema.c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    // Create elements with C API
    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Config");
    int64_t tmp_id1 = 0;
    quiver_database_create_element(db, "Configuration", config, &tmp_id1);
    EXPECT_EQ(quiver_element_destroy(config), QUIVER_OK);

    quiver_element_t* elem = nullptr;
    ASSERT_EQ(quiver_element_create(&elem), QUIVER_OK);
    quiver_element_set_string(elem, "label", "Item 1");
    quiver_element_set_integer(elem, "some_integer", 100);
    int64_t tmp_id2 = 0;
    quiver_database_create_element(db, "Collection", elem, &tmp_id2);
    EXPECT_EQ(quiver_element_destroy(elem), QUIVER_OK);

    quiver_lua_runner_t* lua = nullptr;
    ASSERT_EQ(quiver_lua_runner_new(db, &lua), QUIVER_OK);
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
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", collections_schema.c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_lua_runner_t* lua = nullptr;
    ASSERT_EQ(quiver_lua_runner_new(db, &lua), QUIVER_OK);
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
    quiver_database_free_integer_vectors(vectors, sizes, count);

    quiver_lua_runner_free(lua);
    quiver_database_close(db);
}

TEST_F(LuaRunnerCApiTest, DeleteElement) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", collections_schema.c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_lua_runner_t* lua = nullptr;
    ASSERT_EQ(quiver_lua_runner_new(db, &lua), QUIVER_OK);
    ASSERT_NE(lua, nullptr);

    // Create and delete elements
    auto result = quiver_lua_runner_run(lua, R"(
        db:create_element("Configuration", { label = "Config" })
        db:create_element("Collection", { label = "Item 1" })
        db:create_element("Collection", { label = "Item 2" })

        local ids = db:read_element_ids("Collection")
        assert(#ids == 2, "Expected 2 elements before delete")

        db:delete_element("Collection", 1)

        ids = db:read_element_ids("Collection")
        assert(#ids == 1, "Expected 1 element after delete")
    )");
    EXPECT_EQ(result, QUIVER_OK);

    quiver_lua_runner_free(lua);
    quiver_database_close(db);
}

TEST_F(LuaRunnerCApiTest, UpdateElement) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", collections_schema.c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_lua_runner_t* lua = nullptr;
    ASSERT_EQ(quiver_lua_runner_new(db, &lua), QUIVER_OK);
    ASSERT_NE(lua, nullptr);

    auto result = quiver_lua_runner_run(lua, R"(
        db:create_element("Configuration", { label = "Config" })
        db:create_element("Collection", { label = "Item 1", some_integer = 100 })

        db:update_element("Collection", 1, { some_integer = 999 })

        local scalars = db:read_scalars_by_id("Collection", 1)
        assert(scalars.some_integer == 999, "Expected 999 after update")
    )");
    EXPECT_EQ(result, QUIVER_OK);

    // Verify with C API
    int64_t value = 0;
    int has_value = 0;
    auto read_result =
        quiver_database_read_scalar_integer_by_id(db, "Collection", "some_integer", 1, &value, &has_value);
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
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", collections_schema.c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_lua_runner_t* lua = nullptr;
    ASSERT_EQ(quiver_lua_runner_new(db, &lua), QUIVER_OK);
    ASSERT_NE(lua, nullptr);

    auto result = quiver_lua_runner_run(lua, "");
    EXPECT_EQ(result, QUIVER_OK);

    quiver_lua_runner_free(lua);
    quiver_database_close(db);
}

TEST_F(LuaRunnerCApiTest, CommentOnlyScript) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", collections_schema.c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_lua_runner_t* lua = nullptr;
    ASSERT_EQ(quiver_lua_runner_new(db, &lua), QUIVER_OK);
    ASSERT_NE(lua, nullptr);

    auto result = quiver_lua_runner_run(lua, "-- this is a comment\n-- another comment");
    EXPECT_EQ(result, QUIVER_OK);

    quiver_lua_runner_free(lua);
    quiver_database_close(db);
}

TEST_F(LuaRunnerCApiTest, AssertionFailure) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", collections_schema.c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_lua_runner_t* lua = nullptr;
    ASSERT_EQ(quiver_lua_runner_new(db, &lua), QUIVER_OK);
    ASSERT_NE(lua, nullptr);

    auto result = quiver_lua_runner_run(lua, "assert(false, 'Test assertion failure')");
    EXPECT_NE(result, QUIVER_OK);

    const char* error = quiver_get_last_error();
    EXPECT_NE(error, nullptr);
    EXPECT_NE(std::string(error).find("assertion"), std::string::npos);

    quiver_lua_runner_free(lua);
    quiver_database_close(db);
}

TEST_F(LuaRunnerCApiTest, UndefinedVariableError) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", collections_schema.c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_lua_runner_t* lua = nullptr;
    ASSERT_EQ(quiver_lua_runner_new(db, &lua), QUIVER_OK);
    ASSERT_NE(lua, nullptr);

    auto result = quiver_lua_runner_run(lua, "local x = undefined_variable + 1");
    EXPECT_NE(result, QUIVER_OK);

    const char* error = quiver_get_last_error();
    EXPECT_NE(error, nullptr);

    quiver_lua_runner_free(lua);
    quiver_database_close(db);
}

TEST_F(LuaRunnerCApiTest, ErrorClearedAfterSuccessfulRun) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", collections_schema.c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_lua_runner_t* lua = nullptr;
    ASSERT_EQ(quiver_lua_runner_new(db, &lua), QUIVER_OK);
    ASSERT_NE(lua, nullptr);

    // First, run a failing script
    auto result = quiver_lua_runner_run(lua, "invalid lua syntax !!!");
    EXPECT_NE(result, QUIVER_OK);
    const char* error1 = quiver_get_last_error();
    EXPECT_NE(error1, nullptr);

    // Now run a successful script
    result = quiver_lua_runner_run(lua, "local x = 1 + 1");
    EXPECT_EQ(result, QUIVER_OK);

    quiver_lua_runner_free(lua);
    quiver_database_close(db);
}

TEST_F(LuaRunnerCApiTest, ReadVectorIntegers) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", collections_schema.c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_lua_runner_t* lua = nullptr;
    ASSERT_EQ(quiver_lua_runner_new(db, &lua), QUIVER_OK);
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

TEST_F(LuaRunnerCApiTest, RunScriptUnsupportedAttributeTypeFails) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", collections_schema.c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    quiver_lua_runner_t* lua = nullptr;
    ASSERT_EQ(quiver_lua_runner_new(db, &lua), QUIVER_OK);
    ASSERT_NE(lua, nullptr);

    auto result = quiver_lua_runner_run(lua, R"(db:create_element("Configuration", { label = "X", enabled = true }))");
    EXPECT_EQ(result, QUIVER_ERROR);

    const char* error = quiver_get_last_error();
    ASSERT_NE(error, nullptr);
    EXPECT_NE(std::string(error).find("Cannot table_to_element: attribute 'enabled'"), std::string::npos) << error;

    quiver_lua_runner_free(lua);
    quiver_database_close(db);
}
