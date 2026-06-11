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
