#include "test_utils.h"

#include <gtest/gtest.h>
#include <quiver/database.h>
#include <quiver/element.h>

// ============================================================================
// Time series read tests
// ============================================================================

TEST(Database, ReadTimeSeriesGroupById) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e1;
    e1.set("label", std::string("Item 1"));
    auto id = db.create_element("Collection", e1);

    // Insert time series data
    std::vector<std::map<std::string, quiver::Value>> rows = {
        {{"date_time", std::string("2024-01-01T10:00:00")}, {"value", 1.5}},
        {{"date_time", std::string("2024-01-01T11:00:00")}, {"value", 2.5}},
        {{"date_time", std::string("2024-01-01T12:00:00")}, {"value", 3.5}}};
    db.update_time_series_group("Collection", "data", id, rows);

    // Read back
    auto result = db.read_time_series_group("Collection", "data", id);
    ASSERT_EQ(result.size(), 3);

    // Check ordering by date_time
    EXPECT_EQ(std::get<std::string>(result[0]["date_time"]), "2024-01-01T10:00:00");
    EXPECT_DOUBLE_EQ(std::get<double>(result[0]["value"]), 1.5);

    EXPECT_EQ(std::get<std::string>(result[1]["date_time"]), "2024-01-01T11:00:00");
    EXPECT_DOUBLE_EQ(std::get<double>(result[1]["value"]), 2.5);

    EXPECT_EQ(std::get<std::string>(result[2]["date_time"]), "2024-01-01T12:00:00");
    EXPECT_DOUBLE_EQ(std::get<double>(result[2]["value"]), 3.5);
}

TEST(Database, ReadTimeSeriesGroupByIdEmpty) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e1;
    e1.set("label", std::string("Item 1"));
    auto id = db.create_element("Collection", e1);

    // No time series data inserted
    auto result = db.read_time_series_group("Collection", "data", id);
    EXPECT_TRUE(result.empty());
}

TEST(Database, ReadTimeSeriesGroupByIdNonexistent) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // Read from nonexistent element
    auto result = db.read_time_series_group("Collection", "data", 999);
    EXPECT_TRUE(result.empty());
}

// ============================================================================
// Time series update tests
// ============================================================================

TEST(Database, UpdateTimeSeriesGroup) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e1;
    e1.set("label", std::string("Item 1"));
    auto id = db.create_element("Collection", e1);

    // Insert initial data
    std::vector<std::map<std::string, quiver::Value>> rows1 = {
        {{"date_time", std::string("2024-01-01T10:00:00")}, {"value", 1.0}}};
    db.update_time_series_group("Collection", "data", id, rows1);

    auto result1 = db.read_time_series_group("Collection", "data", id);
    ASSERT_EQ(result1.size(), 1);

    // Replace with new data
    std::vector<std::map<std::string, quiver::Value>> rows2 = {
        {{"date_time", std::string("2024-02-01T10:00:00")}, {"value", 10.0}},
        {{"date_time", std::string("2024-02-01T11:00:00")}, {"value", 20.0}}};
    db.update_time_series_group("Collection", "data", id, rows2);

    auto result2 = db.read_time_series_group("Collection", "data", id);
    ASSERT_EQ(result2.size(), 2);
    EXPECT_EQ(std::get<std::string>(result2[0]["date_time"]), "2024-02-01T10:00:00");
    EXPECT_DOUBLE_EQ(std::get<double>(result2[0]["value"]), 10.0);
}

TEST(Database, UpdateTimeSeriesGroupEmpty) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e1;
    e1.set("label", std::string("Item 1"));
    auto id = db.create_element("Collection", e1);

    // Insert some data first
    std::vector<std::map<std::string, quiver::Value>> rows = {
        {{"date_time", std::string("2024-01-01T10:00:00")}, {"value", 1.0}}};
    db.update_time_series_group("Collection", "data", id, rows);

    // Clear by updating with empty
    std::vector<std::map<std::string, quiver::Value>> empty_rows;
    db.update_time_series_group("Collection", "data", id, empty_rows);

    auto result = db.read_time_series_group("Collection", "data", id);
    EXPECT_TRUE(result.empty());
}

