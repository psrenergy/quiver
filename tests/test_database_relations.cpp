#include "test_utils.h"

#include <gtest/gtest.h>
#include <quiver/database.h>
#include <quiver/element.h>

TEST(Database, SetScalarRelation) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("relations.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    // Create parent
    quiver::Element parent;
    parent.set("label", std::string("Parent 1"));
    db.create_element("Parent", parent);

    // Create child without relation
    quiver::Element child;
    child.set("label", std::string("Child 1"));
    db.create_element("Child", child);

    // Set the relation
    db.update_scalar_relation("Child", "parent_id", "Child 1", "Parent 1");

    // Verify the relation was set using public read API
    auto relations = db.read_scalar_relation("Child", "parent_id");
    EXPECT_EQ(relations.size(), 1);
    EXPECT_EQ(relations[0], "Parent 1");
}

TEST(Database, SetScalarRelationSelfReference) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("relations.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    // Create two children
    quiver::Element child1;
    child1.set("label", std::string("Child 1"));
    db.create_element("Child", child1);

    quiver::Element child2;
    child2.set("label", std::string("Child 2"));
    db.create_element("Child", child2);

    // Set self-referential relation (sibling)
    db.update_scalar_relation("Child", "sibling_id", "Child 1", "Child 2");

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
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("relations.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    // Create parent
    quiver::Element parent;
    parent.set("label", std::string("Parent 1"));
    db.create_element("Parent", parent);

    // Create children without setting parent_id relation
    quiver::Element child1;
    child1.set("label", std::string("Child 1"));
    db.create_element("Child", child1);

    quiver::Element child2;
    child2.set("label", std::string("Child 2"));
    db.create_element("Child", child2);

    // Read relations - should return empty strings for unset relations
    auto relations = db.read_scalar_relation("Child", "parent_id");
    EXPECT_EQ(relations.size(), 2);
    EXPECT_EQ(relations[0], "");  // NULL parent
    EXPECT_EQ(relations[1], "");  // NULL parent
}

TEST(Database, ReadScalarRelationMixedNullsAndValues) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("relations.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    // Create parent
    quiver::Element parent;
    parent.set("label", std::string("Parent 1"));
    db.create_element("Parent", parent);

    // Create children
    quiver::Element child1;
    child1.set("label", std::string("Child 1"));
    db.create_element("Child", child1);

    quiver::Element child2;
    child2.set("label", std::string("Child 2"));
    db.create_element("Child", child2);

    // Set relation for only one child
    db.update_scalar_relation("Child", "parent_id", "Child 1", "Parent 1");

    auto relations = db.read_scalar_relation("Child", "parent_id");
    EXPECT_EQ(relations.size(), 2);
    EXPECT_EQ(relations[0], "Parent 1");  // Has parent
    EXPECT_EQ(relations[1], "");          // NULL parent
}

TEST(Database, ReadScalarRelationEmpty) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("relations.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    // No children created yet
    auto relations = db.read_scalar_relation("Child", "parent_id");
    EXPECT_EQ(relations.size(), 0);
}

// ============================================================================
// Set scalar relation edge cases
// ============================================================================

TEST(Database, SetScalarRelationMultipleChildren) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("relations.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    // Create parent
    quiver::Element parent;
    parent.set("label", std::string("Parent 1"));
    db.create_element("Parent", parent);

    // Create children
    quiver::Element child1;
    child1.set("label", std::string("Child 1"));
    db.create_element("Child", child1);

    quiver::Element child2;
    child2.set("label", std::string("Child 2"));
    db.create_element("Child", child2);

    quiver::Element child3;
    child3.set("label", std::string("Child 3"));
    db.create_element("Child", child3);

    // Set relations for multiple children
    db.update_scalar_relation("Child", "parent_id", "Child 1", "Parent 1");
    db.update_scalar_relation("Child", "parent_id", "Child 3", "Parent 1");

    auto relations = db.read_scalar_relation("Child", "parent_id");
    EXPECT_EQ(relations.size(), 3);
    EXPECT_EQ(relations[0], "Parent 1");
    EXPECT_EQ(relations[1], "");  // Child 2 has no parent
    EXPECT_EQ(relations[2], "Parent 1");
}

TEST(Database, SetScalarRelationOverwrite) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("relations.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    // Create two parents
    quiver::Element parent1;
    parent1.set("label", std::string("Parent 1"));
    db.create_element("Parent", parent1);

    quiver::Element parent2;
    parent2.set("label", std::string("Parent 2"));
    db.create_element("Parent", parent2);

    // Create child
    quiver::Element child;
    child.set("label", std::string("Child 1"));
    db.create_element("Child", child);

    // Set initial relation
    db.update_scalar_relation("Child", "parent_id", "Child 1", "Parent 1");

    auto relations = db.read_scalar_relation("Child", "parent_id");
    EXPECT_EQ(relations[0], "Parent 1");

    // Overwrite relation
    db.update_scalar_relation("Child", "parent_id", "Child 1", "Parent 2");

    relations = db.read_scalar_relation("Child", "parent_id");
    EXPECT_EQ(relations[0], "Parent 2");
}

// ============================================================================
// FK label resolution (resolve_fk_label helper)
// ============================================================================

TEST(Database, ResolveFkLabelInSetCreate) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("relations.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    // Create 2 parents
    quiver::Element parent1;
    parent1.set("label", std::string("Parent 1"));
    db.create_element("Parent", parent1);

    quiver::Element parent2;
    parent2.set("label", std::string("Parent 2"));
    db.create_element("Parent", parent2);

    // Create child with set FK using string labels (mentor_id is unique to set table)
    quiver::Element child;
    child.set("label", std::string("Child 1"));
    child.set("mentor_id", std::vector<std::string>{"Parent 1", "Parent 2"});
    db.create_element("Child", child);

    // Read back resolved integer IDs
    auto sets = db.read_set_integers("Child", "mentor_id");
    ASSERT_EQ(sets.size(), 1);
    ASSERT_EQ(sets[0].size(), 2);

    // Resolved parent IDs should be 1 and 2
    std::vector<int64_t> sorted_ids(sets[0].begin(), sets[0].end());
    std::sort(sorted_ids.begin(), sorted_ids.end());
    EXPECT_EQ(sorted_ids[0], 1);
    EXPECT_EQ(sorted_ids[1], 2);
}

TEST(Database, ResolveFkLabelMissingTarget) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("relations.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    // Create child with set FK referencing nonexistent parent (mentor_id is unique to set table)
    quiver::Element child;
    child.set("label", std::string("Child 1"));
    child.set("mentor_id", std::vector<std::string>{"Nonexistent Parent"});

    try {
        db.create_element("Child", child);
        FAIL() << "Expected std::runtime_error";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "Failed to resolve label 'Nonexistent Parent' to ID in table 'Parent'");
    }
}

