#include "test_utils.h"

#include <gtest/gtest.h>
#include <quiver/c/database.h>
#include <quiver/c/element.h>

TEST(DatabaseCApi, TransactionBeginMultipleWritesCommit) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(
        quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db), QUIVER_OK
    );
    ASSERT_NE(db, nullptr);

    // Create Configuration first
    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t config_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Configuration", config, &config_id), QUIVER_OK);
    quiver_element_destroy(config);

    // Begin transaction
    ASSERT_EQ(quiver_database_begin_transaction(db), QUIVER_OK);

    // Create two elements inside transaction
    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Item 1");
    int64_t id1 = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Collection", e1, &id1), QUIVER_OK);
    quiver_element_destroy(e1);

    quiver_element_t* e2 = nullptr;
    ASSERT_EQ(quiver_element_create(&e2), QUIVER_OK);
    quiver_element_set_string(e2, "label", "Item 2");
    int64_t id2 = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Collection", e2, &id2), QUIVER_OK);
    quiver_element_destroy(e2);

    // Commit
    ASSERT_EQ(quiver_database_commit(db), QUIVER_OK);

    // Read back to verify persistence
    char** labels = nullptr;
    size_t count = 0;
    ASSERT_EQ(quiver_database_read_scalar_strings(db, "Collection", "label", &labels, &count), QUIVER_OK);
    EXPECT_EQ(count, 2u);
    EXPECT_STREQ(labels[0], "Item 1");
    EXPECT_STREQ(labels[1], "Item 2");
    quiver_database_free_string_array(labels, count);

    quiver_database_close(db);
}

TEST(DatabaseCApi, TransactionRollbackDiscardsWrites) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(
        quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db), QUIVER_OK
    );
    ASSERT_NE(db, nullptr);

    // Create Configuration first
    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t config_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Configuration", config, &config_id), QUIVER_OK);
    quiver_element_destroy(config);

    // Begin transaction, create element, rollback
    ASSERT_EQ(quiver_database_begin_transaction(db), QUIVER_OK);

    quiver_element_t* e1 = nullptr;
    ASSERT_EQ(quiver_element_create(&e1), QUIVER_OK);
    quiver_element_set_string(e1, "label", "Discarded Item");
    int64_t id1 = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Collection", e1, &id1), QUIVER_OK);
    quiver_element_destroy(e1);

    ASSERT_EQ(quiver_database_rollback(db), QUIVER_OK);

    // Read back -- Collection should have no elements
    char** labels = nullptr;
    size_t count = 0;
    ASSERT_EQ(quiver_database_read_scalar_strings(db, "Collection", "label", &labels, &count), QUIVER_OK);
    EXPECT_EQ(count, 0u);
    quiver_database_free_string_array(labels, count);

    quiver_database_close(db);
}

TEST(DatabaseCApi, TransactionDoubleBeginReturnsError) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    ASSERT_EQ(quiver_database_begin_transaction(db), QUIVER_OK);
    EXPECT_EQ(quiver_database_begin_transaction(db), QUIVER_ERROR);

    const char* err = quiver_get_last_error();
    EXPECT_STREQ(err, "Cannot begin_transaction: transaction already active");

    // Clean up
    quiver_database_rollback(db);
    quiver_database_close(db);
}

TEST(DatabaseCApi, TransactionCommitWithoutBeginReturnsError) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    EXPECT_EQ(quiver_database_commit(db), QUIVER_ERROR);

    const char* err = quiver_get_last_error();
    EXPECT_STREQ(err, "Cannot commit: no active transaction");

    quiver_database_close(db);
}

TEST(DatabaseCApi, TransactionRollbackWithoutBeginReturnsError) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    EXPECT_EQ(quiver_database_rollback(db), QUIVER_ERROR);

    const char* err = quiver_get_last_error();
    EXPECT_STREQ(err, "Cannot rollback: no active transaction");

    quiver_database_close(db);
}

TEST(DatabaseCApi, InTransactionReflectsState) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    int active = 1;
    ASSERT_EQ(quiver_database_in_transaction(db, &active), QUIVER_OK);
    EXPECT_EQ(active, 0);

    ASSERT_EQ(quiver_database_begin_transaction(db), QUIVER_OK);
    ASSERT_EQ(quiver_database_in_transaction(db, &active), QUIVER_OK);
    EXPECT_NE(active, 0);

    ASSERT_EQ(quiver_database_commit(db), QUIVER_OK);
    ASSERT_EQ(quiver_database_in_transaction(db, &active), QUIVER_OK);
    EXPECT_EQ(active, 0);

    quiver_database_close(db);
}

// The core validates every array before update_element's first write, so a rejected call inside a
// caller-owned transaction leaves nothing for the commit to persist.
TEST(DatabaseCApi, TransactionRejectedUpdateElementWritesNothing) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(
        quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db), QUIVER_OK
    );
    ASSERT_NE(db, nullptr);

    quiver_element_t* config = nullptr;
    ASSERT_EQ(quiver_element_create(&config), QUIVER_OK);
    quiver_element_set_string(config, "label", "Test Config");
    int64_t config_id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Configuration", config, &config_id), QUIVER_OK);
    quiver_element_destroy(config);

    quiver_element_t* item = nullptr;
    ASSERT_EQ(quiver_element_create(&item), QUIVER_OK);
    quiver_element_set_string(item, "label", "Item 1");
    quiver_element_set_integer(item, "some_integer", 1);
    int64_t id = 0;
    ASSERT_EQ(quiver_database_create_element(db, "Collection", item, &id), QUIVER_OK);
    quiver_element_destroy(item);

    ASSERT_EQ(quiver_database_begin_transaction(db), QUIVER_OK);

    quiver_element_t* update = nullptr;
    ASSERT_EQ(quiver_element_create(&update), QUIVER_OK);
    quiver_element_set_integer(update, "some_integer", 2);
    const double tags[] = {1.5};
    ASSERT_EQ(quiver_element_set_array_float(update, "tag", tags, 1, nullptr), QUIVER_OK);
    EXPECT_EQ(quiver_database_update_element(db, "Collection", id, update), QUIVER_ERROR);
    EXPECT_STREQ(
        quiver_get_last_error(), "Cannot update_element: type mismatch for array 'tag' index 0: expected TEXT, got REAL"
    );
    quiver_element_destroy(update);

    ASSERT_EQ(quiver_database_commit(db), QUIVER_OK);

    int64_t value = 0;
    int has_value = 0;
    ASSERT_EQ(
        quiver_database_read_scalar_integer_by_id(db, "Collection", "some_integer", id, &value, &has_value), QUIVER_OK
    );
    EXPECT_EQ(has_value, 1);
    EXPECT_EQ(value, 1);

    quiver_database_close(db);
}
