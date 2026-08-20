#include "test_utils.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <quiver/database.h>
#include <quiver/element.h>

// ============================================================================
// Update vector tests
// ============================================================================

TEST(Database, UpdateVectorIntegers) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e;
    e.set("label", std::string("Item 1")).set("value_int", std::vector<int64_t>{1, 2, 3});
    int64_t id = db.create_element("Collection", e);

    quiver::Element update;
    update.set("value_int", std::vector<int64_t>{10, 20, 30, 40});
    db.update_element("Collection", id, update);

    auto vec = db.read_vector_integers_by_id("Collection", "value_int", id);
    EXPECT_EQ(vec, (std::vector<int64_t>{10, 20, 30, 40}));
}

TEST(Database, UpdateVectorFloats) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e;
    e.set("label", std::string("Item 1")).set("value_float", std::vector<double>{1.5, 2.5, 3.5});
    int64_t id = db.create_element("Collection", e);

    quiver::Element update;
    update.set("value_float", std::vector<double>{10.5, 20.5});
    db.update_element("Collection", id, update);

    auto vec = db.read_vector_floats_by_id("Collection", "value_float", id);
    EXPECT_EQ(vec, (std::vector<double>{10.5, 20.5}));
}

TEST(Database, UpdateVectorToEmpty) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e;
    e.set("label", std::string("Item 1")).set("value_int", std::vector<int64_t>{1, 2, 3});
    int64_t id = db.create_element("Collection", e);

    quiver::Element update;
    update.set("value_int", std::vector<int64_t>{});
    db.update_element("Collection", id, update);

    auto vec = db.read_vector_integers_by_id("Collection", "value_int", id);
    EXPECT_TRUE(vec.empty());
}

TEST(Database, UpdateVectorMultipleElements) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e1;
    e1.set("label", std::string("Item 1")).set("value_int", std::vector<int64_t>{1, 2, 3});
    int64_t id1 = db.create_element("Collection", e1);

    quiver::Element e2;
    e2.set("label", std::string("Item 2")).set("value_int", std::vector<int64_t>{10, 20});
    int64_t id2 = db.create_element("Collection", e2);

    // Update only first element
    quiver::Element update;
    update.set("value_int", std::vector<int64_t>{100, 200});
    db.update_element("Collection", id1, update);

    // Verify first element changed
    auto vec1 = db.read_vector_integers_by_id("Collection", "value_int", id1);
    EXPECT_EQ(vec1, (std::vector<int64_t>{100, 200}));

    // Verify second element unchanged
    auto vec2 = db.read_vector_integers_by_id("Collection", "value_int", id2);
    EXPECT_EQ(vec2, (std::vector<int64_t>{10, 20}));
}

// ============================================================================
// Update set tests
// ============================================================================

TEST(Database, UpdateSetStrings) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e;
    e.set("label", std::string("Item 1")).set("tag", std::vector<std::string>{"important", "urgent"});
    int64_t id = db.create_element("Collection", e);

    quiver::Element update;
    update.set("tag", std::vector<std::string>{"new_tag1", "new_tag2", "new_tag3"});
    db.update_element("Collection", id, update);

    auto set = db.read_set_strings_by_id("Collection", "tag", id);
    std::sort(set.begin(), set.end());
    EXPECT_EQ(set, (std::vector<std::string>{"new_tag1", "new_tag2", "new_tag3"}));
}

TEST(Database, UpdateSetToEmpty) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e;
    e.set("label", std::string("Item 1")).set("tag", std::vector<std::string>{"important", "urgent"});
    int64_t id = db.create_element("Collection", e);

    quiver::Element update;
    update.set("tag", std::vector<std::string>{});
    db.update_element("Collection", id, update);

    auto set = db.read_set_strings_by_id("Collection", "tag", id);
    EXPECT_TRUE(set.empty());
}

TEST(Database, UpdateSetMultipleElements) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e1;
    e1.set("label", std::string("Item 1")).set("tag", std::vector<std::string>{"important"});
    int64_t id1 = db.create_element("Collection", e1);

    quiver::Element e2;
    e2.set("label", std::string("Item 2")).set("tag", std::vector<std::string>{"urgent", "review"});
    int64_t id2 = db.create_element("Collection", e2);

    // Update only first element
    quiver::Element update;
    update.set("tag", std::vector<std::string>{"updated"});
    db.update_element("Collection", id1, update);

    // Verify first element changed
    auto set1 = db.read_set_strings_by_id("Collection", "tag", id1);
    EXPECT_EQ(set1, (std::vector<std::string>{"updated"}));

    // Verify second element unchanged
    auto set2 = db.read_set_strings_by_id("Collection", "tag", id2);
    std::sort(set2.begin(), set2.end());
    EXPECT_EQ(set2, (std::vector<std::string>{"review", "urgent"}));
}

// ============================================================================
// Update vector strings / set integers / set floats (gap-fill using all_types.sql)
// ============================================================================

TEST(Database, UpdateVectorStringsBasic) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("all_types.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e;
    e.set("label", std::string("Item 1"));
    int64_t id = db.create_element("AllTypes", e);

    quiver::Element update;
    update.set("label_value", std::vector<std::string>{"alpha", "beta"});
    db.update_element("AllTypes", id, update);

    auto vec = db.read_vector_strings_by_id("AllTypes", "label_value", id);
    EXPECT_EQ(vec, (std::vector<std::string>{"alpha", "beta"}));
}

TEST(Database, UpdateSetIntegersBasic) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("all_types.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e;
    e.set("label", std::string("Item 1"));
    int64_t id = db.create_element("AllTypes", e);

    quiver::Element update;
    update.set("code", std::vector<int64_t>{10, 20, 30});
    db.update_element("AllTypes", id, update);

    auto set = db.read_set_integers_by_id("AllTypes", "code", id);
    std::sort(set.begin(), set.end());
    EXPECT_EQ(set, (std::vector<int64_t>{10, 20, 30}));
}

TEST(Database, UpdateSetFloatsBasic) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("all_types.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e;
    e.set("label", std::string("Item 1"));
    int64_t id = db.create_element("AllTypes", e);

    quiver::Element update;
    update.set("weight", std::vector<double>{1.1, 2.2});
    db.update_element("AllTypes", id, update);

    auto set = db.read_set_floats_by_id("AllTypes", "weight", id);
    std::sort(set.begin(), set.end());
    EXPECT_EQ(set.size(), 2);
    EXPECT_DOUBLE_EQ(set[0], 1.1);
    EXPECT_DOUBLE_EQ(set[1], 2.2);
}

// ============================================================================
// update_element tests
// ============================================================================