TEST(Database, RejectStringForNonFkIntegerColumn) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("relations.sql"), {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    // Create child, then try to create with string in non-FK INTEGER set column
    quiver::Element child;
    child.set("label", std::string("Child 1"));
    child.set("score", std::vector<std::string>{"not_a_label"});

    EXPECT_THROW(db.create_element("Child", child), std::runtime_error);
}

// ============================================================================
// FK label resolution in create_element (all column types)
// ============================================================================

TEST(Database, CreateElementScalarFkLabel) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("relations.sql"),
        {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    // Create parent
    quiver::Element parent;
    parent.set("label", std::string("Parent 1"));
    db.create_element("Parent", parent);

    // Create child with scalar FK using string label
    quiver::Element child;
    child.set("label", std::string("Child 1"));
    child.set("parent_id", std::string("Parent 1"));  // string label, not int ID
    db.create_element("Child", child);

    // Verify: read back as integer, should be resolved ID
    auto parent_ids = db.read_scalar_integers("Child", "parent_id");
    ASSERT_EQ(parent_ids.size(), 1);
    EXPECT_EQ(parent_ids[0], 1);  // Parent 1's auto-increment ID
}

TEST(Database, CreateElementVectorFkLabels) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("relations.sql"),
        {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    // Create parents
    quiver::Element p1, p2;
    p1.set("label", std::string("Parent 1"));
    p2.set("label", std::string("Parent 2"));
    db.create_element("Parent", p1);
    db.create_element("Parent", p2);

    // Create child with vector FK using string labels
    // parent_ref routes to Child_vector_refs (and Child_set_parents, but
    // the vector path will use the pre-resolved integer values)
    quiver::Element child;
    child.set("label", std::string("Child 1"));
    child.set("parent_ref", std::vector<std::string>{"Parent 1", "Parent 2"});
    db.create_element("Child", child);

    // Verify: read back vector integers
    auto refs = db.read_vector_integers_by_id("Child", "parent_ref", 1);
    ASSERT_EQ(refs.size(), 2);
    EXPECT_EQ(refs[0], 1);
    EXPECT_EQ(refs[1], 2);
}

