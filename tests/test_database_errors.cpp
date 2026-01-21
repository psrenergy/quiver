#include "test_utils.h"

#include <gtest/gtest.h>
#include <margaux/database.h>
#include <margaux/element.h>

// ============================================================================
// No schema loaded error tests
// ============================================================================

TEST(DatabaseErrors, CreateElementNoSchema) {
    margaux::Database db(":memory:", {.console_level = margaux::LogLevel::off});

    margaux::Element element;
    element.set("label", std::string("Test"));

    EXPECT_THROW(db.create_element("Configuration", element), std::runtime_error);
}

TEST(DatabaseErrors, CreateElementCollectionNotFound) {
    auto db = margaux::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = margaux::LogLevel::off});

    margaux::Element element;
    element.set("label", std::string("Test"));

    EXPECT_THROW(db.create_element("NonexistentCollection", element), std::runtime_error);
}

TEST(DatabaseErrors, CreateElementEmptyElement) {
    auto db = margaux::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = margaux::LogLevel::off});

    margaux::Element element;  // Empty element with no scalars

    EXPECT_THROW(db.create_element("Configuration", element), std::runtime_error);
}

TEST(DatabaseErrors, CreateElementEmptyArray) {
    auto db =
        margaux::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = margaux::LogLevel::off});

    // Create required Configuration first
    margaux::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // Try to create element with empty array
    margaux::Element element;
    element.set("label", std::string("Item 1")).set("value_int", std::vector<int64_t>{});

    EXPECT_THROW(db.create_element("Collection", element), std::runtime_error);
}

// ============================================================================
// Update error tests
// ============================================================================

TEST(DatabaseErrors, UpdateElementNoSchema) {
    margaux::Database db(":memory:", {.console_level = margaux::LogLevel::off});

    margaux::Element element;
    element.set("label", std::string("Test"));

    EXPECT_THROW(db.update_element("Configuration", 1, element), std::runtime_error);
}

TEST(DatabaseErrors, UpdateElementCollectionNotFound) {
    auto db = margaux::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = margaux::LogLevel::off});

    margaux::Element element;
    element.set("label", std::string("Test"));

    EXPECT_THROW(db.update_element("NonexistentCollection", 1, element), std::runtime_error);
}

TEST(DatabaseErrors, UpdateElementEmptyElement) {
    auto db = margaux::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = margaux::LogLevel::off});

    // Create an element first
    margaux::Element original;
    original.set("label", std::string("Test"));
    int64_t id = db.create_element("Configuration", original);

    // Try to update with empty element
    margaux::Element empty_element;

    EXPECT_THROW(db.update_element("Configuration", id, empty_element), std::runtime_error);
}

// ============================================================================
// Delete error tests
// ============================================================================

TEST(DatabaseErrors, DeleteElementNoSchema) {
    margaux::Database db(":memory:", {.console_level = margaux::LogLevel::off});

    EXPECT_THROW(db.delete_element_by_id("Configuration", 1), std::runtime_error);
}

TEST(DatabaseErrors, DeleteElementCollectionNotFound) {
    auto db = margaux::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = margaux::LogLevel::off});

    EXPECT_THROW(db.delete_element_by_id("NonexistentCollection", 1), std::runtime_error);
}

// ============================================================================
// Read scalar error tests (no schema)
// ============================================================================

TEST(DatabaseErrors, ReadScalarIntegersNoSchema) {
    margaux::Database db(":memory:", {.console_level = margaux::LogLevel::off});

    // Without schema, executing SQL directly will fail due to missing table
    EXPECT_THROW(db.read_scalar_integers("Configuration", "integer_attribute"), std::runtime_error);
}

TEST(DatabaseErrors, ReadScalarFloatsNoSchema) {
    margaux::Database db(":memory:", {.console_level = margaux::LogLevel::off});

    EXPECT_THROW(db.read_scalar_floats("Configuration", "float_attribute"), std::runtime_error);
}