TEST(Database, UpdateElementSingleScalar) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element e;
    e.set("label", std::string("Config 1")).set("integer_attribute", int64_t{42});
    int64_t id = db.create_element("Configuration", e);

    // Update single scalar attribute
    quiver::Element update;
    update.set("integer_attribute", int64_t{100});
    db.update_element("Configuration", id, update);

    auto val = db.read_scalar_integer_by_id("Configuration", "integer_attribute", id);
    EXPECT_TRUE(val.has_value());
    EXPECT_EQ(*val, 100);

    // Verify label unchanged
    auto label = db.read_scalar_string_by_id("Configuration", "label", id);
    EXPECT_TRUE(label.has_value());
    EXPECT_EQ(*label, "Config 1");
}

TEST(Database, UpdateElementMultipleScalars) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element e;
    e.set("label", std::string("Config 1"))
        .set("integer_attribute", int64_t{42})
        .set("float_attribute", 3.14)
        .set("string_attribute", std::string("hello"));
    int64_t id = db.create_element("Configuration", e);

    // Update multiple scalar attributes at once
    quiver::Element update;
    update.set("integer_attribute", int64_t{100})
        .set("float_attribute", 2.71)
        .set("string_attribute", std::string("world"));
    db.update_element("Configuration", id, update);

    auto integer_val = db.read_scalar_integer_by_id("Configuration", "integer_attribute", id);
    EXPECT_TRUE(integer_val.has_value());
    EXPECT_EQ(*integer_val, 100);

    auto float_val = db.read_scalar_float_by_id("Configuration", "float_attribute", id);
    EXPECT_TRUE(float_val.has_value());
    EXPECT_DOUBLE_EQ(*float_val, 2.71);

    auto str_val = db.read_scalar_string_by_id("Configuration", "string_attribute", id);
    EXPECT_TRUE(str_val.has_value());
    EXPECT_EQ(*str_val, "world");

    // Verify label unchanged
    auto label = db.read_scalar_string_by_id("Configuration", "label", id);
    EXPECT_TRUE(label.has_value());
    EXPECT_EQ(*label, "Config 1");
}

TEST(Database, UpdateElementOtherElementsUnchanged) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element e1;
    e1.set("label", std::string("Config 1")).set("integer_attribute", int64_t{42});
    int64_t id1 = db.create_element("Configuration", e1);

    quiver::Element e2;
    e2.set("label", std::string("Config 2")).set("integer_attribute", int64_t{100});
    int64_t id2 = db.create_element("Configuration", e2);

    // Update only first element
    quiver::Element update;
    update.set("integer_attribute", int64_t{999});
    db.update_element("Configuration", id1, update);

    // Verify first element changed
    auto val1 = db.read_scalar_integer_by_id("Configuration", "integer_attribute", id1);
    EXPECT_TRUE(val1.has_value());
    EXPECT_EQ(*val1, 999);

    // Verify second element unchanged
    auto val2 = db.read_scalar_integer_by_id("Configuration", "integer_attribute", id2);
    EXPECT_TRUE(val2.has_value());
    EXPECT_EQ(*val2, 100);
}

TEST(Database, UpdateElementWithArrays) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e;
    e.set("label", std::string("Item 1")).set("value_int", std::vector<int64_t>{1, 2, 3});
    int64_t id = db.create_element("Collection", e);

    // Update with element that has both scalars and arrays - both should be updated
    quiver::Element update;
    update.set("some_integer", int64_t{42}).set("value_int", std::vector<int64_t>{10, 20, 30});
    db.update_element("Collection", id, update);

    // Verify scalar was updated
    auto integer_val = db.read_scalar_integer_by_id("Collection", "some_integer", id);
    EXPECT_TRUE(integer_val.has_value());
    EXPECT_EQ(*integer_val, 42);

    // Verify vector was also updated
    auto vec = db.read_vector_integers_by_id("Collection", "value_int", id);
    EXPECT_EQ(vec, (std::vector<int64_t>{10, 20, 30}));
}

TEST(Database, UpdateElementWithSetOnly) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e;
    e.set("label", std::string("Item 1")).set("tag", std::vector<std::string>{"important", "urgent"});
    int64_t id = db.create_element("Collection", e);

    // Update with only set attribute
    quiver::Element update;
    update.set("tag", std::vector<std::string>{"new_tag1", "new_tag2"});
    db.update_element("Collection", id, update);

    // Verify set was updated
    auto set = db.read_set_strings_by_id("Collection", "tag", id);
    std::sort(set.begin(), set.end());
    EXPECT_EQ(set, (std::vector<std::string>{"new_tag1", "new_tag2"}));

    // Verify label unchanged
    auto label = db.read_scalar_string_by_id("Collection", "label", id);
    EXPECT_TRUE(label.has_value());
    EXPECT_EQ(*label, "Item 1");
}

TEST(Database, UpdateElementWithVectorAndSet) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e;
    e.set("label", std::string("Item 1"))
        .set("value_int", std::vector<int64_t>{1, 2, 3})
        .set("tag", std::vector<std::string>{"old_tag"});
    int64_t id = db.create_element("Collection", e);

    // Update both vector and set atomically
    quiver::Element update;
    update.set("value_int", std::vector<int64_t>{100, 200})
        .set("tag", std::vector<std::string>{"new_tag1", "new_tag2"});
    db.update_element("Collection", id, update);

    // Verify vector was updated
    auto vec = db.read_vector_integers_by_id("Collection", "value_int", id);
    EXPECT_EQ(vec, (std::vector<int64_t>{100, 200}));

    // Verify set was updated
    auto set = db.read_set_strings_by_id("Collection", "tag", id);
    std::sort(set.begin(), set.end());
    EXPECT_EQ(set, (std::vector<std::string>{"new_tag1", "new_tag2"}));
}

TEST(Database, UpdateElementWithTimeSeries) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e;
    e.set("label", std::string("Item 1"))
        .set("date_time", std::vector<std::string>{"2024-01-01T10:00:00", "2024-01-02T10:00:00"})
        .set("value", std::vector<double>{1.0, 2.0});
    int64_t id = db.create_element("Collection", e);

    // Update time series via update_element
    quiver::Element update;
    update
        .set("date_time", std::vector<std::string>{"2025-06-01T00:00:00", "2025-06-02T00:00:00", "2025-06-03T00:00:00"})
        .set("value", std::vector<double>{10.0, 20.0, 30.0});
    db.update_element("Collection", id, update);

    // Verify time series was updated
    auto rows = db.read_time_series_group("Collection", "data", id);
    EXPECT_EQ(rows.size(), 3);
    EXPECT_EQ(std::get<std::string>(rows[0].at("date_time")), "2025-06-01T00:00:00");
    EXPECT_EQ(std::get<std::string>(rows[1].at("date_time")), "2025-06-02T00:00:00");
    EXPECT_EQ(std::get<std::string>(rows[2].at("date_time")), "2025-06-03T00:00:00");
    EXPECT_DOUBLE_EQ(std::get<double>(rows[0].at("value")), 10.0);
    EXPECT_DOUBLE_EQ(std::get<double>(rows[1].at("value")), 20.0);
    EXPECT_DOUBLE_EQ(std::get<double>(rows[2].at("value")), 30.0);

    // Verify label unchanged
    auto label = db.read_scalar_string_by_id("Collection", "label", id);
    EXPECT_TRUE(label.has_value());
    EXPECT_EQ(*label, "Item 1");
}

