#include <filesystem>
#include <gtest/gtest.h>
#include <psr/database.h>
#include <psr/migration.h>
#include <psr/migrations.h>
#include <string>

namespace fs = std::filesystem;

namespace database_utils {

class MigrationFixture : public ::testing::Test {
protected:
    void SetUp() override {
        path = (fs::temp_directory_path() / "psr_test.db").string();
        migrations_path = (fs::path(__FILE__).parent_path() / "chaining_migrations").string();
    }
    void TearDown() override {
        if (fs::exists(path))
            fs::remove(path);
    }
    std::string path;
    std::string migrations_path;
};

// ============================================================================
// Migration class tests
// ============================================================================

TEST_F(MigrationFixture, MigrationCreation) {
    psr::Migration migration(1, migrations_path + "/1");
    EXPECT_EQ(migration.version(), 1);
    EXPECT_FALSE(migration.path().empty());
}

TEST_F(MigrationFixture, MigrationUpSqlRead) {
    psr::Migration migration(1, migrations_path + "/1");
    std::string sql = migration.up_sql();
    EXPECT_FALSE(sql.empty());
    EXPECT_NE(sql.find("CREATE TABLE Test1"), std::string::npos);
}

TEST_F(MigrationFixture, MigrationDownSqlRead) {
    psr::Migration migration(1, migrations_path + "/1");
    std::string sql = migration.down_sql();
    EXPECT_FALSE(sql.empty());
    EXPECT_NE(sql.find("DROP TABLE"), std::string::npos);
}

TEST_F(MigrationFixture, MigrationComparison) {
    psr::Migration m1(1, migrations_path + "/1");
    psr::Migration m2(2, migrations_path + "/2");
    psr::Migration m3(3, migrations_path + "/3");

    EXPECT_TRUE(m1 < m2);
    EXPECT_TRUE(m2 < m3);
    EXPECT_TRUE(m1 < m3);
    EXPECT_FALSE(m2 < m1);

    EXPECT_TRUE(m1 == m1);
    EXPECT_FALSE(m1 == m2);
    EXPECT_TRUE(m1 != m2);
}

TEST_F(MigrationFixture, MigrationCopy) {
    psr::Migration original(2, migrations_path + "/2");
    psr::Migration copy = original;

    EXPECT_EQ(copy.version(), original.version());
    EXPECT_EQ(copy.path(), original.path());
}

// ============================================================================
// Migrations class tests
// ============================================================================

TEST_F(MigrationFixture, MigrationsLoad) {
    psr::Migrations migrations(migrations_path);

    EXPECT_FALSE(migrations.empty());
    EXPECT_EQ(migrations.count(), 3u);
    EXPECT_EQ(migrations.latest_version(), 3);
}

TEST_F(MigrationFixture, MigrationsOrder) {
    psr::Migrations migrations(migrations_path);
    auto all = migrations.all();

    ASSERT_EQ(all.size(), 3u);
    EXPECT_EQ(all[0].version(), 1);
    EXPECT_EQ(all[1].version(), 2);
    EXPECT_EQ(all[2].version(), 3);
}

TEST_F(MigrationFixture, MigrationsPending) {
    psr::Migrations migrations(migrations_path);

    auto pending_from_0 = migrations.pending(0);
    EXPECT_EQ(pending_from_0.size(), 3u);

    auto pending_from_1 = migrations.pending(1);
    EXPECT_EQ(pending_from_1.size(), 2u);
    EXPECT_EQ(pending_from_1[0].version(), 2);

    auto pending_from_2 = migrations.pending(2);
    EXPECT_EQ(pending_from_2.size(), 1u);
    EXPECT_EQ(pending_from_2[0].version(), 3);

    auto pending_from_3 = migrations.pending(3);
    EXPECT_TRUE(pending_from_3.empty());
}

TEST_F(MigrationFixture, MigrationsIteration) {
    psr::Migrations migrations(migrations_path);

    int64_t expected_version = 1;
    for (const auto& migration : migrations) {
        EXPECT_EQ(migration.version(), expected_version);
        ++expected_version;
    }
    EXPECT_EQ(expected_version, 4);
}

TEST_F(MigrationFixture, MigrationsEmptyPath) {
    psr::Migrations migrations("non_existent_path");
    EXPECT_TRUE(migrations.empty());
    EXPECT_EQ(migrations.count(), 0u);
    EXPECT_EQ(migrations.latest_version(), 0);
}

// ============================================================================
// Database migration tests
// ============================================================================

TEST_F(MigrationFixture, DatabaseCurrentVersion) {
    psr::Database db(":memory:");
    EXPECT_EQ(db.current_version(), 0);
}

TEST_F(MigrationFixture, DatabaseFromMigrations) {
    auto db = psr::Database::from_migrations(path, migrations_path);

    EXPECT_EQ(db.current_version(), 3);
    EXPECT_TRUE(db.is_healthy());
}

}  // namespace database_utils
