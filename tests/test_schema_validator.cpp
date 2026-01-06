#include <filesystem>
#include <gtest/gtest.h>
#include <psr/database.h>

namespace fs = std::filesystem;

class SchemaValidatorFixture : public ::testing::Test {
protected:
    std::string test_data_dir;
    psr::DatabaseOptions opts{.console_level = psr::LogLevel::off};

    void SetUp() override {
        test_data_dir = TEST_DATA_DIR;
    }

    std::string schema_path(const std::string& filename) {
        return (fs::path(test_data_dir) / filename).string();
    }
};

TEST_F(SchemaValidatorFixture, ValidDatabase) {
    EXPECT_NO_THROW(psr::Database::from_schema(":memory:", schema_path("test_valid_database.sql"), opts));
}

TEST_F(SchemaValidatorFixture, InvalidWithoutConfigurationTable) {
    EXPECT_THROW(
        psr::Database::from_schema(":memory:", schema_path("test_invalid_database_without_configuration_table.sql"), opts),
        std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidDuplicatedAttributes) {
    EXPECT_THROW(
        psr::Database::from_schema(":memory:", schema_path("test_invalid_database_with_duplicated_attributes.sql"), opts),
        std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidDuplicatedAttributes2) {
    EXPECT_THROW(
        psr::Database::from_schema(":memory:", schema_path("test_invalid_database_with_duplicated_attributes_2.sql"), opts),
        std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidCollectionName) {
    EXPECT_THROW(
        psr::Database::from_schema(":memory:", schema_path("test_invalid_database_with_invalid_collection_name.sql"), opts),
        std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidVectorTable) {
    EXPECT_THROW(
        psr::Database::from_schema(":memory:", schema_path("test_invalid_vector_table.sql"), opts),
        std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidForeignKeyActions) {
    EXPECT_THROW(
        psr::Database::from_schema(":memory:", schema_path("test_invalid_database_foreign_key_actions.sql"), opts),
        std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidVectorTableWithoutVectorIndex) {
    EXPECT_THROW(
        psr::Database::from_schema(":memory:", schema_path("test_invalid_database_vector_table_without_vector_index.sql"), opts),
        std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidForeignKeyNotNull) {
    EXPECT_THROW(
        psr::Database::from_schema(":memory:", schema_path("test_invalid_foreign_key_has_not_null_constraint.sql"), opts),
        std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidRelationAttribute) {
    EXPECT_THROW(
        psr::Database::from_schema(":memory:", schema_path("test_invalid_relation_attribute.sql"), opts),
        std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidWithoutLabelConstraints) {
    EXPECT_THROW(
        psr::Database::from_schema(":memory:", schema_path("test_invalid_database_without_label_constraints.sql"), opts),
        std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidSetTableNotAllUnique) {
    EXPECT_THROW(
        psr::Database::from_schema(":memory:", schema_path("test_invalid_set_table_not_all_unique.sql"), opts),
        std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidSetTableWithoutUniqueConstraint) {
    EXPECT_THROW(
        psr::Database::from_schema(":memory:", schema_path("test_invalid_set_table_without_unique_constraint.sql"), opts),
        std::runtime_error);
}
