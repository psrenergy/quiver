#include "test_lua_runner.h"

TEST_F(LuaRunnerTest, TransactionCommit) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::Sandbox lua(db);

    lua.run(R"(
        db:begin_transaction()
        db:create_element("Collection", { label = "Item 1", some_integer = 10 })
        db:commit()
    )");

    auto labels = db.read_scalar_strings("Collection", "label");
    EXPECT_EQ(labels.size(), 1);
    EXPECT_EQ(labels[0], "Item 1");
}

TEST_F(LuaRunnerTest, TransactionRollback) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::Sandbox lua(db);

    lua.run(R"(
        db:begin_transaction()
        db:create_element("Collection", { label = "Item 1", some_integer = 10 })
        db:rollback()
    )");

    auto labels = db.read_scalar_strings("Collection", "label");
    EXPECT_EQ(labels.size(), 0);
}

TEST_F(LuaRunnerTest, TransactionDoubleBeginError) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::Sandbox lua(db);

    expect_lua_error(
        lua,
        R"(
                db:begin_transaction()
                db:begin_transaction()
            )",
        "Cannot begin_transaction: transaction already active"
    );
}

TEST_F(LuaRunnerTest, TransactionCommitWithoutBeginError) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::Sandbox lua(db);

    expect_lua_error(lua, R"(db:commit())", "Cannot commit: no active transaction");
}

TEST_F(LuaRunnerTest, TransactionRollbackWithoutBeginError) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::Sandbox lua(db);

    expect_lua_error(lua, R"(db:rollback())", "Cannot rollback: no active transaction");
}

TEST_F(LuaRunnerTest, TransactionInTransaction) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::Sandbox lua(db);

    lua.run(R"(
        assert(db:in_transaction() == false, "Expected false before begin")
        db:begin_transaction()
        assert(db:in_transaction() == true, "Expected true after begin")
        db:commit()
        assert(db:in_transaction() == false, "Expected false after commit")
    )");
}

TEST_F(LuaRunnerTest, TransactionBlockAutoCommit) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::Sandbox lua(db);

    lua.run(R"(
        local result = db:transaction(function(db)
            db:create_element("Collection", { label = "Item 1", some_integer = 42 })
            return 42
        end)
        assert(result == 42, "Expected result 42, got " .. tostring(result))
    )");

    auto labels = db.read_scalar_strings("Collection", "label");
    EXPECT_EQ(labels.size(), 1);
    EXPECT_EQ(labels[0], "Item 1");
}

TEST_F(LuaRunnerTest, TransactionBlockRollbackOnError) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::Sandbox lua(db);

    expect_lua_error(
        lua,
        R"(
                db:transaction(function(db)
                    db:create_element("Collection", { label = "Item 1", some_integer = 10 })
                    error("intentional error")
                end)
            )",
        "intentional error"
    );

    auto labels = db.read_scalar_strings("Collection", "label");
    EXPECT_EQ(labels.size(), 0);
}

// A script that catches an error inside db:transaction commits whatever the failed call left
// behind, so a rejected update_element must leave nothing.
TEST_F(LuaRunnerTest, TransactionBlockCaughtRejectedUpdateWritesNothing) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));
    auto id = db.create_element("Collection", quiver::Element().set("label", "Item 1").set("some_integer", int64_t{1}));
    ASSERT_EQ(id, 1);

    quiver::Sandbox lua(db);
    lua.run(R"(
        db:transaction(function(db)
            local ok, err = pcall(function()
                db:update_element("Collection", 1, { some_integer = 2, tag = { 1.5 } })
            end)
            assert(ok == false, "expected update_element to fail")
            assert(err:find("type mismatch for array 'tag'", 1, true) ~= nil, "unexpected error: " .. tostring(err))
        end)
    )");

    auto value = db.read_scalar_integer_by_id("Collection", "some_integer", id);
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, 1);
}

TEST_F(LuaRunnerTest, TransactionBlockMultiOps) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::Sandbox lua(db);

    lua.run(R"(
        db:transaction(function(db)
            db:create_element("Collection", { label = "Item 1", some_integer = 10 })
            db:create_element("Collection", { label = "Item 2", some_integer = 20 })
            db:update_element("Collection", 1, { some_integer = 100 })
        end)
    )");

    auto labels = db.read_scalar_strings("Collection", "label");
    EXPECT_EQ(labels.size(), 2);

    auto integers = db.read_scalar_integers("Collection", "some_integer");
    EXPECT_EQ(integers.size(), 2);
    // After update, one should be 100 and the other 20
    bool found100 = false, found20 = false;
    for (auto v : integers) {
        if (v == 100) {
            found100 = true;
        }
        if (v == 20) {
            found20 = true;
        }
    }
    EXPECT_TRUE(found100);
    EXPECT_TRUE(found20);
}

