#include "test_utils.h"

#include <gtest/gtest.h>
#include <quiver/database.h>
#include <quiver/element.h>

TEST(DatabaseTransaction, BeginMultipleWritesCommit) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    // Configuration required first
    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    EXPECT_FALSE(db.in_transaction());

    db.begin_transaction();
    EXPECT_TRUE(db.in_transaction());

    // Create two Collection elements inside the transaction
    quiver::Element e1;
    e1.set("label", std::string("Item 1"));
    db.create_element("Collection", e1);

    quiver::Element e2;
    e2.set("label", std::string("Item 2"));
    db.create_element("Collection", e2);

    db.commit();
    EXPECT_FALSE(db.in_transaction());

    // Verify both elements persist
    auto labels = db.read_scalar_strings("Collection", "label");
    EXPECT_EQ(labels.size(), 2);
    EXPECT_EQ(labels[0], "Item 1");
    EXPECT_EQ(labels[1], "Item 2");
}

TEST(DatabaseTransaction, BeginMultipleWritesRollback) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    // Configuration required first
    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    db.begin_transaction();

    // Create two Collection elements inside the transaction
    quiver::Element e1;
    e1.set("label", std::string("Item 1"));
    db.create_element("Collection", e1);

    quiver::Element e2;
    e2.set("label", std::string("Item 2"));
    db.create_element("Collection", e2);

    db.rollback();
    EXPECT_FALSE(db.in_transaction());

    // Verify neither element exists
    auto labels = db.read_scalar_strings("Collection", "label");
    EXPECT_TRUE(labels.empty());
}

TEST(DatabaseTransaction, WriteMethodsInsideTransaction) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    // Configuration required first
    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // Create one Collection element outside transaction
    quiver::Element e1;
    e1.set("label", std::string("Item 1"));
    int64_t id1 = db.create_element("Collection", e1);

    db.begin_transaction();

    // Exercise multiple write method types inside explicit transaction
    quiver::Element update1;
    update1.set("value_int", std::vector<int64_t>{10, 20, 30});
    update1.set("tag", std::vector<std::string>{"alpha", "beta"});
    db.update_element("Collection", id1, update1);

    // Update time series group
    std::vector<std::map<std::string, quiver::Value>> ts_rows = {
        {{"date_time", std::string("2024-01-01T10:00:00")}, {"value", 1.5}},
        {{"date_time", std::string("2024-01-02T10:00:00")}, {"value", 2.5}},
    };
    db.update_time_series_group("Collection", "data", id1, ts_rows);

    // Create a second element inside transaction
    quiver::Element e2;
    e2.set("label", std::string("Item 2"));
    db.create_element("Collection", e2);

    db.commit();

    // Verify all writes persisted
    auto vec = db.read_vector_integers_by_id("Collection", "value_int", id1);
    EXPECT_EQ(vec, (std::vector<int64_t>{10, 20, 30}));

    auto tags = db.read_set_strings_by_id("Collection", "tag", id1);
    std::sort(tags.begin(), tags.end());
    EXPECT_EQ(tags, (std::vector<std::string>{"alpha", "beta"}));

    auto ts = db.read_time_series_group("Collection", "data", id1);
    EXPECT_EQ(ts.size(), 2);
    EXPECT_DOUBLE_EQ(std::get<double>(ts[0].at("value")), 1.5);

    auto labels = db.read_scalar_strings("Collection", "label");
    EXPECT_EQ(labels.size(), 2);
}

TEST(DatabaseTransaction, RollbackUndoesMixedWrites) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    // Configuration required first
    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);

    // Create one Collection element and set its vector outside transaction
    quiver::Element e1;
    e1.set("label", std::string("Item 1"));
    int64_t id1 = db.create_element("Collection", e1);
    quiver::Element update_before;
    update_before.set("value_int", std::vector<int64_t>{1, 2, 3});
    db.update_element("Collection", id1, update_before);

    db.begin_transaction();

    // Update vectors and create another element
    quiver::Element update;
    update.set("value_int", std::vector<int64_t>{99, 98, 97});
    db.update_element("Collection", id1, update);

    quiver::Element e2;
    e2.set("label", std::string("Item 2"));
    db.create_element("Collection", e2);

    db.rollback();

    // Vector should be unchanged (original values)
    auto vec = db.read_vector_integers_by_id("Collection", "value_int", id1);
    EXPECT_EQ(vec, (std::vector<int64_t>{1, 2, 3}));

    // Second element should not exist
    auto labels = db.read_scalar_strings("Collection", "label");
    EXPECT_EQ(labels.size(), 1);
    EXPECT_EQ(labels[0], "Item 1");
}

