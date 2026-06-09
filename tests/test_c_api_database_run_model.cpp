#include "test_utils.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <quiver/c/database.h>
#include <string>

namespace fs = std::filesystem;

// Precondition coverage for quiver_database_run_model. As with the C++ tests, the happy
// path is verified manually (the model directory is a hardcoded constant). Both preconditions
// surface as QUIVER_ERROR with the canonical message from the C++ layer.

TEST(DatabaseCApiRunModel, MemoryDatabaseReturnsError) {
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_open(":memory:", &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    int exit_code = 0;
    EXPECT_EQ(quiver_database_run_model(db, &exit_code), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Cannot run_model: in-memory database has no path to pass to the model");

    quiver_database_close(db);
}

TEST(DatabaseCApiRunModel, ActiveTransactionReturnsError) {
    const auto path = (fs::temp_directory_path() / "quiver_c_run_model_test.db").string();
    if (fs::exists(path))
        fs::remove(path);

    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_open(path.c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    ASSERT_EQ(quiver_database_begin_transaction(db), QUIVER_OK);

    int exit_code = 0;
    EXPECT_EQ(quiver_database_run_model(db, &exit_code), QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Cannot run_model: commit or roll back the active transaction first");

    quiver_database_rollback(db);
    quiver_database_close(db);
    if (fs::exists(path))
        fs::remove(path);
}

TEST(DatabaseCApiRunModel, NullArgsRejected) {
    int exit_code = 0;
    EXPECT_EQ(quiver_database_run_model(nullptr, &exit_code), QUIVER_ERROR);

    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_open(":memory:", &options, &db), QUIVER_OK);
    EXPECT_EQ(quiver_database_run_model(db, nullptr), QUIVER_ERROR);
    quiver_database_close(db);
}
