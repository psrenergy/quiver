#include "test_utils.h"

#include <gtest/gtest.h>
#include <psr/database.h>
#include <psr/element.h>

// ============================================================================
// No schema loaded error tests
// ============================================================================

TEST(DatabaseErrors, CreateElementNoSchema) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});

    psr::Element element;
    element.set("label", std::string("Test"));

    EXPECT_THROW(db.create_element("Configuration", element), std::runtime_error);
}

TEST(DatabaseErrors, CreateElementCollectionNotFound) {
    auto db = psr::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = psr::LogLevel::off});

    psr::Element element;
    element.set("label", std::string("Test"));

    EXPECT_THROW(db.create_element("NonexistentCollection", element), std::runtime_error);
}

TEST(DatabaseErrors, CreateElementEmptyElement) {
    auto db = psr::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = psr::LogLevel::off});

    psr::Element element;  // Empty element with no scalars

    EXPECT_THROW(db.create_element("Configuration", element), std::runtime_error);
}

TEST(DatabaseErrors, CreateElementEmptyArray) {
    auto db =
        psr::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = psr::LogLevel::off});

    // Create required Configuration first
    psr::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // Try to create element with empty array
    psr::Element element;
    element.set("label", std::string("Item 1")).set("value_int", std::vector<int64_t>{});

    EXPECT_THROW(db.create_element("Collection", element), std::runtime_error);
}

// ============================================================================
// Update error tests
// ============================================================================

TEST(DatabaseErrors, UpdateElementNoSchema) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});

    psr::Element element;
    element.set("label", std::string("Test"));

    EXPECT_THROW(db.update_element("Configuration", 1, element), std::runtime_error);
}

TEST(DatabaseErrors, UpdateElementCollectionNotFound) {
    auto db = psr::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = psr::LogLevel::off});

    psr::Element element;
    element.set("label", std::string("Test"));

    EXPECT_THROW(db.update_element("NonexistentCollection", 1, element), std::runtime_error);
}

TEST(DatabaseErrors, UpdateElementEmptyElement) {
    auto db = psr::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = psr::LogLevel::off});

    // Create an element first
    psr::Element original;
    original.set("label", std::string("Test"));
    int64_t id = db.create_element("Configuration", original);

    // Try to update with empty element
    psr::Element empty_element;

    EXPECT_THROW(db.update_element("Configuration", id, empty_element), std::runtime_error);
}

// ============================================================================
// Delete error tests
// ============================================================================

TEST(DatabaseErrors, DeleteElementNoSchema) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});

    EXPECT_THROW(db.delete_element_by_id("Configuration", 1), std::runtime_error);
}

TEST(DatabaseErrors, DeleteElementCollectionNotFound) {
    auto db = psr::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = psr::LogLevel::off});

    EXPECT_THROW(db.delete_element_by_id("NonexistentCollection", 1), std::runtime_error);
}

// ============================================================================
// Read scalar error tests (no schema)
// ============================================================================

TEST(DatabaseErrors, ReadScalarIntegersNoSchema) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});

    // Without schema, executing SQL directly will fail due to missing table
    EXPECT_THROW(db.read_scalar_integers("Configuration", "integer_attribute"), std::runtime_error);
}

TEST(DatabaseErrors, ReadScalarDoublesNoSchema) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});

    EXPECT_THROW(db.read_scalar_doubles("Configuration", "float_attribute"), std::runtime_error);
}

TEST(DatabaseErrors, ReadScalarStringsNoSchema) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});

    EXPECT_THROW(db.read_scalar_strings("Configuration", "label"), std::runtime_error);
}

// ============================================================================
// Read vector error tests
// ============================================================================

TEST(DatabaseErrors, ReadVectorIntegersNoSchemaLoaded) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});

    // This should throw because impl_->schema is null
    EXPECT_THROW(db.read_vector_integers("Collection", "value_int"), std::exception);
}

TEST(DatabaseErrors, ReadVectorDoublesNoSchemaLoaded) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});

    EXPECT_THROW(db.read_vector_doubles("Collection", "value_float"), std::exception);
}

TEST(DatabaseErrors, ReadVectorStringsNoSchemaLoaded) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});

    EXPECT_THROW(db.read_vector_strings("Collection", "some_string"), std::exception);
}

// ============================================================================
// Read set error tests
// ============================================================================

TEST(DatabaseErrors, ReadSetStringsNoSchemaLoaded) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});

    EXPECT_THROW(db.read_set_strings("Collection", "tag"), std::exception);
}

TEST(DatabaseErrors, ReadSetIntegersNoSchemaLoaded) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});

    EXPECT_THROW(db.read_set_integers("Collection", "some_set"), std::exception);
}

TEST(DatabaseErrors, ReadSetDoublesNoSchemaLoaded) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});

    EXPECT_THROW(db.read_set_doubles("Collection", "some_set"), std::exception);
}

// ============================================================================
// GetAttributeType error tests
// ============================================================================

TEST(DatabaseErrors, GetAttributeTypeNoSchema) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});

    // Without schema loaded, get_attribute_type should fail
    EXPECT_THROW(db.get_attribute_type("Configuration", "label"), std::exception);
}

