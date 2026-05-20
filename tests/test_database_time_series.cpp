#include "test_utils.h"

#include <gtest/gtest.h>
#include <quiver/database.h>
#include <quiver/element.h>

// ============================================================================
// Time series metadata tests
// ============================================================================

TEST(Database, GetTimeSeriesMetadata) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    auto metadata = db.get_time_series_metadata("Collection", "data");
    EXPECT_EQ(metadata.group_name, "data");
    EXPECT_EQ(metadata.dimension_column, "date_time");
    EXPECT_EQ(metadata.value_columns.size(), 1);
    EXPECT_EQ(metadata.value_columns[0].name, "value");
    EXPECT_EQ(metadata.value_columns[0].data_type, quiver::DataType::Real);
}

TEST(Database, ListTimeSeriesGroups) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    auto groups = db.list_time_series_groups("Collection");
    EXPECT_EQ(groups.size(), 1);
    EXPECT_EQ(groups[0].group_name, "data");
    EXPECT_EQ(groups[0].dimension_column, "date_time");
    EXPECT_EQ(groups[0].value_columns.size(), 1);
    EXPECT_EQ(groups[0].value_columns[0].name, "value");
}

TEST(Database, ListTimeSeriesGroupsEmpty) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    // Configuration has no time series tables
    auto groups = db.list_time_series_groups("Configuration");
    EXPECT_TRUE(groups.empty());
}

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
// Time series row read tests (read_time_series_row)
// ============================================================================

TEST(Database, ReadTimeSeriesRow) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // Create two elements
    quiver::Element e1;
    e1.set("label", std::string("Item 1"));
    auto id1 = db.create_element("Collection", e1);

    quiver::Element e2;
    e2.set("label", std::string("Item 2"));
    auto id2 = db.create_element("Collection", e2);

    // Insert time series data for both elements
    db.update_time_series_group("Collection",
                                "data",
                                id1,
                                {{{"date_time", std::string("2024-01-01")}, {"value", 1.0}},
                                 {{"date_time", std::string("2024-01-02")}, {"value", 2.0}},
                                 {{"date_time", std::string("2024-01-03")}, {"value", 3.0}}});

    db.update_time_series_group("Collection",
                                "data",
                                id2,
                                {{{"date_time", std::string("2024-01-01")}, {"value", 10.0}},
                                 {{"date_time", std::string("2024-01-02")}, {"value", 20.0}}});

    // Query at 2024-01-02: Item 1 -> 2.0, Item 2 -> 20.0
    auto result = db.read_time_series_row("Collection", "data", "value", "2024-01-02");
    ASSERT_EQ(result.size(), 2);
    EXPECT_DOUBLE_EQ(std::get<double>(result[0]), 2.0);
    EXPECT_DOUBLE_EQ(std::get<double>(result[1]), 20.0);

    // Query at 2024-01-03: Item 1 -> 3.0, Item 2 -> 20.0 (last value at or before)
    auto result2 = db.read_time_series_row("Collection", "data", "value", "2024-01-03");
    ASSERT_EQ(result2.size(), 2);
    EXPECT_DOUBLE_EQ(std::get<double>(result2[0]), 3.0);
    EXPECT_DOUBLE_EQ(std::get<double>(result2[1]), 20.0);
}

TEST(Database, ReadTimeSeriesRowWithMissingElements) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // Create two elements
    quiver::Element e1;
    e1.set("label", std::string("Item 1"));
    auto id1 = db.create_element("Collection", e1);

    quiver::Element e2;
    e2.set("label", std::string("Item 2"));
    auto id2 = db.create_element("Collection", e2);

    // Insert time series data for both elements
    db.update_time_series_group("Collection",
                                "data",
                                id1,
                                {{{"date_time", std::string("2024-01-01")}, {"value", 1.0}},
                                 {{"date_time", std::string("2024-01-02")}, {"value", 2.0}},
                                 {{"date_time", std::string("2024-01-03")}, {"value", 3.0}}});

    db.update_time_series_group("Collection",
                                "data",
                                id2,
                                {{{"date_time", std::string("2024-01-01")}, {"value", 10.0}},
                                 {{"date_time", std::string("2024-01-04")}, {"value", 20.0}}});

    // Query at 2024-01-02: Item 1 -> 2.0, Item 2 -> 10.0
    auto result = db.read_time_series_row("Collection", "data", "value", "2024-01-02");
    ASSERT_EQ(result.size(), 2);
    EXPECT_DOUBLE_EQ(std::get<double>(result[0]), 2.0);
    EXPECT_DOUBLE_EQ(std::get<double>(result[1]), 10.0);

    // Query at 2024-01-03: Item 1 -> 3.0, Item 2 -> 10.0 (last value at or before)
    auto result2 = db.read_time_series_row("Collection", "data", "value", "2024-01-03");
    ASSERT_EQ(result2.size(), 2);
    EXPECT_DOUBLE_EQ(std::get<double>(result2[0]), 3.0);
    EXPECT_DOUBLE_EQ(std::get<double>(result2[1]), 10.0);
}

