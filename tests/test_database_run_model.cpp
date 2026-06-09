#include "test_utils.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <quiver/database.h>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

// Precondition coverage for run_model. The happy path (a real exit code from CarbSteeler)
// is not testable hermetically because the model directory is a hardcoded constant; it is
// verified manually against the real model. Both preconditions below fire before the model
// directory is consulted, so they are deterministic on every machine/CI runner.
class RunModelFixture : public ::testing::Test {
protected:
    void SetUp() override { path = (fs::temp_directory_path() / "quiver_run_model_test.db").string(); }
    void TearDown() override {
        if (fs::exists(path))
            fs::remove(path);
    }
    std::string path;
};

TEST_F(RunModelFixture, ThrowsOnMemoryDatabase) {
    quiver::Database db(":memory:", {.read_only = false, .console_level = quiver::LogLevel::Off});
    try {
        db.run_model();
        FAIL() << "Expected std::runtime_error";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "Cannot run_model: in-memory database has no path to pass to the model");
    }
}

TEST_F(RunModelFixture, ThrowsWhenTransactionActive) {
    quiver::Database db(path, {.read_only = false, .console_level = quiver::LogLevel::Off});
    db.begin_transaction();
    try {
        db.run_model();
        FAIL() << "Expected std::runtime_error";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "Cannot run_model: commit or roll back the active transaction first");
    }
    db.rollback();
}