TEST(DatabaseTransaction, DoubleBeginThrows) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    db.begin_transaction();

    try {
        db.begin_transaction();
        FAIL() << "Expected std::runtime_error";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "Cannot begin_transaction: transaction already active");
    }

    // Clean up
    db.rollback();
}

TEST(DatabaseTransaction, CommitWithoutBeginThrows) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    try {
        db.commit();
        FAIL() << "Expected std::runtime_error";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "Cannot commit: no active transaction");
    }
}

TEST(DatabaseTransaction, RollbackWithoutBeginThrows) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    try {
        db.rollback();
        FAIL() << "Expected std::runtime_error";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "Cannot rollback: no active transaction");
    }
}

TEST(DatabaseTransaction, InTransactionReflectsState) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    // Initially false
    EXPECT_FALSE(db.in_transaction());

    // True after begin
    db.begin_transaction();
    EXPECT_TRUE(db.in_transaction());

    // False after commit
    db.commit();
    EXPECT_FALSE(db.in_transaction());

    // True after begin again
    db.begin_transaction();
    EXPECT_TRUE(db.in_transaction());

    // False after rollback
    db.rollback();
    EXPECT_FALSE(db.in_transaction());
}

// ============================================================================
// Dry runs
// ============================================================================

namespace {

quiver::Database dry_run_database() {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("collections.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    quiver::Element config;
    config.set("label", std::string("Test Config"));
    db.create_element("Configuration", config);
    return db;
}

void create_collection(quiver::Database& db, const std::string& label) {
    quiver::Element element;
    element.set("label", label);
    db.create_element("Collection", element);
}

}  // namespace

TEST(DatabaseDryRun, WritesAreRolledBack) {
    auto db = dry_run_database();

    db.begin_dry_run();
    create_collection(db, "Item 1");
    create_collection(db, "Item 2");
    EXPECT_EQ(db.read_scalar_strings("Collection", "label").size(), 2);
    db.end_dry_run();

    EXPECT_TRUE(db.read_scalar_strings("Collection", "label").empty());
}

TEST(DatabaseDryRun, ReflectsStateAndHoldsARealTransaction) {
    auto db = dry_run_database();

    EXPECT_FALSE(db.in_dry_run());
    EXPECT_FALSE(db.in_transaction());

    db.begin_dry_run();
    EXPECT_TRUE(db.in_dry_run());
    // A real transaction is open, and in_transaction() must keep saying so.
    EXPECT_TRUE(db.in_transaction());

    db.end_dry_run();
    EXPECT_FALSE(db.in_dry_run());
    EXPECT_FALSE(db.in_transaction());
}

TEST(DatabaseDryRun, AbsorbsNestedTransactionCalls) {
    auto db = dry_run_database();

    db.begin_dry_run();

    // A nested begin/commit pair would normally throw "transaction already active".
    db.begin_transaction();
    create_collection(db, "Committed inside");
    db.commit();

    // A nested rollback is absorbed too: it does NOT partially undo.
    db.begin_transaction();
    create_collection(db, "Rolled back inside");
    db.rollback();

    EXPECT_EQ(db.read_scalar_strings("Collection", "label").size(), 2);

    db.end_dry_run();

    // The dry run undoes everything regardless of what the nested calls asked for.
    EXPECT_TRUE(db.read_scalar_strings("Collection", "label").empty());
}

TEST(DatabaseDryRun, RejectsNestedDryRun) {
    auto db = dry_run_database();

    db.begin_dry_run();
    try {
        db.begin_dry_run();
        FAIL() << "Expected std::runtime_error";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "Cannot begin_dry_run: dry run already active");
    }
    db.end_dry_run();
}

TEST(DatabaseDryRun, RejectsStartWhileTransactionActive) {
    auto db = dry_run_database();

    db.begin_transaction();
    try {
        db.begin_dry_run();
        FAIL() << "Expected std::runtime_error";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "Cannot begin_dry_run: transaction already active");
    }
    db.rollback();
}

TEST(DatabaseDryRun, RejectsEndWithoutStart) {
    auto db = dry_run_database();

    try {
        db.end_dry_run();
        FAIL() << "Expected std::runtime_error";
    } catch (const std::runtime_error& e) {
        EXPECT_STREQ(e.what(), "Cannot end_dry_run: no active dry run");
    }
}

TEST(DatabaseDryRun, ImportCsvStillRefusesToNest) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("csv_export.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    db.begin_dry_run();
    // import_csv toggles PRAGMA foreign_keys, which is a no-op mid-transaction — the dry run's
    // transaction is a real one, so the existing precondition still fires.
    EXPECT_THROW(db.import_csv("Items", "", "does_not_matter.csv"), std::runtime_error);
    db.end_dry_run();
}