TEST(DatabaseErrors, ReadScalarStringsNoSchema) {
    margaux::Database db(":memory:", {.console_level = margaux::LogLevel::off});

    EXPECT_THROW(db.read_scalar_strings("Configuration", "label"), std::runtime_error);
}

// ============================================================================
// Read vector error tests
// Note: read_vector_* methods without schema cause segfault (null pointer dereference)
// because impl_->schema->find_vector_table() is called without null check.
// These tests are skipped until the library adds proper null checks.
// ============================================================================

TEST(DatabaseErrors, ReadVectorIntegersCollectionNotFound) {
    auto db =
        margaux::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = margaux::LogLevel::off});

    // Create required Configuration
    margaux::Element config;
    config.set("label", std::string("Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.read_vector_integers("NonexistentCollection", "value_int"), std::exception);
}

TEST(DatabaseErrors, ReadVectorFloatsCollectionNotFound) {
    auto db =
        margaux::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = margaux::LogLevel::off});

    margaux::Element config;
    config.set("label", std::string("Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.read_vector_floats("NonexistentCollection", "value_float"), std::exception);
}

// ============================================================================
// Read set error tests
// Note: read_set_* methods without schema cause segfault (null pointer dereference)
// because impl_->schema->find_set_table() is called without null check.
// These tests are skipped until the library adds proper null checks.
// ============================================================================

TEST(DatabaseErrors, ReadSetStringsCollectionNotFound) {
    auto db =
        margaux::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = margaux::LogLevel::off});

    margaux::Element config;
    config.set("label", std::string("Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.read_set_strings("NonexistentCollection", "tag"), std::exception);
}

// ============================================================================
// GetAttributeType error tests
// Note: get_attribute_type without schema causes segfault (null pointer dereference)
// because impl_->schema is dereferenced without null check.
// ============================================================================

TEST(DatabaseErrors, GetAttributeTypeCollectionNotFound) {
    auto db = margaux::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = margaux::LogLevel::off});

    EXPECT_THROW(db.get_attribute_type("NonexistentCollection", "label"), std::runtime_error);
}

TEST(DatabaseErrors, GetAttributeTypeAttributeNotFound) {
    auto db = margaux::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = margaux::LogLevel::off});

    EXPECT_THROW(db.get_attribute_type("Configuration", "nonexistent_attribute"), std::runtime_error);
}

// ============================================================================
// Relation error tests
// ============================================================================

TEST(DatabaseErrors, SetScalarRelationNoSchema) {
    margaux::Database db(":memory:", {.console_level = margaux::LogLevel::off});

    EXPECT_THROW(db.set_scalar_relation("Child", "parent_id", "Child 1", "Parent 1"), std::runtime_error);
}

TEST(DatabaseErrors, SetScalarRelationCollectionNotFound) {
    auto db =
        margaux::Database::from_schema(":memory:", VALID_SCHEMA("relations.sql"), {.console_level = margaux::LogLevel::off});

    EXPECT_THROW(db.set_scalar_relation("NonexistentCollection", "parent_id", "Child 1", "Parent 1"),
                 std::runtime_error);
}

TEST(DatabaseErrors, SetScalarRelationNotForeignKey) {
    auto db =
        margaux::Database::from_schema(":memory:", VALID_SCHEMA("relations.sql"), {.console_level = margaux::LogLevel::off});

    // 'label' is not a foreign key
    EXPECT_THROW(db.set_scalar_relation("Child", "label", "Child 1", "Parent 1"), std::runtime_error);
}

TEST(DatabaseErrors, SetScalarRelationTargetNotFound) {
    auto db =
        margaux::Database::from_schema(":memory:", VALID_SCHEMA("relations.sql"), {.console_level = margaux::LogLevel::off});

    // Create parent and child
    margaux::Element parent;
    parent.set("label", std::string("Parent 1"));
    db.create_element("Parent", parent);

    margaux::Element child;
    child.set("label", std::string("Child 1"));
    db.create_element("Child", child);

    // Try to set relation to nonexistent parent
    EXPECT_THROW(db.set_scalar_relation("Child", "parent_id", "Child 1", "Nonexistent Parent"), std::runtime_error);
}

