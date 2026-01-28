#include <gtest/gtest.h>
#include <quiver/database.h>
#include <quiver/element.h>
#include <filesystem>

namespace fs = std::filesystem;

class IssuesFixture : public ::testing::Test {
protected:
    void SetUp() override {
        issues_path = (fs::path(__FILE__).parent_path() / "schemas" / "issues").string();
    }
    std::string issues_path;
};

// Issue 52: Configuration table without label column should fail validation
TEST_F(IssuesFixture, Issue52) {
    auto migrations_path = issues_path + "/issue52";

    EXPECT_THROW(
        quiver::Database::from_migrations(":memory:", migrations_path),
        std::runtime_error
    );
}