TEST(Database, CreateElementTimeSeriesFkLabels) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("relations.sql"),
        {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    // Create parents
    quiver::Element p1, p2;
    p1.set("label", std::string("Parent 1"));
    p2.set("label", std::string("Parent 2"));
    db.create_element("Parent", p1);
    db.create_element("Parent", p2);

    // Create child with time series FK using string labels
    // sponsor_id is unique to Child_time_series_events
    quiver::Element child;
    child.set("label", std::string("Child 1"));
    child.set("date_time", std::vector<std::string>{"2024-01-01", "2024-01-02"});
    child.set("sponsor_id", std::vector<std::string>{"Parent 1", "Parent 2"});
    db.create_element("Child", child);

    // Verify: read time series group and check sponsor_id values
    auto ts_data = db.read_time_series_group("Child", "events", 1);
    ASSERT_EQ(ts_data.size(), 2);

    // Each row is a map with "date_time" and "sponsor_id" keys
    // sponsor_id should be resolved to integer IDs
    EXPECT_EQ(std::get<int64_t>(ts_data[0].at("sponsor_id")), 1);
    EXPECT_EQ(std::get<int64_t>(ts_data[1].at("sponsor_id")), 2);
}

TEST(Database, CreateElementAllFkTypesInOneCall) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("relations.sql"),
        {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    // Create parents
    quiver::Element p1, p2;
    p1.set("label", std::string("Parent 1"));
    p2.set("label", std::string("Parent 2"));
    db.create_element("Parent", p1);
    db.create_element("Parent", p2);

    // Create child with ALL FK types in one call:
    // - scalar FK: parent_id -> Parent 1
    // - set FK: mentor_id -> Parent 2 (unique to Child_set_mentors)
    // - vector+set FK: parent_ref -> Parent 1 (routes to vector AND set tables)
    // - time series FK: sponsor_id -> Parent 2 (unique to Child_time_series_events)
    quiver::Element child;
    child.set("label", std::string("Child 1"));
    child.set("parent_id", std::string("Parent 1"));                      // scalar FK
    child.set("mentor_id", std::vector<std::string>{"Parent 2"});         // set FK
    child.set("parent_ref", std::vector<std::string>{"Parent 1"});        // vector+set FK
    child.set("date_time", std::vector<std::string>{"2024-01-01"});       // time series dimension
    child.set("sponsor_id", std::vector<std::string>{"Parent 2"});        // time series FK
    db.create_element("Child", child);

    // Verify scalar FK
    auto parent_ids = db.read_scalar_integers("Child", "parent_id");
    ASSERT_EQ(parent_ids.size(), 1);
    EXPECT_EQ(parent_ids[0], 1);

    // Verify set FK (mentor_id)
    auto mentors = db.read_set_integers("Child", "mentor_id");
    ASSERT_EQ(mentors.size(), 1);
    ASSERT_EQ(mentors[0].size(), 1);
    EXPECT_EQ(mentors[0][0], 2);

    // Verify vector FK (parent_ref in Child_vector_refs)
    auto vrefs = db.read_vector_integers_by_id("Child", "parent_ref", 1);
    ASSERT_EQ(vrefs.size(), 1);
    EXPECT_EQ(vrefs[0], 1);

    // Verify time series FK (sponsor_id in Child_time_series_events)
    auto ts_data = db.read_time_series_group("Child", "events", 1);
    ASSERT_EQ(ts_data.size(), 1);
    EXPECT_EQ(std::get<int64_t>(ts_data[0].at("sponsor_id")), 2);
}

TEST(Database, CreateElementNoFkColumnsUnchanged) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"),
        {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    // basic.sql has no FK columns -- this tests that the pre-resolve pass
    // passes all values through unchanged for schemas with no FKs
    quiver::Element element;
    element.set("label", std::string("Config 1"));
    element.set("integer_attribute", int64_t{42});
    element.set("float_attribute", 3.14);

    int64_t id = db.create_element("Configuration", element);
    EXPECT_EQ(id, 1);

    auto labels = db.read_scalar_strings("Configuration", "label");
    EXPECT_EQ(labels[0], "Config 1");
    auto integers = db.read_scalar_integers("Configuration", "integer_attribute");
    EXPECT_EQ(integers[0], 42);
    auto floats = db.read_scalar_floats("Configuration", "float_attribute");
    EXPECT_DOUBLE_EQ(floats[0], 3.14);
}

TEST(Database, ScalarFkResolutionFailureCausesNoPartialWrites) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("relations.sql"),
        {.read_only = 0, .console_level = QUIVER_LOG_OFF});

    // Create child with scalar FK referencing nonexistent parent
    quiver::Element child;
    child.set("label", std::string("Orphan Child"));
    child.set("parent_id", std::string("Nonexistent Parent"));

    EXPECT_THROW(db.create_element("Child", child), std::runtime_error);

    // Verify: no child was created (zero partial writes)
    auto labels = db.read_scalar_strings("Child", "label");
    EXPECT_EQ(labels.size(), 0);
}