TEST(Database, UpdateElementInvalidArrayAttribute) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e;
    e.set("label", std::string("Item 1"));
    int64_t id = db.create_element("Collection", e);

    // Try to update non-existent array attribute
    quiver::Element update;
    update.set("nonexistent_attr", std::vector<int64_t>{1, 2, 3});

    EXPECT_THROW(db.update_element("Collection", id, update), std::runtime_error);
}

// ============================================================================
// Update edge case tests
// ============================================================================

TEST(Database, UpdateVectorSingleElement) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e;
    e.set("label", std::string("Item 1")).set("value_int", std::vector<int64_t>{1, 2, 3});
    int64_t id = db.create_element("Collection", e);

    // Update to single element vector
    quiver::Element update;
    update.set("value_int", std::vector<int64_t>{42});
    db.update_element("Collection", id, update);

    auto vec = db.read_vector_integers_by_id("Collection", "value_int", id);
    EXPECT_EQ(vec, (std::vector<int64_t>{42}));
}

TEST(Database, UpdateSetSingleElement) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e;
    e.set("label", std::string("Item 1")).set("tag", std::vector<std::string>{"important", "urgent"});
    int64_t id = db.create_element("Collection", e);

    // Update to single element set
    quiver::Element update;
    update.set("tag", std::vector<std::string>{"single_tag"});
    db.update_element("Collection", id, update);

    auto set = db.read_set_strings_by_id("Collection", "tag", id);
    EXPECT_EQ(set, (std::vector<std::string>{"single_tag"}));
}

TEST(Database, UpdateVectorInvalidCollection) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element update;
    update.set("value_int", std::vector<int64_t>{1, 2, 3});
    EXPECT_THROW(db.update_element("NonexistentCollection", 1, update), std::runtime_error);
}

TEST(Database, UpdateSetInvalidCollection) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element update;
    update.set("tag", std::vector<std::string>{"tag1"});
    EXPECT_THROW(db.update_element("NonexistentCollection", 1, update), std::runtime_error);
}

TEST(Database, UpdateVectorFromEmptyToNonEmpty) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // Create element without vector data
    quiver::Element e;
    e.set("label", std::string("Item 1"));
    int64_t id = db.create_element("Collection", e);

    // Verify initially empty
    auto vec_initial = db.read_vector_integers_by_id("Collection", "value_int", id);
    EXPECT_TRUE(vec_initial.empty());

    // Update to non-empty vector
    quiver::Element update;
    update.set("value_int", std::vector<int64_t>{1, 2, 3});
    db.update_element("Collection", id, update);

    auto vec = db.read_vector_integers_by_id("Collection", "value_int", id);
    EXPECT_EQ(vec, (std::vector<int64_t>{1, 2, 3}));
}

TEST(Database, UpdateSetFromEmptyToNonEmpty) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // Create element without set data
    quiver::Element e;
    e.set("label", std::string("Item 1"));
    int64_t id = db.create_element("Collection", e);

    // Verify initially empty
    auto set_initial = db.read_set_strings_by_id("Collection", "tag", id);
    EXPECT_TRUE(set_initial.empty());

    // Update to non-empty set
    quiver::Element update;
    update.set("tag", std::vector<std::string>{"important", "urgent"});
    db.update_element("Collection", id, update);

    auto set = db.read_set_strings_by_id("Collection", "tag", id);
    std::sort(set.begin(), set.end());
    EXPECT_EQ(set, (std::vector<std::string>{"important", "urgent"}));
}

// ============================================================================
// DateTime update tests
// ============================================================================

TEST(Database, UpdateDateTimeScalar) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element e;
    e.set("label", std::string("Config 1"));
    int64_t id = db.create_element("Configuration", e);

    quiver::Element update;
    update.set("date_attribute", std::string("2024-03-17T09:00:00"));
    db.update_element("Configuration", id, update);

    auto date = db.read_scalar_string_by_id("Configuration", "date_attribute", id);
    EXPECT_TRUE(date.has_value());
    EXPECT_EQ(date.value(), "2024-03-17T09:00:00");
}

// ============================================================================
// Identifier validation tests
// ============================================================================

TEST(Database, UpdateVectorIntegersInvalidColumnThrows) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e;
    e.set("label", std::string("Item 1")).set("value_int", std::vector<int64_t>{1, 2, 3});
    int64_t id = db.create_element("Collection", e);

    EXPECT_THROW(
        {
            quiver::Element update;
            update.set("nonexistent_column", std::vector<int64_t>{1, 2, 3});
            db.update_element("Collection", id, update);
        },
        std::runtime_error);
}

TEST(Database, UpdateScalarStringTrimsWhitespace) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element e;
    e.set("label", std::string("Config 1")).set("string_attribute", std::string("hello"));
    int64_t id = db.create_element("Configuration", e);

    quiver::Element update;
    update.set("string_attribute", std::string("  world  "));
    db.update_element("Configuration", id, update);

    auto val = db.read_scalar_string_by_id("Configuration", "string_attribute", id);
    EXPECT_TRUE(val.has_value());
    EXPECT_EQ(*val, "world");
}

TEST(Database, UpdateSetStringsTrimsWhitespace) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e;
    e.set("label", std::string("Item 1"));
    int64_t id = db.create_element("Collection", e);

    quiver::Element update;
    update.set("tag", std::vector<std::string>{"  alpha  ", "\tbeta\n", " gamma "});
    db.update_element("Collection", id, update);

    auto set_vals = db.read_set_strings_by_id("Collection", "tag", id);
    std::sort(set_vals.begin(), set_vals.end());
    EXPECT_EQ(set_vals, (std::vector<std::string>{"alpha", "beta", "gamma"}));
}

// ============================================================================
// Update element FK label resolution tests
// ============================================================================

TEST(Database, UpdateElementScalarFkLabel) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("relations.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    // Create two parents
    quiver::Element p1, p2;
    p1.set("label", std::string("Parent 1"));
    p2.set("label", std::string("Parent 2"));
    db.create_element("Parent", p1);
    db.create_element("Parent", p2);

    // Create child with parent_id pointing to Parent 1
    quiver::Element child;
    child.set("label", std::string("Child 1"));
    child.set("parent_id", std::string("Parent 1"));
    db.create_element("Child", child);

    // Update child: change parent_id to Parent 2 using string label
    quiver::Element update;
    update.set("parent_id", std::string("Parent 2"));
    db.update_element("Child", 1, update);

    // Verify: parent_id resolved to Parent 2's ID (2)
    auto parent_ids = db.read_scalar_integers("Child", "parent_id");
    ASSERT_EQ(parent_ids.size(), 1);
    EXPECT_EQ(parent_ids[0], 2);
}

