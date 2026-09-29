#include "test_utils.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <quiver/database.h>
#include <quiver/element.h>

// ============================================================================
// Read vector tests
// ============================================================================

TEST(Database, ReadVectorIntegers) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e1;
    e1.set("label", std::string("Item 1")).set("value_int", std::vector<int64_t>{1, 2, 3});
    db.create_element("Collection", e1);

    quiver::Element e2;
    e2.set("label", std::string("Item 2")).set("value_int", std::vector<int64_t>{10, 20});
    db.create_element("Collection", e2);

    auto vectors = db.read_vector_integers("Collection", "value_int");
    EXPECT_EQ(vectors.size(), 2);
    EXPECT_EQ(vectors[0], (std::vector<std::optional<int64_t>>{1, 2, 3}));
    EXPECT_EQ(vectors[1], (std::vector<std::optional<int64_t>>{10, 20}));
}

TEST(Database, ReadVectorFloats) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e1;
    e1.set("label", std::string("Item 1")).set("value_float", std::vector<double>{1.5, 2.5, 3.5});
    db.create_element("Collection", e1);

    quiver::Element e2;
    e2.set("label", std::string("Item 2")).set("value_float", std::vector<double>{10.5, 20.5});
    db.create_element("Collection", e2);

    auto vectors = db.read_vector_floats("Collection", "value_float");
    EXPECT_EQ(vectors.size(), 2);
    EXPECT_EQ(vectors[0], (std::vector<std::optional<double>>{1.5, 2.5, 3.5}));
    EXPECT_EQ(vectors[1], (std::vector<std::optional<double>>{10.5, 20.5}));
}

TEST(Database, ReadVectorEmpty) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // No Collection elements created
    auto integer_vectors = db.read_vector_integers("Collection", "value_int");
    auto float_vectors = db.read_vector_floats("Collection", "value_float");

    EXPECT_TRUE(integer_vectors.empty());
    EXPECT_TRUE(float_vectors.empty());
}

TEST(Database, ReadVectorIncludesElementsWithNoRows) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // Element with vector data
    quiver::Element e1;
    e1.set("label", std::string("Item 1")).set("value_int", std::vector<int64_t>{1, 2, 3});
    db.create_element("Collection", e1);

    // Element without vector data
    quiver::Element e2;
    e2.set("label", std::string("Item 2"));
    db.create_element("Collection", e2);

    // Another element with vector data
    quiver::Element e3;
    e3.set("label", std::string("Item 3")).set("value_int", std::vector<int64_t>{4, 5});
    db.create_element("Collection", e3);

    // One entry per element, positionally aligned with read_element_ids: the element with no rows
    // is an empty vector, not a gap
    auto ids = db.read_element_ids("Collection");
    auto vectors = db.read_vector_integers("Collection", "value_int");
    ASSERT_EQ(ids.size(), 3);
    ASSERT_EQ(vectors.size(), ids.size());
    EXPECT_EQ(vectors[0], (std::vector<std::optional<int64_t>>{1, 2, 3}));
    EXPECT_TRUE(vectors[1].empty());
    EXPECT_EQ(vectors[2], (std::vector<std::optional<int64_t>>{4, 5}));
}

// ============================================================================
// Read vector by ID tests
// ============================================================================

TEST(Database, ReadVectorIntegerById) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e1;
    e1.set("label", std::string("Item 1")).set("value_int", std::vector<int64_t>{1, 2, 3});
    int64_t id1 = db.create_element("Collection", e1);

    quiver::Element e2;
    e2.set("label", std::string("Item 2")).set("value_int", std::vector<int64_t>{10, 20});
    int64_t id2 = db.create_element("Collection", e2);

    auto vec1 = db.read_vector_integers_by_id("Collection", "value_int", id1);
    auto vec2 = db.read_vector_integers_by_id("Collection", "value_int", id2);

    EXPECT_EQ(vec1, (std::vector<std::optional<int64_t>>{1, 2, 3}));
    EXPECT_EQ(vec2, (std::vector<std::optional<int64_t>>{10, 20}));
}

