#include "database_fixture.h"

#include <gtest/gtest.h>
#include <psr/database.h>
#include <string>

namespace database_utils {

TEST_F(DatabaseFixture, CurrentVersion) {
    psr::Database db(":memory:");

    db.execute("PRAGMA user_version = 1;");
    int version = db.current_version();
    EXPECT_EQ(version, 1);

    db.execute("PRAGMA user_version = 42;");
    version = db.current_version();
    EXPECT_EQ(version, 42);
}

}  // namespace database_utils