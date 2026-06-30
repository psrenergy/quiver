#include "test_lua_runner.h"

#include <algorithm>

class LuaRunnerFkTest : public ::testing::Test {
protected:
    void SetUp() override {
        relations_schema = VALID_SCHEMA("relations.sql");
        basic_schema = VALID_SCHEMA("basic.sql");
    }
    std::string relations_schema;
    std::string basic_schema;
};

TEST_F(LuaRunnerFkTest, CreateElementSetFkLabels) {
    auto db = quiver::Database::from_schema(":memory:", relations_schema);
    quiver::LuaRunner lua(db);

    lua.run(R"(
    db:create_element("Parent", { label = "Parent 1" })
    db:create_element("Parent", { label = "Parent 2" })
    db:create_element("Child", {
        label = "Child 1",
        mentor_id = {"Parent 1", "Parent 2"}
    })
)");

    auto sets = db.read_set_integers("Child", "mentor_id");
    ASSERT_EQ(sets.size(), 1);
    ASSERT_EQ(sets[0].size(), 2);

    std::vector<int64_t> sorted_ids(sets[0].begin(), sets[0].end());
    std::sort(sorted_ids.begin(), sorted_ids.end());
    EXPECT_EQ(sorted_ids[0], 1);
    EXPECT_EQ(sorted_ids[1], 2);
}

TEST_F(LuaRunnerFkTest, CreateElementMissingFkTarget) {
    auto db = quiver::Database::from_schema(":memory:", relations_schema);
    quiver::LuaRunner lua(db);

    EXPECT_THROW(
        {
            lua.run(R"(
        db:create_element("Child", {
            label = "Child 1",
            mentor_id = {"Nonexistent Parent"}
        })
    )");
        },
        std::runtime_error);
}

TEST_F(LuaRunnerFkTest, CreateElementStringForNonFkInteger) {
    auto db = quiver::Database::from_schema(":memory:", relations_schema);
    quiver::LuaRunner lua(db);

    EXPECT_THROW(
        {
            lua.run(R"(
        db:create_element("Child", {
            label = "Child 1",
            score = {"not_a_label"}
        })
    )");
        },
        std::runtime_error);
}

TEST_F(LuaRunnerFkTest, CreateElementScalarFkLabel) {
    auto db = quiver::Database::from_schema(":memory:", relations_schema);
    quiver::LuaRunner lua(db);

    lua.run(R"(
    db:create_element("Parent", { label = "Parent 1" })
    db:create_element("Child", {
        label = "Child 1",
        parent_id = "Parent 1"
    })
)");

    auto parent_ids = db.read_scalar_integers("Child", "parent_id");
    ASSERT_EQ(parent_ids.size(), 1);
    EXPECT_EQ(parent_ids[0], 1);
}

TEST_F(LuaRunnerFkTest, CreateElementScalarFkInteger) {
    auto db = quiver::Database::from_schema(":memory:", relations_schema);
    quiver::LuaRunner lua(db);

    lua.run(R"(
    db:create_element("Parent", { label = "Parent 1" })
    db:create_element("Child", {
        label = "Child 1",
        parent_id = 1
    })
)");

    auto parent_ids = db.read_scalar_integers("Child", "parent_id");
    ASSERT_EQ(parent_ids.size(), 1);
    EXPECT_EQ(parent_ids[0], 1);
}

TEST_F(LuaRunnerFkTest, CreateElementVectorFkLabels) {
    auto db = quiver::Database::from_schema(":memory:", relations_schema);
    quiver::LuaRunner lua(db);

    lua.run(R"(
    db:create_element("Parent", { label = "Parent 1" })
    db:create_element("Parent", { label = "Parent 2" })
    db:create_element("Child", {
        label = "Child 1",
        parent_ref = {"Parent 1", "Parent 2"}
    })
)");

    auto refs = db.read_vector_integers_by_id("Child", "parent_ref", 1);
    ASSERT_EQ(refs.size(), 2);
    EXPECT_EQ(refs[0], 1);
    EXPECT_EQ(refs[1], 2);
}

