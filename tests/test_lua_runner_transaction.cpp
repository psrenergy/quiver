#include "test_lua_runner.h"

TEST_F(LuaRunnerTest, TransactionCommit) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

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

    quiver::LuaRunner lua(db);

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

    quiver::LuaRunner lua(db);

    EXPECT_THROW(
        {
            lua.run(R"(
                db:begin_transaction()
                db:begin_transaction()
            )");
        },
        std::runtime_error);
}

TEST_F(LuaRunnerTest, TransactionCommitWithoutBeginError) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

    EXPECT_THROW({ lua.run(R"(db:commit())"); }, std::runtime_error);
}

TEST_F(LuaRunnerTest, TransactionRollbackWithoutBeginError) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

    EXPECT_THROW({ lua.run(R"(db:rollback())"); }, std::runtime_error);
}

TEST_F(LuaRunnerTest, TransactionInTransaction) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

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

    quiver::LuaRunner lua(db);

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

    quiver::LuaRunner lua(db);

    EXPECT_THROW(
        {
            lua.run(R"(
                db:transaction(function(db)
                    db:create_element("Collection", { label = "Item 1", some_integer = 10 })
                    error("intentional error")
                end)
            )");
        },
        std::runtime_error);

    auto labels = db.read_scalar_strings("Collection", "label");
    EXPECT_EQ(labels.size(), 0);
}

TEST_F(LuaRunnerTest, TransactionBlockMultiOps) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

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
        if (v == 100)
            found100 = true;
        if (v == 20)
            found20 = true;
    }
    EXPECT_TRUE(found100);
    EXPECT_TRUE(found20);
}

// ============================================================================
// Dry runs
// ============================================================================

TEST_F(LuaRunnerTest, DryRunBlockRollsBack) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

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

    quiver::LuaRunner lua(db);

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

    quiver::LuaRunner lua(db);

    expect_lua_error(lua,
                     R"(
        db:dry_run(function(db)
            db:create_element("Collection", { label = "Item 1", some_integer = 10 })
            error("boom")
        end)
    )",
                     "boom");

    EXPECT_TRUE(db.read_scalar_strings("Collection", "label").empty());
    EXPECT_FALSE(db.in_dry_run());
    EXPECT_FALSE(db.in_transaction());
}

TEST_F(LuaRunnerTest, DryRunExplicitBeginEnd) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

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

TEST_F(LuaRunnerTest, HostDryRunWrapsWholeScript) {
    auto db = quiver::Database::from_schema(":memory:", collections_schema);
    db.create_element("Configuration", quiver::Element().set("label", "Config"));

    quiver::LuaRunner lua(db);

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
