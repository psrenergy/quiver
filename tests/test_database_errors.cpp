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
// Note: read_vector_* methods without schema cause segfault (null pointer dereference)
// because impl_->schema->find_vector_table() is called without null check.
// These tests are skipped until the library adds proper null checks.
// ============================================================================

TEST(DatabaseErrors, ReadVectorIntegersCollectionNotFound) {
    auto db =
        psr::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = psr::LogLevel::off});

    // Create required Configuration
    psr::Element config;
    config.set("label", std::string("Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.read_vector_integers("NonexistentCollection", "value_int"), std::exception);
}

TEST(DatabaseErrors, ReadVectorDoublesCollectionNotFound) {
    auto db =
        psr::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = psr::LogLevel::off});

    psr::Element config;
    config.set("label", std::string("Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.read_vector_doubles("NonexistentCollection", "value_float"), std::exception);
}

// ============================================================================
// Read set error tests
// Note: read_set_* methods without schema cause segfault (null pointer dereference)
// because impl_->schema->find_set_table() is called without null check.
// These tests are skipped until the library adds proper null checks.
// ============================================================================

TEST(DatabaseErrors, ReadSetStringsCollectionNotFound) {
    auto db =
        psr::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = psr::LogLevel::off});

    psr::Element config;
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
// Note: update_vector_* methods without schema cause segfault (null pointer dereference)
// because impl_->schema->find_vector_table() is called without null check.
// These tests use a loaded schema and test collection-not-found instead.
// ============================================================================

TEST(DatabaseErrors, UpdateVectorIntegersCollectionNotFound) {
    auto db =
        psr::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = psr::LogLevel::off});

    psr::Element config;
    config.set("label", std::string("Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.update_vector_integers("NonexistentCollection", "value_int", 1, {1, 2, 3}), std::exception);
}

TEST(DatabaseErrors, UpdateVectorDoublesCollectionNotFound) {
    auto db =
        psr::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = psr::LogLevel::off});

    psr::Element config;
    config.set("label", std::string("Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.update_vector_doubles("NonexistentCollection", "value_float", 1, {1.5, 2.5}), std::exception);
}

// ============================================================================
// Update set error tests
// Note: update_set_* methods without schema cause segfault (null pointer dereference)
// because impl_->schema->find_set_table() is called without null check.
// These tests use a loaded schema and test collection-not-found instead.
// ============================================================================

TEST(DatabaseErrors, UpdateSetStringsCollectionNotFound) {
    auto db =
        psr::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = psr::LogLevel::off});

    psr::Element config;
    config.set("label", std::string("Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.update_set_strings("NonexistentCollection", "tag", 1, {"a", "b"}), std::exception);
}

// ============================================================================
// Read scalar with non-existent attribute tests
// ============================================================================

TEST(DatabaseErrors, ReadScalarIntegersAttributeNotFound) {
    auto db = psr::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = psr::LogLevel::off});

    psr::Element e;
    e.set("label", std::string("Test"));
    db.create_element("Configuration", e);

    // Reading non-existent column throws because SQL is invalid
    EXPECT_THROW(db.read_scalar_integers("Configuration", "nonexistent_attribute"), std::runtime_error);
}

TEST(DatabaseErrors, ReadScalarDoublesAttributeNotFound) {
    auto db = psr::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = psr::LogLevel::off});

    psr::Element e;
    e.set("label", std::string("Test"));
    db.create_element("Configuration", e);

    EXPECT_THROW(db.read_scalar_doubles("Configuration", "nonexistent_attribute"), std::runtime_error);
}

TEST(DatabaseErrors, ReadScalarStringsAttributeNotFound) {
    auto db = psr::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = psr::LogLevel::off});

    psr::Element e;
    e.set("label", std::string("Test"));
    db.create_element("Configuration", e);

    EXPECT_THROW(db.read_scalar_strings("Configuration", "nonexistent_attribute"), std::runtime_error);
}

// ============================================================================
// Read vector with non-existent attribute tests
// ============================================================================

TEST(DatabaseErrors, ReadVectorIntegersAttributeNotFound) {
    auto db =
        psr::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = psr::LogLevel::off});

    psr::Element config;
    config.set("label", std::string("Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.read_vector_integers("Collection", "nonexistent_attribute"), std::exception);
}

TEST(DatabaseErrors, ReadVectorDoublesAttributeNotFound) {
    auto db =
        psr::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = psr::LogLevel::off});

    psr::Element config;
    config.set("label", std::string("Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.read_vector_doubles("Collection", "nonexistent_attribute"), std::exception);
}

TEST(DatabaseErrors, ReadVectorStringsAttributeNotFound) {
    auto db =
        psr::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = psr::LogLevel::off});

    psr::Element config;
    config.set("label", std::string("Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.read_vector_strings("Collection", "nonexistent_attribute"), std::exception);
}

// ============================================================================
// Read set with non-existent attribute tests
// ============================================================================

TEST(DatabaseErrors, ReadSetIntegersAttributeNotFound) {
    auto db =
        psr::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = psr::LogLevel::off});

    psr::Element config;
    config.set("label", std::string("Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.read_set_integers("Collection", "nonexistent_attribute"), std::exception);
}

TEST(DatabaseErrors, ReadSetDoublesAttributeNotFound) {
    auto db =
        psr::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"), {.console_level = psr::LogLevel::off});

    psr::Element config;
    config.set("label", std::string("Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.read_set_doubles("Collection", "nonexistent_attribute"), std::exception);
}

// ============================================================================
// Schema file error tests
// ============================================================================

TEST(DatabaseErrors, ApplySchemaEmptyPath) {
    EXPECT_THROW(psr::Database::from_schema(":memory:", "", {.console_level = psr::LogLevel::off}), std::runtime_error);
}

TEST(DatabaseErrors, ApplySchemaFileNotFound) {
    EXPECT_THROW(
        psr::Database::from_schema(":memory:", "nonexistent/path/schema.sql", {.console_level = psr::LogLevel::off}),
        std::runtime_error);
}

// ============================================================================
// Update scalar with collection not found
// ============================================================================

TEST(DatabaseErrors, UpdateScalarIntegerCollectionNotFound) {
    auto db = psr::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = psr::LogLevel::off});

    EXPECT_THROW(db.update_scalar_integer("NonexistentCollection", "value", 1, 42), std::runtime_error);
}

TEST(DatabaseErrors, UpdateScalarDoubleCollectionNotFound) {
    auto db = psr::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = psr::LogLevel::off});

    EXPECT_THROW(db.update_scalar_double("NonexistentCollection", "value", 1, 3.14), std::runtime_error);
}

TEST(DatabaseErrors, UpdateScalarStringCollectionNotFound) {
    auto db = psr::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"), {.console_level = psr::LogLevel::off});

    EXPECT_THROW(db.update_scalar_string("NonexistentCollection", "value", 1, "test"), std::runtime_error);
}

// ============================================================================
// Read element IDs errors
// ============================================================================

TEST(DatabaseErrors, ReadElementIdsNoSchema) {
    psr::Database db(":memory:", {.console_level = psr::LogLevel::off});

    // Without schema, executing SQL will fail due to missing table
    EXPECT_THROW(db.read_element_ids("Configuration"), std::runtime_error);
}
