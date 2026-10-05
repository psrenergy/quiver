#ifndef QUIVER_TEST_SANDBOX_H
#define QUIVER_TEST_SANDBOX_H

#include "test_utils.h"

#include <gtest/gtest.h>
#include <quiver/database.h>
#include <quiver/element.h>
#include <quiver/sandbox.h>

#include <filesystem>
#include <string>

class SandboxTest : public ::testing::Test {
protected:
    void SetUp() override {
        collections_schema = VALID_SCHEMA("collections.sql");
    }
    std::string collections_schema;
};

// File-backed database in a dedicated per-test temp directory — the sandbox root for the
// db-scoped Lua file operations (db:open_file, db:bin_to_csv, db:csv_to_bin, db:export_csv,
// db:import_csv, db:validate_migrations, expr:save). TearDown removes the directory wholesale (database, log file,
// and any .qvr/.toml/.csv outputs); test-local Database objects are destroyed before TearDown.
class LuaSandboxTest : public ::testing::Test {
protected:
    void SetUp() override {
        const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
        sandbox_path = std::filesystem::temp_directory_path() /
                  (std::string("quiver_sandbox_") + info->test_suite_name() + "_" + info->name());
        std::filesystem::remove_all(sandbox_path);
        std::filesystem::create_directories(sandbox_path);
    }
    void TearDown() override {
        std::filesystem::remove_all(sandbox_path);
    }

    std::string db_path() const {
        return (sandbox_path / "test.db").string();
    }

    std::filesystem::path sandbox_path;
};

// Asserts that the script throws and that the error message carries the expected substring —
// EXPECT_THROW alone passes vacuously when a removed function raises "attempt to call a nil value".
inline void expect_sandbox_error(quiver::Sandbox& sandbox, const std::string& script, const std::string& substring) {
    try {
        sandbox.run(script);
        FAIL() << "expected script to throw: " << script;
    } catch (const std::exception& e) {
        EXPECT_NE(std::string(e.what()).find(substring), std::string::npos) << e.what();
    }
}

#endif  // QUIVER_TEST_SANDBOX_H
