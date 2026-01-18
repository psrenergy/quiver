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

    EXPECT_EQ(type.structure, psr::DataStructure::Scalar);
    EXPECT_EQ(type.data_type, psr::DataType::Integer);
}

TEST_F(SchemaValidatorFixture, GetAttributeTypeScalarReal) {
    auto db = psr::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), opts);

    auto type = db.get_attribute_type("Configuration", "float_attribute");

    EXPECT_EQ(type.structure, psr::DataStructure::Scalar);
    EXPECT_EQ(type.data_type, psr::DataType::Real);
}

TEST_F(SchemaValidatorFixture, GetAttributeTypeScalarText) {
    auto db = psr::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), opts);

    auto type = db.get_attribute_type("Configuration", "label");

    EXPECT_EQ(type.structure, psr::DataStructure::Scalar);
    EXPECT_EQ(type.data_type, psr::DataType::Text);
}

TEST_F(SchemaValidatorFixture, GetAttributeTypeVectorInteger) {
    auto db = psr::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), opts);

    auto type = db.get_attribute_type("Collection", "value_int");

    EXPECT_EQ(type.structure, psr::DataStructure::Vector);
    EXPECT_EQ(type.data_type, psr::DataType::Integer);
}

TEST_F(SchemaValidatorFixture, GetAttributeTypeSetText) {
    auto db = psr::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), opts);

    auto type = db.get_attribute_type("Collection", "tag");

    EXPECT_EQ(type.structure, psr::DataStructure::Set);
    EXPECT_EQ(type.data_type, psr::DataType::Text);
}

// ============================================================================
// Additional attribute type tests
// ============================================================================

TEST_F(SchemaValidatorFixture, GetAttributeTypeVectorReal) {
    auto db = psr::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), opts);

    auto type = db.get_attribute_type("Collection", "value_float");

    EXPECT_EQ(type.structure, psr::DataStructure::Vector);
    EXPECT_EQ(type.data_type, psr::DataType::Real);
}

TEST_F(SchemaValidatorFixture, GetAttributeTypeForeignKeyAsInteger) {
    auto db = psr::Database::from_schema(":memory:", VALID_SCHEMA("relations.sql"), opts);

    // parent_id is a foreign key but stored as INTEGER
    auto type = db.get_attribute_type("Child", "parent_id");

    EXPECT_EQ(type.structure, psr::DataStructure::Scalar);
    EXPECT_EQ(type.data_type, psr::DataType::Integer);
}

TEST_F(SchemaValidatorFixture, GetAttributeTypeSelfReference) {
    auto db = psr::Database::from_schema(":memory:", VALID_SCHEMA("relations.sql"), opts);

    // sibling_id is a self-referencing foreign key
    auto type = db.get_attribute_type("Child", "sibling_id");

    EXPECT_EQ(type.structure, psr::DataStructure::Scalar);
    EXPECT_EQ(type.data_type, psr::DataType::Integer);
}

// ============================================================================
// Schema loading and validation edge cases
// ============================================================================

TEST_F(SchemaValidatorFixture, CollectionWithOptionalScalars) {
    auto db = psr::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), opts);

    // Create element with only label (other scalars are optional)
    psr::Element config;
    config.set("label", std::string("Config"));
    db.create_element("Configuration", config);

    psr::Element e;
    e.set("label", std::string("Item 1"));
    int64_t id = db.create_element("Collection", e);

    EXPECT_GT(id, 0);

    // Read back - optional scalars should be null
    auto integers = db.read_scalar_integers("Collection", "some_integer");
    EXPECT_TRUE(integers.empty());  // NULL values are skipped
}

TEST_F(SchemaValidatorFixture, RelationsSchemaWithVectorFK) {
    auto db = psr::Database::from_schema(":memory:", VALID_SCHEMA("relations.sql"), opts);

    // Verify the schema loaded successfully with vector FK table
    auto type = db.get_attribute_type("Child", "label");
    EXPECT_EQ(type.data_type, psr::DataType::Text);
}

// ============================================================================
// Type validation edge cases with create_element
// ============================================================================

TEST_F(SchemaValidatorFixture, CreateElementWithDefaultValue) {
    auto db = psr::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), opts);

    // The basic.sql schema has integer_attribute with DEFAULT 6
    psr::Element e;
    e.set("label", std::string("Test"));
    int64_t id = db.create_element("Configuration", e);

    // Read back the default value
    auto val = db.read_scalar_integers_by_id("Configuration", "integer_attribute", id);
    EXPECT_TRUE(val.has_value());
    EXPECT_EQ(*val, 6);
}

TEST_F(SchemaValidatorFixture, CreateElementWithNullableColumn) {
    auto db = psr::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), opts);

    // float_attribute is nullable (no NOT NULL constraint)
    psr::Element e;
    e.set("label", std::string("Test")).set_null("float_attribute");
    int64_t id = db.create_element("Configuration", e);

    auto val = db.read_scalar_doubles_by_id("Configuration", "float_attribute", id);
    EXPECT_FALSE(val.has_value());  // NULL
}

// ============================================================================
// Multiple collections with different structures
// ============================================================================

TEST_F(SchemaValidatorFixture, ReadFromMultipleCollections) {
    auto db = psr::Database::from_schema(":memory:", VALID_SCHEMA("relations.sql"), opts);

    // Create elements in different collections
    psr::Element parent;
    parent.set("label", std::string("Parent 1"));
    db.create_element("Parent", parent);

    psr::Element child;
    child.set("label", std::string("Child 1"));
    db.create_element("Child", child);

    // Verify both can be read
    auto parent_labels = db.read_scalar_strings("Parent", "label");
    auto child_labels = db.read_scalar_strings("Child", "label");

    EXPECT_EQ(parent_labels.size(), 1);
    EXPECT_EQ(child_labels.size(), 1);
    EXPECT_EQ(parent_labels[0], "Parent 1");
    EXPECT_EQ(child_labels[0], "Child 1");
}

// ============================================================================
// get_attribute_type error cases
// ============================================================================

TEST_F(SchemaValidatorFixture, GetAttributeTypeIdColumn) {
    auto db = psr::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), opts);

    // 'id' column should exist and be INTEGER
    auto type = db.get_attribute_type("Configuration", "id");
    EXPECT_EQ(type.structure, psr::DataStructure::Scalar);
    EXPECT_EQ(type.data_type, psr::DataType::Integer);
}
