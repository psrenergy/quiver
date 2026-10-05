#include "test_sandbox.h"

#include <filesystem>

// db:validate_migrations paths are sandboxed: relative paths resolve against the database directory.
class Sandbox_Migrations : public LuaSandboxTest {};

TEST_F(Sandbox_Migrations, AppliesAndRevertsSharedFixture) {
    std::filesystem::copy(
        SCHEMA_PATH("schemas/migrations"),
        sandbox / "migrations",
        std::filesystem::copy_options::recursive
    );

    auto db = quiver::Database::from_schema(db_path(), VALID_SCHEMA("collections.sql"));
    quiver::Sandbox sandbox(db);

    EXPECT_NO_THROW(sandbox.run(R"(db:validate_migrations("migrations"))"));
}

TEST_F(Sandbox_Migrations, PathNotFoundThrows) {
    auto db = quiver::Database::from_schema(db_path(), VALID_SCHEMA("collections.sql"));
    quiver::Sandbox sandbox(db);

    expect_sandbox_error(
        sandbox,
        R"(db:validate_migrations("missing"))",
        "Cannot validate_migrations: migrations path not found:"
    );
}

TEST_F(Sandbox_Migrations, EscapeThrows) {
    auto db = quiver::Database::from_schema(db_path(), VALID_SCHEMA("collections.sql"));
    quiver::Sandbox sandbox(db);

    expect_sandbox_error(sandbox, R"(db:validate_migrations("../outside"))", "escapes the database directory");
}

TEST_F(Sandbox_Migrations, InMemoryThrows) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("collections.sql"));
    quiver::Sandbox sandbox(db);

    expect_sandbox_error(
        sandbox,
        R"(db:validate_migrations("migrations"))",
        "Cannot validate_migrations: database is in-memory, file operations are unavailable"
    );
}