TEST(DatabaseErrors, ReadScalarRelationNoSchema) {
    margaux::Database db(":memory:", {.console_level = margaux::LogLevel::off});

    EXPECT_THROW(db.read_scalar_relation("Child", "parent_id"), std::runtime_error);
}

TEST(DatabaseErrors, ReadScalarRelationCollectionNotFound) {
    auto db =
        margaux::Database::from_schema(":memory:", VALID_SCHEMA("relations.sql"), {.console_level = margaux::LogLevel::off});

    EXPECT_THROW(db.read_scalar_relation("NonexistentCollection", "parent_id"), std::runtime_error);
}

TEST(DatabaseErrors, ReadScalarRelationNotForeignKey) {
    auto db =
        margaux::Database::from_schema(":memory:", VALID_SCHEMA("relations.sql"), {.console_level = margaux::LogLevel::off});

    // 'label' is not a foreign key
    EXPECT_THROW(db.read_scalar_relation("Child", "label"), std::runtime_error);
}

// ============================================================================
// Update scalar error tests
// ============================================================================

TEST(DatabaseErrors, UpdateScalarIntegerNoSchema) {
    margaux::Database db(":memory:", {.console_level = margaux::LogLevel::off});

    EXPECT_THROW(db.update_scalar_integer("Configuration", "integer_attribute", 1, 42), std::exception);
}

TEST(DatabaseErrors, UpdateScalarFloatNoSchema) {
    margaux::Database db(":memory:", {.console_level = margaux::LogLevel::off});

    EXPECT_THROW(db.update_scalar_float("Configuration", "float_attribute", 1, 3.14), std::exception);
}

TEST(DatabaseErrors, UpdateScalarStringNoSchema) {
    margaux::Database db(":memory:", {.console_level = margaux::LogLevel::off});

    EXPECT_THROW(db.update_scalar_string("Configuration", "label", 1, "new value"), std::exception);
}

// ============================================================================
// Update vector error tests
// Note: update_vector_* methods without schema cause segfault (null pointer dereference)
// because impl_->schema->find_vector_table() is called without null check.
// These tests use a loaded schema and test collection-not-found instead.
// ============================================================================

TEST(DatabaseErrors, UpdateVectorIntegersCollectionNotFound) {
    auto db =
        margaux::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = margaux::LogLevel::off});

    margaux::Element config;
    config.set("label", std::string("Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.update_vector_integers("NonexistentCollection", "value_int", 1, {1, 2, 3}), std::exception);
}

TEST(DatabaseErrors, UpdateVectorFloatsCollectionNotFound) {
    auto db =
        margaux::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = margaux::LogLevel::off});

    margaux::Element config;
    config.set("label", std::string("Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.update_vector_floats("NonexistentCollection", "value_float", 1, {1.5, 2.5}), std::exception);
}

// ============================================================================
// Update set error tests
// Note: update_set_* methods without schema cause segfault (null pointer dereference)
// because impl_->schema->find_set_table() is called without null check.
// These tests use a loaded schema and test collection-not-found instead.
// ============================================================================

TEST(DatabaseErrors, UpdateSetStringsCollectionNotFound) {
    auto db =
        margaux::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = margaux::LogLevel::off});

    margaux::Element config;
    config.set("label", std::string("Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.update_set_strings("NonexistentCollection", "tag", 1, {"a", "b"}), std::exception);
}

// ============================================================================
// Read scalar with non-existent attribute tests
// ============================================================================

TEST(DatabaseErrors, ReadScalarIntegersAttributeNotFound) {
    auto db = margaux::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = margaux::LogLevel::off});

    margaux::Element e;
    e.set("label", std::string("Test"));
    db.create_element("Configuration", e);

    // Reading non-existent column throws because SQL is invalid
    EXPECT_THROW(db.read_scalar_integers("Configuration", "nonexistent_attribute"), std::runtime_error);
}