TEST(Database, UpdateElementScalarFkInteger) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("relations.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    // Create two parents
    quiver::Element p1, p2;
    p1.set("label", std::string("Parent 1"));
    p2.set("label", std::string("Parent 2"));
    db.create_element("Parent", p1);
    db.create_element("Parent", p2);

    // Create child with parent_id = 1 (integer)
    quiver::Element child;
    child.set("label", std::string("Child 1"));
    child.set("parent_id", int64_t{1});
    db.create_element("Child", child);

    // Update child: change parent_id to 2 using integer ID directly
    quiver::Element update;
    update.set("parent_id", int64_t{2});
    db.update_element("Child", 1, update);

    // Verify: parent_id updated to 2
    auto parent_ids = db.read_scalar_integers("Child", "parent_id");
    ASSERT_EQ(parent_ids.size(), 1);
    EXPECT_EQ(parent_ids[0], 2);
}

TEST(Database, UpdateElementVectorFkLabels) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("relations.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    // Create two parents
    quiver::Element p1, p2;
    p1.set("label", std::string("Parent 1"));
    p2.set("label", std::string("Parent 2"));
    db.create_element("Parent", p1);
    db.create_element("Parent", p2);

    // Create child with vector FK pointing to Parent 1
    quiver::Element child;
    child.set("label", std::string("Child 1"));
    child.set("parent_ref", std::vector<std::string>{"Parent 1"});
    db.create_element("Child", child);

    // Update child: change vector FK to {Parent 2, Parent 1}
    quiver::Element update;
    update.set("parent_ref", std::vector<std::string>{"Parent 2", "Parent 1"});
    db.update_element("Child", 1, update);

    // Verify: vector resolved to {2, 1}
    auto refs = db.read_vector_integers_by_id("Child", "parent_ref", 1);
    ASSERT_EQ(refs.size(), 2);
    EXPECT_EQ(refs[0], 2);
    EXPECT_EQ(refs[1], 1);
}

TEST(Database, UpdateElementSetFkLabels) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("relations.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    // Create two parents
    quiver::Element p1, p2;
    p1.set("label", std::string("Parent 1"));
    p2.set("label", std::string("Parent 2"));
    db.create_element("Parent", p1);
    db.create_element("Parent", p2);

    // Create child with set FK pointing to Parent 1
    quiver::Element child;
    child.set("label", std::string("Child 1"));
    child.set("mentor_id", std::vector<std::string>{"Parent 1"});
    db.create_element("Child", child);

    // Update child: change set FK to {Parent 2}
    quiver::Element update;
    update.set("mentor_id", std::vector<std::string>{"Parent 2"});
    db.update_element("Child", 1, update);

    // Verify: set resolved to {2}
    auto mentors = db.read_set_integers("Child", "mentor_id");
    ASSERT_EQ(mentors.size(), 1);
    ASSERT_EQ(mentors[0].size(), 1);
    EXPECT_EQ(mentors[0][0], 2);
}

TEST(Database, UpdateElementTimeSeriesFkLabels) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("relations.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    // Create two parents
    quiver::Element p1, p2;
    p1.set("label", std::string("Parent 1"));
    p2.set("label", std::string("Parent 2"));
    db.create_element("Parent", p1);
    db.create_element("Parent", p2);

    // Create child with time series FK pointing to Parent 1
    quiver::Element child;
    child.set("label", std::string("Child 1"));
    child.set("date_time", std::vector<std::string>{"2024-01-01"});
    child.set("sponsor_id", std::vector<std::string>{"Parent 1"});
    db.create_element("Child", child);

    // Update child: change time series FK to {Parent 2, Parent 1}
    quiver::Element update;
    update.set("date_time", std::vector<std::string>{"2024-06-01", "2024-06-02"});
    update.set("sponsor_id", std::vector<std::string>{"Parent 2", "Parent 1"});
    db.update_element("Child", 1, update);

    // Verify: time series resolved to {2, 1}
    auto ts_data = db.read_time_series_group("Child", "events", 1);
    ASSERT_EQ(ts_data.size(), 2);
    EXPECT_EQ(std::get<int64_t>(ts_data[0].at("sponsor_id")), 2);
    EXPECT_EQ(std::get<int64_t>(ts_data[1].at("sponsor_id")), 1);
}

TEST(Database, UpdateElementAllFkTypesInOneCall) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("relations.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    // Create two parents
    quiver::Element p1, p2;
    p1.set("label", std::string("Parent 1"));
    p2.set("label", std::string("Parent 2"));
    db.create_element("Parent", p1);
    db.create_element("Parent", p2);

    // Create child with all FK types pointing to Parent 1
    quiver::Element child;
    child.set("label", std::string("Child 1"));
    child.set("parent_id", std::string("Parent 1"));
    child.set("mentor_id", std::vector<std::string>{"Parent 1"});
    child.set("parent_ref", std::vector<std::string>{"Parent 1"});
    child.set("date_time", std::vector<std::string>{"2024-01-01"});
    child.set("sponsor_id", std::vector<std::string>{"Parent 1"});
    db.create_element("Child", child);

    // Update child: change all FK types to point to Parent 2
    quiver::Element update;
    update.set("parent_id", std::string("Parent 2"));
    update.set("mentor_id", std::vector<std::string>{"Parent 2"});
    update.set("parent_ref", std::vector<std::string>{"Parent 2"});
    update.set("date_time", std::vector<std::string>{"2025-01-01"});
    update.set("sponsor_id", std::vector<std::string>{"Parent 2"});
    db.update_element("Child", 1, update);

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

TEST(Database, UpdateElementNoFkColumnsUnchanged) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    // Create element in non-FK schema
    quiver::Element e;
    e.set("label", std::string("Config 1"))
        .set("integer_attribute", int64_t{42})
        .set("float_attribute", 3.14)
        .set("string_attribute", std::string("hello"));
    int64_t id = db.create_element("Configuration", e);

    // Update scalar attributes via update_element
    quiver::Element update;
    update.set("integer_attribute", int64_t{100})
        .set("float_attribute", 2.71)
        .set("string_attribute", std::string("world"));
    db.update_element("Configuration", id, update);

    // Verify values updated correctly (pre-resolve passthrough safe for non-FK schemas)
    auto integer_val = db.read_scalar_integer_by_id("Configuration", "integer_attribute", id);
    EXPECT_TRUE(integer_val.has_value());
    EXPECT_EQ(*integer_val, 100);

    auto float_val = db.read_scalar_float_by_id("Configuration", "float_attribute", id);
    EXPECT_TRUE(float_val.has_value());
    EXPECT_DOUBLE_EQ(*float_val, 2.71);

    auto str_val = db.read_scalar_string_by_id("Configuration", "string_attribute", id);
    EXPECT_TRUE(str_val.has_value());
    EXPECT_EQ(*str_val, "world");
}

