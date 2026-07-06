#include "test_utils.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <quiver/database.h>
#include <quiver/element.h>

// ============================================================================
// Read set tests
// ============================================================================

TEST(Database, ReadSetStrings) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e1;
    e1.set("label", std::string("Item 1")).set("tag", std::vector<std::string>{"important", "urgent"});
    db.create_element("Collection", e1);

    quiver::Element e2;
    e2.set("label", std::string("Item 2")).set("tag", std::vector<std::string>{"review"});
    db.create_element("Collection", e2);

    auto sets = db.read_set_strings("Collection", "tag");
    EXPECT_EQ(sets.size(), 2);
    // Sets are unordered, so sort before comparison
    auto set1 = sets[0];
    auto set2 = sets[1];
    std::sort(set1.begin(), set1.end());
    std::sort(set2.begin(), set2.end());
    EXPECT_EQ(set1, (std::vector<std::string>{"important", "urgent"}));
    EXPECT_EQ(set2, (std::vector<std::string>{"review"}));
}

TEST(Database, ReadSetEmpty) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // No Collection elements created
    auto sets = db.read_set_strings("Collection", "tag");
    EXPECT_TRUE(sets.empty());
}

TEST(Database, ReadSetOnlyReturnsElementsWithData) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // Element with set data
    quiver::Element e1;
    e1.set("label", std::string("Item 1")).set("tag", std::vector<std::string>{"important"});
    db.create_element("Collection", e1);

    // Element without set data
    quiver::Element e2;
    e2.set("label", std::string("Item 2"));
    db.create_element("Collection", e2);

    // Another element with set data
    quiver::Element e3;
    e3.set("label", std::string("Item 3")).set("tag", std::vector<std::string>{"urgent", "review"});
    db.create_element("Collection", e3);

    // Only elements with set data are returned
    auto sets = db.read_set_strings("Collection", "tag");
    EXPECT_EQ(sets.size(), 2);
}

// ============================================================================
// Read set by ID tests
// ============================================================================

TEST(Database, ReadSetStringById) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e1;
    e1.set("label", std::string("Item 1")).set("tag", std::vector<std::string>{"important", "urgent"});
    int64_t id1 = db.create_element("Collection", e1);

    quiver::Element e2;
    e2.set("label", std::string("Item 2")).set("tag", std::vector<std::string>{"review"});
    int64_t id2 = db.create_element("Collection", e2);

    auto set1 = db.read_set_strings_by_id("Collection", "tag", id1);
    auto set2 = db.read_set_strings_by_id("Collection", "tag", id2);

    // Sets are unordered, so sort before comparison
    std::sort(set1.begin(), set1.end());
    EXPECT_EQ(set1, (std::vector<std::string>{"important", "urgent"}));
    EXPECT_EQ(set2, (std::vector<std::string>{"review"}));
}

TEST(Database, ReadSetByIdEmpty) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e;
    e.set("label", std::string("Item 1"));  // No set data
    int64_t id = db.create_element("Collection", e);

    auto set = db.read_set_strings_by_id("Collection", "tag", id);
    EXPECT_TRUE(set.empty());
}

TEST(Database, ReadSetStringsInvalidCollection) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.read_set_strings("NonexistentCollection", "tag"), std::runtime_error);
}

TEST(Database, ReadSetStringsInvalidAttribute) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.read_set_strings("Collection", "nonexistent_attribute"), std::runtime_error);
}

TEST(Database, ReadSetStringsByIdInvalidCollection) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.read_set_strings_by_id("NonexistentCollection", "tag", 1), std::runtime_error);
}

// ============================================================================
// Read set integers/floats tests (gap-fill using all_types.sql)
// ============================================================================

TEST(Database, ReadSetIntegersBulk) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("all_types.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e1;
    e1.set("label", std::string("Item 1"));
    int64_t id1 = db.create_element("AllTypes", e1);
    quiver::Element update1;
    update1.set("code", std::vector<int64_t>{10, 20, 30});
    db.update_element("AllTypes", id1, update1);

    quiver::Element e2;
    e2.set("label", std::string("Item 2"));
    int64_t id2 = db.create_element("AllTypes", e2);
    quiver::Element update2;
    update2.set("code", std::vector<int64_t>{40, 50});
    db.update_element("AllTypes", id2, update2);

    auto sets = db.read_set_integers("AllTypes", "code");
    EXPECT_EQ(sets.size(), 2);
    auto set1 = sets[0];
    auto set2 = sets[1];
    std::sort(set1.begin(), set1.end());
    std::sort(set2.begin(), set2.end());
    EXPECT_EQ(set1, (std::vector<int64_t>{10, 20, 30}));
    EXPECT_EQ(set2, (std::vector<int64_t>{40, 50}));
}

TEST(Database, ReadSetIntegersByIdBasic) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("all_types.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e;
    e.set("label", std::string("Item 1"));
    int64_t id = db.create_element("AllTypes", e);
    quiver::Element update;
    update.set("code", std::vector<int64_t>{100, 200, 300});
    db.update_element("AllTypes", id, update);

    auto set = db.read_set_integers_by_id("AllTypes", "code", id);
    std::sort(set.begin(), set.end());
    EXPECT_EQ(set, (std::vector<int64_t>{100, 200, 300}));
}

TEST(Database, ReadSetFloatsBulk) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("all_types.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e1;
    e1.set("label", std::string("Item 1"));
    int64_t id1 = db.create_element("AllTypes", e1);
    quiver::Element update1;
    update1.set("weight", std::vector<double>{1.1, 2.2, 3.3});
    db.update_element("AllTypes", id1, update1);

    quiver::Element e2;
    e2.set("label", std::string("Item 2"));
    int64_t id2 = db.create_element("AllTypes", e2);
    quiver::Element update2;
    update2.set("weight", std::vector<double>{4.4, 5.5});
    db.update_element("AllTypes", id2, update2);

    auto sets = db.read_set_floats("AllTypes", "weight");
    EXPECT_EQ(sets.size(), 2);
    auto set1 = sets[0];
    auto set2 = sets[1];
    std::sort(set1.begin(), set1.end());
    std::sort(set2.begin(), set2.end());
    EXPECT_EQ(set1.size(), 3);
    EXPECT_DOUBLE_EQ(set1[0], 1.1);
    EXPECT_DOUBLE_EQ(set1[1], 2.2);
    EXPECT_DOUBLE_EQ(set1[2], 3.3);
    EXPECT_EQ(set2.size(), 2);
    EXPECT_DOUBLE_EQ(set2[0], 4.4);
    EXPECT_DOUBLE_EQ(set2[1], 5.5);
}

TEST(Database, ReadSetFloatsByIdBasic) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("all_types.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e;
    e.set("label", std::string("Item 1"));
    int64_t id = db.create_element("AllTypes", e);
    quiver::Element update;
    update.set("weight", std::vector<double>{9.9, 8.8});
    db.update_element("AllTypes", id, update);

    auto set = db.read_set_floats_by_id("AllTypes", "weight", id);
    std::sort(set.begin(), set.end());
    EXPECT_EQ(set.size(), 2);
    EXPECT_DOUBLE_EQ(set[0], 8.8);
    EXPECT_DOUBLE_EQ(set[1], 9.9);
}
