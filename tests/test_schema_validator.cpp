#include "test_utils.h"

#include <gtest/gtest.h>
#include <quiver/database.h>

class SchemaValidatorFixture : public ::testing::Test {
protected:
    quiver::DatabaseOptions opts{.read_only = 0, .console_level = QUIVER_LOG_OFF};
};

// Valid schemas
TEST_F(SchemaValidatorFixture, ValidSchemaBasic) {
    EXPECT_NO_THROW(quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), opts));
}

TEST_F(SchemaValidatorFixture, ValidSchemaCollections) {
    EXPECT_NO_THROW(quiver::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), opts));
}

TEST_F(SchemaValidatorFixture, ValidSchemaRelations) {
    EXPECT_NO_THROW(quiver::Database::from_schema(":memory:", VALID_SCHEMA("relations.sql"), opts));
}

// Invalid schemas
TEST_F(SchemaValidatorFixture, InvalidNoConfiguration) {
    EXPECT_THROW(quiver::Database::from_schema(":memory:", INVALID_SCHEMA("no_configuration.sql"), opts),
                 std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidLabelNotNull) {
    EXPECT_THROW(quiver::Database::from_schema(":memory:", INVALID_SCHEMA("label_not_null.sql"), opts),
                 std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidLabelNotUnique) {
    EXPECT_THROW(quiver::Database::from_schema(":memory:", INVALID_SCHEMA("label_not_unique.sql"), opts),
                 std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidLabelWrongType) {
    EXPECT_THROW(quiver::Database::from_schema(":memory:", INVALID_SCHEMA("label_wrong_type.sql"), opts),
                 std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidDuplicateAttribute) {
    EXPECT_THROW(quiver::Database::from_schema(":memory:", INVALID_SCHEMA("duplicate_attribute_vector.sql"), opts),
                 std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidDuplicateAttributeTimeSeries) {
    EXPECT_THROW(quiver::Database::from_schema(":memory:", INVALID_SCHEMA("duplicate_attribute_time_series.sql"), opts),
                 std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidVectorNoIndex) {
    EXPECT_THROW(quiver::Database::from_schema(":memory:", INVALID_SCHEMA("vector_no_index.sql"), opts),
                 std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidSetNoUnique) {
    EXPECT_THROW(quiver::Database::from_schema(":memory:", INVALID_SCHEMA("set_no_unique.sql"), opts),
                 std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidFkNotNullSetNull) {
    EXPECT_THROW(quiver::Database::from_schema(":memory:", INVALID_SCHEMA("fk_not_null_set_null.sql"), opts),
                 std::runtime_error);
}

TEST_F(SchemaValidatorFixture, InvalidFkActions) {
    EXPECT_THROW(quiver::Database::from_schema(":memory:", INVALID_SCHEMA("fk_actions.sql"), opts), std::runtime_error);
}

// ============================================================================
// Type validation tests (via create_element errors)
// ============================================================================

TEST_F(SchemaValidatorFixture, TypeMismatchIntegerExpectedReal) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), opts);

    // Try to set a string where a float is expected
    quiver::Element e;
    e.set("label", std::string("Test")).set("float_attribute", std::string("not a float"));

    EXPECT_THROW(db.create_element("Configuration", e), std::runtime_error);
}

TEST_F(SchemaValidatorFixture, TypeMismatchStringExpectedInteger) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), opts);

    // Try to set a string where an integer is expected
    quiver::Element e;
    e.set("label", std::string("Test")).set("integer_attribute", std::string("not an integer"));

    EXPECT_THROW(db.create_element("Configuration", e), std::runtime_error);
}

TEST_F(SchemaValidatorFixture, TypeMismatchIntegerExpectedText) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), opts);

    // Try to set an integer where a string is expected
    quiver::Element e;
    e.set("label", int64_t{42});  // label expects TEXT

    EXPECT_THROW(db.create_element("Configuration", e), std::runtime_error);
}

