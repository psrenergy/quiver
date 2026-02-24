#include "test_utils.h"

#include <algorithm>
#include <gtest/gtest.h>
#include <quiver/database.h>
#include <quiver/element.h>

TEST(Database, CreateElementWithScalars) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

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
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

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
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

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
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

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
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

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
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

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
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

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
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    quiver::Element element;
    element.set("label", std::string("Test"));

    EXPECT_THROW(db.create_element("NonexistentCollection", element), std::runtime_error);
}

TEST(Database, CreateElementLargeVector) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

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
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

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

TEST(Database, CreateElementWithTimeSeries) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    // Configuration required first
    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // Create element with time series arrays
    quiver::Element element;
    element.set("label", std::string("Item 1"))
        .set("date_time", std::vector<std::string>{"2024-01-01T10:00:00", "2024-01-02T10:00:00", "2024-01-03T10:00:00"})
        .set("value", std::vector<double>{1.5, 2.5, 3.5});

    int64_t id = db.create_element("Collection", element);
    EXPECT_EQ(id, 1);

    // Verify using read_time_series_group
    auto rows = db.read_time_series_group("Collection", "data", id);
    EXPECT_EQ(rows.size(), 3);
    EXPECT_EQ(std::get<std::string>(rows[0].at("date_time")), "2024-01-01T10:00:00");
    EXPECT_EQ(std::get<std::string>(rows[1].at("date_time")), "2024-01-02T10:00:00");
    EXPECT_EQ(std::get<std::string>(rows[2].at("date_time")), "2024-01-03T10:00:00");
    EXPECT_DOUBLE_EQ(std::get<double>(rows[0].at("value")), 1.5);
    EXPECT_DOUBLE_EQ(std::get<double>(rows[1].at("value")), 2.5);
    EXPECT_DOUBLE_EQ(std::get<double>(rows[2].at("value")), 3.5);
}

TEST(Database, CreateElementWithMultiTimeSeries) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("multi_time_series.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    // Configuration required first
    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // Create element with shared date_time routed to both time series tables
    quiver::Element element;
    element.set("label", std::string("Sensor 1"))
        .set("date_time", std::vector<std::string>{"2024-01-01T10:00:00", "2024-01-02T10:00:00", "2024-01-03T10:00:00"})
        .set("temperature", std::vector<double>{20.0, 21.5, 22.0})
        .set("humidity", std::vector<double>{45.0, 50.0, 55.0});

    int64_t id = db.create_element("Sensor", element);
    EXPECT_EQ(id, 1);

    // Verify temperature group
    auto temp_rows = db.read_time_series_group("Sensor", "temperature", id);
    EXPECT_EQ(temp_rows.size(), 3);
    EXPECT_EQ(std::get<std::string>(temp_rows[0].at("date_time")), "2024-01-01T10:00:00");
    EXPECT_EQ(std::get<std::string>(temp_rows[1].at("date_time")), "2024-01-02T10:00:00");
    EXPECT_EQ(std::get<std::string>(temp_rows[2].at("date_time")), "2024-01-03T10:00:00");
    EXPECT_DOUBLE_EQ(std::get<double>(temp_rows[0].at("temperature")), 20.0);
    EXPECT_DOUBLE_EQ(std::get<double>(temp_rows[1].at("temperature")), 21.5);
    EXPECT_DOUBLE_EQ(std::get<double>(temp_rows[2].at("temperature")), 22.0);

    // Verify humidity group
    auto hum_rows = db.read_time_series_group("Sensor", "humidity", id);
    EXPECT_EQ(hum_rows.size(), 3);
    EXPECT_EQ(std::get<std::string>(hum_rows[0].at("date_time")), "2024-01-01T10:00:00");
    EXPECT_EQ(std::get<std::string>(hum_rows[1].at("date_time")), "2024-01-02T10:00:00");
    EXPECT_EQ(std::get<std::string>(hum_rows[2].at("date_time")), "2024-01-03T10:00:00");
    EXPECT_DOUBLE_EQ(std::get<double>(hum_rows[0].at("humidity")), 45.0);
    EXPECT_DOUBLE_EQ(std::get<double>(hum_rows[1].at("humidity")), 50.0);
    EXPECT_DOUBLE_EQ(std::get<double>(hum_rows[2].at("humidity")), 55.0);
}

TEST(Database, CreateElementWithMultiTimeSeriesMismatchedLengths) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("multi_time_series.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    // Configuration required first
    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // date_time has 3 values, temperature has 3, humidity has 2 — mismatch in humidity table
    quiver::Element element;
    element.set("label", std::string("Sensor 1"))
        .set("date_time", std::vector<std::string>{"2024-01-01T10:00:00", "2024-01-02T10:00:00", "2024-01-03T10:00:00"})
        .set("temperature", std::vector<double>{20.0, 21.5, 22.0})
        .set("humidity", std::vector<double>{45.0, 50.0});

    EXPECT_THROW(db.create_element("Sensor", element), std::runtime_error);
}

TEST(Database, CreateElementTrimsWhitespaceFromStrings) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // Create element with whitespace-padded scalar, vector strings, and set strings
    quiver::Element element;
    element.set("label", std::string("  Item 1  "))
        .set("tag", std::vector<std::string>{"  important  ", "\turgent\n", " review "});

    int64_t id = db.create_element("Collection", element);
    EXPECT_EQ(id, 1);

    // Scalar string should be trimmed
    auto labels = db.read_scalar_strings("Collection", "label");
    EXPECT_EQ(labels.size(), 1);
    EXPECT_EQ(labels[0], "Item 1");

    // Set strings should be trimmed
    auto sets = db.read_set_strings("Collection", "tag");
    EXPECT_EQ(sets.size(), 1);
    auto tags = sets[0];
    std::sort(tags.begin(), tags.end());
    EXPECT_EQ(tags, (std::vector<std::string>{"important", "review", "urgent"}));
}

TEST(Database, CreateElementWithDatetime) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    quiver::Element element;
    element.set("label", std::string("Config 1")).set("date_attribute", std::string("2024-03-15T14:30:45"));

    int64_t id = db.create_element("Configuration", element);
    EXPECT_EQ(id, 1);

    auto dates = db.read_scalar_strings("Configuration", "date_attribute");
    EXPECT_EQ(dates.size(), 1);
    EXPECT_EQ(dates[0], "2024-03-15T14:30:45");
}
