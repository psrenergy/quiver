#include "test_utils.h"

#include <gtest/gtest.h>
#include <quiver/database.h>
#include <quiver/element.h>
#include <stdexcept>
#include <string>

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
