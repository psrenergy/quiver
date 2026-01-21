#include "test_utils.h"

#include <gtest/gtest.h>
#include <margaux/database.h>
#include <margaux/element.h>

TEST(Database, SetScalarRelation) {
    auto db =
        margaux::Database::from_schema(":memory:", VALID_SCHEMA("relations.sql"), {.console_level = margaux::LogLevel::off});

    // Create parent
    margaux::Element parent;
    parent.set("label", std::string("Parent 1"));
    db.create_element("Parent", parent);

    // Create child without relation
    margaux::Element child;
    child.set("label", std::string("Child 1"));
    db.create_element("Child", child);

    // Set the relation
    db.set_scalar_relation("Child", "parent_id", "Child 1", "Parent 1");

    // Verify the relation was set using public read API
    auto relations = db.read_scalar_relation("Child", "parent_id");
    EXPECT_EQ(relations.size(), 1);
    EXPECT_EQ(relations[0], "Parent 1");
}

TEST(Database, SetScalarRelationSelfReference) {
    auto db =
        margaux::Database::from_schema(":memory:", VALID_SCHEMA("relations.sql"), {.console_level = margaux::LogLevel::off});

    // Create two children
    margaux::Element child1;
    child1.set("label", std::string("Child 1"));
    db.create_element("Child", child1);

    margaux::Element child2;
    child2.set("label", std::string("Child 2"));
    db.create_element("Child", child2);

    // Set self-referential relation (sibling)
    db.set_scalar_relation("Child", "sibling_id", "Child 1", "Child 2");

    // Verify the relation was set using public read API
    auto relations = db.read_scalar_relation("Child", "sibling_id");
    EXPECT_EQ(relations.size(), 2);
    // Child 1 has sibling_id pointing to Child 2, Child 2 has no sibling
    EXPECT_EQ(relations[0], "Child 2");
    EXPECT_EQ(relations[1], "");  // Child 2 has no sibling set
}

// ============================================================================
// Read scalar relation edge cases
// ============================================================================

TEST(Database, ReadScalarRelationWithNulls) {
    auto db =
        margaux::Database::from_schema(":memory:", VALID_SCHEMA("relations.sql"), {.console_level = margaux::LogLevel::off});

    // Create parent
    margaux::Element parent;
    parent.set("label", std::string("Parent 1"));
    db.create_element("Parent", parent);

    // Create children without setting parent_id relation
    margaux::Element child1;
    child1.set("label", std::string("Child 1"));
    db.create_element("Child", child1);

    margaux::Element child2;
    child2.set("label", std::string("Child 2"));
    db.create_element("Child", child2);

    // Read relations - should return empty strings for unset relations
    auto relations = db.read_scalar_relation("Child", "parent_id");
    EXPECT_EQ(relations.size(), 2);
    EXPECT_EQ(relations[0], "");  // NULL parent
    EXPECT_EQ(relations[1], "");  // NULL parent
}

TEST(Database, ReadScalarRelationMixedNullsAndValues) {
    auto db =
        margaux::Database::from_schema(":memory:", VALID_SCHEMA("relations.sql"), {.console_level = margaux::LogLevel::off});

    // Create parent
    margaux::Element parent;
    parent.set("label", std::string("Parent 1"));
    db.create_element("Parent", parent);

    // Create children
    margaux::Element child1;
    child1.set("label", std::string("Child 1"));
    db.create_element("Child", child1);

    margaux::Element child2;
    child2.set("label", std::string("Child 2"));
    db.create_element("Child", child2);

    // Set relation for only one child
    db.set_scalar_relation("Child", "parent_id", "Child 1", "Parent 1");

    auto relations = db.read_scalar_relation("Child", "parent_id");
    EXPECT_EQ(relations.size(), 2);
    EXPECT_EQ(relations[0], "Parent 1");  // Has parent
    EXPECT_EQ(relations[1], "");          // NULL parent
}

TEST(Database, ReadScalarRelationEmpty) {
    auto db =
        margaux::Database::from_schema(":memory:", VALID_SCHEMA("relations.sql"), {.console_level = margaux::LogLevel::off});

    // No children created yet
    auto relations = db.read_scalar_relation("Child", "parent_id");
    EXPECT_EQ(relations.size(), 0);
}

// ============================================================================
// Set scalar relation edge cases
// ============================================================================

TEST(Database, SetScalarRelationMultipleChildren) {
    auto db =
        margaux::Database::from_schema(":memory:", VALID_SCHEMA("relations.sql"), {.console_level = margaux::LogLevel::off});

    // Create parent
    margaux::Element parent;
    parent.set("label", std::string("Parent 1"));
    db.create_element("Parent", parent);

    // Create children
    margaux::Element child1;
    child1.set("label", std::string("Child 1"));
    db.create_element("Child", child1);

    margaux::Element child2;
    child2.set("label", std::string("Child 2"));
    db.create_element("Child", child2);

    margaux::Element child3;
    child3.set("label", std::string("Child 3"));
    db.create_element("Child", child3);

    // Set relations for multiple children
    db.set_scalar_relation("Child", "parent_id", "Child 1", "Parent 1");
    db.set_scalar_relation("Child", "parent_id", "Child 3", "Parent 1");

    auto relations = db.read_scalar_relation("Child", "parent_id");
    EXPECT_EQ(relations.size(), 3);
    EXPECT_EQ(relations[0], "Parent 1");
    EXPECT_EQ(relations[1], "");  // Child 2 has no parent
    EXPECT_EQ(relations[2], "Parent 1");
}

TEST(Database, SetScalarRelationOverwrite) {
    auto db =
        margaux::Database::from_schema(":memory:", VALID_SCHEMA("relations.sql"), {.console_level = margaux::LogLevel::off});

    // Create two parents
    margaux::Element parent1;
    parent1.set("label", std::string("Parent 1"));
    db.create_element("Parent", parent1);

    margaux::Element parent2;
    parent2.set("label", std::string("Parent 2"));
    db.create_element("Parent", parent2);

    // Create child
    margaux::Element child;
    child.set("label", std::string("Child 1"));
    db.create_element("Child", child);

    // Set initial relation
    db.set_scalar_relation("Child", "parent_id", "Child 1", "Parent 1");

    auto relations = db.read_scalar_relation("Child", "parent_id");
    EXPECT_EQ(relations[0], "Parent 1");

    // Overwrite relation
    db.set_scalar_relation("Child", "parent_id", "Child 1", "Parent 2");

    relations = db.read_scalar_relation("Child", "parent_id");
    EXPECT_EQ(relations[0], "Parent 2");
}