TEST(Database, ReadTimeSeriesRowBeforeAllData) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e1;
    e1.set("label", std::string("Item 1"));
    auto id1 = db.create_element("Collection", e1);

    db.update_time_series_group(
        "Collection", "data", id1, {{{"date_time", std::string("2024-01-02")}, {"value", 1.0}}});

    // Query before any data exists: should return nullptr for the element
    auto result = db.read_time_series_row("Collection", "data", "value", "2024-01-01");
    ASSERT_EQ(result.size(), 1);
    EXPECT_TRUE(std::holds_alternative<std::nullptr_t>(result[0]));
}

TEST(Database, ReadTimeSeriesRowEmptyCollection) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // No elements in Collection -> empty result
    auto result = db.read_time_series_row("Collection", "data", "value", "2024-01-01");
    EXPECT_TRUE(result.empty());
}

TEST(Database, ReadTimeSeriesRowMixedElements) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e1;
    e1.set("label", std::string("Item 1"));
    auto id1 = db.create_element("Collection", e1);

    quiver::Element e2;
    e2.set("label", std::string("Item 2"));
    db.create_element("Collection", e2);  // no time series data

    db.update_time_series_group(
        "Collection", "data", id1, {{{"date_time", std::string("2024-01-01")}, {"value", 5.0}}});

    // Item 1 has data, Item 2 doesn't
    auto result = db.read_time_series_row("Collection", "data", "value", "2024-01-01");
    ASSERT_EQ(result.size(), 2);
    EXPECT_DOUBLE_EQ(std::get<double>(result[0]), 5.0);
    EXPECT_TRUE(std::holds_alternative<std::nullptr_t>(result[1]));
}

TEST(Database, ReadTimeSeriesRowAttributeNotFound) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    EXPECT_THROW(db.read_time_series_row("Collection", "data", "nonexistent", "2024-01-01"), std::runtime_error);
}

TEST(Database, ReadTimeSeriesRowGroupNotFound) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    EXPECT_THROW(db.read_time_series_row("Collection", "nonexistent", "value", "2024-01-01"), std::runtime_error);
}