TEST(Database, ReadVectorFloatById) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e1;
    e1.set("label", std::string("Item 1")).set("value_float", std::vector<double>{1.5, 2.5, 3.5});
    int64_t id1 = db.create_element("Collection", e1);

    quiver::Element e2;
    e2.set("label", std::string("Item 2")).set("value_float", std::vector<double>{10.5, 20.5});
    int64_t id2 = db.create_element("Collection", e2);

    auto vec1 = db.read_vector_floats_by_id("Collection", "value_float", id1);
    auto vec2 = db.read_vector_floats_by_id("Collection", "value_float", id2);

    EXPECT_EQ(vec1, (std::vector<std::optional<double>>{1.5, 2.5, 3.5}));
    EXPECT_EQ(vec2, (std::vector<std::optional<double>>{10.5, 20.5}));
}

TEST(Database, ReadVectorByIdEmpty) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e;
    e.set("label", std::string("Item 1"));  // No vector data
    int64_t id = db.create_element("Collection", e);

    auto vec = db.read_vector_integers_by_id("Collection", "value_int", id);
    EXPECT_TRUE(vec.empty());
}

TEST(Database, ReadVectorIntegersInvalidCollection) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.read_vector_integers("NonexistentCollection", "value_int"), std::runtime_error);
}

TEST(Database, ReadVectorIntegersInvalidAttribute) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.read_vector_integers("Collection", "nonexistent_attribute"), std::runtime_error);
}

TEST(Database, ReadVectorIntegerByIdInvalidCollection) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(db.read_vector_integers_by_id("NonexistentCollection", "value_int", 1), std::runtime_error);
}

// ============================================================================
// Read vector strings tests (gap-fill using all_types.sql)
// ============================================================================

TEST(Database, ReadVectorStringsBulk) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("all_types.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e1;
    e1.set("label", std::string("Item 1"));
    int64_t id1 = db.create_element("AllTypes", e1);
    quiver::Element update1;
    update1.set("label_value", std::vector<std::string>{"alpha", "beta", "gamma"});
    db.update_element("AllTypes", id1, update1);

    quiver::Element e2;
    e2.set("label", std::string("Item 2"));
    int64_t id2 = db.create_element("AllTypes", e2);
    quiver::Element update2;
    update2.set("label_value", std::vector<std::string>{"delta", "epsilon"});
    db.update_element("AllTypes", id2, update2);

    auto vectors = db.read_vector_strings("AllTypes", "label_value");
    EXPECT_EQ(vectors.size(), 2);
    EXPECT_EQ(vectors[0], (std::vector<std::optional<std::string>>{"alpha", "beta", "gamma"}));
    EXPECT_EQ(vectors[1], (std::vector<std::optional<std::string>>{"delta", "epsilon"}));
}

TEST(Database, ReadVectorStringsByIdBasic) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("all_types.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    quiver::Element e;
    e.set("label", std::string("Item 1"));
    int64_t id = db.create_element("AllTypes", e);
    quiver::Element update;
    update.set("label_value", std::vector<std::string>{"hello", "world"});
    db.update_element("AllTypes", id, update);

    auto vec = db.read_vector_strings_by_id("AllTypes", "label_value", id);
    EXPECT_EQ(vec, (std::vector<std::optional<std::string>>{"hello", "world"}));
}

TEST(Database, ReadVectorIntegersInvalidColumnThrows) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    EXPECT_THROW(
        {
            try {
                db.read_vector_integers("Collection", "nonexistent_column");
            } catch (const std::runtime_error& e) {
                EXPECT_THAT(std::string(e.what()), testing::HasSubstr("not found"));
                throw;
            }
        },
        std::runtime_error);
}

// ============================================================================
// NULL handling in vector reads
// ============================================================================