TEST(DatabaseErrors, ReadScalarFloatsAttributeNotFound) {
    auto db = margaux::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = margaux::LogLevel::off});

    margaux::Element e;
    e.set("label", std::string("Test"));
    db.create_element("Configuration", e);

    EXPECT_THROW(db.read_scalar_floats("Configuration", "nonexistent_attribute"), std::runtime_error);
}

TEST(DatabaseErrors, ReadScalarStringsAttributeNotFound) {
    auto db = margaux::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = margaux::LogLevel::off});

    margaux::Element e;
    e.set("label", std::string("Test"));
    db.create_element("Configuration", e);

    EXPECT_THROW(db.read_scalar_strings("Configuration", "nonexistent_attribute"), std::runtime_error);
}

// ============================================================================
// Read vector with non-existent attribute tests
// ============================================================================

TEST(DatabaseErrors, ReadVectorIntegersAttributeNotFound) {
    auto db =
        margaux::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = margaux::LogLevel::off});

    margaux::Element config;
    config.set("label", std::string("Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.read_vector_integers("Collection", "nonexistent_attribute"), std::exception);
}

TEST(DatabaseErrors, ReadVectorFloatsAttributeNotFound) {
    auto db =
        margaux::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = margaux::LogLevel::off});

    margaux::Element config;
    config.set("label", std::string("Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.read_vector_floats("Collection", "nonexistent_attribute"), std::exception);
}

TEST(DatabaseErrors, ReadVectorStringsAttributeNotFound) {
    auto db =
        margaux::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = margaux::LogLevel::off});

    margaux::Element config;
    config.set("label", std::string("Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.read_vector_strings("Collection", "nonexistent_attribute"), std::exception);
}

// ============================================================================
// Read set with non-existent attribute tests
// ============================================================================

TEST(DatabaseErrors, ReadSetIntegersAttributeNotFound) {
    auto db =
        margaux::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = margaux::LogLevel::off});

    margaux::Element config;
    config.set("label", std::string("Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.read_set_integers("Collection", "nonexistent_attribute"), std::exception);
}

TEST(DatabaseErrors, ReadSetFloatsAttributeNotFound) {
    auto db =
        margaux::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = margaux::LogLevel::off});

    margaux::Element config;
    config.set("label", std::string("Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.read_set_floats("Collection", "nonexistent_attribute"), std::exception);
}

// ============================================================================
// Schema file error tests
// ============================================================================

TEST(DatabaseErrors, ApplySchemaEmptyPath) {
    EXPECT_THROW(margaux::Database::from_schema(":memory:", "", {.console_level = margaux::LogLevel::off}), std::runtime_error);
}

TEST(DatabaseErrors, ApplySchemaFileNotFound) {
    EXPECT_THROW(
        margaux::Database::from_schema(":memory:", "nonexistent/path/schema.sql", {.console_level = margaux::LogLevel::off}),
        std::runtime_error);
}

// ============================================================================
// Update scalar with collection not found
// ============================================================================

TEST(DatabaseErrors, UpdateScalarIntegerCollectionNotFound) {
    auto db = margaux::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = margaux::LogLevel::off});

    EXPECT_THROW(db.update_scalar_integer("NonexistentCollection", "value", 1, 42), std::runtime_error);
}

TEST(DatabaseErrors, UpdateScalarFloatCollectionNotFound) {
    auto db = margaux::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = margaux::LogLevel::off});

    EXPECT_THROW(db.update_scalar_float("NonexistentCollection", "value", 1, 3.14), std::runtime_error);
}

TEST(DatabaseErrors, UpdateScalarStringCollectionNotFound) {
    auto db = margaux::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = margaux::LogLevel::off});

    EXPECT_THROW(db.update_scalar_string("NonexistentCollection", "value", 1, "test"), std::runtime_error);
}

// ============================================================================
// Read element IDs errors
// ============================================================================

TEST(DatabaseErrors, ReadElementIdsNoSchema) {
    margaux::Database db(":memory:", {.console_level = margaux::LogLevel::off});

    // Without schema, executing SQL will fail due to missing table
    EXPECT_THROW(db.read_element_ids("Configuration"), std::runtime_error);
}
