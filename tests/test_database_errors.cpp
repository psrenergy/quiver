#include "test_utils.h"

#include <gtest/gtest.h>
#include <quiver/database.h>
#include <quiver/element.h>

// ============================================================================
// No schema loaded error tests
// ============================================================================

TEST(DatabaseErrors, CreateElementNoSchema) {
    quiver::Database db(":memory:", {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element element;
    element.set("label", std::string("Test"));

    EXPECT_THROW(db.create_element("Configuration", element), std::runtime_error);
}

TEST(DatabaseErrors, CreateElementCollectionNotFound) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element element;
    element.set("label", std::string("Test"));

    EXPECT_THROW(db.create_element("NonexistentCollection", element), std::runtime_error);
}

TEST(DatabaseErrors, CreateElementEmptyElement) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element element;  // Empty element with no scalars

    EXPECT_THROW(db.create_element("Configuration", element), std::runtime_error);
}

TEST(DatabaseErrors, CreateElementEmptyArray) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    // Create required Configuration first
    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // Try to create element with empty array
    quiver::Element element;
    element.set("label", std::string("Item 1")).set("value_int", std::vector<int64_t>{});

    EXPECT_THROW(db.create_element("Collection", element), std::runtime_error);
}

// ============================================================================
// Update error tests
// ============================================================================

TEST(DatabaseErrors, UpdateElementNoSchema) {
    quiver::Database db(":memory:", {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element element;
    element.set("label", std::string("Test"));

    EXPECT_THROW(db.update_element("Configuration", 1, element), std::runtime_error);
}

TEST(DatabaseErrors, UpdateElementCollectionNotFound) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element element;
    element.set("label", std::string("Test"));

    EXPECT_THROW(db.update_element("NonexistentCollection", 1, element), std::runtime_error);
}

TEST(DatabaseErrors, UpdateElementEmptyElement) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    // Create an element first
    quiver::Element original;
    original.set("label", std::string("Test"));
    int64_t id = db.create_element("Configuration", original);

    // Try to update with empty element
    quiver::Element empty_element;

    EXPECT_THROW(db.update_element("Configuration", id, empty_element), std::runtime_error);
}

// ============================================================================
// Delete error tests
// ============================================================================

TEST(DatabaseErrors, DeleteElementNoSchema) {
    quiver::Database db(":memory:", {.read_only = false, .console_level = quiver::LogLevel::Off});

    EXPECT_THROW(db.delete_element("Configuration", 1), std::runtime_error);
}

TEST(DatabaseErrors, DeleteElementCollectionNotFound) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    EXPECT_THROW(db.delete_element("NonexistentCollection", 1), std::runtime_error);
}

// ============================================================================
// Read scalar error tests (no schema)
// ============================================================================

TEST(DatabaseErrors, ReadScalarIntegersNoSchema) {
    quiver::Database db(":memory:", {.read_only = false, .console_level = quiver::LogLevel::Off});

    // Without schema, executing SQL directly will fail due to missing table
    EXPECT_THROW(db.read_scalar_integers("Configuration", "integer_attribute"), std::runtime_error);
}

TEST(DatabaseErrors, ReadScalarFloatsNoSchema) {
    quiver::Database db(":memory:", {.read_only = false, .console_level = quiver::LogLevel::Off});

    EXPECT_THROW(db.read_scalar_floats("Configuration", "float_attribute"), std::runtime_error);
}

TEST(DatabaseErrors, ReadScalarStringsNoSchema) {
    quiver::Database db(":memory:", {.read_only = false, .console_level = quiver::LogLevel::Off});

    EXPECT_THROW(db.read_scalar_strings("Configuration", "label"), std::runtime_error);
}

// ============================================================================
// Read vector error tests
// Note: read_vector_* methods without schema cause segfault (null pointer dereference)
// because impl_->schema->find_vector_table() is called without null check.
// These tests are skipped until the library adds proper null checks.
// ============================================================================

TEST(DatabaseErrors, ReadVectorIntegersCollectionNotFound) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    // Create required Configuration
    quiver::Element config;
    config.set("label", std::string("Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.read_vector_integers("NonexistentCollection", "value_int"), std::exception);
}

TEST(DatabaseErrors, ReadVectorFloatsCollectionNotFound) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
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
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.read_set_strings("NonexistentCollection", "tag"), std::exception);
}

