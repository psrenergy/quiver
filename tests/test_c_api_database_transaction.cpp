#include "test_utils.h"

#include <gtest/gtest.h>
#include <quiver/c/database.h>
#include <quiver/c/element.h>

TEST(DatabaseCApi, TransactionBeginMultipleWritesCommit) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
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
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("collections.sql").c_str(), &options, &db),
              QUIVER_OK);
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
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
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
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    EXPECT_EQ(quiver_database_commit(db), QUIVER_ERROR);

    const char* err = quiver_get_last_error();
    EXPECT_STREQ(err, "Cannot commit: no active transaction");

    quiver_database_close(db);
}

TEST(DatabaseCApi, TransactionRollbackWithoutBeginReturnsError) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    EXPECT_EQ(quiver_database_rollback(db), QUIVER_ERROR);

    const char* err = quiver_get_last_error();
    EXPECT_STREQ(err, "Cannot rollback: no active transaction");

    quiver_database_close(db);
}

TEST(DatabaseCApi, InTransactionReflectsState) {
    auto options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(":memory:", VALID_SCHEMA("basic.sql").c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    bool active = true;
    ASSERT_EQ(quiver_database_in_transaction(db, &active), QUIVER_OK);
    EXPECT_FALSE(active);

    ASSERT_EQ(quiver_database_begin_transaction(db), QUIVER_OK);
    ASSERT_EQ(quiver_database_in_transaction(db, &active), QUIVER_OK);
    EXPECT_TRUE(active);

    ASSERT_EQ(quiver_database_commit(db), QUIVER_OK);
    ASSERT_EQ(quiver_database_in_transaction(db, &active), QUIVER_OK);
    EXPECT_FALSE(active);

    quiver_database_close(db);
}