TEST_F(LuaRunnerFkTest, CreateElementTimeSeriesFkLabels) {
    auto db = quiver::Database::from_schema(":memory:", relations_schema);
    quiver::LuaRunner lua(db);

    lua.run(R"(
    db:create_element("Parent", { label = "Parent 1" })
    db:create_element("Parent", { label = "Parent 2" })
    db:create_element("Child", {
        label = "Child 1",
        date_time = {"2024-01-01", "2024-01-02"},
        sponsor_id = {"Parent 1", "Parent 2"}
    })
)");

    auto ts_data = db.read_time_series_group("Child", "events", 1);
    ASSERT_EQ(ts_data.size(), 2);
    EXPECT_EQ(std::get<int64_t>(ts_data[0].at("sponsor_id")), 1);
    EXPECT_EQ(std::get<int64_t>(ts_data[1].at("sponsor_id")), 2);
}

TEST_F(LuaRunnerFkTest, CreateElementAllFkTypes) {
    auto db = quiver::Database::from_schema(":memory:", relations_schema);
    quiver::LuaRunner lua(db);

    lua.run(R"(
    db:create_element("Parent", { label = "Parent 1" })
    db:create_element("Parent", { label = "Parent 2" })
    db:create_element("Child", {
        label = "Child 1",
        parent_id = "Parent 1",
        mentor_id = {"Parent 2"},
        parent_ref = {"Parent 1"},
        date_time = {"2024-01-01"},
        sponsor_id = {"Parent 2"}
    })
)");

    // Verify scalar FK
    auto parent_ids = db.read_scalar_integers("Child", "parent_id");
    ASSERT_EQ(parent_ids.size(), 1);
    EXPECT_EQ(parent_ids[0], 1);

    // Verify set FK (mentor_id)
    auto mentors = db.read_set_integers("Child", "mentor_id");
    ASSERT_EQ(mentors.size(), 1);
    ASSERT_EQ(mentors[0].size(), 1);
    EXPECT_EQ(mentors[0][0], 2);

    // Verify vector FK (parent_ref)
    auto vrefs = db.read_vector_integers_by_id("Child", "parent_ref", 1);
    ASSERT_EQ(vrefs.size(), 1);
    EXPECT_EQ(vrefs[0], 1);

    // Verify time series FK (sponsor_id)
    auto ts_data = db.read_time_series_group("Child", "events", 1);
    ASSERT_EQ(ts_data.size(), 1);
    EXPECT_EQ(std::get<int64_t>(ts_data[0].at("sponsor_id")), 2);
}

TEST_F(LuaRunnerFkTest, CreateElementNoFkUnchanged) {
    auto db = quiver::Database::from_schema(":memory:", basic_schema);
    quiver::LuaRunner lua(db);

    lua.run(R"(
    db:create_element("Configuration", {
        label = "Config 1",
        integer_attribute = 42,
        float_attribute = 3.14
    })
)");

    auto labels = db.read_scalar_strings("Configuration", "label");
    ASSERT_EQ(labels.size(), 1);
    EXPECT_EQ(labels[0], "Config 1");

    auto integers = db.read_scalar_integers("Configuration", "integer_attribute");
    ASSERT_EQ(integers.size(), 1);
    EXPECT_EQ(integers[0], 42);

    auto floats = db.read_scalar_floats("Configuration", "float_attribute");
    ASSERT_EQ(floats.size(), 1);
    EXPECT_DOUBLE_EQ(*floats[0], 3.14);
}

TEST_F(LuaRunnerFkTest, CreateElementFkResolutionNoPartialWrites) {
    auto db = quiver::Database::from_schema(":memory:", relations_schema);
    quiver::LuaRunner lua(db);

    EXPECT_THROW(
        {
            lua.run(R"(
        db:create_element("Child", {
            label = "Orphan Child",
            parent_id = "Nonexistent"
        })
    )");
        },
        std::runtime_error);

    // Verify: no child was created (zero partial writes)
    auto labels = db.read_scalar_strings("Child", "label");
    EXPECT_EQ(labels.size(), 0);
}

