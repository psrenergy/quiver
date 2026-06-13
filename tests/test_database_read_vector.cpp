#include "test_utils.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <quiver/database.h>
#include <quiver/element.h>

// ============================================================================
// Read vector tests
// ============================================================================

TEST(Database, ReadVectorIntegers) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e1;
    e1.set("label", std::string("Item 1")).set("value_int", std::vector<int64_t>{1, 2, 3});
    db.create_element("Collection", e1);

    quiver::Element e2;
    e2.set("label", std::string("Item 2")).set("value_int", std::vector<int64_t>{10, 20});
    db.create_element("Collection", e2);

    auto vectors = db.read_vector_integers("Collection", "value_int");
    EXPECT_EQ(vectors.size(), 2);
    EXPECT_EQ(vectors[0], (std::vector<int64_t>{1, 2, 3}));
    EXPECT_EQ(vectors[1], (std::vector<int64_t>{10, 20}));
}

TEST(Database, ReadVectorFloats) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e1;
    e1.set("label", std::string("Item 1")).set("value_float", std::vector<double>{1.5, 2.5, 3.5});
    db.create_element("Collection", e1);

    quiver::Element e2;
    e2.set("label", std::string("Item 2")).set("value_float", std::vector<double>{10.5, 20.5});
    db.create_element("Collection", e2);

    auto vectors = db.read_vector_floats("Collection", "value_float");
    EXPECT_EQ(vectors.size(), 2);
    EXPECT_EQ(vectors[0], (std::vector<double>{1.5, 2.5, 3.5}));
    EXPECT_EQ(vectors[1], (std::vector<double>{10.5, 20.5}));
}

TEST(Database, ReadVectorEmpty) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // No Collection elements created
    auto integer_vectors = db.read_vector_integers("Collection", "value_int");
    auto float_vectors = db.read_vector_floats("Collection", "value_float");

    EXPECT_TRUE(integer_vectors.empty());
    EXPECT_TRUE(float_vectors.empty());
}

TEST(Database, ReadVectorOnlyReturnsElementsWithData) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // Element with vector data
    quiver::Element e1;
    e1.set("label", std::string("Item 1")).set("value_int", std::vector<int64_t>{1, 2, 3});
    db.create_element("Collection", e1);

    // Element without vector data
    quiver::Element e2;
    e2.set("label", std::string("Item 2"));
    db.create_element("Collection", e2);

    // Another element with vector data
    quiver::Element e3;
    e3.set("label", std::string("Item 3")).set("value_int", std::vector<int64_t>{4, 5});
    db.create_element("Collection", e3);

    // Only elements with vector data are returned
    auto vectors = db.read_vector_integers("Collection", "value_int");
    EXPECT_EQ(vectors.size(), 2);
    EXPECT_EQ(vectors[0], (std::vector<int64_t>{1, 2, 3}));
    EXPECT_EQ(vectors[1], (std::vector<int64_t>{4, 5}));
}

// ============================================================================
// Read vector by ID tests
// ============================================================================

TEST(Database, ReadVectorIntegerById) {
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

    auto vec1 = db.read_vector_integers_by_id("Collection", "value_int", id1);
    auto vec2 = db.read_vector_integers_by_id("Collection", "value_int", id2);

    EXPECT_EQ(vec1, (std::vector<int64_t>{1, 2, 3}));
    EXPECT_EQ(vec2, (std::vector<int64_t>{10, 20}));
}

TEST(Database, ReadVectorFloatById) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e1;
    e1.set("label", std::string("Item 1")).set("value_float", std::vector<double>{1.5, 2.5, 3.5});
    int64_t id1 = db.create_element("Collection", e1);

    quiver::Element e2;
    e2.set("label", std::string("Item 2")).set("value_float", std::vector<double>{10.5, 20.5});
    int64_t id2 = db.create_element("Collection", e2);

    auto vec1 = db.read_vector_floats_by_id("Collection", "value_float", id1);
    auto vec2 = db.read_vector_floats_by_id("Collection", "value_float", id2);

    EXPECT_EQ(vec1, (std::vector<double>{1.5, 2.5, 3.5}));
    EXPECT_EQ(vec2, (std::vector<double>{10.5, 20.5}));
}

TEST(Database, ReadVectorByIdEmpty) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e;
    e.set("label", std::string("Item 1"));  // No vector data
    int64_t id = db.create_element("Collection", e);

    auto vec = db.read_vector_integers_by_id("Collection", "value_int", id);
    EXPECT_TRUE(vec.empty());
}

TEST(Database, ReadVectorIntegersInvalidCollection) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.read_vector_integers("NonexistentCollection", "value_int"), std::runtime_error);
}

TEST(Database, ReadVectorIntegersInvalidAttribute) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.read_vector_integers("Collection", "nonexistent_attribute"), std::runtime_error);
}

TEST(Database, ReadVectorIntegerByIdInvalidCollection) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.read_vector_integers_by_id("NonexistentCollection", "value_int", 1), std::runtime_error);
}

// ============================================================================
// Read vector strings tests (gap-fill using all_types.sql)
// ============================================================================

TEST(Database, ReadVectorStringsBulk) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("all_types.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e1;
    e1.set("label", std::string("Item 1"));
    int64_t id1 = db.create_element("AllTypes", e1);
    quiver::Element update1;
    update1.set("label_value", std::vector<std::string>{"alpha", "beta", "gamma"});
    db.update_element("AllTypes", id1, update1);

    quiver::Element e2;
    e2.set("label", std::string("Item 2"));
    int64_t id2 = db.create_element("AllTypes", e2);
    quiver::Element update2;
    update2.set("label_value", std::vector<std::string>{"delta", "epsilon"});
    db.update_element("AllTypes", id2, update2);

    auto vectors = db.read_vector_strings("AllTypes", "label_value");
    EXPECT_EQ(vectors.size(), 2);
    EXPECT_EQ(vectors[0], (std::vector<std::string>{"alpha", "beta", "gamma"}));
    EXPECT_EQ(vectors[1], (std::vector<std::string>{"delta", "epsilon"}));
}

TEST(Database, ReadVectorStringsByIdBasic) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("all_types.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e;
    e.set("label", std::string("Item 1"));
    int64_t id = db.create_element("AllTypes", e);
    quiver::Element update;
    update.set("label_value", std::vector<std::string>{"hello", "world"});
    db.update_element("AllTypes", id, update);

    auto vec = db.read_vector_strings_by_id("AllTypes", "label_value", id);
    EXPECT_EQ(vec, (std::vector<std::string>{"hello", "world"}));
}

TEST(Database, ReadVectorIntegersInvalidColumnThrows) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(
        {
            try {
                db.read_vector_integers("Collection", "nonexistent_column");
            } catch (const std::runtime_error& e) {
                EXPECT_THAT(std::string(e.what()), testing::HasSubstr("not found"));
                throw;
            }
        },
        std::runtime_error);
}