TEST(DatabaseErrors, GetAttributeTypeCollectionNotFound) {
    auto db = psr::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = psr::LogLevel::off});

    EXPECT_THROW(db.get_attribute_type("NonexistentCollection", "label"), std::runtime_error);
}

TEST(DatabaseErrors, GetAttributeTypeAttributeNotFound) {
    auto db = psr::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = psr::LogLevel::off});

    EXPECT_THROW(db.get_attribute_type("Configuration", "nonexistent_attribute"), std::runtime_error);
}

// ============================================================================
// Relation error tests
// ============================================================================

TEST(DatabaseErrors, SetScalarRelationNoSchema) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});

    EXPECT_THROW(db.set_scalar_relation("Child", "parent_id", "Child 1", "Parent 1"), std::runtime_error);
}

TEST(DatabaseErrors, SetScalarRelationCollectionNotFound) {
    auto db =
        psr::Database::from_schema(":memory:", VALID_SCHEMA("relations.sql"), {.console_level = psr::LogLevel::off});

    EXPECT_THROW(db.set_scalar_relation("NonexistentCollection", "parent_id", "Child 1", "Parent 1"),
                 std::runtime_error);
}

TEST(DatabaseErrors, SetScalarRelationNotForeignKey) {
    auto db =
        psr::Database::from_schema(":memory:", VALID_SCHEMA("relations.sql"), {.console_level = psr::LogLevel::off});

    // 'label' is not a foreign key
    EXPECT_THROW(db.set_scalar_relation("Child", "label", "Child 1", "Parent 1"), std::runtime_error);
}

TEST(DatabaseErrors, SetScalarRelationTargetNotFound) {
    auto db =
        psr::Database::from_schema(":memory:", VALID_SCHEMA("relations.sql"), {.console_level = psr::LogLevel::off});

    // Create parent and child
    psr::Element parent;
    parent.set("label", std::string("Parent 1"));
    db.create_element("Parent", parent);

    psr::Element child;
    child.set("label", std::string("Child 1"));
    db.create_element("Child", child);

    // Try to set relation to nonexistent parent
    EXPECT_THROW(db.set_scalar_relation("Child", "parent_id", "Child 1", "Nonexistent Parent"), std::runtime_error);
}

TEST(DatabaseErrors, ReadScalarRelationNoSchema) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});

    EXPECT_THROW(db.read_scalar_relation("Child", "parent_id"), std::runtime_error);
}

TEST(DatabaseErrors, ReadScalarRelationCollectionNotFound) {
    auto db =
        psr::Database::from_schema(":memory:", VALID_SCHEMA("relations.sql"), {.console_level = psr::LogLevel::off});

    EXPECT_THROW(db.read_scalar_relation("NonexistentCollection", "parent_id"), std::runtime_error);
}

TEST(DatabaseErrors, ReadScalarRelationNotForeignKey) {
    auto db =
        psr::Database::from_schema(":memory:", VALID_SCHEMA("relations.sql"), {.console_level = psr::LogLevel::off});

    // 'label' is not a foreign key
    EXPECT_THROW(db.read_scalar_relation("Child", "label"), std::runtime_error);
}

// ============================================================================
// Update scalar error tests
// ============================================================================

TEST(DatabaseErrors, UpdateScalarIntegerNoSchema) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});

    EXPECT_THROW(db.update_scalar_integer("Configuration", "integer_attribute", 1, 42), std::exception);
}

TEST(DatabaseErrors, UpdateScalarDoubleNoSchema) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});

    EXPECT_THROW(db.update_scalar_double("Configuration", "float_attribute", 1, 3.14), std::exception);
}

TEST(DatabaseErrors, UpdateScalarStringNoSchema) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});

    EXPECT_THROW(db.update_scalar_string("Configuration", "label", 1, "new value"), std::exception);
}

// ============================================================================
// Update vector error tests
// ============================================================================

TEST(DatabaseErrors, UpdateVectorIntegersNoSchema) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});

    EXPECT_THROW(db.update_vector_integers("Collection", "value_int", 1, {1, 2, 3}), std::exception);
}

TEST(DatabaseErrors, UpdateVectorDoublesNoSchema) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});

    EXPECT_THROW(db.update_vector_doubles("Collection", "value_float", 1, {1.5, 2.5}), std::exception);
}

TEST(DatabaseErrors, UpdateVectorStringsNoSchema) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});

    EXPECT_THROW(db.update_vector_strings("Collection", "some_string", 1, {"a", "b"}), std::exception);
}

// ============================================================================
// Update set error tests
// ============================================================================

TEST(DatabaseErrors, UpdateSetIntegersNoSchema) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});

    EXPECT_THROW(db.update_set_integers("Collection", "some_set", 1, {1, 2, 3}), std::exception);
}

TEST(DatabaseErrors, UpdateSetDoublesNoSchema) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});

    EXPECT_THROW(db.update_set_doubles("Collection", "some_set", 1, {1.5, 2.5}), std::exception);
}

TEST(DatabaseErrors, UpdateSetStringsNoSchema) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});

    EXPECT_THROW(db.update_set_strings("Collection", "tag", 1, {"a", "b"}), std::exception);
}