TEST(Database, ReadVectorPreservesNullCells) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    db.create_element("Configuration", quiver::Element().set("label", std::string("Test Config")));

    // value_int is nullable, so a null cell is stored as SQL NULL.
    quiver::Element e;
    e.set("label", std::string("Item 1"))
        .set("value_int", std::vector<quiver::Value>{int64_t{10}, nullptr, int64_t{30}});
    int64_t id = db.create_element("Collection", e);

    auto vectors = db.read_vector_integers("Collection", "value_int");
    ASSERT_EQ(vectors.size(), 1u);
    EXPECT_EQ(vectors[0], (std::vector<std::optional<int64_t>>{10, std::nullopt, 30}));

    EXPECT_EQ(db.read_vector_integers_by_id("Collection", "value_int", id),
              (std::vector<std::optional<int64_t>>{10, std::nullopt, 30}));
}

TEST(Database, ReadVectorDistinguishesNoRowsFromNullOnlyRow) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    db.create_element("Configuration", quiver::Element().set("label", std::string("Test Config")));

    // Item 1 has no group rows at all; Item 2 has exactly one row whose value is NULL.
    db.create_element("Collection", quiver::Element().set("label", std::string("Item 1")));
    quiver::Element e2;
    e2.set("label", std::string("Item 2")).set("value_int", std::vector<quiver::Value>{nullptr});
    db.create_element("Collection", e2);

    auto vectors = db.read_vector_integers("Collection", "value_int");
    ASSERT_EQ(vectors.size(), 2u);
    EXPECT_TRUE(vectors[0].empty());
    EXPECT_EQ(vectors[1], (std::vector<std::optional<int64_t>>{std::nullopt}));
}

TEST(Database, ReadVectorBulkKeepsElementWithIdMinusOne) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    db.create_element("Configuration", quiver::Element().set("label", std::string("Test Config")));

    // -1 is a valid id (create_element accepts an explicit one) and, as the smallest rowid, the
    // first row of the LEFT JOIN: it must not be taken for "no element read yet".
    db.create_element("Collection",
                      quiver::Element()
                          .set("label", std::string("Negative"))
                          .set("id", int64_t{-1})
                          .set("value_int", std::vector<int64_t>{1, 2}));
    db.create_element("Collection", quiver::Element().set("label", std::string("Empty")));
    db.create_element(
        "Collection",
        quiver::Element().set("label", std::string("Positive")).set("value_int", std::vector<int64_t>{7}));

    auto ids = db.read_element_ids("Collection");
    auto vectors = db.read_vector_integers("Collection", "value_int");
    ASSERT_EQ(ids.size(), 3u);
    ASSERT_EQ(vectors.size(), ids.size());
    EXPECT_EQ(ids[0], -1);
    EXPECT_EQ(vectors[0], (std::vector<std::optional<int64_t>>{1, 2}));
    EXPECT_TRUE(vectors[1].empty());
    EXPECT_EQ(vectors[2], (std::vector<std::optional<int64_t>>{7}));
}

// ============================================================================
// Column-name resolution (shared_group_columns.sql)
// ============================================================================

namespace {

struct SharedGroupColumnsFixture {
    quiver::Database db;
    int64_t parent_a;
    int64_t parent_b;
    int64_t child;

    SharedGroupColumnsFixture()
        : db(quiver::Database::from_schema(":memory:",
                                           VALID_SCHEMA("shared_group_columns.sql"),
                                           {.read_only = false, .console_level = quiver::LogLevel::Off})) {
        db.create_element("Configuration", quiver::Element().set("label", std::string("Config")));
        parent_a = db.create_element("Parent", quiver::Element().set("label", std::string("Parent A")));
        parent_b = db.create_element("Parent", quiver::Element().set("label", std::string("Parent B")));
        child = db.create_element("Child", quiver::Element().set("label", std::string("Child 1")));
    }
};

}  // namespace

