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