TEST(Database, ReadTimeSeriesRowMultiColumn) {
    auto db = quiver::Database::from_schema(":memory:",
                                            VALID_SCHEMA("mixed_time_series.sql"),
                                            {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element sensor;
    sensor.set("label", std::string("Sensor 1"));
    auto id = db.create_element("Sensor", sensor);

    db.update_time_series_group("Sensor",
                                "readings",
                                id,
                                {{{"date_time", std::string("2024-01-01")},
                                  {"temperature", 20.5},
                                  {"humidity", int64_t{65}},
                                  {"status", std::string("ok")}},
                                 {{"date_time", std::string("2024-01-02")},
                                  {"temperature", 21.0},
                                  {"humidity", int64_t{70}},
                                  {"status", std::string("warn")}}});

    // Read temperature (REAL) at 2024-01-01
    auto temps = db.read_time_series_row("Sensor", "readings", "temperature", "2024-01-01");
    ASSERT_EQ(temps.size(), 1);
    EXPECT_DOUBLE_EQ(std::get<double>(temps[0]), 20.5);

    // Read humidity (INTEGER) at 2024-01-02
    auto humids = db.read_time_series_row("Sensor", "readings", "humidity", "2024-01-02");
    ASSERT_EQ(humids.size(), 1);
    EXPECT_EQ(std::get<int64_t>(humids[0]), 70);

    // Read status (TEXT) at 2024-01-02
    auto stats = db.read_time_series_row("Sensor", "readings", "status", "2024-01-02");
    ASSERT_EQ(stats.size(), 1);
    EXPECT_EQ(std::get<std::string>(stats[0]), "warn");
}

TEST(Database, ReadTimeSeriesRowSkipsNullValues) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e1;
    e1.set("label", std::string("Item 1"));
    auto id1 = db.create_element("Collection", e1);

    // Insert: non-null at 01-01, null at 01-02
    db.update_time_series_group("Collection",
                                "data",
                                id1,
                                {{{"date_time", std::string("2024-01-01")}, {"value", 5.0}},
                                 {{"date_time", std::string("2024-01-02")}, {"value", nullptr}}});

    // Query at 01-02: should find the non-null value at 01-01 (skips null at 01-02)
    auto result = db.read_time_series_row("Collection", "data", "value", "2024-01-02");
    ASSERT_EQ(result.size(), 1);
    EXPECT_DOUBLE_EQ(std::get<double>(result[0]), 5.0);
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

static std::string capture_add_row_error(quiver::Database& db,
                                         const std::string& collection,
                                         const std::string& group,
                                         int64_t id,
                                         const std::map<std::string, quiver::Value>& row) {
    try {
        db.add_time_series_row(collection, group, id, row);
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

TEST(Database, TimeSeriesTypeMismatchIntegerToReal) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e1;
    e1.set("label", std::string("Item 1"));
    auto id = db.create_element("Collection", e1);

    std::vector<std::map<std::string, quiver::Value>> rows = {
        {{"date_time", std::string("2024-01-01T10:00:00")}, {"value", int64_t{1}}}};
    auto msg = capture_update_error(db, "Collection", "data", id, rows);
    EXPECT_NE(msg.find("column 'value' has type REAL but received INTEGER"), std::string::npos) << "Actual: " << msg;
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
// Time series files tests
// ============================================================================

TEST(Database, HasTimeSeriesFiles) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    EXPECT_TRUE(db.has_time_series_files("Collection"));
    EXPECT_FALSE(db.has_time_series_files("Configuration"));
}

TEST(Database, ListTimeSeriesFilesColumns) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    auto columns = db.list_time_series_files_columns("Collection");
    EXPECT_EQ(columns.size(), 2);
    EXPECT_TRUE(std::find(columns.begin(), columns.end(), "data_file") != columns.end());
    EXPECT_TRUE(std::find(columns.begin(), columns.end(), "metadata_file") != columns.end());
}

TEST(Database, ReadTimeSeriesFilesEmpty) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    auto paths = db.read_time_series_files("Collection");
    EXPECT_EQ(paths.size(), 2);
    EXPECT_FALSE(paths["data_file"].has_value());
    EXPECT_FALSE(paths["metadata_file"].has_value());
}

TEST(Database, UpdateAndReadTimeSeriesFiles) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    std::map<std::string, std::optional<std::string>> paths;
    paths["data_file"] = "/path/to/data.csv";
    paths["metadata_file"] = "/path/to/meta.json";

    db.update_time_series_files("Collection", paths);

    auto result = db.read_time_series_files("Collection");
    EXPECT_EQ(result.size(), 2);
    EXPECT_TRUE(result["data_file"].has_value());
    EXPECT_EQ(result["data_file"].value(), "/path/to/data.csv");
    EXPECT_TRUE(result["metadata_file"].has_value());
    EXPECT_EQ(result["metadata_file"].value(), "/path/to/meta.json");
}

TEST(Database, UpdateTimeSeriesFilesWithNulls) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    std::map<std::string, std::optional<std::string>> paths;
    paths["data_file"] = "/path/to/data.csv";
    paths["metadata_file"] = std::nullopt;

    db.update_time_series_files("Collection", paths);

    auto result = db.read_time_series_files("Collection");
    EXPECT_TRUE(result["data_file"].has_value());
    EXPECT_EQ(result["data_file"].value(), "/path/to/data.csv");
    EXPECT_FALSE(result["metadata_file"].has_value());
}