TEST_F(SchemaValidatorFixture, ArrayTypeValidation) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), opts);

    quiver::Element config;
    config.set("label", std::string("Config"));
    db.create_element("Configuration", config);

    // Try to create element with wrong array type
    quiver::Element e;
    e.set("label", std::string("Item 1"));
    // value_int expects integers, but we'll try to pass strings
    // Note: This depends on how the Element class handles type conversion
    e.set("value_int", std::vector<std::string>{"not", "integers"});

    EXPECT_THROW(db.create_element("Collection", e), std::runtime_error);
}

// ============================================================================
// Schema attribute lookup tests (using metadata API)
// ============================================================================

TEST_F(SchemaValidatorFixture, GetScalarMetadataInteger) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), opts);

    auto metadata = db.get_scalar_metadata("Configuration", "integer_attribute");

    EXPECT_EQ(metadata.name, "integer_attribute");
    EXPECT_EQ(metadata.data_type, quiver::DataType::Integer);
}

TEST_F(SchemaValidatorFixture, GetScalarMetadataReal) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), opts);

    auto metadata = db.get_scalar_metadata("Configuration", "float_attribute");

    EXPECT_EQ(metadata.name, "float_attribute");
    EXPECT_EQ(metadata.data_type, quiver::DataType::Real);
}

TEST_F(SchemaValidatorFixture, GetScalarMetadataText) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), opts);

    auto metadata = db.get_scalar_metadata("Configuration", "label");

    EXPECT_EQ(metadata.name, "label");
    EXPECT_EQ(metadata.data_type, quiver::DataType::Text);
}

TEST_F(SchemaValidatorFixture, GetVectorMetadataValues) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), opts);

    // Vector table is Collection_vector_values, so group name is "values"
    auto metadata = db.get_vector_metadata("Collection", "values");

    EXPECT_EQ(metadata.group_name, "values");
    EXPECT_EQ(metadata.value_columns.size(), 2);  // value_int and value_float columns

    // Check both columns exist with correct types (order may vary)
    bool found_int = false, found_float = false;
    for (const auto& col : metadata.value_columns) {
        if (col.name == "value_int") {
            EXPECT_EQ(col.data_type, quiver::DataType::Integer);
            found_int = true;
        } else if (col.name == "value_float") {
            EXPECT_EQ(col.data_type, quiver::DataType::Real);
            found_float = true;
        }
    }
    EXPECT_TRUE(found_int);
    EXPECT_TRUE(found_float);
}

TEST_F(SchemaValidatorFixture, GetSetMetadataTags) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), opts);

    // Set table is Collection_set_tags, so group name is "tags"
    auto metadata = db.get_set_metadata("Collection", "tags");

    EXPECT_EQ(metadata.group_name, "tags");
    EXPECT_FALSE(metadata.value_columns.empty());
    EXPECT_EQ(metadata.value_columns[0].name, "tag");
    EXPECT_EQ(metadata.value_columns[0].data_type, quiver::DataType::Text);
}

TEST_F(SchemaValidatorFixture, GetScalarMetadataForeignKeyAsInteger) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("relations.sql"), opts);

    // parent_id is a foreign key but stored as INTEGER
    auto metadata = db.get_scalar_metadata("Child", "parent_id");

    EXPECT_EQ(metadata.name, "parent_id");
    EXPECT_EQ(metadata.data_type, quiver::DataType::Integer);
    EXPECT_TRUE(metadata.is_foreign_key);
    EXPECT_EQ(metadata.references_collection, "Parent");
    EXPECT_EQ(metadata.references_column, "id");
}

TEST_F(SchemaValidatorFixture, GetScalarMetadataSelfReference) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("relations.sql"), opts);

    // sibling_id is a self-referencing foreign key
    auto metadata = db.get_scalar_metadata("Child", "sibling_id");

    EXPECT_EQ(metadata.name, "sibling_id");
    EXPECT_EQ(metadata.data_type, quiver::DataType::Integer);
    EXPECT_TRUE(metadata.is_foreign_key);
    EXPECT_EQ(metadata.references_collection, "Child");
    EXPECT_EQ(metadata.references_column, "id");
}

