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

// ============================================================================
// FK label resolution (resolve_fk_label helper)
// ============================================================================

TEST(Database, ResolveFkLabelInSetCreate) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("relations.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    // Create 2 parents
    quiver::Element parent1;
    parent1.set("label", std::string("Parent 1"));
    db.create_element("Parent", parent1);

    quiver::Element parent2;
    parent2.set("label", std::string("Parent 2"));
    db.create_element("Parent", parent2);

    // Create child with set FK using string labels (mentor_id is unique to set table)
    quiver::Element child;
    child.set("label", std::string("Child 1"));
    child.set("mentor_id", std::vector<std::string>{"Parent 1", "Parent 2"});
    db.create_element("Child", child);

    // Read back resolved integer IDs
    auto sets = db.read_set_integers("Child", "mentor_id");
    ASSERT_EQ(sets.size(), 1);
    ASSERT_EQ(sets[0].size(), 2);

    // Resolved parent IDs should be 1 and 2
    std::vector<int64_t> sorted_ids(sets[0].begin(), sets[0].end());
    std::sort(sorted_ids.begin(), sorted_ids.end());
    EXPECT_EQ(sorted_ids[0], 1);
    EXPECT_EQ(sorted_ids[1], 2);
}

TEST(Database, ResolveFkLabelMissingTarget) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("relations.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    // Create child with set FK referencing nonexistent parent (mentor_id is unique to set table)
    quiver::Element child;
    child.set("label", std::string("Child 1"));
    child.set("mentor_id", std::vector<std::string>{"Nonexistent Parent"});

    try {
        db.create_element("Child", child);
        FAIL() << "Expected std::runtime_error";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "Failed to resolve label 'Nonexistent Parent' to ID in table 'Parent'");
    }
}

TEST(Database, RejectStringForNonFkIntegerColumn) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("relations.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    // Create child, then try to create with string in non-FK INTEGER set column
    quiver::Element child;
    child.set("label", std::string("Child 1"));
    child.set("score", std::vector<std::string>{"not_a_label"});

    EXPECT_THROW(db.create_element("Child", child), std::runtime_error);
}

// ============================================================================
// FK label resolution in create_element (all column types)
// ============================================================================

TEST(Database, CreateElementScalarFkLabel) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("relations.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    // Create parent
    quiver::Element parent;
    parent.set("label", std::string("Parent 1"));
    db.create_element("Parent", parent);

    // Create child with scalar FK using string label
    quiver::Element child;
    child.set("label", std::string("Child 1"));
    child.set("parent_id", std::string("Parent 1"));  // string label, not int ID
    db.create_element("Child", child);

    // Verify: read back as integer, should be resolved ID
    auto parent_ids = db.read_scalar_integers("Child", "parent_id");
    ASSERT_EQ(parent_ids.size(), 1);
    EXPECT_EQ(parent_ids[0], 1);  // Parent 1's auto-increment ID
}

TEST(Database, CreateElementScalarFkInteger) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("relations.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    // Create parent
    quiver::Element parent;
    parent.set("label", std::string("Parent 1"));
    db.create_element("Parent", parent);

    // Create child with scalar FK using integer ID directly (bypasses label resolution)
    quiver::Element child;
    child.set("label", std::string("Child 1"));
    child.set("parent_id", int64_t{1});
    db.create_element("Child", child);

    // Verify: read back integer, should be stored as-is
    auto parent_ids = db.read_scalar_integers("Child", "parent_id");
    ASSERT_EQ(parent_ids.size(), 1);
    EXPECT_EQ(parent_ids[0], 1);
}

TEST(Database, CreateElementVectorFkLabels) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("relations.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    // Create parents
    quiver::Element p1, p2;
    p1.set("label", std::string("Parent 1"));
    p2.set("label", std::string("Parent 2"));
    db.create_element("Parent", p1);
    db.create_element("Parent", p2);

    // Create child with vector FK using string labels
    // parent_ref routes to Child_vector_refs (and Child_set_parents, but
    // the vector path will use the pre-resolved integer values)
    quiver::Element child;
    child.set("label", std::string("Child 1"));
    child.set("parent_ref", std::vector<std::string>{"Parent 1", "Parent 2"});
    db.create_element("Child", child);

    // Verify: read back vector integers
    auto refs = db.read_vector_integers_by_id("Child", "parent_ref", 1);
    ASSERT_EQ(refs.size(), 2);
    EXPECT_EQ(refs[0], 1);
    EXPECT_EQ(refs[1], 2);
}