TEST(Database, UpdateTimeSeriesFilesReplace) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    // First update
    std::map<std::string, std::optional<std::string>> paths1;
    paths1["data_file"] = "/old/data.csv";
    paths1["metadata_file"] = "/old/meta.json";
    db.update_time_series_files("Collection", paths1);

    // Second update replaces
    std::map<std::string, std::optional<std::string>> paths2;
    paths2["data_file"] = "/new/data.csv";
    paths2["metadata_file"] = "/new/meta.json";
    db.update_time_series_files("Collection", paths2);

    auto result = db.read_time_series_files("Collection");
    EXPECT_EQ(result["data_file"].value(), "/new/data.csv");
    EXPECT_EQ(result["metadata_file"].value(), "/new/meta.json");
}

TEST(Database, TimeSeriesFilesNotFound) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    EXPECT_THROW(db.read_time_series_files("Configuration"), std::runtime_error);
    EXPECT_THROW(db.list_time_series_files_columns("Configuration"), std::runtime_error);
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
    EXPECT_NE(msg.find("Cannot update_time_series_group: row missing required 'block' column"),
              std::string::npos)
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

    std::vector<std::map<std::string, quiver::Value>> rows = {
        {{"block", int64_t{1}}, {"load", 10.0}}};
    auto msg = capture_update_error(db, "Resource", "load", id, rows);
    EXPECT_NE(msg.find("Cannot update_time_series_group: row missing required 'date_time' column"),
              std::string::npos)
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
    EXPECT_NE(msg.find("Cannot update_time_series_group: row missing required 'block' column"),
              std::string::npos)
        << "Actual: " << msg;

    auto result = db.read_time_series_group("Resource", "load", id);
    EXPECT_TRUE(result.empty());
}

// ============================================================================
// add_time_series_row tests (CORE-11..14)
// ============================================================================

TEST(Database, AddTimeSeriesRowInsert) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element item;
    item.set("label", std::string("Item 1"));
    auto id = db.create_element("Collection", item);

    db.add_time_series_row(
        "Collection", "data", id, {{"date_time", std::string("2024-01-01T10:00:00")}, {"value", 1.5}});

    auto rows = db.read_time_series_group("Collection", "data", id);
    ASSERT_EQ(rows.size(), 1);
    EXPECT_EQ(std::get<std::string>(rows[0]["date_time"]), "2024-01-01T10:00:00");
    EXPECT_DOUBLE_EQ(std::get<double>(rows[0]["value"]), 1.5);

    auto single = db.read_time_series_row("Collection", "data", "value", "2024-01-01T10:00:00");
    ASSERT_EQ(single.size(), 1);
    EXPECT_DOUBLE_EQ(std::get<double>(single[0]), 1.5);
}

TEST(Database, AddTimeSeriesRowUpsertSamePK) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element item;
    item.set("label", std::string("Item 1"));
    auto id = db.create_element("Collection", item);

    db.add_time_series_row(
        "Collection", "data", id, {{"date_time", std::string("2024-01-01T10:00:00")}, {"value", 1.0}});
    db.add_time_series_row(
        "Collection", "data", id, {{"date_time", std::string("2024-01-01T10:00:00")}, {"value", 99.0}});

    auto rows = db.read_time_series_group("Collection", "data", id);
    ASSERT_EQ(rows.size(), 1);  // upsert: NOT 2 rows
    EXPECT_EQ(std::get<std::string>(rows[0]["date_time"]), "2024-01-01T10:00:00");
    EXPECT_DOUBLE_EQ(std::get<double>(rows[0]["value"]), 99.0);
}