// The argument is checked before the transaction opens, so a bad one neither leaves a transaction
// behind nor collides with one the script already opened.
TEST_F(LuaRunnerTest, TransactionBlockRejectsNonFunction) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    quiver::Sandbox lua(db);

    expect_lua_error(lua, "db:transaction(5)", "Cannot transaction: fn must be a function, got number");
    EXPECT_FALSE(db.in_transaction());

    expect_lua_error(
        lua,
        R"(
            db:begin_transaction()
            db:transaction("x")
        )",
        "Cannot transaction: fn must be a function, got string"
    );
    EXPECT_TRUE(db.in_transaction());
    db.rollback();

    expect_lua_error(
        lua,
        "db:transaction(setmetatable({}, { __call = function() end }))",
        "Cannot transaction: fn must be a function, got table"
    );
    EXPECT_FALSE(db.in_transaction());
}

// A deferred foreign key fails only at COMMIT, after the callback returned: the block must still be
// rolled back, not left open for the host to commit.
TEST_F(LuaRunnerTest, TransactionBlockCommitFailureRollsBack) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("relations.sql"));

    quiver::Sandbox lua(db);

    expect_lua_error(
        lua,
        R"lua(
            db:transaction(function(d)
                d:query_string("PRAGMA defer_foreign_keys = ON")
                d:query_string("INSERT INTO Child (label, parent_id) VALUES ('orphan', 999)")
            end)
        )lua",
        "Failed to commit transaction: FOREIGN KEY constraint failed"
    );
    EXPECT_FALSE(db.in_transaction());
    EXPECT_EQ(db.query_integer("SELECT COUNT(*) FROM Child"), 0);
}

// Errors raised by the closing call itself still reach the script with their own text.
TEST_F(LuaRunnerTest, ScopedBlockFinishErrorsStillSurface) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    quiver::Sandbox lua(db);

    expect_lua_error(lua, "db:transaction(function(d) d:commit() end)", "Cannot commit: no active transaction");
    EXPECT_FALSE(db.in_transaction());

    expect_lua_error(lua, "db:dry_run(function(d) d:end_dry_run() end)", "Cannot end_dry_run: no active dry run");
    EXPECT_FALSE(db.in_dry_run());
}

// ============================================================================
// Dry runs
// ============================================================================

TEST_F(LuaRunnerTest, DryRunBlockRollsBack) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::Sandbox lua(db);

    auto result = lua.run(R"(
        local inside = db:dry_run(function(db)
            db:create_element("Collection", { label = "Item 1", some_integer = 10 })
            return #db:read_element_ids("Collection")
        end)
        return { inside = inside, after = #db:read_element_ids("Collection") }
    )");

    // The block's return value passes through, and nothing survived it.
    EXPECT_EQ(result, R"({"after":0,"inside":1})");
    EXPECT_TRUE(db.read_scalar_strings("Collection", "label").empty());
}

TEST_F(LuaRunnerTest, DryRunAbsorbsNestedTransaction) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::Sandbox lua(db);

    // db:transaction is the pattern the Lua reference recommends; a dry run must not break it.
    auto result = lua.run(R"(
        return db:dry_run(function(db)
            db:transaction(function(db)
                db:create_element("Collection", { label = "Item 1", some_integer = 10 })
            end)
            return #db:read_element_ids("Collection")
        end)
    )");

    EXPECT_EQ(result, "1");
    EXPECT_TRUE(db.read_scalar_strings("Collection", "label").empty());
}

TEST_F(LuaRunnerTest, DryRunBlockRollsBackOnError) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::Sandbox lua(db);

    expect_lua_error(
        lua,
        R"(
        db:dry_run(function(db)
            db:create_element("Collection", { label = "Item 1", some_integer = 10 })
            error("boom")
        end)
    )",
        "boom"
    );

    EXPECT_TRUE(db.read_scalar_strings("Collection", "label").empty());
    EXPECT_FALSE(db.in_dry_run());
    EXPECT_FALSE(db.in_transaction());
}

TEST_F(LuaRunnerTest, DryRunExplicitBeginEnd) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::Sandbox lua(db);

    auto result = lua.run(R"(
        db:begin_dry_run()
        db:create_element("Collection", { label = "Item 1", some_integer = 10 })
        local active = db:in_dry_run()
        db:end_dry_run()
        return { active = active, after = db:in_dry_run() }
    )");

    EXPECT_EQ(result, R"({"active":true,"after":false})");
    EXPECT_TRUE(db.read_scalar_strings("Collection", "label").empty());
}

TEST_F(LuaRunnerTest, DryRunBlockRejectsNonFunction) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);

    quiver::Sandbox lua(db);

    expect_lua_error(lua, "db:dry_run(5)", "Cannot dry_run: fn must be a function, got number");
    EXPECT_FALSE(db.in_dry_run());
    EXPECT_FALSE(db.in_transaction());
}

TEST_F(LuaRunnerTest, HostDryRunWrapsWholeScript) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::Sandbox lua(db);

    // This is how a host previews a script it did not write.
    db.begin_dry_run();
    auto result = lua.run(R"(
        db:transaction(function(db)
            db:create_element("Collection", { label = "Item 1", some_integer = 10 })
        end)
        return db:read_element_ids("Collection")
    )");
    db.end_dry_run();

    EXPECT_EQ(result, "[1]");
    EXPECT_TRUE(db.read_scalar_strings("Collection", "label").empty());
}
