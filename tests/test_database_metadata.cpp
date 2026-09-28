#include "test_utils.h"

#include <gtest/gtest.h>
#include <quiver/database.h>

// ============================================================================
// Group metadata foreign key tests
// ============================================================================

TEST(Database, GetVectorMetadataForeignKey) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("relations.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    auto metadata = db.get_vector_metadata("Child", "refs");
    ASSERT_EQ(metadata.value_columns.size(), 1);
    EXPECT_EQ(metadata.value_columns[0].name, "parent_ref");
    EXPECT_TRUE(metadata.value_columns[0].is_foreign_key);
    EXPECT_EQ(metadata.value_columns[0].references_collection, "Parent");
    EXPECT_EQ(metadata.value_columns[0].references_column, "id");
}

TEST(Database, GetSetMetadataForeignKey) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("relations.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    auto metadata = db.get_set_metadata("Child", "parents");
    ASSERT_EQ(metadata.value_columns.size(), 1);
    EXPECT_EQ(metadata.value_columns[0].name, "parent_ref");
    EXPECT_TRUE(metadata.value_columns[0].is_foreign_key);
    EXPECT_EQ(metadata.value_columns[0].references_collection, "Parent");
    EXPECT_EQ(metadata.value_columns[0].references_column, "id");
}

TEST(Database, GetSetMetadataNonForeignKeyColumn) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("relations.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    auto metadata = db.get_set_metadata("Child", "scores");
    ASSERT_EQ(metadata.value_columns.size(), 1);
    EXPECT_EQ(metadata.value_columns[0].name, "score");
    EXPECT_FALSE(metadata.value_columns[0].is_foreign_key);
    EXPECT_FALSE(metadata.value_columns[0].references_collection.has_value());
}

// ============================================================================
// List groups: unknown collection
// ============================================================================

TEST(Database, ListGroupsCollectionNotFound) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    try {
        (void)db.list_vector_groups("Nope");
        FAIL() << "Expected list_vector_groups to reject an unknown collection";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "Cannot list_vector_groups: collection not found: Nope");
    }

    try {
        (void)db.list_set_groups("Nope");
        FAIL() << "Expected list_set_groups to reject an unknown collection";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "Cannot list_set_groups: collection not found: Nope");
    }
}
