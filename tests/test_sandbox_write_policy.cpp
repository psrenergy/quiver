#include "test_sandbox.h"

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

// The write half of the Lua file sandbox: on top of resolve_sandbox_path's containment, a Lua
// writer may never target the database file or its journals, and none may run when the database
// was opened read-only. Every writer goes through one gate, so each one is checked here.

class SandboxWritePolicyTest : public LuaSandboxTest {
protected:
    void SetUp() override {
        LuaSandboxTest::SetUp();
        schema = VALID_SCHEMA("collections.sql");
    }

    // Builds, in a read-write session, what the tests read back: a populated database, a CSV
    // file and a binary file (bin_a.qvr / bin_a.toml).
    void seed() const {
        auto db = quiver::Database::from_schema(db_path(), schema);
        db.create_element("Configuration", quiver::Element().set("label", "Config"));
        db.create_element("Collection", quiver::Element().set("label", "Item 1").set("some_integer", int64_t{10}));
        quiver::Sandbox sandbox(db);
        sandbox.run(R"(
            local w = db:write_csv('a.csv', { header = { 'x' } })
            w:write_row({ 1 })
            w:close()
            local md = quiver.metadata{ initial_datetime='2025-01-01T00:00:00', unit='x',
                labels={'v'}, dimensions={'row'}, dimension_sizes={3} }
            local f = db:open_file('bin_a', 'w', md)
            f:write({1.0}, {row=1})
            f:close()
        )");
    }

    std::string schema;
};

TEST_F(SandboxWritePolicyTest, WritersRefuseTheDatabaseFileAndItsJournals) {
    seed();
    const auto size_before = fs::file_size(db_path());

    auto db = quiver::Database(db_path());
    quiver::Sandbox sandbox(db);
    for (const std::string name : {"test.db", "test.db-journal", "test.db-wal", "test.db-shm"}) {
        expect_sandbox_error(
            sandbox,
            "db:write_csv('" + name + "')",
            "Cannot write_csv: path '" + name + "' is the database file or one of its journals"
        );
        expect_sandbox_error(
            sandbox,
            "db:export_csv('Collection', '', '" + name + "')",
            "Cannot export_csv: path '" + name + "' is the database file or one of its journals"
        );
    }
    // Inside a host transaction too: a rollback cannot restore a truncated file.
    db.begin_transaction();
    expect_sandbox_error(sandbox, "db:write_csv('./test.db')", "is the database file or one of its journals");
    db.rollback();

    EXPECT_EQ(fs::file_size(db_path()), size_before);
    EXPECT_EQ(db.read_scalar_strings("Collection", "label").size(), 1);
}

// A writer that creates the parent directories (export_csv) could otherwise make a directory named
// like the journal, after which SQLite can no longer open its journal and every write fails.
TEST_F(SandboxWritePolicyTest, JournalNameAsADirectoryIsRefused) {
    seed();
    auto db = quiver::Database(db_path());
    quiver::Sandbox sandbox(db);
    expect_sandbox_error(
        sandbox,
        "db:export_csv('Collection', '', 'test.db-journal/x.csv')",
        "Cannot export_csv: path 'test.db-journal/x.csv' is the database file or one of its journals"
    );
    expect_sandbox_error(sandbox, "db:write_csv('Test.DB-WAL/x.csv')", "is the database file or one of its journals");
    EXPECT_FALSE(fs::exists(sandbox_path / "test.db-journal"));
    sandbox.run("db:export_csv('Collection', '', 'sub/test.db-journal')");
    EXPECT_TRUE(fs::exists(sandbox_path / "sub" / "test.db-journal"));
}

// Win32 opens "x." and "x " as x and "x::$DATA" as x's own data, and "x::$INDEX_ALLOCATION" makes
// directory x, so on Windows these spellings would create the journal (or a directory in its place).
TEST_F(SandboxWritePolicyTest, WindowsSpellingsOfTheJournalAreRefused) {
    seed();
    auto db = quiver::Database(db_path());
    quiver::Sandbox sandbox(db);
    for (
        const std::string name :
        {"test.db-journal.",
         "test.db-wal ",
         "test.db-journal::$DATA",
         "test.db-journal./x.csv",
         "test.db-journal::$INDEX_ALLOCATION/x.csv"}
    ) {
        expect_sandbox_error(
            sandbox,
            "db:export_csv('Collection', '', '" + name + "')",
            "is the database file or one of its journals"
        );
    }
    EXPECT_FALSE(fs::exists(sandbox_path / "test.db-journal"));
    EXPECT_FALSE(fs::exists(sandbox_path / "test.db-wal"));
}

