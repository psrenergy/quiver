#include "test_lua_runner.h"

class LuaRunnerAllTypesTest : public ::testing::Test {
protected:
    void SetUp() override { all_types_schema = VALID_SCHEMA("all_types.sql"); }
    std::string all_types_schema;
};

TEST_F(LuaRunnerAllTypesTest, ReadVectorStringsBulk) {
    auto db = quiver::Database::from_schema(":memory:", all_types_schema);
    db.create_element("AllTypes", quiver::Element().set("label", "Item 1"));
    db.create_element("AllTypes", quiver::Element().set("label", "Item 2"));
    db.update_element("AllTypes", 1, quiver::Element().set("label_value", std::vector<std::string>{"alpha", "beta"}));
    db.update_element(
        "AllTypes", 2, quiver::Element().set("label_value", std::vector<std::string>{"gamma", "delta", "epsilon"}));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        local vectors = db:read_vector_strings("AllTypes", "label_value")
        assert(#vectors == 2, "Expected 2 vectors, got " .. #vectors)
        assert(#vectors[1] == 2, "First vector should have 2 elements")
        assert(vectors[1][1] == "alpha", "First element should be 'alpha'")
        assert(vectors[1][2] == "beta", "Second element should be 'beta'")
        assert(#vectors[2] == 3, "Second vector should have 3 elements")
        assert(vectors[2][1] == "gamma", "Third vector first element should be 'gamma'")
    )");
}

TEST_F(LuaRunnerAllTypesTest, ReadSetIntegersBulk) {
    auto db = quiver::Database::from_schema(":memory:", all_types_schema);
    db.create_element("AllTypes", quiver::Element().set("label", "Item 1"));
    db.create_element("AllTypes", quiver::Element().set("label", "Item 2"));
    db.update_element("AllTypes", 1, quiver::Element().set("code", std::vector<int64_t>{10, 20}));
    db.update_element("AllTypes", 2, quiver::Element().set("code", std::vector<int64_t>{30, 40, 50}));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        local sets = db:read_set_integers("AllTypes", "code")
        assert(#sets == 2, "Expected 2 sets, got " .. #sets)
        assert(#sets[1] == 2, "First set should have 2 values")
        assert(#sets[2] == 3, "Second set should have 3 values")
    )");
}

TEST_F(LuaRunnerAllTypesTest, ReadSetFloatsBulk) {
    auto db = quiver::Database::from_schema(":memory:", all_types_schema);
    db.create_element("AllTypes", quiver::Element().set("label", "Item 1"));
    db.create_element("AllTypes", quiver::Element().set("label", "Item 2"));
    db.update_element("AllTypes", 1, quiver::Element().set("weight", std::vector<double>{1.1, 2.2}));
    db.update_element("AllTypes", 2, quiver::Element().set("weight", std::vector<double>{3.3}));

    quiver::LuaRunner lua(db);

    lua.run(R"(
        local sets = db:read_set_floats("AllTypes", "weight")
        assert(#sets == 2, "Expected 2 sets, got " .. #sets)
        assert(#sets[1] == 2, "First set should have 2 values")
        assert(#sets[2] == 1, "Second set should have 1 value")
    )");
}

TEST_F(LuaRunnerAllTypesTest, UpdateSetIntegers) {
    auto db = quiver::Database::from_schema(":memory:", all_types_schema);
    int64_t id1 = db.create_element("AllTypes", quiver::Element().set("label", "Item 1"));

    quiver::LuaRunner lua(db);

    std::string script = R"(
        db:update_element("AllTypes", )" +
                         std::to_string(id1) + R"(, { code = {10, 20, 30} })
    )";
    lua.run(script);

    auto result = db.read_set_integers_by_id("AllTypes", "code", id1);
    EXPECT_EQ(result.size(), 3);
}

TEST_F(LuaRunnerAllTypesTest, UpdateSetFloats) {
    auto db = quiver::Database::from_schema(":memory:", all_types_schema);
    int64_t id1 = db.create_element("AllTypes", quiver::Element().set("label", "Item 1"));

    quiver::LuaRunner lua(db);

    std::string script = R"(
        db:update_element("AllTypes", )" +
                         std::to_string(id1) + R"(, { weight = {1.1, 2.2} })
    )";
    lua.run(script);

    auto result = db.read_set_floats_by_id("AllTypes", "weight", id1);
    EXPECT_EQ(result.size(), 2);
}
