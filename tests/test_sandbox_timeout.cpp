#include "test_sandbox.h"

#include <chrono>
#include <cstdint>
#include <fstream>

namespace {

constexpr int64_t kLimitMs = 100;
const std::string kTimeoutMessage = "Cannot run: script exceeded its time limit of 100 ms";

// Exactly the timeout message: no "Failed to run Lua script" wrapper, no Lua position prefix.
void expect_timeout(quiver::Sandbox& sandbox, const std::string& script) {
    try {
        sandbox.run(script, kLimitMs);
        FAIL() << "expected script to time out: " << script;
    } catch (const std::exception& e) {
        EXPECT_EQ(std::string(e.what()), kTimeoutMessage);
    }
}

// More than kTimeLimitCheckEvery (10000) VM steps, so a progress handler left installed would fire.
const std::string kLongQuery =
    "WITH RECURSIVE c(x) AS (SELECT 1 UNION ALL SELECT x + 1 FROM c WHERE x < 100000) SELECT count(*) FROM c";

}  // namespace

TEST_F(SandboxTest, TimeLimitStopsInfiniteLoop) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::Sandbox sandbox(db);

    const auto start = std::chrono::steady_clock::now();
    expect_timeout(sandbox, "while true do end");
    EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::milliseconds(2000));
}

TEST_F(SandboxTest, TimeLimitCannotBeSwallowedByPcall) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::Sandbox sandbox(db);

    expect_timeout(sandbox, "while true do pcall(function() while true do end end) end");
}

TEST_F(SandboxTest, TimeLimitCannotBeSwallowedByXpcall) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::Sandbox sandbox(db);

    expect_timeout(sandbox, "while true do xpcall(function() while true do end end, function(e) return e end) end");
}

TEST_F(SandboxTest, TimeLimitPcallKeepsStockResults) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::Sandbox sandbox(db);

    EXPECT_EQ(
        sandbox.run(
            R"(
                local a = table.pack(pcall(function(...) return ... end, 1, nil, 3, nil))
                local b = table.pack(pcall(error, "boom", 0))
                local c = table.pack(xpcall(function() error("x", 0) end, function(e) return e .. "!" end))
                return { a.n, a[1], a[4], b.n, b[1], b[2], c.n, c[2] }
            )",
            kLimitMs
        ),
        R"([5,true,3,2,false,"boom",2,"x!"])"
    );
}

TEST_F(SandboxTest, TimeLimitStopsCoroutine) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::Sandbox sandbox(db);

    expect_timeout(sandbox, R"(
        while true do
            coroutine.resume(coroutine.create(function() while true do end end))
        end
    )");
}

// The query is one C++ call, so only SQLite's progress handler can stop it.
TEST_F(SandboxTest, TimeLimitInterruptsRunawayQuery) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::Sandbox sandbox(db);

    expect_timeout(
        sandbox,
        "return db:query_integer('WITH RECURSIVE c(x) AS (SELECT 1 UNION ALL SELECT x + 1 FROM c) SELECT count(*) FROM "
        "c')"
    );
}

TEST_F(SandboxTest, TimeLimitStopsTransactionCallback) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    quiver::Sandbox sandbox(db);

    expect_timeout(sandbox, R"(
        db:transaction(function(db)
            db:create_element("Collection", { label = "Item 1" })
            while true do end
        end)
    )");

    EXPECT_FALSE(db.in_transaction());
    EXPECT_TRUE(db.read_element_ids("Collection").empty());
    EXPECT_EQ(sandbox.run(R"(return db:create_element("Collection", { label = "Item 2" }))"), "1");
}

TEST_F(SandboxTest, TimeLimitShortScriptReturnsNormally) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::Sandbox sandbox(db);

    EXPECT_EQ(sandbox.run("local s = 0 for i = 1, 10000 do s = s + i end return s", kLimitMs), "50005000");
}

TEST_F(SandboxTest, TimeLimitZeroMeansNoLimit) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::Sandbox sandbox(db);

    EXPECT_EQ(sandbox.run("local s = 0 for i = 1, 2e7 do s = s + i end return s", 0), "200000010000000");
}

TEST_F(SandboxTest, TimeLimitIsRemovedAfterTimeout) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::Sandbox sandbox(db);

    expect_timeout(sandbox, "while true do end");

    // Neither the host's next query nor the next unlimited run is interrupted.
    EXPECT_EQ(db.query_integer(kLongQuery), 100000);
    EXPECT_EQ(sandbox.run("local s = 0 for i = 1, 1000000 do s = s + i end return s"), "500000500000");
    EXPECT_EQ(sandbox.run("return db:query_integer('" + kLongQuery + "')"), "100000");
}

TEST_F(SandboxTest, TimeLimitNegativeIsRejected) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    quiver::Sandbox sandbox(db);

    try {
        sandbox.run("return 1", -1);
        FAIL() << "expected a negative timeout_ms to throw";
    } catch (const std::exception& e) {
        EXPECT_STREQ(e.what(), "Cannot run: timeout_ms must not be negative, got -1");
    }
}

class Sandbox_TimeLimit : public LuaSandboxTest {};

TEST_F(Sandbox_TimeLimit, StopsReadCsvStreamCallback) {
    auto db = quiver::Database::from_schema(db_path(), VALID_SCHEMA("collections.sql"));
    std::ofstream(sandbox_path / "rows.csv") << "a,b\n1,2\n3,4\n";
    quiver::Sandbox sandbox(db);

    expect_timeout(sandbox, R"(db:read_csv_stream("rows.csv", function(row) while true do end end))");

    EXPECT_FALSE(db.in_transaction());
    EXPECT_EQ(sandbox.run(R"(return #db:read_csv("rows.csv").rows)"), "2");
}

// SQLite rolls back the whole transaction when it interrupts a write, so the limit never interrupts
// one: a host transaction survives a write that runs past the deadline.
TEST_F(Sandbox_TimeLimit, DoesNotInterruptWrite) {
    auto db = quiver::Database::from_schema(db_path(), VALID_SCHEMA("collections.sql"));
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    db.create_element("Collection", quiver::Element().set("label", "Big"));
    quiver::test::exec_sql(
        db_path(),
        "INSERT INTO Collection_vector_values (id, vector_index, value_int) "
        "WITH RECURSIVE c(x) AS (SELECT 1 UNION ALL SELECT x + 1 FROM c WHERE x < 50000) SELECT 1, x, 7 FROM c"
    );

    db.begin_transaction();
    quiver::Sandbox sandbox(db);
    // One DELETE that cascades to 50000 vector rows: far more VM steps, and time, than a 1 ms limit.
    sandbox.run(R"(db:delete_element("Collection", 1))", 1);

    EXPECT_TRUE(db.in_transaction());
    EXPECT_TRUE(db.read_element_ids("Collection").empty());
    db.rollback();
    EXPECT_EQ(db.read_element_ids("Collection").size(), 1);
}