TEST(Database, AddTimeSeriesRowMixedInsertAndUpsert) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element item;
    item.set("label", std::string("Item 1"));
    auto id = db.create_element("Collection", item);

    db.add_time_series_row(
        "Collection", "data", id, {{"date_time", std::string("2024-01-01T10:00:00")}, {"value", 1.0}});
    db.add_time_series_row(
        "Collection", "data", id, {{"date_time", std::string("2024-01-02T10:00:00")}, {"value", 2.0}});
    // Upsert on t1 — must replace only the t1 row, leaving t2 untouched.
    db.add_time_series_row(
        "Collection", "data", id, {{"date_time", std::string("2024-01-01T10:00:00")}, {"value", 10.0}});

    auto rows = db.read_time_series_group("Collection", "data", id);
    ASSERT_EQ(rows.size(), 2);
    // read_time_series_group sorts by dim_col ascending.
    EXPECT_EQ(std::get<std::string>(rows[0]["date_time"]), "2024-01-01T10:00:00");
    EXPECT_DOUBLE_EQ(std::get<double>(rows[0]["value"]), 10.0);
    EXPECT_EQ(std::get<std::string>(rows[1]["date_time"]), "2024-01-02T10:00:00");
    EXPECT_DOUBLE_EQ(std::get<double>(rows[1]["value"]), 2.0);
}

TEST(Database, AddTimeSeriesRowMultiDimSchemaInsert) {
    auto db = quiver::Database::from_schema(":memory:",
                                            VALID_SCHEMA("multi_dim_time_series.sql"),
                                            {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element resource;
    resource.set("label", std::string("Resource 1"));
    auto id = db.create_element("Resource", resource);

    db.add_time_series_row(
        "Resource",
        "load",
        id,
        {{"date_time", std::string("2024-01-01")}, {"block", int64_t{1}}, {"load", 10.0}, {"flag", int64_t{1}}});
    db.add_time_series_row(
        "Resource",
        "load",
        id,
        {{"date_time", std::string("2024-01-01")}, {"block", int64_t{2}}, {"load", 20.0}, {"flag", int64_t{1}}});
    db.add_time_series_row(
        "Resource",
        "load",
        id,
        {{"date_time", std::string("2024-01-02")}, {"block", int64_t{1}}, {"load", 30.0}, {"flag", int64_t{0}}});
    db.add_time_series_row(
        "Resource",
        "load",
        id,
        {{"date_time", std::string("2024-01-02")}, {"block", int64_t{2}}, {"load", 40.0}, {"flag", int64_t{0}}});

    auto rows = db.read_time_series_group("Resource", "load", id);
    ASSERT_EQ(rows.size(), 4);

    // read_time_series_group sorts only by dim_col (date_time) — block order within
    // a date_time is undefined. Sort by (date_time, block) for stable assertions.
    std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) {
        const auto& da = std::get<std::string>(a.at("date_time"));
        const auto& db_ = std::get<std::string>(b.at("date_time"));
        if (da != db_)
            return da < db_;
        return std::get<int64_t>(a.at("block")) < std::get<int64_t>(b.at("block"));
    });

    EXPECT_EQ(std::get<std::string>(rows[0]["date_time"]), "2024-01-01");
    EXPECT_EQ(std::get<int64_t>(rows[0]["block"]), 1);
    EXPECT_DOUBLE_EQ(std::get<double>(rows[0]["load"]), 10.0);
    EXPECT_EQ(std::get<int64_t>(rows[0]["flag"]), 1);

    EXPECT_EQ(std::get<std::string>(rows[1]["date_time"]), "2024-01-01");
    EXPECT_EQ(std::get<int64_t>(rows[1]["block"]), 2);
    EXPECT_DOUBLE_EQ(std::get<double>(rows[1]["load"]), 20.0);
    EXPECT_EQ(std::get<int64_t>(rows[1]["flag"]), 1);

    EXPECT_EQ(std::get<std::string>(rows[2]["date_time"]), "2024-01-02");
    EXPECT_EQ(std::get<int64_t>(rows[2]["block"]), 1);
    EXPECT_DOUBLE_EQ(std::get<double>(rows[2]["load"]), 30.0);
    EXPECT_EQ(std::get<int64_t>(rows[2]["flag"]), 0);

    EXPECT_EQ(std::get<std::string>(rows[3]["date_time"]), "2024-01-02");
    EXPECT_EQ(std::get<int64_t>(rows[3]["block"]), 2);
    EXPECT_DOUBLE_EQ(std::get<double>(rows[3]["load"]), 40.0);
    EXPECT_EQ(std::get<int64_t>(rows[3]["flag"]), 0);
}