// ============================================================================
// Type validation regression tests (BUG-01)
// ============================================================================

TEST(Database, UpdateElementTypeMismatchIntegerVectorWithStrings) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // Create element with valid integer vector data
    quiver::Element e;
    e.set("label", std::string("Item 1")).set("value_int", std::vector<int64_t>{1, 2, 3});
    int64_t id = db.create_element("Collection", e);

    // Try to update with string values in the integer vector column -- should throw
    // (error comes from FK resolution or type validation, either way it must not succeed)
    quiver::Element update;
    update.set("value_int", std::vector<std::string>{"bad", "data"});

    EXPECT_THROW(db.update_element("Collection", id, update), std::runtime_error);
}

TEST(Database, UpdateElementTypeMismatchTextSetWithIntegers) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // Create element with valid text set data
    quiver::Element e;
    e.set("label", std::string("Item 1")).set("tag", std::vector<std::string>{"important", "urgent"});
    int64_t id = db.create_element("Collection", e);

    // Try to update with integer values in the text set column -- should throw type validation error
    quiver::Element update;
    update.set("tag", std::vector<int64_t>{1, 2, 3});

    try {
        db.update_element("Collection", id, update);
        FAIL() << "Expected std::runtime_error for type mismatch in update_element";
    } catch (const std::runtime_error& e) {
        std::string msg = e.what();
        EXPECT_TRUE(msg.find("Cannot update_element: type mismatch") != std::string::npos)
            << "Expected type mismatch error, got: " << msg;
    }
}

// ============================================================================
// Empty array behavior tests
// ============================================================================

TEST(Database, UpdateElementEmptyArrayClearsRows) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // Create element with vector data
    quiver::Element e;
    e.set("label", std::string("Item 1")).set("value_int", std::vector<int64_t>{1, 2, 3});
    int64_t id = db.create_element("Collection", e);

    // Verify data exists
    auto vec_before = db.read_vector_integers_by_id("Collection", "value_int", id);
    EXPECT_EQ(vec_before.size(), 3);

    // Update with empty array -- should clear existing rows
    quiver::Element update;
    update.set("value_int", std::vector<int64_t>{});
    db.update_element("Collection", id, update);

    auto vec_after = db.read_vector_integers_by_id("Collection", "value_int", id);
    EXPECT_TRUE(vec_after.empty());
}

TEST(Database, UpdateElementFkResolutionFailurePreservesExisting) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("relations.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    // Create parent and child with parent_id pointing to Parent 1
    quiver::Element parent;
    parent.set("label", std::string("Parent 1"));
    db.create_element("Parent", parent);

    quiver::Element child;
    child.set("label", std::string("Child 1"));
    child.set("parent_id", std::string("Parent 1"));
    db.create_element("Child", child);

    // Attempt update with nonexistent parent label
    quiver::Element update;
    update.set("parent_id", std::string("Nonexistent Parent"));
    EXPECT_THROW(db.update_element("Child", 1, update), std::runtime_error);

    // Verify: original value preserved (parent_id still points to Parent 1's ID)
    auto parent_ids = db.read_scalar_integers("Child", "parent_id");
    ASSERT_EQ(parent_ids.size(), 1);
    EXPECT_EQ(parent_ids[0], 1);
}

TEST(Database, UpdateElementByIdNonExistent) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    db.create_element("Configuration", quiver::Element().set("label", std::string("Config 1")));

    // Updating a non-existent id throws "Element not found" rather than silently no-op'ing
    quiver::Element update;
    update.set("integer_attribute", int64_t{99});
    EXPECT_THROW(db.update_element("Configuration", 999, update), std::runtime_error);
}

TEST(Database, UpdateScalarTypeCoercionPolicy) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    int64_t id = db.create_element("Configuration", quiver::Element().set("label", std::string("Config 1")));

    // A float (even a whole-valued one) is rejected for an INTEGER column.
    EXPECT_THROW(db.update_element("Configuration", id, quiver::Element().set("integer_attribute", 42.0)),
                 std::runtime_error);

    // An integer is accepted for a REAL column (coerced to real on insert).
    db.update_element("Configuration", id, quiver::Element().set("float_attribute", int64_t{7}));
    auto val = db.read_scalar_float_by_id("Configuration", "float_attribute", id);
    ASSERT_TRUE(val.has_value());
    EXPECT_DOUBLE_EQ(*val, 7.0);
}

// ============================================================================
// Group update tests (update_vector_group / update_set_group)
// ============================================================================

namespace {

// relations.sql gives Child both a vector group and a set group that legally share the
// FK column name "parent_ref" (validate_no_duplicate_attributes exempts FK columns), which
// is exactly the shape that makes routing an array by column name alone ambiguous.
struct SharedFkFixture {
    quiver::Database db;
    int64_t parent_a;
    int64_t parent_b;
    int64_t child;

    SharedFkFixture()
        : db(quiver::Database::from_schema(":memory:",
                                           VALID_SCHEMA("relations.sql"),
                                           {.read_only = false, .console_level = quiver::LogLevel::Off})) {
        db.create_element("Configuration", quiver::Element().set("label", std::string("Config")));
        parent_a = db.create_element("Parent", quiver::Element().set("label", std::string("Parent A")));
        parent_b = db.create_element("Parent", quiver::Element().set("label", std::string("Parent B")));
        child = db.create_element("Child", quiver::Element().set("label", std::string("Child 1")));
    }
};

}  // namespace

TEST(Database, UpdateVectorGroupReplacesRows) {
    SharedFkFixture f;

    f.db.update_vector_group("Child", "refs", f.child, {{{"parent_ref", f.parent_a}}, {{"parent_ref", f.parent_b}}});
    EXPECT_EQ(f.db.read_vector_integers_by_id("Child", "parent_ref", f.child),
              (std::vector<int64_t>{f.parent_a, f.parent_b}));

    // A second call replaces rather than appends.
    f.db.update_vector_group("Child", "refs", f.child, {{{"parent_ref", f.parent_b}}});
    EXPECT_EQ(f.db.read_vector_integers_by_id("Child", "parent_ref", f.child), (std::vector<int64_t>{f.parent_b}));
}

TEST(Database, UpdateVectorGroupEmptyClearsRows) {
    SharedFkFixture f;

    f.db.update_vector_group("Child", "refs", f.child, {{{"parent_ref", f.parent_a}}});
    f.db.update_vector_group("Child", "refs", f.child, {});
    EXPECT_TRUE(f.db.read_vector_integers_by_id("Child", "parent_ref", f.child).empty());
}