TEST_F(SchemaValidatorFixture, GetScalarMetadataNonForeignKey) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("relations.sql"), opts);

    auto metadata = db.get_scalar_metadata("Child", "label");

    EXPECT_FALSE(metadata.is_foreign_key);
    EXPECT_FALSE(metadata.references_collection.has_value());
    EXPECT_FALSE(metadata.references_column.has_value());
}

TEST_F(SchemaValidatorFixture, ListScalarAttributesForeignKeys) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("relations.sql"), opts);

    auto attributes = db.list_scalar_attributes("Child");

    // Find parent_id
    auto it = std::find_if(attributes.begin(), attributes.end(), [](const auto& a) { return a.name == "parent_id"; });
    ASSERT_NE(it, attributes.end());
    EXPECT_TRUE(it->is_foreign_key);
    EXPECT_EQ(it->references_collection, "Parent");
    EXPECT_EQ(it->references_column, "id");

    // Find label (not a FK)
    it = std::find_if(attributes.begin(), attributes.end(), [](const auto& a) { return a.name == "label"; });
    ASSERT_NE(it, attributes.end());
    EXPECT_FALSE(it->is_foreign_key);
    EXPECT_FALSE(it->references_collection.has_value());
}

// ============================================================================
// Schema loading and validation edge cases
// ============================================================================

TEST_F(SchemaValidatorFixture, CollectionWithOptionalScalars) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), opts);

    // Create element with only label (other scalars are optional)
    quiver::Element config;
    config.set("label", std::string("Config"));
    db.create_element("Configuration", config);

    quiver::Element e;
    e.set("label", std::string("Item 1"));
    int64_t id = db.create_element("Collection", e);

    EXPECT_GT(id, 0);

    // Read back - optional scalars should be null
    auto integers = db.read_scalar_integers("Collection", "some_integer");
    EXPECT_TRUE(integers.empty());  // NULL values are skipped
}

TEST_F(SchemaValidatorFixture, RelationsSchemaWithVectorFK) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("relations.sql"), opts);

    // Verify the schema loaded successfully with vector FK table
    auto metadata = db.get_scalar_metadata("Child", "label");
    EXPECT_EQ(metadata.data_type, quiver::DataType::Text);
}

// ============================================================================
// Type validation edge cases with create_element
// ============================================================================

TEST_F(SchemaValidatorFixture, CreateElementWithDefaultValue) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), opts);

    // The basic.sql schema has integer_attribute with DEFAULT 6
    quiver::Element e;
    e.set("label", std::string("Test"));
    int64_t id = db.create_element("Configuration", e);

    // Read back the default value
    auto val = db.read_scalar_integer_by_id("Configuration", "integer_attribute", id);
    EXPECT_TRUE(val.has_value());
    EXPECT_EQ(*val, 6);
}

TEST_F(SchemaValidatorFixture, CreateElementWithNullableColumn) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), opts);

    // float_attribute is nullable (no NOT NULL constraint)
    quiver::Element e;
    e.set("label", std::string("Test")).set_null("float_attribute");
    int64_t id = db.create_element("Configuration", e);

    auto val = db.read_scalar_float_by_id("Configuration", "float_attribute", id);
    EXPECT_FALSE(val.has_value());  // NULL
}

// ============================================================================
// Multiple collections with different structures
// ============================================================================

TEST_F(SchemaValidatorFixture, ReadFromMultipleCollections) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("relations.sql"), opts);

    // Create elements in different collections
    quiver::Element parent;
    parent.set("label", std::string("Parent 1"));
    db.create_element("Parent", parent);

    quiver::Element child;
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
// Metadata edge cases
// ============================================================================

TEST_F(SchemaValidatorFixture, GetScalarMetadataIdColumn) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), opts);

    // 'id' column should exist and be INTEGER
    auto metadata = db.get_scalar_metadata("Configuration", "id");
    EXPECT_EQ(metadata.name, "id");
    EXPECT_EQ(metadata.data_type, quiver::DataType::Integer);
}
