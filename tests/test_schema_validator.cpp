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

// ============================================================================
// Type validation tests (via create_element errors)
// ============================================================================

TEST_F(SchemaValidatorFixture, TypeMismatchIntegerExpectedReal) {
    auto db = psr::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), opts);

    // Try to set a string where a float is expected
    psr::Element e;
    e.set("label", std::string("Test")).set("float_attribute", std::string("not a float"));

    EXPECT_THROW(db.create_element("Configuration", e), std::runtime_error);
}

TEST_F(SchemaValidatorFixture, TypeMismatchStringExpectedInteger) {
    auto db = psr::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), opts);

    // Try to set a string where an integer is expected
    psr::Element e;
    e.set("label", std::string("Test")).set("integer_attribute", std::string("not an integer"));

    EXPECT_THROW(db.create_element("Configuration", e), std::runtime_error);
}

TEST_F(SchemaValidatorFixture, TypeMismatchIntegerExpectedText) {
    auto db = psr::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), opts);

    // Try to set an integer where a string is expected
    psr::Element e;
    e.set("label", int64_t{42});  // label expects TEXT

    EXPECT_THROW(db.create_element("Configuration", e), std::runtime_error);
}

TEST_F(SchemaValidatorFixture, ArrayTypeValidation) {
    auto db = psr::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), opts);

    psr::Element config;
    config.set("label", std::string("Config"));
    db.create_element("Configuration", config);

    // Try to create element with wrong array type
    psr::Element e;
    e.set("label", std::string("Item 1"));
    // value_int expects integers, but we'll try to pass strings
    // Note: This depends on how the Element class handles type conversion
    e.set("value_int", std::vector<std::string>{"not", "integers"});

    EXPECT_THROW(db.create_element("Collection", e), std::runtime_error);
}

// ============================================================================
// Schema attribute lookup tests
// ============================================================================

TEST_F(SchemaValidatorFixture, GetAttributeTypeScalarInteger) {
    auto db = psr::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), opts);

    auto type = db.get_attribute_type("Configuration", "integer_attribute");

    EXPECT_EQ(type.structure, psr::AttributeStructure::Scalar);
    EXPECT_EQ(type.data_type, psr::AttributeDataType::Integer);
}

TEST_F(SchemaValidatorFixture, GetAttributeTypeScalarReal) {
    auto db = psr::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), opts);

    auto type = db.get_attribute_type("Configuration", "float_attribute");

    EXPECT_EQ(type.structure, psr::AttributeStructure::Scalar);
    EXPECT_EQ(type.data_type, psr::AttributeDataType::Real);
}

TEST_F(SchemaValidatorFixture, GetAttributeTypeScalarText) {
    auto db = psr::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), opts);

    auto type = db.get_attribute_type("Configuration", "label");

    EXPECT_EQ(type.structure, psr::AttributeStructure::Scalar);
    EXPECT_EQ(type.data_type, psr::AttributeDataType::Text);
}

TEST_F(SchemaValidatorFixture, GetAttributeTypeVectorInteger) {
    auto db = psr::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), opts);

    auto type = db.get_attribute_type("Collection", "value_int");

    EXPECT_EQ(type.structure, psr::AttributeStructure::Vector);
    EXPECT_EQ(type.data_type, psr::AttributeDataType::Integer);
}

TEST_F(SchemaValidatorFixture, GetAttributeTypeSetText) {
    auto db = psr::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), opts);

    auto type = db.get_attribute_type("Collection", "tag");

    EXPECT_EQ(type.structure, psr::AttributeStructure::Set);
    EXPECT_EQ(type.data_type, psr::AttributeDataType::Text);
}