TEST(Database, UpdateSetGroupReplacesRows) {
    SharedFkFixture f;

    f.db.update_set_group("Child", "parents", f.child, {{{"parent_ref", f.parent_a}}, {{"parent_ref", f.parent_b}}});
    EXPECT_EQ(f.db.read_set_integers_by_id("Child", "parent_ref", f.child),
              (std::vector<int64_t>{f.parent_a, f.parent_b}));

    f.db.update_set_group("Child", "parents", f.child, {});
    EXPECT_TRUE(f.db.read_set_integers_by_id("Child", "parent_ref", f.child).empty());
}

TEST(Database, UpdateGroupDoesNotTouchSiblingGroupSharingAColumnName) {
    SharedFkFixture f;

    f.db.update_set_group("Child", "parents", f.child, {{{"parent_ref", f.parent_a}}});
    f.db.update_vector_group("Child", "refs", f.child, {{{"parent_ref", f.parent_b}}});

    // Each group keeps its own rows: (collection, group) names exactly one table.
    EXPECT_EQ(f.db.read_set_integers_by_id("Child", "parent_ref", f.child), (std::vector<int64_t>{f.parent_a}));
    EXPECT_EQ(f.db.read_vector_integers_by_id("Child", "parent_ref", f.child), (std::vector<int64_t>{f.parent_b}));

    // Clearing one group leaves the other intact.
    f.db.update_vector_group("Child", "refs", f.child, {});
    EXPECT_EQ(f.db.read_set_integers_by_id("Child", "parent_ref", f.child), (std::vector<int64_t>{f.parent_a}));
}

TEST(Database, UpdateGroupResolvesFkLabels) {
    SharedFkFixture f;

    f.db.update_vector_group("Child", "refs", f.child, {{{"parent_ref", std::string("Parent B")}}});
    EXPECT_EQ(f.db.read_vector_integers_by_id("Child", "parent_ref", f.child), (std::vector<int64_t>{f.parent_b}));
}

TEST(Database, UpdateGroupFkResolutionFailurePreservesExistingRows) {
    SharedFkFixture f;

    f.db.update_vector_group("Child", "refs", f.child, {{{"parent_ref", f.parent_a}}});
    EXPECT_THROW(f.db.update_vector_group("Child", "refs", f.child, {{{"parent_ref", std::string("No Such Parent")}}}),
                 std::runtime_error);

    // The failed lookup happens before the DELETE, so the group is untouched.
    EXPECT_EQ(f.db.read_vector_integers_by_id("Child", "parent_ref", f.child), (std::vector<int64_t>{f.parent_a}));
}

TEST(Database, UpdateGroupUnknownGroupThrows) {
    SharedFkFixture f;

    EXPECT_THROW(f.db.update_vector_group("Child", "nope", f.child, {{{"parent_ref", f.parent_a}}}),
                 std::runtime_error);
    EXPECT_THROW(f.db.update_set_group("Child", "nope", f.child, {{{"parent_ref", f.parent_a}}}), std::runtime_error);
}

TEST(Database, UpdateGroupUnknownColumnThrows) {
    SharedFkFixture f;

    EXPECT_THROW(f.db.update_vector_group("Child", "refs", f.child, {{{"not_a_column", int64_t{1}}}}),
                 std::runtime_error);
}

TEST(Database, UpdateGroupPreservesNullCells) {
    SharedFkFixture f;

    f.db.update_vector_group("Child",
                             "refs",
                             f.child,
                             {{{"parent_ref", f.parent_a}}, {{"parent_ref", nullptr}}, {{"parent_ref", f.parent_b}}});

    // read_vector_group_by_id keeps NULL cells positionally (the dense per-column reader drops them).
    auto rows = f.db.read_vector_group_by_id("Child", "refs", f.child);
    ASSERT_EQ(rows.size(), 3u);
    EXPECT_EQ(std::get<int64_t>(rows[0].at("parent_ref")), f.parent_a);
    EXPECT_TRUE(std::holds_alternative<std::nullptr_t>(rows[1].at("parent_ref")));
    EXPECT_EQ(std::get<int64_t>(rows[2].at("parent_ref")), f.parent_b);
}

// Characterization test for the hazard the group API exists to avoid: routing an array by
// column name through update_element writes it into EVERY group table of the collection that
// has a column with that name, so a write aimed at the vector group silently rewrites the set
// group. Long-standing behaviour, pinned here rather than changed (it now logs a warning);
// callers who need one group use update_vector_group / update_set_group (see
// UpdateGroupDoesNotTouchSiblingGroupSharingAColumnName).
TEST(Database, UpdateElementSharedColumnNameWritesEveryMatchingGroup) {
    SharedFkFixture f;
    f.db.update_set_group("Child", "parents", f.child, {{{"parent_ref", f.parent_a}}});

    quiver::Element e;
    e.set("parent_ref", std::vector<int64_t>{f.parent_b});
    f.db.update_element("Child", f.child, e);

    EXPECT_EQ(f.db.read_vector_integers_by_id("Child", "parent_ref", f.child), (std::vector<int64_t>{f.parent_b}));
    // The set group was collateral damage - it now holds the vector's value, not parent_a.
    EXPECT_EQ(f.db.read_set_integers_by_id("Child", "parent_ref", f.child), (std::vector<int64_t>{f.parent_b}));
}

namespace {

// A vector group and a set group whose value columns are more than one, so the core's
// name-ordered column map has a first entry that is not the only entry.
struct MultiColumnGroupFixture {
    quiver::Database db;
    int64_t item;

    MultiColumnGroupFixture()
        : db(quiver::Database::from_schema(":memory:",
                                           VALID_SCHEMA("multi_column_groups.sql"),
                                           {.read_only = false, .console_level = quiver::LogLevel::Off})) {
        db.create_element("Configuration", quiver::Element().set("label", std::string("Config")));
        item = db.create_element("Items", quiver::Element().set("label", std::string("Item 1")));
    }
};

}  // namespace

// The column map is name-sorted, so "amount" is visited first. An empty first column used to
// leave the row count at 0 for "score" to overwrite, skipping the same-length check and then
// indexing the empty vector - a heap read past the end, bound straight into SQLite.
TEST(Database, UpdateElementEmptyFirstColumnOfMultiColumnGroupThrows) {
    MultiColumnGroupFixture f;

    quiver::Element e;
    e.set("amount", std::vector<double>{});
    e.set("score", std::vector<double>{1.0, 2.0, 3.0});
    EXPECT_THROW(f.db.update_element("Items", f.item, e), std::runtime_error);

    // Reversed roles: a non-empty first column with an empty second one was already caught.
    quiver::Element e2;
    e2.set("amount", std::vector<double>{1.0, 2.0, 3.0});
    e2.set("score", std::vector<double>{});
    EXPECT_THROW(f.db.update_element("Items", f.item, e2), std::runtime_error);
}

