#include "test_utils.h"

#include <algorithm>
#include <gtest/gtest.h>
#include <quiver/database.h>
#include <quiver/element.h>

TEST(Database, CreateElementWithScalars) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = QUIVER_LOG_OFF});

    // Create element
    quiver::Element element;
    element.set("label", std::string("Config 1")).set("integer_attribute", int64_t{42}).set("float_attribute", 3.14);

    int64_t id = db.create_element("Configuration", element);
    EXPECT_EQ(id, 1);

    // Verify using public read APIs
    auto labels = db.read_scalar_strings("Configuration", "label");
    auto integers = db.read_scalar_integers("Configuration", "integer_attribute");
    auto floats = db.read_scalar_floats("Configuration", "float_attribute");

    EXPECT_EQ(labels.size(), 1);
    EXPECT_EQ(labels[0], "Config 1");
    EXPECT_EQ(integers[0], 42);
    EXPECT_EQ(floats[0], 3.14);
}

TEST(Database, CreateElementWithVector) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = QUIVER_LOG_OFF});

    // Configuration required first
    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // Create element with vector array
    quiver::Element element;
    element.set("label", std::string("Item 1"))
        .set("value_int", std::vector<int64_t>{1, 2, 3})
        .set("value_float", std::vector<double>{1.5, 2.5, 3.5});

    int64_t id = db.create_element("Collection", element);
    EXPECT_EQ(id, 1);

    // Verify using public read APIs
    auto labels = db.read_scalar_strings("Collection", "label");
    EXPECT_EQ(labels.size(), 1);
    EXPECT_EQ(labels[0], "Item 1");

    auto integer_vectors = db.read_vector_integers("Collection", "value_int");
    EXPECT_EQ(integer_vectors.size(), 1);
    EXPECT_EQ(integer_vectors[0], (std::vector<int64_t>{1, 2, 3}));

    auto float_vectors = db.read_vector_floats("Collection", "value_float");
    EXPECT_EQ(float_vectors.size(), 1);
    EXPECT_EQ(float_vectors[0], (std::vector<double>{1.5, 2.5, 3.5}));
}

TEST(Database, CreateElementWithVectorGroup) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = QUIVER_LOG_OFF});

    // Configuration required first
    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // Create element with vector group containing multiple attributes per row
    quiver::Element element;
    element.set("label", std::string("Item 1"))
        .set("value_int", std::vector<int64_t>{10, 20, 30})
        .set("value_float", std::vector<double>{1.5, 2.5, 3.5});

    int64_t id = db.create_element("Collection", element);
    EXPECT_EQ(id, 1);

    // Verify using public read APIs
    auto integer_vectors = db.read_vector_integers("Collection", "value_int");
    EXPECT_EQ(integer_vectors.size(), 1);
    EXPECT_EQ(integer_vectors[0], (std::vector<int64_t>{10, 20, 30}));

    auto float_vectors = db.read_vector_floats("Collection", "value_float");
    EXPECT_EQ(float_vectors.size(), 1);
    EXPECT_EQ(float_vectors[0], (std::vector<double>{1.5, 2.5, 3.5}));
}

TEST(Database, CreateElementWithSetGroup) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = QUIVER_LOG_OFF});

    // Configuration required first
    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // Create element with set attribute
    quiver::Element element;
    element.set("label", std::string("Item 1")).set("tag", std::vector<std::string>{"important", "urgent", "review"});

    int64_t id = db.create_element("Collection", element);
    EXPECT_EQ(id, 1);

    // Verify using public read APIs
    auto sets = db.read_set_strings("Collection", "tag");
    EXPECT_EQ(sets.size(), 1);
    auto tags = sets[0];
    std::sort(tags.begin(), tags.end());
    EXPECT_EQ(tags, (std::vector<std::string>{"important", "review", "urgent"}));
}