// ============================================================================
// Update vector error tests
// Note: update_vector_* methods without schema cause segfault (null pointer dereference)
// because impl_->schema->find_vector_table() is called without null check.
// These tests use a loaded schema and test collection-not-found instead.
// ============================================================================

TEST(DatabaseErrors, UpdateVectorIntegersCollectionNotFound) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.update_element(
                     "NonexistentCollection", 1, quiver::Element().set("value_int", std::vector<int64_t>{1, 2, 3})),
                 std::exception);
}

TEST(DatabaseErrors, UpdateVectorFloatsCollectionNotFound) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.update_element(
                     "NonexistentCollection", 1, quiver::Element().set("value_float", std::vector<double>{1.5, 2.5})),
                 std::exception);
}

// ============================================================================
// Update set error tests
// Note: update_element method with non-existent collection throws std::exception
// because impl_->schema->find_set_table() is called.
// These tests use a loaded schema and test collection-not-found instead.
// ============================================================================

TEST(DatabaseErrors, UpdateSetStringsCollectionNotFound) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(
        db.update_element("NonexistentCollection", 1, quiver::Element().set("tag", std::vector<std::string>{"a", "b"})),
        std::exception);
}

// ============================================================================
// Read scalar with non-existent attribute tests
// ============================================================================

TEST(DatabaseErrors, ReadScalarIntegersAttributeNotFound) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element e;
    e.set("label", std::string("Test"));
    db.create_element("Configuration", e);

    // Reading non-existent column throws because SQL is invalid
    EXPECT_THROW(db.read_scalar_integers("Configuration", "nonexistent_attribute"), std::runtime_error);
}

TEST(DatabaseErrors, ReadScalarFloatsAttributeNotFound) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element e;
    e.set("label", std::string("Test"));
    db.create_element("Configuration", e);

    EXPECT_THROW(db.read_scalar_floats("Configuration", "nonexistent_attribute"), std::runtime_error);
}

TEST(DatabaseErrors, ReadScalarStringsAttributeNotFound) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element e;
    e.set("label", std::string("Test"));
    db.create_element("Configuration", e);

    EXPECT_THROW(db.read_scalar_strings("Configuration", "nonexistent_attribute"), std::runtime_error);
}

// ============================================================================
// Read vector with non-existent attribute tests
// ============================================================================

TEST(DatabaseErrors, ReadVectorIntegersAttributeNotFound) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.read_vector_integers("Collection", "nonexistent_attribute"), std::exception);
}

TEST(DatabaseErrors, ReadVectorFloatsAttributeNotFound) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.read_vector_floats("Collection", "nonexistent_attribute"), std::exception);
}

TEST(DatabaseErrors, ReadVectorStringsAttributeNotFound) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.read_vector_strings("Collection", "nonexistent_attribute"), std::exception);
}

// ============================================================================
// Read set with non-existent attribute tests
// ============================================================================

TEST(DatabaseErrors, ReadSetIntegersAttributeNotFound) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.read_set_integers("Collection", "nonexistent_attribute"), std::exception);
}

TEST(DatabaseErrors, ReadSetFloatsAttributeNotFound) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.read_set_floats("Collection", "nonexistent_attribute"), std::exception);
}

// ============================================================================
// Schema file error tests
// ============================================================================

TEST(DatabaseErrors, ApplySchemaEmptyPath) {
    EXPECT_THROW(
        quiver::Database::from_schema(":memory:", "", {.read_only = false, .console_level = quiver::LogLevel::Off}),
        std::runtime_error);
}

TEST(DatabaseErrors, ApplySchemaFileNotFound) {
    EXPECT_THROW(
        quiver::Database::from_schema(
            ":memory:", "nonexistent/path/schema.sql", {.read_only = false, .console_level = quiver::LogLevel::Off}),
        std::runtime_error);
}

// ============================================================================
// Read element Ids errors
// ============================================================================

TEST(DatabaseErrors, ReadElementIdsNoSchema) {
    quiver::Database db(":memory:", {.read_only = false, .console_level = quiver::LogLevel::Off});

    // Without schema, executing SQL will fail due to missing table
    EXPECT_THROW(db.read_element_ids("Configuration"), std::runtime_error);
}
