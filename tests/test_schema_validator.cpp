#include "test_utils.h"

#include <gtest/gtest.h>
#include <psr/database.h>

class SchemaValidatorFixture : public ::testing::Test {
protected:
    psr::DatabaseOptions opts{.console_level = psr::LogLevel::off};
};

// Valid schemas
TEST_F(SchemaValidatorFixture, ValidSchemaBasic) {
    EXPECT_NO_THROW(psr::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), opts));
}

TEST_F(SchemaValidatorFixture, ValidSchemaCollections) {
    EXPECT_NO_THROW(psr::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), opts));
}

TEST_F(SchemaValidatorFixture, ValidSchemaRelations) {
    EXPECT_NO_THROW(psr::Database::from_schema(":memory:", VALID_SCHEMA("relations.sql"), opts));
}

// Invalid schemas
TEST_F(SchemaValidatorFixture, InvalidNoConfiguration) {
    EXPECT_THROW(psr::Database::from_schema(":memory:", INVALID_SCHEMA("no_configuration.sql"), opts),
                 std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidLabelNotNull) {
    EXPECT_THROW(psr::Database::from_schema(":memory:", INVALID_SCHEMA("label_not_null.sql"), opts),
                 std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidLabelNotUnique) {
    EXPECT_THROW(psr::Database::from_schema(":memory:", INVALID_SCHEMA("label_not_unique.sql"), opts),
                 std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidLabelWrongType) {
    EXPECT_THROW(psr::Database::from_schema(":memory:", INVALID_SCHEMA("label_wrong_type.sql"), opts),
                 std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidDuplicateAttribute) {
    EXPECT_THROW(psr::Database::from_schema(":memory:", INVALID_SCHEMA("duplicate_attribute.sql"), opts),
                 std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidVectorNoIndex) {
    EXPECT_THROW(psr::Database::from_schema(":memory:", INVALID_SCHEMA("vector_no_index.sql"), opts),
                 std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidSetNoUnique) {
    EXPECT_THROW(psr::Database::from_schema(":memory:", INVALID_SCHEMA("set_no_unique.sql"), opts), std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidFkNotNullSetNull) {
    EXPECT_THROW(psr::Database::from_schema(":memory:", INVALID_SCHEMA("fk_not_null_set_null.sql"), opts),
                 std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidFkActions) {
    EXPECT_THROW(psr::Database::from_schema(":memory:", INVALID_SCHEMA("fk_actions.sql"), opts), std::runtime_error);
}