TEST(Database, AddTimeSeriesRowMultiDimSchemaUpsert) {
    auto db = quiver::Database::from_schema(":memory:",
                                            VALID_SCHEMA("multi_dim_time_series.sql"),
                                            {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element resource;
    resource.set("label", std::string("Resource 1"));
    auto id = db.create_element("Resource", resource);

    // Initial insert at (2024-01-01, 1).
    db.add_time_series_row(
        "Resource",
        "load",
        id,
        {{"date_time", std::string("2024-01-01")}, {"block", int64_t{1}}, {"load", 10.0}, {"flag", int64_t{1}}});
    // Upsert same (date_time, block) — must overwrite.
    db.add_time_series_row(
        "Resource",
        "load",
        id,
        {{"date_time", std::string("2024-01-01")}, {"block", int64_t{1}}, {"load", 99.0}, {"flag", int64_t{0}}});
    // Different block, same date_time — must be a new row.
    db.add_time_series_row(
        "Resource",
        "load",
        id,
        {{"date_time", std::string("2024-01-01")}, {"block", int64_t{2}}, {"load", 20.0}, {"flag", int64_t{1}}});

    auto rows = db.read_time_series_group("Resource", "load", id);
    ASSERT_EQ(rows.size(), 2);

    std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) {
        return std::get<int64_t>(a.at("block")) < std::get<int64_t>(b.at("block"));
    });

    // (date_time=2024-01-01, block=1) — overwritten by the second insert.
    EXPECT_EQ(std::get<int64_t>(rows[0]["block"]), 1);
    EXPECT_DOUBLE_EQ(std::get<double>(rows[0]["load"]), 99.0);
    EXPECT_EQ(std::get<int64_t>(rows[0]["flag"]), 0);

    // (date_time=2024-01-01, block=2) — independent new row.
    EXPECT_EQ(std::get<int64_t>(rows[1]["block"]), 2);
    EXPECT_DOUBLE_EQ(std::get<double>(rows[1]["load"]), 20.0);
    EXPECT_EQ(std::get<int64_t>(rows[1]["flag"]), 1);
}

TEST(Database, AddTimeSeriesRowPartialValueColumns) {
    auto db = quiver::Database::from_schema(":memory:",
                                            VALID_SCHEMA("multi_dim_time_series.sql"),
                                            {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element resource;
    resource.set("label", std::string("Resource 1"));
    auto id = db.create_element("Resource", resource);

    // Row 1: supply (date_time, block, load) — omit `flag`.
    db.add_time_series_row(
        "Resource", "load", id, {{"date_time", std::string("2024-01-01")}, {"block", int64_t{1}}, {"load", 10.0}});

    // Row 2: supply (date_time, block, flag) — omit `load`.
    db.add_time_series_row("Resource",
                           "load",
                           id,
                           {{"date_time", std::string("2024-01-01")}, {"block", int64_t{2}}, {"flag", int64_t{5}}});

    auto rows = db.read_time_series_group("Resource", "load", id);
    ASSERT_EQ(rows.size(), 2);

    std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) {
        return std::get<int64_t>(a.at("block")) < std::get<int64_t>(b.at("block"));
    });

    // Row 1 (block=1): load=10.0, flag omitted → NULL.
    EXPECT_EQ(std::get<int64_t>(rows[0]["block"]), 1);
    EXPECT_DOUBLE_EQ(std::get<double>(rows[0]["load"]), 10.0);
    EXPECT_TRUE(std::holds_alternative<std::nullptr_t>(rows[0]["flag"]));

    // Row 2 (block=2): load omitted → NULL, flag=5.
    EXPECT_EQ(std::get<int64_t>(rows[1]["block"]), 2);
    EXPECT_TRUE(std::holds_alternative<std::nullptr_t>(rows[1]["load"]));
    EXPECT_EQ(std::get<int64_t>(rows[1]["flag"]), 5);
}

