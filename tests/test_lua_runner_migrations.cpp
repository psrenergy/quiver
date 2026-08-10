#include "test_lua_runner.h"

#include <filesystem>

// db:test_migrations paths are sandboxed: relative paths resolve against the database directory.
class LuaRunner_Migrations : public LuaSandboxTest {};

TEST_F(LuaRunner_Migrations, AppliesAndRevertsSharedFixture) {
    std::filesystem::copy(
        SCHEMA_PATH("schemas/migrations"), sandbox / "migrations", std::filesystem::copy_options::recursive);

    auto db = quiver::Database::from_schema(db_path(), VALID_SCHEMA("collections.sql"));
    quiver::LuaRunner lua(db);

    EXPECT_NO_THROW(lua.run(R"(db:test_migrations("migrations"))"));
}

TEST_F(LuaRunner_Migrations, PathNotFoundThrows) {
    auto db = quiver::Database::from_schema(db_path(), VALID_SCHEMA("collections.sql"));
    quiver::LuaRunner lua(db);

    expect_lua_error(lua, R"(db:test_migrations("missing"))", "Cannot test_migrations: migrations path not found:");
}

TEST_F(LuaRunner_Migrations, EscapeThrows) {
    auto db = quiver::Database::from_schema(db_path(), VALID_SCHEMA("collections.sql"));
    quiver::LuaRunner lua(db);

    expect_lua_error(lua, R"(db:test_migrations("../outside"))", "escapes the database directory");
}

TEST_F(LuaRunner_Migrations, InMemoryThrows) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"));
    quiver::LuaRunner lua(db);

    expect_lua_error(lua,
                     R"(db:test_migrations("migrations"))",
                     "Cannot test_migrations: database is in-memory, file operations are unavailable");
}