TEST(Database, UpdateGroupMultiColumnRoundTrips) {
    MultiColumnGroupFixture f;

    f.db.update_vector_group(
        "Items", "readings", f.item, {{{"amount", 1.5}, {"score", 10.0}}, {{"amount", 2.5}, {"score", 20.0}}});

    auto rows = f.db.read_vector_group_by_id("Items", "readings", f.item);
    ASSERT_EQ(rows.size(), 2u);
    EXPECT_DOUBLE_EQ(std::get<double>(rows[0].at("amount")), 1.5);
    EXPECT_DOUBLE_EQ(std::get<double>(rows[1].at("score")), 20.0);
}

// A column named only in a later row used to be dropped silently: the column set came from
// rows[0]. update_time_series_group validates (and keeps) every row's keys.
TEST(Database, UpdateGroupKeepsColumnPresentOnlyInALaterRow) {
    MultiColumnGroupFixture f;

    f.db.update_vector_group("Items", "readings", f.item, {{{"amount", 1.5}}, {{"amount", 2.5}, {"score", 20.0}}});

    auto rows = f.db.read_vector_group_by_id("Items", "readings", f.item);
    ASSERT_EQ(rows.size(), 2u);
    // The row that omitted "score" gets SQL NULL, not a dropped column.
    EXPECT_TRUE(std::holds_alternative<std::nullptr_t>(rows[0].at("score")));
    EXPECT_DOUBLE_EQ(std::get<double>(rows[1].at("score")), 20.0);
}

TEST(Database, UpdateGroupUnknownColumnInALaterRowThrows) {
    MultiColumnGroupFixture f;

    EXPECT_THROW(f.db.update_vector_group("Items", "readings", f.item, {{{"amount", 1.5}}, {{"not_a_column", 2.5}}}),
                 std::runtime_error);
    // Nothing was written: validation runs before the DELETE and before any insert.
    EXPECT_TRUE(f.db.read_vector_group_by_id("Items", "readings", f.item).empty());
}

// id and vector_index are derived (the element and the row's position). Passing either used to
// duplicate it in the INSERT column list, where SQLite keeps the first occurrence - so the
// caller's value was discarded and the call still reported success.
TEST(Database, UpdateGroupRejectsStructuralColumns) {
    MultiColumnGroupFixture f;

    EXPECT_THROW(
        f.db.update_vector_group("Items", "readings", f.item, {{{"amount", 1.5}, {"vector_index", int64_t{7}}}}),
        std::runtime_error);
    EXPECT_THROW(f.db.update_vector_group("Items", "readings", f.item, {{{"amount", 1.5}, {"id", int64_t{2}}}}),
                 std::runtime_error);
    EXPECT_THROW(f.db.update_set_group("Items", "codes", f.item, {{{"code", std::string("a")}, {"id", int64_t{2}}}}),
                 std::runtime_error);
}

TEST(Database, UpdateGroupMissingElementThrowsNotFound) {
    MultiColumnGroupFixture f;

    EXPECT_THROW(f.db.update_vector_group("Items", "readings", 999, {{{"amount", 1.5}}}), std::runtime_error);
    // The clear path used to succeed silently: the DELETE simply matched nothing.
    EXPECT_THROW(f.db.update_vector_group("Items", "readings", 999, {}), std::runtime_error);
    EXPECT_THROW(f.db.update_set_group("Items", "codes", 999, {}), std::runtime_error);

    try {
        f.db.update_vector_group("Items", "readings", 999, {{{"amount", 1.5}}});
        FAIL() << "expected a throw";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "Element not found: 999 in collection 'Items'");
    }
}

// Pattern 2 capitalizes the entity, and get_vector_metadata / read_vector_group_by_id already
// report this exact condition that way.
TEST(Database, UpdateGroupNotFoundMessageMatchesMetadata) {
    MultiColumnGroupFixture f;

    try {
        f.db.update_vector_group("Items", "nope", f.item, {{{"amount", 1.5}}});
        FAIL() << "expected a throw";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "Vector group not found: 'nope' in collection 'Items'");
    }
    try {
        f.db.update_set_group("Items", "nope", f.item, {{{"code", std::string("a")}}});
        FAIL() << "expected a throw";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "Set group not found: 'nope' in collection 'Items'");
    }
}

// TransactionGuard no-ops while a transaction is already open, so a throw after the DELETE has
// nothing to roll back. Validation must therefore precede the DELETE, not follow it.
TEST(Database, UpdateGroupTypeErrorInsideDryRunKeepsExistingRows) {
    MultiColumnGroupFixture f;
    f.db.update_set_group("Items", "codes", f.item, {{{"code", std::string("keep")}}});

    f.db.begin_dry_run();
    EXPECT_THROW(f.db.update_set_group("Items", "codes", f.item, {{{"code", 1.5}}}), std::runtime_error);
    auto rows = f.db.read_set_group_by_id("Items", "codes", f.item);
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(std::get<std::string>(rows[0].at("code")), "keep");
    f.db.end_dry_run();
}

// ============================================================================
// Update by label tests
// ============================================================================

TEST(Database, UpdateElementByLabel) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e;
    e.set("label", std::string("Item 1")).set("some_integer", int64_t{100}).set("tag", std::vector<std::string>{"old"});
    int64_t id = db.create_element("Collection", e);

    // A second element so the assertions below cannot pass vacuously: with only one element an
    // update that ignored the label entirely would still hit the right row.
    quiver::Element other;
    other.set("label", std::string("Item 2"))
        .set("some_integer", int64_t{200})
        .set("tag", std::vector<std::string>{"keep"});
    int64_t other_id = db.create_element("Collection", other);

    quiver::Element update;
    update.set("some_integer", int64_t{999}).set("tag", std::vector<std::string>{"alpha", "beta"});
    db.update_element_by_label("Collection", "Item 1", update);

    auto value = db.read_scalar_integer_by_id("Collection", "some_integer", id);
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, 999);

    // Only the labelled element changed
    auto other_value = db.read_scalar_integer_by_id("Collection", "some_integer", other_id);
    ASSERT_TRUE(other_value.has_value());
    EXPECT_EQ(*other_value, 200);

    // An update that does not name the label leaves it alone
    auto label = db.read_scalar_string_by_id("Collection", "label", id);
    ASSERT_TRUE(label.has_value());
    EXPECT_EQ(*label, "Item 1");

    // Arrays route to their group tables through the delegation, replacing the existing rows -
    // and only the labelled element's.
    auto tags = db.read_set_strings_by_id("Collection", "tag", id);
    std::sort(tags.begin(), tags.end());
    EXPECT_EQ(tags, (std::vector<std::string>{"alpha", "beta"}));
    EXPECT_EQ(db.read_set_strings_by_id("Collection", "tag", other_id), (std::vector<std::string>{"keep"}));
}

