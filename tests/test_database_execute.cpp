#include "database_fixture.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <psr/database.h>
#include <string>

namespace database_execute {

namespace fs = std::filesystem;

TEST_F(DatabaseFixture, CreateTable) {
    psr::Database db(":memory:");

    auto result = db.execute("CREATE TABLE test (id INTEGER PRIMARY KEY, name TEXT)");
    EXPECT_TRUE(result.empty());
    EXPECT_EQ(result.row_count(), 0);
}

}  // namespace database_execute