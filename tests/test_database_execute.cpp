#include "database_fixture.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <psr/database.h>
#include <string>

namespace fs = std::filesystem;

TEST_F(DatabaseFixture, OpenInMemory) {
    psr::Database db(":memory:");
    EXPECT_TRUE(db.is_open());


}