// The label is an ordinary scalar, so a rename resolves against the old value and then writes the
// new one - after which only the new label resolves.
TEST(Database, UpdateElementByLabelRenames) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e;
    e.set("label", std::string("Item 1"));
    int64_t id = db.create_element("Collection", e);

    quiver::Element rename;
    rename.set("label", std::string("Renamed"));
    db.update_element_by_label("Collection", "Item 1", rename);

    auto label = db.read_scalar_string_by_id("Collection", "label", id);
    ASSERT_TRUE(label.has_value());
    EXPECT_EQ(*label, "Renamed");

    // The old label no longer resolves
    try {
        db.update_element_by_label("Collection", "Item 1", rename);
        FAIL() << "expected a throw";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "Element not found: label 'Item 1' in collection 'Collection'");
    }

    // ...and the new one does
    quiver::Element update;
    update.set("some_integer", int64_t{7});
    db.update_element_by_label("Collection", "Renamed", update);
    EXPECT_EQ(*db.read_scalar_integer_by_id("Collection", "some_integer", id), 7);
}

TEST(Database, UpdateElementByLabelNonExistent) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e;
    e.set("label", std::string("Item 1")).set("some_integer", int64_t{100});
    int64_t id = db.create_element("Collection", e);

    quiver::Element update;
    update.set("some_integer", int64_t{999});

    // Updating an unresolvable label throws "Element not found" rather than silently no-op'ing
    try {
        db.update_element_by_label("Collection", "No Such Item", update);
        FAIL() << "expected a throw";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "Element not found: label 'No Such Item' in collection 'Collection'");
    }

    // Nothing was written
    EXPECT_EQ(*db.read_scalar_integer_by_id("Collection", "some_integer", id), 100);

    // A label is unique per collection, not per database: one naming an element of another
    // collection must not resolve here.
    try {
        db.update_element_by_label("Collection", "Test Config", update);
        FAIL() << "expected a throw";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "Element not found: label 'Test Config' in collection 'Collection'");
    }
}

// require_collection only checks has_table, so a group table reaches resolve_label. The explicit
// require_column("label") is what stops it there instead of letting the SELECT leak a raw
// "no such column: label" prepare error -- and the message must name the public method called.
TEST(Database, UpdateElementByLabelOnTableWithoutLabelColumn) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element update;
    update.set("some_integer", int64_t{999});

    try {
        db.update_element_by_label("Collection_set_tags", "anything", update);
        FAIL() << "expected a throw";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(),
                     "Cannot update_element_by_label: column 'label' not found in table 'Collection_set_tags'");
    }
}

// resolve_label owns the lookup; everything past it is update_element's, so the element
// validation reports "Cannot update_element" - the operation that validated.
TEST(Database, UpdateElementByLabelValidationNamesTheIdForm) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e;
    e.set("label", std::string("Item 1"));
    db.create_element("Collection", e);

    try {
        db.update_element_by_label("Collection", "Item 1", quiver::Element{});
        FAIL() << "expected a throw";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "Cannot update_element: element must have at least one attribute to update");
    }

    quiver::Element bad_type;
    bad_type.set("some_integer", 1.5);
    try {
        db.update_element_by_label("Collection", "Item 1", bad_type);
        FAIL() << "expected a throw";
    } catch (const std::runtime_error& e) {
        std::string msg = e.what();
        EXPECT_TRUE(msg.find("Cannot update_element: type mismatch") != std::string::npos) << "Actual: " << msg;
    }
}

TEST(Database, UpdateVectorGroupByLabel) {
    SharedFkFixture f;

    // A second child so the assertions cannot pass vacuously: with only one element an update that
    // ignored the label entirely would still hit the right row.
    int64_t other = f.db.create_element("Child", quiver::Element().set("label", std::string("Child 2")));
    f.db.update_vector_group("Child", "refs", other, {{{"parent_ref", f.parent_a}}});

    f.db.update_vector_group_by_label(
        "Child", "refs", "Child 1", {{{"parent_ref", f.parent_a}}, {{"parent_ref", f.parent_b}}});
    EXPECT_EQ(f.db.read_vector_integers_by_id("Child", "parent_ref", f.child),
              (std::vector<int64_t>{f.parent_a, f.parent_b}));
    EXPECT_EQ(f.db.read_vector_integers_by_id("Child", "parent_ref", other), (std::vector<int64_t>{f.parent_a}));

    // A second call replaces rather than appends, and an empty row list clears.
    f.db.update_vector_group_by_label("Child", "refs", "Child 1", {{{"parent_ref", f.parent_b}}});
    EXPECT_EQ(f.db.read_vector_integers_by_id("Child", "parent_ref", f.child), (std::vector<int64_t>{f.parent_b}));

    f.db.update_vector_group_by_label("Child", "refs", "Child 1", {});
    EXPECT_TRUE(f.db.read_vector_integers_by_id("Child", "parent_ref", f.child).empty());
    EXPECT_EQ(f.db.read_vector_integers_by_id("Child", "parent_ref", other), (std::vector<int64_t>{f.parent_a}));
}

TEST(Database, UpdateVectorGroupByLabelNonExistent) {
    SharedFkFixture f;

    f.db.update_vector_group("Child", "refs", f.child, {{{"parent_ref", f.parent_a}}});

    try {
        f.db.update_vector_group_by_label("Child", "refs", "No Such Child", {{{"parent_ref", f.parent_b}}});
        FAIL() << "expected a throw";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "Element not found: label 'No Such Child' in collection 'Child'");
    }

    // Nothing was written - the lookup throws before the group is cleared.
    EXPECT_EQ(f.db.read_vector_integers_by_id("Child", "parent_ref", f.child), (std::vector<int64_t>{f.parent_a}));

    // A label is unique per collection, not per database: one naming an element of another
    // collection must not resolve here.
    try {
        f.db.update_vector_group_by_label("Child", "refs", "Parent A", {});
        FAIL() << "expected a throw";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "Element not found: label 'Parent A' in collection 'Child'");
    }
}

// require_collection only checks has_table, so a group table reaches resolve_label. The explicit
// require_column("label") is what stops it there instead of letting the SELECT leak a raw
// "no such column: label" prepare error -- and the message must name the public method called.
TEST(Database, UpdateVectorGroupByLabelOnTableWithoutLabelColumn) {
    SharedFkFixture f;

    try {
        f.db.update_vector_group_by_label("Child_vector_refs", "refs", "anything", {});
        FAIL() << "expected a throw";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(),
                     "Cannot update_vector_group_by_label: column 'label' not found in table 'Child_vector_refs'");
    }
}

// resolve_label owns the lookup; everything past it is update_vector_group's, so the column
// validation reports "Cannot update_vector_group" - the operation that validated.
TEST(Database, UpdateVectorGroupByLabelValidationNamesTheIdForm) {
    SharedFkFixture f;

    try {
        f.db.update_vector_group_by_label("Child", "refs", "Child 1", {{{"nope", int64_t{1}}}});
        FAIL() << "expected a throw";
    } catch (const std::runtime_error& e) {
        std::string msg = e.what();
        EXPECT_TRUE(msg.find("Cannot update_vector_group:") != std::string::npos) << "Actual: " << msg;
    }
}