TEST_F(LuaRunnerFkTest, UpdateElementScalarFkLabel) {
    auto db = quiver::Database::from_schema(":memory:", relations_schema);
    quiver::LuaRunner lua(db);

    lua.run(R"(
    db:create_element("Parent", { label = "Parent 1" })
    db:create_element("Parent", { label = "Parent 2" })
    db:create_element("Child", {
        label = "Child 1",
        parent_id = "Parent 1"
    })
    db:update_element("Child", 1, { parent_id = "Parent 2" })
)");

    auto parent_ids = db.read_scalar_integers("Child", "parent_id");
    ASSERT_EQ(parent_ids.size(), 1);
    EXPECT_EQ(parent_ids[0], 2);
}

TEST_F(LuaRunnerFkTest, UpdateElementScalarFkInteger) {
    auto db = quiver::Database::from_schema(":memory:", relations_schema);
    quiver::LuaRunner lua(db);

    lua.run(R"(
    db:create_element("Parent", { label = "Parent 1" })
    db:create_element("Parent", { label = "Parent 2" })
    db:create_element("Child", {
        label = "Child 1",
        parent_id = 1
    })
    db:update_element("Child", 1, { parent_id = 2 })
)");

    auto parent_ids = db.read_scalar_integers("Child", "parent_id");
    ASSERT_EQ(parent_ids.size(), 1);
    EXPECT_EQ(parent_ids[0], 2);
}

TEST_F(LuaRunnerFkTest, UpdateElementSetFkLabels) {
    auto db = quiver::Database::from_schema(":memory:", relations_schema);
    quiver::LuaRunner lua(db);

    lua.run(R"(
    db:create_element("Parent", { label = "Parent 1" })
    db:create_element("Parent", { label = "Parent 2" })
    db:create_element("Child", {
        label = "Child 1",
        mentor_id = {"Parent 1"}
    })
    db:update_element("Child", 1, { mentor_id = {"Parent 2"} })
)");

    auto mentors = db.read_set_integers("Child", "mentor_id");
    ASSERT_EQ(mentors.size(), 1);
    ASSERT_EQ(mentors[0].size(), 1);
    EXPECT_EQ(mentors[0][0], 2);
}

TEST_F(LuaRunnerFkTest, UpdateElementAllFkTypes) {
    auto db = quiver::Database::from_schema(":memory:", relations_schema);
    quiver::LuaRunner lua(db);

    lua.run(R"(
    db:create_element("Parent", { label = "Parent 1" })
    db:create_element("Parent", { label = "Parent 2" })
    db:create_element("Child", {
        label = "Child 1",
        parent_id = "Parent 1",
        mentor_id = {"Parent 1"},
        parent_ref = {"Parent 1"},
        date_time = {"2024-01-01"},
        sponsor_id = {"Parent 1"}
    })
    db:update_element("Child", 1, {
        parent_id = "Parent 2",
        mentor_id = {"Parent 2"},
        parent_ref = {"Parent 2"},
        date_time = {"2025-01-01"},
        sponsor_id = {"Parent 2"}
    })
)");

    // Verify scalar FK
    auto parent_ids = db.read_scalar_integers("Child", "parent_id");
    ASSERT_EQ(parent_ids.size(), 1);
    EXPECT_EQ(parent_ids[0], 2);

    // Verify set FK (mentor_id)
    auto mentors = db.read_set_integers("Child", "mentor_id");
    ASSERT_EQ(mentors.size(), 1);
    ASSERT_EQ(mentors[0].size(), 1);
    EXPECT_EQ(mentors[0][0], 2);

    // Verify vector FK (parent_ref)
    auto vrefs = db.read_vector_integers_by_id("Child", "parent_ref", 1);
    ASSERT_EQ(vrefs.size(), 1);
    EXPECT_EQ(vrefs[0], 2);

    // Verify time series FK (sponsor_id)
    auto ts_data = db.read_time_series_group("Child", "events", 1);
    ASSERT_EQ(ts_data.size(), 1);
    EXPECT_EQ(std::get<int64_t>(ts_data[0].at("sponsor_id")), 2);
}