TEST(Database, TimeSeriesOrdering) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e1;
    e1.set("label", std::string("Item 1"));
    auto id = db.create_element("Collection", e1);

    // Insert data out of order
    std::vector<std::map<std::string, quiver::Value>> rows = {
        {{"date_time", std::string("2024-01-03T10:00:00")}, {"value", 3.0}},
        {{"date_time", std::string("2024-01-01T10:00:00")}, {"value", 1.0}},
        {{"date_time", std::string("2024-01-02T10:00:00")}, {"value", 2.0}}};
    db.update_time_series_group("Collection", "data", id, rows);

    // Should be returned ordered by date_time
    auto result = db.read_time_series_group("Collection", "data", id);
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(std::get<std::string>(result[0]["date_time"]), "2024-01-01T10:00:00");
    EXPECT_EQ(std::get<std::string>(result[1]["date_time"]), "2024-01-02T10:00:00");
    EXPECT_EQ(std::get<std::string>(result[2]["date_time"]), "2024-01-03T10:00:00");
}

// ============================================================================
// Time series error handling tests
// ============================================================================

TEST(Database, TimeSeriesGroupNotFound) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    EXPECT_THROW(db.get_time_series_metadata("Collection", "nonexistent"), std::runtime_error);

    EXPECT_THROW(db.read_time_series_group("Collection", "nonexistent", 1), std::runtime_error);
}

TEST(Database, TimeSeriesCollectionNotFound) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    // Nonexistent collection returns empty list (matches list_vector_groups behavior)
    auto groups = db.list_time_series_groups("NonexistentCollection");
    EXPECT_TRUE(groups.empty());
}

static std::string capture_update_error(quiver::Database& db,
                                        const std::string& collection,
                                        const std::string& group,
                                        int64_t id,
                                        const std::vector<std::map<std::string, quiver::Value>>& rows) {
    try {
        db.update_time_series_group(collection, group, id, rows);
    } catch (const std::runtime_error& e) {
        return e.what();
    }
    return {};
}

TEST(Database, TimeSeriesMissingDateTime) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e1;
    e1.set("label", std::string("Item 1"));
    auto id = db.create_element("Collection", e1);

    std::vector<std::map<std::string, quiver::Value>> rows = {{{"value", 1.0}}};
    auto msg = capture_update_error(db, "Collection", "data", id, rows);
    EXPECT_NE(msg.find("row missing required 'date_time' column"), std::string::npos) << "Actual: " << msg;
}

TEST(Database, TimeSeriesUnknownColumn) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e1;
    e1.set("label", std::string("Item 1"));
    auto id = db.create_element("Collection", e1);

    std::vector<std::map<std::string, quiver::Value>> rows = {
        {{"date_time", std::string("2024-01-01T10:00:00")}, {"value", 1.0}, {"pressure", 1013.25}}};
    auto msg = capture_update_error(db, "Collection", "data", id, rows);
    EXPECT_NE(msg.find("column 'pressure' not found in group 'data'"), std::string::npos) << "Actual: " << msg;
}

TEST(Database, TimeSeriesIntegerAcceptedForRealColumn) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e1;
    e1.set("label", std::string("Item 1"));
    auto id = db.create_element("Collection", e1);

    // Integer values are accepted for REAL columns and converted on insert
    std::vector<std::map<std::string, quiver::Value>> rows = {
        {{"date_time", std::string("2024-01-01T10:00:00")}, {"value", int64_t{1}}}};
    db.update_time_series_group("Collection", "data", id, rows);

    auto result = db.read_time_series_group("Collection", "data", id);
    ASSERT_EQ(result.size(), 1);
    EXPECT_DOUBLE_EQ(std::get<double>(result[0].at("value")), 1.0);
}

