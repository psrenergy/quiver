#include <filesystem>
#include <gtest/gtest.h>
#include <quiver/database.h>
#include <quiver/element.h>
#include <quiver/options.h>

namespace fs = std::filesystem;

class IssuesFixture : public ::testing::Test {
protected:
    void SetUp() override {
        schemas_path = (fs::path(__FILE__).parent_path() / "schemas").string();
        issues_path = (fs::path(__FILE__).parent_path() / "schemas" / "issues").string();
    }
    std::string schemas_path;
    std::string issues_path;
};

TEST_F(IssuesFixture, Issue52) {
    auto migrations_path = issues_path + "/issue52";

    EXPECT_THROW(quiver::Database::from_migrations(":memory:", migrations_path), std::runtime_error);
}

TEST_F(IssuesFixture, Issue161) {
    auto migrations_path = schemas_path + "/migrations";
    auto schema_path = schemas_path + "/valid/basic.sql";

    quiver::DatabaseOptions options;
    options.read_only = true;
    EXPECT_THROW(quiver::Database::from_migrations(":memory:", migrations_path, options), std::runtime_error);
    EXPECT_THROW(quiver::Database::from_schema(":memory:", schema_path, options), std::runtime_error);
}
