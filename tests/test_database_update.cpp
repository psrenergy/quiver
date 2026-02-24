#include "test_utils.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <quiver/database.h>
#include <quiver/element.h>

// ============================================================================
// Update scalar tests
// ============================================================================

TEST(Database, UpdateScalarInteger) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    quiver::Element e;
    e.set("label", std::string("Config 1")).set("integer_attribute", int64_t{42});
    int64_t id = db.create_element("Configuration", e);

    db.update_scalar_integer("Configuration", "integer_attribute", id, 100);

    auto val = db.read_scalar_integer_by_id("Configuration", "integer_attribute", id);
    EXPECT_TRUE(val.has_value());
    EXPECT_EQ(*val, 100);
}

TEST(Database, UpdateScalarFloat) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    quiver::Element e;
    e.set("label", std::string("Config 1")).set("float_attribute", 3.14);
    int64_t id = db.create_element("Configuration", e);

    db.update_scalar_float("Configuration", "float_attribute", id, 2.71);

    auto val = db.read_scalar_float_by_id("Configuration", "float_attribute", id);
    EXPECT_TRUE(val.has_value());
    EXPECT_DOUBLE_EQ(*val, 2.71);
}

TEST(Database, UpdateScalarString) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    quiver::Element e;
    e.set("label", std::string("Config 1")).set("string_attribute", std::string("hello"));
    int64_t id = db.create_element("Configuration", e);

    db.update_scalar_string("Configuration", "string_attribute", id, "world");

    auto val = db.read_scalar_string_by_id("Configuration", "string_attribute", id);
    EXPECT_TRUE(val.has_value());
    EXPECT_EQ(*val, "world");
}

TEST(Database, UpdateScalarMultipleElements) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    quiver::Element e1;
    e1.set("label", std::string("Config 1")).set("integer_attribute", int64_t{42});
    int64_t id1 = db.create_element("Configuration", e1);

    quiver::Element e2;
    e2.set("label", std::string("Config 2")).set("integer_attribute", int64_t{100});
    int64_t id2 = db.create_element("Configuration", e2);

    // Update only first element
    db.update_scalar_integer("Configuration", "integer_attribute", id1, 999);

    // Verify first element changed
    auto val1 = db.read_scalar_integer_by_id("Configuration", "integer_attribute", id1);
    EXPECT_TRUE(val1.has_value());
    EXPECT_EQ(*val1, 999);

    // Verify second element unchanged
    auto val2 = db.read_scalar_integer_by_id("Configuration", "integer_attribute", id2);
    EXPECT_TRUE(val2.has_value());
    EXPECT_EQ(*val2, 100);
}

// ============================================================================
// Update vector tests
// ============================================================================

TEST(Database, UpdateVectorIntegers) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e;
    e.set("label", std::string("Item 1")).set("value_int", std::vector<int64_t>{1, 2, 3});
    int64_t id = db.create_element("Collection", e);

    db.update_vector_integers("Collection", "value_int", id, {10, 20, 30, 40});

    auto vec = db.read_vector_integers_by_id("Collection", "value_int", id);
    EXPECT_EQ(vec, (std::vector<int64_t>{10, 20, 30, 40}));
}

TEST(Database, UpdateVectorFloats) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e;
    e.set("label", std::string("Item 1")).set("value_float", std::vector<double>{1.5, 2.5, 3.5});
    int64_t id = db.create_element("Collection", e);

    db.update_vector_floats("Collection", "value_float", id, {10.5, 20.5});

    auto vec = db.read_vector_floats_by_id("Collection", "value_float", id);
    EXPECT_EQ(vec, (std::vector<double>{10.5, 20.5}));
}

TEST(Database, UpdateVectorToEmpty) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e;
    e.set("label", std::string("Item 1")).set("value_int", std::vector<int64_t>{1, 2, 3});
    int64_t id = db.create_element("Collection", e);

    db.update_vector_integers("Collection", "value_int", id, {});

    auto vec = db.read_vector_integers_by_id("Collection", "value_int", id);
    EXPECT_TRUE(vec.empty());
}

TEST(Database, UpdateVectorMultipleElements) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

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
    db.update_vector_integers("Collection", "value_int", id1, {100, 200});

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
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e;
    e.set("label", std::string("Item 1")).set("tag", std::vector<std::string>{"important", "urgent"});
    int64_t id = db.create_element("Collection", e);

    db.update_set_strings("Collection", "tag", id, {"new_tag1", "new_tag2", "new_tag3"});

    auto set = db.read_set_strings_by_id("Collection", "tag", id);
    std::sort(set.begin(), set.end());
    EXPECT_EQ(set, (std::vector<std::string>{"new_tag1", "new_tag2", "new_tag3"}));
}

TEST(Database, UpdateSetToEmpty) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e;
    e.set("label", std::string("Item 1")).set("tag", std::vector<std::string>{"important", "urgent"});
    int64_t id = db.create_element("Collection", e);

    db.update_set_strings("Collection", "tag", id, {});

    auto set = db.read_set_strings_by_id("Collection", "tag", id);
    EXPECT_TRUE(set.empty());
}

