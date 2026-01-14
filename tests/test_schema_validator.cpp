#include <filesystem>
#include <gtest/gtest.h>
#include <psr/database.h>

namespace fs = std::filesystem;

class SchemaValidatorFixture : public ::testing::Test {
protected:
    psr::DatabaseOptions opts{.console_level = psr::LogLevel::off};

    std::string tests_path() {
        return fs::path(__FILE__).parent_path().string();
    }

    std::string schema_path(const std::string& filename) {
        return (fs::path(__FILE__).parent_path() / filename).string();
    }

    std::string invalid_path(const std::string& filename) {
        return (fs::path(__FILE__).parent_path() / "invalid" / filename).string();
    }
};

// Valid schemas
TEST_F(SchemaValidatorFixture, ValidSchemaBasic) {
    EXPECT_NO_THROW(psr::Database::from_schema(":memory:", schema_path("schema_basic.sql"), opts));
}

TEST_F(SchemaValidatorFixture, ValidSchemaCollections) {
    EXPECT_NO_THROW(psr::Database::from_schema(":memory:", schema_path("schema_collections.sql"), opts));
}

TEST_F(SchemaValidatorFixture, ValidSchemaRelations) {
    EXPECT_NO_THROW(psr::Database::from_schema(":memory:", schema_path("schema_relations.sql"), opts));
}

// Invalid schemas
TEST_F(SchemaValidatorFixture, InvalidNoConfiguration) {
    EXPECT_THROW(psr::Database::from_schema(":memory:", invalid_path("no_configuration.sql"), opts),
                 std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidLabelNotNull) {
    EXPECT_THROW(psr::Database::from_schema(":memory:", invalid_path("label_not_null.sql"), opts),
                 std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidLabelNotUnique) {
    EXPECT_THROW(psr::Database::from_schema(":memory:", invalid_path("label_not_unique.sql"), opts),
                 std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidLabelWrongType) {
    EXPECT_THROW(psr::Database::from_schema(":memory:", invalid_path("label_wrong_type.sql"), opts),
                 std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidDuplicateAttribute) {
    EXPECT_THROW(psr::Database::from_schema(":memory:", invalid_path("duplicate_attribute.sql"), opts),
                 std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidVectorNoIndex) {
    EXPECT_THROW(psr::Database::from_schema(":memory:", invalid_path("vector_no_index.sql"), opts),
                 std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidSetNoUnique) {
    EXPECT_THROW(psr::Database::from_schema(":memory:", invalid_path("set_no_unique.sql"), opts),
                 std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidFkNotNullSetNull) {
    EXPECT_THROW(psr::Database::from_schema(":memory:", invalid_path("fk_not_null_set_null.sql"), opts),
                 std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidFkActions) {
    EXPECT_THROW(psr::Database::from_schema(":memory:", invalid_path("fk_actions.sql"), opts),
                 std::runtime_error);
}