TEST(Database, CreateElementTimeSeriesFkLabels) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("relations.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    // Create parents
    quiver::Element p1, p2;
    p1.set("label", std::string("Parent 1"));
    p2.set("label", std::string("Parent 2"));
    db.create_element("Parent", p1);
    db.create_element("Parent", p2);

    // Create child with time series FK using string labels
    // sponsor_id is unique to Child_time_series_events
    quiver::Element child;
    child.set("label", std::string("Child 1"));
    child.set("date_time", std::vector<std::string>{"2024-01-01", "2024-01-02"});
    child.set("sponsor_id", std::vector<std::string>{"Parent 1", "Parent 2"});
    db.create_element("Child", child);

    // Verify: read time series group and check sponsor_id values
    auto ts_data = db.read_time_series_group("Child", "events", 1);
    ASSERT_EQ(ts_data.size(), 2);

    // Each row is a map with "date_time" and "sponsor_id" keys
    // sponsor_id should be resolved to integer IDs
    EXPECT_EQ(std::get<int64_t>(ts_data[0].at("sponsor_id")), 1);
    EXPECT_EQ(std::get<int64_t>(ts_data[1].at("sponsor_id")), 2);
}

TEST(Database, CreateElementAllFkTypesInOneCall) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("relations.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    // Create parents
    quiver::Element p1, p2;
    p1.set("label", std::string("Parent 1"));
    p2.set("label", std::string("Parent 2"));
    db.create_element("Parent", p1);
    db.create_element("Parent", p2);

    // Create child with ALL FK types in one call:
    // - scalar FK: parent_id -> Parent 1
    // - set FK: mentor_id -> Parent 2 (unique to Child_set_mentors)
    // - vector+set FK: parent_ref -> Parent 1 (routes to vector AND set tables)
    // - time series FK: sponsor_id -> Parent 2 (unique to Child_time_series_events)
    quiver::Element child;
    child.set("label", std::string("Child 1"));
    child.set("parent_id", std::string("Parent 1"));                 // scalar FK
    child.set("mentor_id", std::vector<std::string>{"Parent 2"});    // set FK
    child.set("parent_ref", std::vector<std::string>{"Parent 1"});   // vector+set FK
    child.set("date_time", std::vector<std::string>{"2024-01-01"});  // time series dimension
    child.set("sponsor_id", std::vector<std::string>{"Parent 2"});   // time series FK
    db.create_element("Child", child);

    // Verify scalar FK
    auto parent_ids = db.read_scalar_integers("Child", "parent_id");
    ASSERT_EQ(parent_ids.size(), 1);
    EXPECT_EQ(parent_ids[0], 1);

    // Verify set FK (mentor_id)
    auto mentors = db.read_set_integers("Child", "mentor_id");
    ASSERT_EQ(mentors.size(), 1);
    ASSERT_EQ(mentors[0].size(), 1);
    EXPECT_EQ(mentors[0][0], 2);

    // Verify vector FK (parent_ref in Child_vector_refs)
    auto vrefs = db.read_vector_integers_by_id("Child", "parent_ref", 1);
    ASSERT_EQ(vrefs.size(), 1);
    EXPECT_EQ(vrefs[0], 1);

    // Verify time series FK (sponsor_id in Child_time_series_events)
    auto ts_data = db.read_time_series_group("Child", "events", 1);
    ASSERT_EQ(ts_data.size(), 1);
    EXPECT_EQ(std::get<int64_t>(ts_data[0].at("sponsor_id")), 2);
}

TEST(Database, CreateElementNoFkColumnsUnchanged) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    // basic.sql has no FK columns -- this tests that the pre-resolve pass
    // passes all values through unchanged for schemas with no FKs
    quiver::Element element;
    element.set("label", std::string("Config 1"));
    element.set("integer_attribute", int64_t{42});
    element.set("float_attribute", 3.14);

    int64_t id = db.create_element("Configuration", element);
    EXPECT_EQ(id, 1);

    auto labels = db.read_scalar_strings("Configuration", "label");
    EXPECT_EQ(labels[0], "Config 1");
    auto integers = db.read_scalar_integers("Configuration", "integer_attribute");
    EXPECT_EQ(integers[0], 42);
    auto floats = db.read_scalar_floats("Configuration", "float_attribute");
    EXPECT_DOUBLE_EQ(floats[0], 3.14);
}

TEST(Database, ScalarFkResolutionFailureCausesNoPartialWrites) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("relations.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    // Create child with scalar FK referencing nonexistent parent
    quiver::Element child;
    child.set("label", std::string("Orphan Child"));
    child.set("parent_id", std::string("Nonexistent Parent"));

    EXPECT_THROW(db.create_element("Child", child), std::runtime_error);

    // Verify: no child was created (zero partial writes)
    auto labels = db.read_scalar_strings("Child", "label");
    EXPECT_EQ(labels.size(), 0);
}