// A group named after a column it does not hold (Child_vector_cost holds "amount") must not stop
// the per-column read of Child_vector_routes' "cost": it used to throw "column 'cost' not found in
// table 'Child_vector_cost'", so no reader in any layer could read that column.
TEST(Database, ReadGroupColumnSkipsGroupNamedAfterAColumnItLacks) {
    SharedGroupColumnsFixture f;
    f.db.update_vector_group(
        "Child",
        "routes",
        f.child,
        {{{"parent_ref", f.parent_b}, {"cost", 1.5}}, {{"parent_ref", f.parent_b}, {"cost", 2.5}}});
    f.db.update_vector_group("Child", "cost", f.child, {{{"amount", 9.0}}});

    EXPECT_EQ(f.db.read_vector_floats_by_id("Child", "cost", f.child), (std::vector<std::optional<double>>{1.5, 2.5}));
    EXPECT_EQ(f.db.read_vector_floats("Child", "cost"), (std::vector<std::vector<std::optional<double>>>{{1.5, 2.5}}));
    EXPECT_EQ(f.db.read_vector_floats_by_id("Child", "amount", f.child), (std::vector<std::optional<double>>{9.0}));

    // The set counterpart: Child_set_tier holds "rank", while "tier" belongs to sponsors.
    f.db.update_set_group(
        "Child",
        "sponsors",
        f.child,
        {{{"parent_ref", f.parent_b}, {"tier", int64_t{1}}}, {{"parent_ref", f.parent_b}, {"tier", int64_t{2}}}});
    f.db.update_set_group("Child", "tier", f.child, {{{"rank", int64_t{9}}}});
    auto tiers = f.db.read_set_integers_by_id("Child", "tier", f.child);
    std::sort(tiers.begin(), tiers.end());
    EXPECT_EQ(tiers, (std::vector<std::optional<int64_t>>{1, 2}));
    EXPECT_EQ(f.db.read_set_integers_by_id("Child", "rank", f.child), (std::vector<std::optional<int64_t>>{9}));
}

// Two groups of one kind share the FK column parent_ref. A per-column read resolves the NAME, to the
// group table whose name sorts first; the whole-group reader reads the group it is given.
TEST(Database, ReadGroupByIdReadsItsOwnTableWhenGroupsShareAColumn) {
    SharedGroupColumnsFixture f;
    f.db.update_vector_group("Child", "links", f.child, {{{"parent_ref", f.parent_a}}});
    f.db.update_vector_group(
        "Child",
        "routes",
        f.child,
        {{{"parent_ref", f.parent_b}, {"cost", 1.5}}, {{"parent_ref", f.parent_b}, {"cost", 2.5}}});
    f.db.update_set_group("Child", "mentors", f.child, {{{"parent_ref", f.parent_a}}});
    f.db.update_set_group(
        "Child",
        "sponsors",
        f.child,
        {{{"parent_ref", f.parent_b}, {"tier", int64_t{1}}}, {{"parent_ref", f.parent_b}, {"tier", int64_t{2}}}});

    EXPECT_EQ(f.db.read_vector_integers_by_id("Child", "parent_ref", f.child),
              (std::vector<std::optional<int64_t>>{f.parent_a}));
    EXPECT_EQ(f.db.read_set_integers_by_id("Child", "parent_ref", f.child),
              (std::vector<std::optional<int64_t>>{f.parent_a}));

    auto routes = f.db.read_vector_group_by_id("Child", "routes", f.child);
    ASSERT_EQ(routes.size(), 2u);
    for (const auto& row : routes) {
        EXPECT_EQ(std::get<int64_t>(row.at("parent_ref")), f.parent_b);
    }
    EXPECT_EQ(std::get<double>(routes[0].at("cost")), 1.5);
    EXPECT_EQ(std::get<double>(routes[1].at("cost")), 2.5);

    auto sponsors = f.db.read_set_group_by_id("Child", "sponsors", f.child);
    ASSERT_EQ(sponsors.size(), 2u);
    std::vector<int64_t> tiers;
    for (const auto& row : sponsors) {
        EXPECT_EQ(std::get<int64_t>(row.at("parent_ref")), f.parent_b);
        tiers.push_back(std::get<int64_t>(row.at("tier")));
    }
    std::sort(tiers.begin(), tiers.end());
    EXPECT_EQ(tiers, (std::vector<int64_t>{1, 2}));
}
