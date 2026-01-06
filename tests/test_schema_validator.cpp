#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <psr/database.h>
#include <sstream>

namespace fs = std::filesystem;

class SchemaValidatorFixture : public ::testing::Test {
protected:
    std::string test_data_dir;

    void SetUp() override {
        test_data_dir = TEST_DATA_DIR;
    }

    std::string read_schema(const std::string& filename) {
        fs::path path = fs::path(test_data_dir) / filename;
        std::ifstream file(path);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open schema file: " + path.string());
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
};

TEST_F(SchemaValidatorFixture, ValidDatabase) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});
    std::string schema = read_schema("test_valid_database.sql");
    EXPECT_NO_THROW(db.apply_schema_string(schema));
}

TEST_F(SchemaValidatorFixture, InvalidWithoutConfigurationTable) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});
    std::string schema = read_schema("test_invalid_database_without_configuration_table.sql");
    EXPECT_THROW(db.apply_schema_string(schema), std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidDuplicatedAttributes) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});
    std::string schema = read_schema("test_invalid_database_with_duplicated_attributes.sql");
    EXPECT_THROW(db.apply_schema_string(schema), std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidDuplicatedAttributes2) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});
    std::string schema = read_schema("test_invalid_database_with_duplicated_attributes_2.sql");
    // This file doesn't have Configuration table label constraints
    EXPECT_THROW(db.apply_schema_string(schema), std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidCollectionName) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});
    std::string schema = read_schema("test_invalid_database_with_invalid_collection_name.sql");
    EXPECT_THROW(db.apply_schema_string(schema), std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidVectorTable) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});
    std::string schema = read_schema("test_invalid_vector_table.sql");
    EXPECT_THROW(db.apply_schema_string(schema), std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidForeignKeyActions) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});
    std::string schema = read_schema("test_invalid_database_foreign_key_actions.sql");
    EXPECT_THROW(db.apply_schema_string(schema), std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidVectorTableWithoutVectorIndex) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});
    std::string schema = read_schema("test_invalid_database_vector_table_without_vector_index.sql");
    EXPECT_THROW(db.apply_schema_string(schema), std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidForeignKeyNotNull) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});
    std::string schema = read_schema("test_invalid_foreign_key_has_not_null_constraint.sql");
    EXPECT_THROW(db.apply_schema_string(schema), std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidRelationAttribute) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});
    std::string schema = read_schema("test_invalid_relation_attribute.sql");
    EXPECT_THROW(db.apply_schema_string(schema), std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidWithoutLabelConstraints) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});
    std::string schema = read_schema("test_invalid_database_without_label_constraints.sql");
    EXPECT_THROW(db.apply_schema_string(schema), std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidSetTableNotAllUnique) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});
    std::string schema = read_schema("test_invalid_set_table_not_all_unique.sql");
    EXPECT_THROW(db.apply_schema_string(schema), std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidSetTableWithoutUniqueConstraint) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});
    std::string schema = read_schema("test_invalid_set_table_without_unique_constraint.sql");
    EXPECT_THROW(db.apply_schema_string(schema), std::runtime_error);
}
