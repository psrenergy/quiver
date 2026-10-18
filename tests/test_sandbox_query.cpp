#include "test_sandbox.h"

TEST_F(SandboxTest, QueryString) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1").set("some_integer", int64_t{42}));

    quiver::Sandbox sandbox(db);

    sandbox.run(R"(
        local label = db:query_string("SELECT label FROM Collection WHERE label = ?", {"Item 1"})
        assert(label == "Item 1", "Expected 'Item 1', got " .. tostring(label))
    )");
}

TEST_F(SandboxTest, QueryStringNoParams) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1"));

    quiver::Sandbox sandbox(db);

    sandbox.run(R"(
        local label = db:query_string("SELECT label FROM Collection")
        assert(label == "Item 1", "Expected 'Item 1', got " .. tostring(label))
    )");
}

TEST_F(SandboxTest, QueryStringReturnsNil) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::Sandbox sandbox(db);

    sandbox.run(R"(
        local result = db:query_string("SELECT label FROM Collection WHERE 1 = 0")
        assert(result == nil, "Expected nil, got " .. tostring(result))
    )");
}

TEST_F(SandboxTest, QueryInteger) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1").set("some_integer", int64_t{42}));

    quiver::Sandbox sandbox(db);

    sandbox.run(R"(
        local val = db:query_integer("SELECT some_integer FROM Collection WHERE label = ?", {"Item 1"})
        assert(val == 42, "Expected 42, got " .. tostring(val))
    )");
}

TEST_F(SandboxTest, QueryIntegerCount) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1"));
    db.create_element("Collection", quiver::Element().set("label", "Item 2"));

    quiver::Sandbox sandbox(db);

    sandbox.run(R"(
        local count = db:query_integer("SELECT COUNT(*) FROM Collection")
        assert(count == 2, "Expected 2, got " .. tostring(count))
    )");
}

TEST_F(SandboxTest, QueryFloat) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1").set("some_float", 3.14));

    quiver::Sandbox sandbox(db);

    sandbox.run(R"(
        local val = db:query_float("SELECT some_float FROM Collection WHERE label = ?", {"Item 1"})
        assert(val == 3.14, "Expected 3.14, got " .. tostring(val))
    )");
}

TEST_F(SandboxTest, QueryWithMultipleParams) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1").set("some_integer", int64_t{10}));
    db.create_element("Collection", quiver::Element().set("label", "Item 2").set("some_integer", int64_t{20}));

    quiver::Sandbox sandbox(db);

    sandbox.run(R"(
        local val = db:query_integer(
            "SELECT some_integer FROM Collection WHERE label = ? AND some_integer > ?",
            {"Item 2", 5}
        )
        assert(val == 20, "Expected 20, got " .. tostring(val))
    )");
}

TEST_F(SandboxTest, IsHealthy) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::Sandbox sandbox(db);

    sandbox.run(R"(
        local healthy = db:is_healthy()
        assert(healthy == true, "Expected is_healthy to return true")
    )");
}

TEST_F(SandboxTest, CurrentVersion) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::Sandbox sandbox(db);

    sandbox.run(R"(
        local version = db:current_version()
        assert(version == 0, "Expected version 0 for a schema database, got " .. tostring(version))
    )");
}

TEST_F(SandboxTest, CurrentVersionAfterMigrations) {
    auto db = quiver::Database::from_migrations(":memory:", SCHEMA_PATH("schemas/migrations"));
    quiver::Sandbox sandbox(db);

    sandbox.run(R"(
        local version = db:current_version()
        assert(version == 3, "Expected version 3 after three migrations, got " .. tostring(version))
    )");
}

TEST_F(SandboxTest, Path) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::Sandbox sandbox(db);

    sandbox.run(R"(
        local p = db:path()
        assert(type(p) == "string", "Expected path to be a string")
        assert(p == ":memory:", "Expected ':memory:', got " .. p)
    )");
}

TEST_F(SandboxTest, Describe) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"));
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::Sandbox sandbox(db);

    // describe() returns a text report for agents
    sandbox.run(R"(
        local report = db:describe()
        assert(report:find("Collection: Configuration"), "describe should mention Configuration")
    )");
}

TEST_F(SandboxTest, QueryParameterCountMismatch) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1").set("some_integer", int64_t{42}));

    quiver::Sandbox sandbox(db);

    // Too few parameters for the single placeholder
    expect_sandbox_error(
        sandbox,
        R"(db:query_string("SELECT label FROM Collection WHERE some_integer = ?", {}))",
        "expected 1 bound"
    );

    // Too many parameters for the single placeholder
    expect_sandbox_error(
        sandbox,
        R"(db:query_string("SELECT label FROM Collection WHERE some_integer = ?", {42, 43}))",
        "expected 1 bound"
    );

    // Exactly one parameter succeeds
    sandbox.run(R"(
        local label = db:query_string("SELECT label FROM Collection WHERE some_integer = ?", {42})
        assert(label == "Item 1", "expected Item 1, got " .. tostring(label))
    )");
}

// Lua stores no key for a nil, so a query parameter table's length is the `#` border: an interior
// nil in a constructor ({ nil, 5 }) is counted and binds NULL, a trailing one ({ 5, nil }) is not.
TEST_F(SandboxTest, QueryInteriorNilParamBindsNull) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::Sandbox sandbox(db);

    sandbox.run(R"(
        local r = db:query_integer("SELECT CASE WHEN ? IS NULL THEN ? ELSE -1 END", { nil, 5 })
        assert(r == 5, "interior nil must bind NULL, got " .. tostring(r))
    )");
}