// NTFS and APFS match names case-insensitively, so the check folds case on every platform.
TEST_F(SandboxWritePolicyTest, DatabaseFileMatchIgnoresCase) {
    seed();
    auto db = quiver::Database(db_path());
    quiver::Sandbox sandbox(db);
    expect_sandbox_error(sandbox, "db:write_csv('TEST.DB')", "is the database file or one of its journals");
    expect_sandbox_error(sandbox, "db:write_csv('Test.Db-Journal')", "is the database file or one of its journals");
}

// A binary writer appends its extensions, so a database named like its output is refused too.
TEST_F(SandboxWritePolicyTest, BinaryWritersCheckTheFilesTheyCreate) {
    const auto qvr_db = (sandbox_path / "study.qvr").string();
    {
        auto db = quiver::Database::from_schema(qvr_db, schema);
    }
    auto db = quiver::Database(qvr_db);
    quiver::Sandbox sandbox(db);
    expect_sandbox_error(
        sandbox,
        "db:open_file('study', 'w', quiver.metadata{ initial_datetime='2025-01-01T00:00:00', unit='x',"
        " labels={'v'}, dimensions={'row'}, dimension_sizes={3} })",
        "Cannot open_file: path 'study' is the database file or one of its journals"
    );
    expect_sandbox_error(sandbox, "db:csv_to_bin('study')", "Cannot csv_to_bin: path 'study' is the database file");
}

TEST_F(SandboxWritePolicyTest, NamesThatOnlyShareThePrefixAreWritable) {
    seed();
    auto db = quiver::Database(db_path());
    quiver::Sandbox sandbox(db);
    sandbox.run(R"(
        db:write_csv('test.db.csv'):close()
        db:write_csv('test.db-journal.csv'):close()
    )");
    EXPECT_TRUE(fs::exists(sandbox_path / "test.db.csv"));
    EXPECT_TRUE(fs::exists(sandbox_path / "test.db-journal.csv"));
}

TEST_F(SandboxWritePolicyTest, ReadOnlyDatabaseRefusesEveryWriter) {
    seed();
    auto db = quiver::Database(db_path(), {.read_only = true});
    quiver::Sandbox sandbox(db);

    const std::vector<std::pair<std::string, std::string>> writers = {
        {"write_csv", "db:write_csv('out.csv')"},
        {"export_csv", "db:export_csv('Collection', '', 'out.csv')"},
        {"open_file", "db:open_file('bin_b', 'w', db:open_file('bin_a', 'r'):get_metadata())"},
        {"bin_to_csv", "db:bin_to_csv('bin_a')"},
        {"csv_to_bin", "db:csv_to_bin('bin_a')"},
        {"save", "db:open_file('bin_a', 'r'):save('out')"},
        {"save", "(db:open_file('bin_a', 'r') + 1):save('out')"},
    };
    for (const auto& [op, script] : writers) {
        expect_sandbox_error(
            sandbox,
            script,
            "Cannot " + op + ": database is open read-only, so scripts cannot write files"
        );
    }
    EXPECT_FALSE(fs::exists(sandbox_path / "out.csv"));
    EXPECT_FALSE(fs::exists(sandbox_path / "bin_a.csv"));
    EXPECT_FALSE(fs::exists(sandbox_path / "bin_b.qvr"));
    EXPECT_FALSE(fs::exists(sandbox_path / "out.qvr"));
}

TEST_F(SandboxWritePolicyTest, ReadOnlyDatabaseStillReadsFiles) {
    seed();
    auto db = quiver::Database(db_path(), {.read_only = true});
    quiver::Sandbox sandbox(db);
    auto result = sandbox.run(R"(
        local csv = db:read_csv('a.csv')
        local f = db:open_file('bin_a', 'r')
        local cell = f:read({row=1})
        f:close()
        return { rows = #csv.rows, cell = cell[1] }
    )");
    EXPECT_EQ(result, R"({"cell":1,"rows":1})");
}
