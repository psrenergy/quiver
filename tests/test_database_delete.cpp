#include "test_utils.h"

#include <gtest/gtest.h>
#include <quiver/database.h>
#include <quiver/element.h>

TEST(Database, DeleteElementById) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element e;
    e.set("label", std::string("Config 1")).set("integer_attribute", int64_t{42});
    int64_t id = db.create_element("Configuration", e);

    // Verify element exists
    auto ids = db.read_element_ids("Configuration");
    EXPECT_EQ(ids.size(), 1);

    // Delete element
    db.delete_element("Configuration", id);

    // Verify element is gone
    ids = db.read_element_ids("Configuration");
    EXPECT_TRUE(ids.empty());
}

TEST(Database, DeleteElementByIdWithVectorData) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e;
    e.set("label", std::string("Item 1")).set("value_int", std::vector<int64_t>{1, 2, 3});
    int64_t id = db.create_element("Collection", e);

    // Verify vector data exists
    auto vec = db.read_vector_integers_by_id("Collection", "value_int", id);
    EXPECT_EQ(vec.size(), 3);

    // Delete element - CASCADE should delete vector rows too
    db.delete_element("Collection", id);

    // Verify element is gone
    auto ids = db.read_element_ids("Collection");
    EXPECT_TRUE(ids.empty());

    // Verify vector data is also gone (via CASCADE DELETE)
    auto all_vectors = db.read_vector_integers("Collection", "value_int");
    EXPECT_TRUE(all_vectors.empty());
}

TEST(Database, DeleteElementByIdWithSetData) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e;
    e.set("label", std::string("Item 1")).set("tag", std::vector<std::string>{"important", "urgent"});
    int64_t id = db.create_element("Collection", e);

    // Verify set data exists
    auto set = db.read_set_strings_by_id("Collection", "tag", id);
    EXPECT_EQ(set.size(), 2);

    // Delete element - CASCADE should delete set rows too
    db.delete_element("Collection", id);

    // Verify element is gone
    auto ids = db.read_element_ids("Collection");
    EXPECT_TRUE(ids.empty());

    // Verify set data is also gone (via CASCADE DELETE)
    auto all_sets = db.read_set_strings("Collection", "tag");
    EXPECT_TRUE(all_sets.empty());
}

TEST(Database, DeleteElementByIdNonExistent) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element e;
    e.set("label", std::string("Config 1")).set("integer_attribute", int64_t{42});
    db.create_element("Configuration", e);

    // Deleting a non-existent id throws "Element not found" rather than silently no-op'ing
    EXPECT_THROW(db.delete_element("Configuration", 999), std::runtime_error);

    // Verify original element still exists
    auto ids = db.read_element_ids("Configuration");
    EXPECT_EQ(ids.size(), 1);
}

TEST(Database, DeleteElementByIdOtherElementsUnchanged) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element e1;
    e1.set("label", std::string("Config 1")).set("integer_attribute", int64_t{42});
    int64_t id1 = db.create_element("Configuration", e1);

    quiver::Element e2;
    e2.set("label", std::string("Config 2")).set("integer_attribute", int64_t{100});
    int64_t id2 = db.create_element("Configuration", e2);

    quiver::Element e3;
    e3.set("label", std::string("Config 3")).set("integer_attribute", int64_t{200});
    int64_t id3 = db.create_element("Configuration", e3);

    // Delete middle element
    db.delete_element("Configuration", id2);

    // Verify only two elements remain
    auto ids = db.read_element_ids("Configuration");
    EXPECT_EQ(ids.size(), 2);
    EXPECT_EQ(ids[0], id1);
    EXPECT_EQ(ids[1], id3);

    // Verify first element unchanged
    auto val1 = db.read_scalar_integer_by_id("Configuration", "integer_attribute", id1);
    EXPECT_TRUE(val1.has_value());
    EXPECT_EQ(*val1, 42);

    // Verify third element unchanged
    auto val3 = db.read_scalar_integer_by_id("Configuration", "integer_attribute", id3);
    EXPECT_TRUE(val3.has_value());
    EXPECT_EQ(*val3, 200);
}

// ============================================================================
// Delete by label tests
// ============================================================================

TEST(Database, DeleteElementByLabel) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e;
    e.set("label", std::string("Item 1")).set("tag", std::vector<std::string>{"important", "urgent"});
    db.create_element("Collection", e);

    db.delete_element("Collection", "Item 1");

    // Verify element is gone
    auto ids = db.read_element_ids("Collection");
    EXPECT_TRUE(ids.empty());

    // CASCADE still applies - the label form delegates to the id form, it does not re-implement it
    auto all_sets = db.read_set_strings("Collection", "tag");
    EXPECT_TRUE(all_sets.empty());
}

TEST(Database, DeleteElementByLabelMatchesIdForm) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e1;
    e1.set("label", std::string("Item 1"));
    int64_t id1 = db.create_element("Collection", e1);

    quiver::Element e2;
    e2.set("label", std::string("Item 2"));
    db.create_element("Collection", e2);

    quiver::Element e3;
    e3.set("label", std::string("Item 3"));
    int64_t id3 = db.create_element("Collection", e3);

    // One deleted by id, one by label - same outcome
    db.delete_element("Collection", id1);
    db.delete_element("Collection", "Item 2");

    auto ids = db.read_element_ids("Collection");
    EXPECT_EQ(ids.size(), 1);
    EXPECT_EQ(ids[0], id3);
}

TEST(Database, DeleteElementByLabelNonExistent) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e;
    e.set("label", std::string("Item 1"));
    db.create_element("Collection", e);

    // Deleting an unresolvable label throws "Element not found" rather than silently no-op'ing
    try {
        db.delete_element("Collection", "No Such Item");
        FAIL() << "expected a throw";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "Element not found: label 'No Such Item' in collection 'Collection'");
    }

    // Verify original element still exists
    auto ids = db.read_element_ids("Collection");
    EXPECT_EQ(ids.size(), 1);
}

// A label is unique per collection, not per database: one naming an element of another
// collection must not resolve here.
TEST(Database, DeleteElementByLabelDoesNotResolveAcrossCollections) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e;
    e.set("label", std::string("Item 1"));
    db.create_element("Collection", e);

    try {
        db.delete_element("Collection", "Test Config");
        FAIL() << "expected a throw";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "Element not found: label 'Test Config' in collection 'Collection'");
    }

    // Both elements survive
    EXPECT_EQ(db.read_element_ids("Configuration").size(), 1);
    EXPECT_EQ(db.read_element_ids("Collection").size(), 1);
}