TEST_F(SandboxTest, QueryTrailingNilParamIsACountMismatch) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::Sandbox sandbox(db);

    expect_sandbox_error(
        sandbox,
        R"(db:query_integer("SELECT ? + ?", { 5, nil }))",
        "expected 2 bound parameter(s) but got 1"
    );
}

// A wrong-typed params argument used to be ignored, so the query ran with no parameters.
TEST_F(SandboxTest, QueryRejectsWrongTypedParams) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::Sandbox sandbox(db);

    for (const std::string op : {"query_string", "query_integer", "query_float"}) {
        expect_sandbox_error(
            sandbox,
            "db:" + op + "('SELECT 1', 5)",
            "Cannot " + op + ": params must be a table, got number"
        );
        expect_sandbox_error(
            sandbox,
            "db:" + op + "('SELECT 1', db)",
            "Cannot " + op + ": params must be a table, got userdata"
        );
    }
    EXPECT_EQ(sandbox.run("return db:query_integer('SELECT 1', nil)"), "1");
    EXPECT_EQ(sandbox.run("return db:query_integer('SELECT ?', { 7 })"), "7");
}

// ============================================================================
// query_* only runs read-only SQL: SQLite's authorizer refuses writes, PRAGMA, ATTACH, DETACH and
// transaction control while the statement compiles, and sqlite3_stmt_readonly refuses VACUUM, which
// the authorizer never sees. Everything else a script can reach goes through the typed db: API.
// ============================================================================

namespace {

constexpr auto kNotReadOnly =
    "Cannot query: SQL must be read-only; writes, PRAGMA, ATTACH, VACUUM and transaction control are not allowed";

}

TEST_F(SandboxTest, QueryAllowsReadOnlyStatements) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    quiver::Sandbox sandbox(db);

    EXPECT_EQ(
        sandbox.run(
            R"(return db:query_integer("WITH RECURSIVE n(x) AS (SELECT 1 UNION ALL SELECT x + 1 FROM n WHERE x < 5) SELECT SUM(x) FROM n"))"
        ),
        "15"
    );
    // The table-valued pragma functions are reads, unlike PRAGMA statements.
    sandbox.run(R"lua(
        local columns = db:query_integer("SELECT COUNT(*) FROM pragma_table_info('Configuration')")
        assert(columns > 0, "pragma_table_info must stay available, got " .. tostring(columns))
        assert(type(db:query_integer("SELECT total_changes()")) == "number")
    )lua");
}

TEST_F(SandboxTest, QueryRefusesWritesPragmaAndTransactionControl) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Item 1").set("some_integer", int64_t{1}));
    quiver::Sandbox sandbox(db);

    for (
        const std::string sql :
        {"INSERT INTO Collection (label) VALUES ('x')",
         "UPDATE Collection SET some_integer = 2",
         "DELETE FROM Collection",
         "CREATE TABLE t (x)",
         "DROP TABLE Collection",
         "ALTER TABLE Collection ADD COLUMN extra INTEGER",
         "PRAGMA user_version",
         "PRAGMA foreign_keys = OFF",
         "BEGIN",
         "COMMIT",
         "ROLLBACK",
         "SAVEPOINT s",
         "VACUUM"}
    ) {
        expect_sandbox_error(sandbox, "db:query_integer(\"" + sql + "\")", kNotReadOnly);
    }
    EXPECT_FALSE(db.in_transaction());
    EXPECT_EQ(db.query_integer("SELECT some_integer FROM Collection"), 1);
}

TEST_F(LuaSandboxTest, QueryRefusesAttachAndVacuumInto) {
    auto db = quiver::Database::from_schema(db_path(), VALID_SCHEMA("collections.sql"));
    quiver::Sandbox sandbox(db);
    const auto escape = (sandbox_path / "escape.db").generic_string();

    expect_sandbox_error(sandbox, "db:query_integer(\"ATTACH DATABASE '" + escape + "' AS e\")", kNotReadOnly);
    expect_sandbox_error(sandbox, "db:query_integer(\"VACUUM INTO '" + escape + "'\")", kNotReadOnly);
    EXPECT_FALSE(std::filesystem::exists(escape));
}

// A raw COMMIT used to end the host's transaction from inside the block; now the block fails and rolls back.
TEST_F(SandboxTest, QueryCommitInsideTransactionBlockRollsBack) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    quiver::Sandbox sandbox(db);

    expect_sandbox_error(
        sandbox,
        R"(
            db:transaction(function(d)
                d:create_element("Collection", { label = "Item 1" })
                d:query_integer("COMMIT")
            end)
        )",
        kNotReadOnly
    );
    EXPECT_FALSE(db.in_transaction());
    EXPECT_EQ(db.query_integer("SELECT COUNT(*) FROM Collection"), 0);
}

// The authorizer is installed only while a query_* statement compiles: the schema load, the typed
// writers (which run BEGIN/COMMIT and PRAGMA themselves) and later queries are untouched by a refusal.
TEST_F(SandboxTest, QueryRestrictionDoesNotOutliveTheStatement) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::Sandbox sandbox(db);

    expect_sandbox_error(sandbox, R"(db:query_integer("PRAGMA user_version"))", kNotReadOnly);
    sandbox.run(R"(
        db:create_element("Configuration", { label = "Config" })
        db:transaction(function(d) d:create_element("Collection", { label = "Item 1" }) end)
        assert(db:current_version() == 0)
        assert(db:query_integer("SELECT COUNT(*) FROM Collection") == 1)
    )");
    expect_sandbox_error(sandbox, R"(db:query_integer("SELEC 1"))", "Failed to prepare statement");
}
