#include "test_utils.h"

#include <gtest/gtest.h>
#include <quiver/database.h>
#include <quiver/element.h>
#include <stdexcept>
#include <string>
#include <vector>

TEST(DatabaseCount, NumberOfElementsTracksCurrentRows) {
    auto db = quiver::Database::from_schema(":memory:",
                                            VALID_SCHEMA("describe_multi_group.sql"),
                                            {.read_only = false, .console_level = quiver::LogLevel::Off});
    const auto& const_db = db;

    EXPECT_EQ(const_db.number_of_elements("Items"), 0);

    db.create_element("Items", quiver::Element().set("label", std::string("a")));
    const int64_t middle_id = db.create_element("Items", quiver::Element().set("label", std::string("b")));
    db.create_element("Items", quiver::Element().set("label", std::string("c")));

    EXPECT_EQ(const_db.number_of_elements("Items"), 3);

    db.delete_element("Items", middle_id);
    EXPECT_EQ(const_db.number_of_elements("Items"), 2);

    db.begin_transaction();
    db.create_element("Items", quiver::Element().set("label", std::string("d")));
    EXPECT_EQ(const_db.number_of_elements("Items"), 3);
    db.rollback();

    EXPECT_EQ(const_db.number_of_elements("Items"), 2);
}

TEST(DatabaseCount, NumberOfElementsIgnoresGroupRows) {
    auto db = quiver::Database::from_schema(":memory:",
                                            VALID_SCHEMA("describe_multi_group.sql"),
                                            {.read_only = false, .console_level = quiver::LogLevel::Off});
    const auto& const_db = db;

    // One element owning rows in two different group tables, one owning none.
    db.create_element("Items",
                      quiver::Element()
                          .set("label", std::string("a"))
                          .set("amount", std::vector<double>{1.0, 2.0, 3.0})
                          .set("tag", std::vector<std::string>{"x", "y"}));
    db.create_element("Items", quiver::Element().set("label", std::string("b")));

    // Guards against a vacuous pass: the group rows must really be there for the count to ignore.
    EXPECT_EQ(db.query_integer("SELECT COUNT(*) FROM Items_vector_values"), 3);
    EXPECT_EQ(db.query_integer("SELECT COUNT(*) FROM Items_set_tags"), 2);

    // The main table's row count, even though these two elements own 5 group rows between them.
    EXPECT_EQ(const_db.number_of_elements("Items"), 2);
}

TEST(DatabaseCount, NumberOfElementsCountsGroupTables) {
    auto db = quiver::Database::from_schema(":memory:",
                                            VALID_SCHEMA("describe_multi_group.sql"),
                                            {.read_only = false, .console_level = quiver::LogLevel::Off});
    const auto& const_db = db;

    db.create_element("Items",
                      quiver::Element()
                          .set("label", std::string("a"))
                          .set("amount", std::vector<double>{1.0, 2.0, 3.0})
                          .set("tag", std::vector<std::string>{"x", "y"}));

    // Vector and set tables are compatible with this method: each reports its own row count.
    EXPECT_EQ(const_db.number_of_elements("Items_vector_values"), 3);
    EXPECT_EQ(const_db.number_of_elements("Items_set_tags"), 2);

    // Acceptance follows the schema, not the data: an untouched group table is 0, not an error.
    EXPECT_EQ(const_db.number_of_elements("Items_vector_scores"), 0);
}

TEST(DatabaseCount, NumberOfElementsNotFound) {
    auto db = quiver::Database::from_schema(":memory:",
                                            VALID_SCHEMA("describe_multi_group.sql"),
                                            {.read_only = false, .console_level = quiver::LogLevel::Off});

    try {
        (void)db.number_of_elements("Nope");
        FAIL() << "Expected number_of_elements to reject an unknown collection";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "Cannot number_of_elements: collection not found: Nope");
    }
}
