#include "test_utils.h"

#include <gtest/gtest.h>
#include <quiver/c/database.h>
#include <quiver/c/element.h>

TEST(DatabaseCApiCount, NumberOfElementsTracksCurrentRows) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    int64_t count = -1;
    ASSERT_EQ(quiver_database_number_of_elements(db, "Collection", &count), QUIVER_OK);
    EXPECT_EQ(count, 0);

    const char* labels[] = {"Item 1", "Item 2", "Item 3"};
    int64_t ids[3] = {};
    for (size_t i = 0; i < 3; ++i) {
        quiver_element_t* element = nullptr;
        ASSERT_EQ(quiver_element_create(&element), QUIVER_OK);
        ASSERT_EQ(quiver_element_set_string(element, "label", labels[i]), QUIVER_OK);
        ASSERT_EQ(quiver_database_create_element(db, "Collection", element, &ids[i]), QUIVER_OK);
        ASSERT_EQ(quiver_element_destroy(element), QUIVER_OK);
    }

    ASSERT_EQ(quiver_database_number_of_elements(db, "Collection", &count), QUIVER_OK);
    EXPECT_EQ(count, 3);

    ASSERT_EQ(quiver_database_delete_element(db, "Collection", ids[1]), QUIVER_OK);
    ASSERT_EQ(quiver_database_number_of_elements(db, "Collection", &count), QUIVER_OK);
    EXPECT_EQ(count, 2);

    quiver_database_close(db);
}

TEST(DatabaseCApiCount, NumberOfElementsNotFound) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    int64_t count = -1;
    EXPECT_EQ(quiver_database_number_of_elements(db, "Nope", &count), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Cannot number_of_elements: collection not found: Nope");

    quiver_database_close(db);
}

TEST(DatabaseCApiCount, NumberOfElementsNullArguments) {
    int64_t count = -1;
    EXPECT_EQ(quiver_database_number_of_elements(nullptr, "Collection", &count), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: db");

    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    EXPECT_EQ(quiver_database_number_of_elements(db, nullptr, &count), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: collection");

    EXPECT_EQ(quiver_database_number_of_elements(db, "Collection", nullptr), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Null argument: out_count");

    quiver_database_close(db);
}