TEST(Database, AddTimeSeriesRowErrors) {
    auto db = quiver::Database::from_schema(":memory:",
                                            VALID_SCHEMA("multi_dim_time_series.sql"),
                                            {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element resource;
    resource.set("label", std::string("Resource 1"));
    auto id = db.create_element("Resource", resource);

    // a. Missing dimension column ('block' omitted, date_time present).
    {
        auto msg = capture_add_row_error(
            db, "Resource", "load", id, {{"date_time", std::string("2024-01-01")}, {"load", 1.0}});
        EXPECT_NE(msg.find("Cannot add_time_series_row: row missing required 'block' column"), std::string::npos)
            << "Actual: " << msg;
    }

    // b. Missing OTHER dimension column ('date_time' omitted, block present).
    {
        auto msg = capture_add_row_error(db, "Resource", "load", id, {{"block", int64_t{1}}, {"load", 1.0}});
        EXPECT_NE(msg.find("Cannot add_time_series_row: row missing required 'date_time' column"), std::string::npos)
            << "Actual: " << msg;
    }

    // c. Unknown column.
    {
        auto msg = capture_add_row_error(
            db,
            "Resource",
            "load",
            id,
            {{"date_time", std::string("2024-01-01")}, {"block", int64_t{1}}, {"load", 1.0}, {"pressure", 1013.25}});
        EXPECT_NE(msg.find("Cannot add_time_series_row: column 'pressure' not found in group 'load'"),
                  std::string::npos)
            << "Actual: " << msg;
    }

    // d. Type mismatch: INTEGER passed for REAL column 'load'.
    {
        auto msg = capture_add_row_error(
            db,
            "Resource",
            "load",
            id,
            {{"date_time", std::string("2024-01-01")}, {"block", int64_t{1}}, {"load", int64_t{42}}});
        EXPECT_NE(msg.find("Cannot add_time_series_row: column 'load' has type REAL but received INTEGER"),
                  std::string::npos)
            << "Actual: " << msg;
    }

    // e. Type mismatch: REAL passed for INTEGER column 'block'.
    {
        auto msg = capture_add_row_error(
            db, "Resource", "load", id, {{"date_time", std::string("2024-01-01")}, {"block", 1.5}, {"load", 1.0}});
        EXPECT_NE(msg.find("Cannot add_time_series_row: column 'block' has type INTEGER but received REAL"),
                  std::string::npos)
            << "Actual: " << msg;
    }
}

TEST(Database, AddTimeSeriesRowTransactionRollback) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element item;
    item.set("label", std::string("Item 1"));
    auto id = db.create_element("Collection", item);

    db.begin_transaction();
    EXPECT_TRUE(db.in_transaction());
    db.add_time_series_row("Collection", "data", id, {{"date_time", std::string("2024-01-01")}, {"value", 1.0}});
    EXPECT_TRUE(db.in_transaction());  // nest-aware: still inside the outer txn
    db.rollback();
    EXPECT_FALSE(db.in_transaction());

    auto rows = db.read_time_series_group("Collection", "data", id);
    EXPECT_TRUE(rows.empty());  // rolled back — nothing persisted
}

TEST(Database, AddTimeSeriesRowTransactionCommitAndStandalone) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element item;
    item.set("label", std::string("Item 1"));
    auto id = db.create_element("Collection", item);

    // Phase A: explicit transaction, two writes batched together.
    db.begin_transaction();
    db.add_time_series_row("Collection", "data", id, {{"date_time", std::string("2024-01-01")}, {"value", 1.0}});
    db.add_time_series_row("Collection", "data", id, {{"date_time", std::string("2024-01-02")}, {"value", 2.0}});
    EXPECT_TRUE(db.in_transaction());
    db.commit();
    EXPECT_FALSE(db.in_transaction());

    auto rows1 = db.read_time_series_group("Collection", "data", id);
    EXPECT_EQ(rows1.size(), 2);

    // Phase B: standalone (no explicit txn) — TransactionGuard commits automatically.
    EXPECT_FALSE(db.in_transaction());
    db.add_time_series_row("Collection", "data", id, {{"date_time", std::string("2024-01-03")}, {"value", 3.0}});
    EXPECT_FALSE(db.in_transaction());  // returned to autocommit

    auto rows2 = db.read_time_series_group("Collection", "data", id);
    EXPECT_EQ(rows2.size(), 3);  // prior 2 (from Phase A) + 1 (Phase B)
}