TEST(Database, UpdateSetMultipleElements) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

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
    db.update_set_strings("Collection", "tag", id1, {"updated"});

    // Verify first element changed
    auto set1 = db.read_set_strings_by_id("Collection", "tag", id1);
    EXPECT_EQ(set1, (std::vector<std::string>{"updated"}));

    // Verify second element unchanged
    auto set2 = db.read_set_strings_by_id("Collection", "tag", id2);
    std::sort(set2.begin(), set2.end());
    EXPECT_EQ(set2, (std::vector<std::string>{"review", "urgent"}));
}

// ============================================================================
// update_element tests
// ============================================================================

TEST(Database, UpdateElementSingleScalar) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

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
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

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
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

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
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

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
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

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
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

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
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

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
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

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
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e;
    e.set("label", std::string("Item 1")).set("value_int", std::vector<int64_t>{1, 2, 3});
    int64_t id = db.create_element("Collection", e);

    // Update to single element vector
    db.update_vector_integers("Collection", "value_int", id, {42});

    auto vec = db.read_vector_integers_by_id("Collection", "value_int", id);
    EXPECT_EQ(vec, (std::vector<int64_t>{42}));
}

TEST(Database, UpdateSetSingleElement) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e;
    e.set("label", std::string("Item 1")).set("tag", std::vector<std::string>{"important", "urgent"});
    int64_t id = db.create_element("Collection", e);

    // Update to single element set
    db.update_set_strings("Collection", "tag", id, {"single_tag"});

    auto set = db.read_set_strings_by_id("Collection", "tag", id);
    EXPECT_EQ(set, (std::vector<std::string>{"single_tag"}));
}

TEST(Database, UpdateScalarInvalidCollection) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    EXPECT_THROW(db.update_scalar_integer("NonexistentCollection", "integer_attribute", 1, 42), std::runtime_error);
}

TEST(Database, UpdateScalarInvalidAttribute) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    quiver::Element e;
    e.set("label", std::string("Config 1")).set("integer_attribute", int64_t{42});
    int64_t id = db.create_element("Configuration", e);

    EXPECT_THROW(db.update_scalar_integer("Configuration", "nonexistent_attribute", id, 100), std::runtime_error);
}

TEST(Database, UpdateVectorInvalidCollection) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.update_vector_integers("NonexistentCollection", "value_int", 1, {1, 2, 3}), std::runtime_error);
}

TEST(Database, UpdateSetInvalidCollection) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.update_set_strings("NonexistentCollection", "tag", 1, {"tag1"}), std::runtime_error);
}

TEST(Database, UpdateVectorFromEmptyToNonEmpty) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

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
    db.update_vector_integers("Collection", "value_int", id, {1, 2, 3});

    auto vec = db.read_vector_integers_by_id("Collection", "value_int", id);
    EXPECT_EQ(vec, (std::vector<int64_t>{1, 2, 3}));
}

TEST(Database, UpdateSetFromEmptyToNonEmpty) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

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
    db.update_set_strings("Collection", "tag", id, {"important", "urgent"});

    auto set = db.read_set_strings_by_id("Collection", "tag", id);
    std::sort(set.begin(), set.end());
    EXPECT_EQ(set, (std::vector<std::string>{"important", "urgent"}));
}

// ============================================================================
// DateTime update tests
// ============================================================================

TEST(Database, UpdateDateTimeScalar) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    quiver::Element e;
    e.set("label", std::string("Config 1"));
    int64_t id = db.create_element("Configuration", e);

    db.update_scalar_string("Configuration", "date_attribute", id, "2024-03-17T09:00:00");

    auto date = db.read_scalar_string_by_id("Configuration", "date_attribute", id);
    EXPECT_TRUE(date.has_value());
    EXPECT_EQ(date.value(), "2024-03-17T09:00:00");
}

// ============================================================================
// Identifier validation tests
// ============================================================================

TEST(Database, UpdateVectorIntegersInvalidColumnThrows) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e;
    e.set("label", std::string("Item 1")).set("value_int", std::vector<int64_t>{1, 2, 3});
    int64_t id = db.create_element("Collection", e);

    EXPECT_THROW(
        {
            try {
                db.update_vector_integers("Collection", "nonexistent_column", id, {1, 2, 3});
            } catch (const std::runtime_error& err) {
                EXPECT_THAT(std::string(err.what()), testing::HasSubstr("not found"));
                throw;
            }
        },
        std::runtime_error);
}

// ============================================================================
// Update element FK label resolution tests
// ============================================================================

TEST(Database, UpdateElementScalarFkLabel) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("relations.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

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
        ":memory:", VALID_SCHEMA("relations.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

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
        ":memory:", VALID_SCHEMA("relations.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

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
        ":memory:", VALID_SCHEMA("relations.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

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
        ":memory:", VALID_SCHEMA("relations.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

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
        ":memory:", VALID_SCHEMA("relations.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

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
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

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

TEST(Database, UpdateElementFkResolutionFailurePreservesExisting) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("relations.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

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