TEST(Database, TimeSeriesTypeMismatchRealToInteger) {
    auto db = quiver::Database::from_schema(":memory:",
                                            VALID_SCHEMA("mixed_time_series.sql"),
                                            {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element sensor;
    sensor.set("label", std::string("Sensor 1"));
    auto id = db.create_element("Sensor", sensor);

    std::vector<std::map<std::string, quiver::Value>> rows = {{{"date_time", std::string("2024-01-01T10:00:00")},
                                                               {"temperature", 20.5},
                                                               {"humidity", 55.5},  // INTEGER column, sending REAL
                                                               {"status", std::string("ok")}}};
    auto msg = capture_update_error(db, "Sensor", "readings", id, rows);
    EXPECT_NE(msg.find("column 'humidity' has type INTEGER but received REAL"), std::string::npos) << "Actual: " << msg;
}

TEST(Database, TimeSeriesTypeMismatchStringToReal) {
    auto db = quiver::Database::from_schema(":memory:",
                                            VALID_SCHEMA("mixed_time_series.sql"),
                                            {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element sensor;
    sensor.set("label", std::string("Sensor 1"));
    auto id = db.create_element("Sensor", sensor);

    std::vector<std::map<std::string, quiver::Value>> rows = {{{"date_time", std::string("2024-01-01T10:00:00")},
                                                               {"temperature", std::string("hot")},  // REAL, sent TEXT
                                                               {"humidity", int64_t{55}},
                                                               {"status", std::string("ok")}}};
    auto msg = capture_update_error(db, "Sensor", "readings", id, rows);
    EXPECT_NE(msg.find("column 'temperature' has type REAL but received TEXT"), std::string::npos) << "Actual: " << msg;
}

TEST(Database, TimeSeriesNullValue) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e1;
    e1.set("label", std::string("Item 1"));
    auto id = db.create_element("Collection", e1);

    // NULL values must be accepted for any column regardless of declared type.
    std::vector<std::map<std::string, quiver::Value>> rows = {
        {{"date_time", std::string("2024-01-01T10:00:00")}, {"value", nullptr}}};
    db.update_time_series_group("Collection", "data", id, rows);

    auto result = db.read_time_series_group("Collection", "data", id);
    ASSERT_EQ(result.size(), 1);
    EXPECT_TRUE(std::holds_alternative<std::nullptr_t>(result[0]["value"]));
}

// ============================================================================
// update_time_series_group multi-dim PK validation
// ============================================================================

TEST(Database, UpdateTimeSeriesGroupMissingMultiDimColumn) {
    auto db = quiver::Database::from_schema(":memory:",
                                            VALID_SCHEMA("multi_dim_time_series.sql"),
                                            {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element resource;
    resource.set("label", std::string("Resource 1"));
    auto id = db.create_element("Resource", resource);

    std::vector<std::map<std::string, quiver::Value>> rows = {
        {{"date_time", std::string("2024-01-01")}, {"load", 10.0}}};
    auto msg = capture_update_error(db, "Resource", "load", id, rows);
    EXPECT_NE(msg.find("Cannot update_time_series_group: row missing required 'block' column"), std::string::npos)
        << "Actual: " << msg;
}

TEST(Database, UpdateTimeSeriesGroupMissingDateTimeOnMultiDim) {
    auto db = quiver::Database::from_schema(":memory:",
                                            VALID_SCHEMA("multi_dim_time_series.sql"),
                                            {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element resource;
    resource.set("label", std::string("Resource 1"));
    auto id = db.create_element("Resource", resource);

    std::vector<std::map<std::string, quiver::Value>> rows = {{{"block", int64_t{1}}, {"load", 10.0}}};
    auto msg = capture_update_error(db, "Resource", "load", id, rows);
    EXPECT_NE(msg.find("Cannot update_time_series_group: row missing required 'date_time' column"), std::string::npos)
        << "Actual: " << msg;
}

TEST(Database, UpdateTimeSeriesGroupMultiDimHappyPath) {
    auto db = quiver::Database::from_schema(":memory:",
                                            VALID_SCHEMA("multi_dim_time_series.sql"),
                                            {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element resource;
    resource.set("label", std::string("Resource 1"));
    auto id = db.create_element("Resource", resource);

    std::vector<std::map<std::string, quiver::Value>> rows = {
        {{"date_time", std::string("2024-01-01")}, {"block", int64_t{1}}, {"load", 10.0}, {"flag", int64_t{1}}},
        {{"date_time", std::string("2024-01-01")}, {"block", int64_t{2}}, {"load", 20.0}, {"flag", int64_t{1}}}};
    db.update_time_series_group("Resource", "load", id, rows);

    auto result = db.read_time_series_group("Resource", "load", id);
    EXPECT_EQ(result.size(), 2);
}

TEST(Database, UpdateTimeSeriesGroupMissingBlockInLaterRow) {
    auto db = quiver::Database::from_schema(":memory:",
                                            VALID_SCHEMA("multi_dim_time_series.sql"),
                                            {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element resource;
    resource.set("label", std::string("Resource 1"));
    auto id = db.create_element("Resource", resource);

    // First row complete, second row missing block. Validation must reject the
    // whole batch before any DELETE runs — no partial writes.
    std::vector<std::map<std::string, quiver::Value>> rows = {
        {{"date_time", std::string("2024-01-01")}, {"block", int64_t{1}}, {"load", 10.0}},
        {{"date_time", std::string("2024-01-02")}, {"load", 20.0}}};
    auto msg = capture_update_error(db, "Resource", "load", id, rows);
    EXPECT_NE(msg.find("Cannot update_time_series_group: row missing required 'block' column"), std::string::npos)
        << "Actual: " << msg;

    auto result = db.read_time_series_group("Resource", "load", id);
    EXPECT_TRUE(result.empty());
}