TEST(Database, CreateMultipleElements) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = QUIVER_LOG_OFF});

    // Create multiple Configuration elements
    quiver::Element e1;
    e1.set("label", std::string("Config A")).set("integer_attribute", int64_t{100});
    int64_t id1 = db.create_element("Configuration", e1);

    quiver::Element e2;
    e2.set("label", std::string("Config B")).set("integer_attribute", int64_t{200});
    int64_t id2 = db.create_element("Configuration", e2);

    EXPECT_EQ(id1, 1);
    EXPECT_EQ(id2, 2);

    // Verify using public read APIs
    auto labels = db.read_scalar_strings("Configuration", "label");
    EXPECT_EQ(labels.size(), 2);
}

// ============================================================================
// Vector/Set edge case tests
// ============================================================================

TEST(Database, CreateElementSingleElementVector) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = QUIVER_LOG_OFF});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element element;
    element.set("label", std::string("Item 1")).set("value_int", std::vector<int64_t>{42});

    int64_t id = db.create_element("Collection", element);
    EXPECT_EQ(id, 1);

    auto vectors = db.read_vector_integers("Collection", "value_int");
    EXPECT_EQ(vectors.size(), 1);
    EXPECT_EQ(vectors[0], (std::vector<int64_t>{42}));
}

TEST(Database, CreateElementSingleElementSet) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = QUIVER_LOG_OFF});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element element;
    element.set("label", std::string("Item 1")).set("tag", std::vector<std::string>{"single_tag"});

    int64_t id = db.create_element("Collection", element);
    EXPECT_EQ(id, 1);

    auto sets = db.read_set_strings("Collection", "tag");
    EXPECT_EQ(sets.size(), 1);
    EXPECT_EQ(sets[0], (std::vector<std::string>{"single_tag"}));
}

TEST(Database, CreateElementInvalidCollection) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = QUIVER_LOG_OFF});

    quiver::Element element;
    element.set("label", std::string("Test"));

    EXPECT_THROW(db.create_element("NonexistentCollection", element), std::runtime_error);
}

TEST(Database, CreateElementLargeVector) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = QUIVER_LOG_OFF});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // Create a vector with many elements
    std::vector<int64_t> large_vec;
    for (int i = 0; i < 100; i++) {
        large_vec.push_back(i);
    }

    quiver::Element element;
    element.set("label", std::string("Item 1")).set("value_int", large_vec);

    int64_t id = db.create_element("Collection", element);
    EXPECT_EQ(id, 1);

    auto vectors = db.read_vector_integers("Collection", "value_int");
    EXPECT_EQ(vectors.size(), 1);
    EXPECT_EQ(vectors[0].size(), 100);
    EXPECT_EQ(vectors[0][0], 0);
    EXPECT_EQ(vectors[0][99], 99);
}

TEST(Database, CreateElementWithNoOptionalAttributes) {
    auto db =
        quiver::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = QUIVER_LOG_OFF});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // Create element with only required label
    quiver::Element element;
    element.set("label", std::string("Item 1"));

    int64_t id = db.create_element("Collection", element);
    EXPECT_EQ(id, 1);

    // Verify vector attributes are empty
    auto vectors = db.read_vector_integers("Collection", "value_int");
    EXPECT_TRUE(vectors.empty());

    auto sets = db.read_set_strings("Collection", "tag");
    EXPECT_TRUE(sets.empty());
}

TEST(Database, CreateElementWithDatetime) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = QUIVER_LOG_OFF});

    quiver::Element element;
    element.set("label", std::string("Config 1")).set("date_attribute", std::string("2024-03-15T14:30:45"));

    int64_t id = db.create_element("Configuration", element);
    EXPECT_EQ(id, 1);

    auto dates = db.read_scalar_strings("Configuration", "date_attribute");
    EXPECT_EQ(dates.size(), 1);
    EXPECT_EQ(dates[0], "2024-03-15T14:30:45");
}