TEST_F(LuaRunnerFkTest, UpdateElementFkFailurePreservesExisting) {
    auto db = quiver::Database::from_schema(":memory:", relations_schema);
    quiver::LuaRunner lua(db);

    lua.run(R"(
    db:create_element("Parent", { label = "Parent 1" })
    db:create_element("Child", {
        label = "Child 1",
        parent_id = "Parent 1"
    })
)");

    EXPECT_THROW(
        {
            lua.run(R"(
        db:update_element("Child", 1, { parent_id = "Nonexistent" })
    )");
        },
        std::runtime_error);

    // Verify: original value preserved
    auto parent_ids = db.read_scalar_integers("Child", "parent_id");
    ASSERT_EQ(parent_ids.size(), 1);
    EXPECT_EQ(parent_ids[0], 1);
}

TEST_F(LuaRunnerFkTest, UpdateElementNoFkUnchanged) {
    auto db = quiver::Database::from_schema(":memory:", basic_schema);
    quiver::LuaRunner lua(db);

    lua.run(R"(
    db:create_element("Configuration", {
        label = "Config 1",
        integer_attribute = 42,
        float_attribute = 3.14,
        string_attribute = "hello"
    })
    db:update_element("Configuration", 1, {
        integer_attribute = 100,
        float_attribute = 2.71,
        string_attribute = "world"
    })
)");

    auto integer_val = db.read_scalar_integer_by_id("Configuration", "integer_attribute", 1);
    EXPECT_TRUE(integer_val.has_value());
    EXPECT_EQ(*integer_val, 100);

    auto float_val = db.read_scalar_float_by_id("Configuration", "float_attribute", 1);
    EXPECT_TRUE(float_val.has_value());
    EXPECT_DOUBLE_EQ(*float_val, 2.71);

    auto str_val = db.read_scalar_string_by_id("Configuration", "string_attribute", 1);
    EXPECT_TRUE(str_val.has_value());
    EXPECT_EQ(*str_val, "world");
}

TEST_F(LuaRunnerFkTest, UpdateVectorFkViaTypedMethod) {
    auto db = quiver::Database::from_schema(":memory:", relations_schema);
    quiver::LuaRunner lua(db);

    lua.run(R"(
    db:create_element("Parent", { label = "Parent 1" })
    db:create_element("Parent", { label = "Parent 2" })
    db:create_element("Child", {
        label = "Child 1",
        parent_ref = {"Parent 1"}
    })
    db:update_element("Child", 1, { parent_ref = {2, 1} })
)");

    auto refs = db.read_vector_integers_by_id("Child", "parent_ref", 1);
    ASSERT_EQ(refs.size(), 2);
    EXPECT_EQ(refs[0], 2);
    EXPECT_EQ(refs[1], 1);
}

TEST_F(LuaRunnerFkTest, UpdateTimeSeriesFkViaTypedMethod) {
    auto db = quiver::Database::from_schema(":memory:", relations_schema);
    quiver::LuaRunner lua(db);

    lua.run(R"(
    db:create_element("Parent", { label = "Parent 1" })
    db:create_element("Parent", { label = "Parent 2" })
    db:create_element("Child", {
        label = "Child 1",
        date_time = {"2024-01-01"},
        sponsor_id = {"Parent 1"}
    })
    db:update_time_series_group("Child", "events", 1, {
        date_time = {"2025-01-01", "2025-01-02"},
        sponsor_id = {2, 1}
    })
)");

    auto ts_data = db.read_time_series_group("Child", "events", 1);
    ASSERT_EQ(ts_data.size(), 2);
    EXPECT_EQ(std::get<int64_t>(ts_data[0].at("sponsor_id")), 2);
    EXPECT_EQ(std::get<int64_t>(ts_data[1].at("sponsor_id")), 1);
}
