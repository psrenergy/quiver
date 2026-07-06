#include "test_lua_runner.h"

#include <filesystem>
#include <fstream>

static void write_lua_csv_file(const std::string& path, const std::string& content) {
    std::ofstream f(path, std::ios::binary);
    f << content;
}

// db:import_csv paths are sandboxed: relative paths resolve against the database directory.
class LuaRunner_ImportCSV : public LuaSandboxTest {};

TEST_F(LuaRunner_ImportCSV, ScalarRoundTrip) {
    auto csv_schema = VALID_SCHEMA("csv_export.sql");
    auto db = quiver::Database::from_schema(db_path(), csv_schema);
    quiver::LuaRunner lua(db);

    lua.run(R"(
        db:create_element("Items", {
            label = "Item1", name = "Alpha", status = 1,
            price = 9.99, date_created = "2024-01-15T10:30:00", notes = "first"
        })
        db:create_element("Items", {
            label = "Item2", name = "Beta", status = 2,
            price = 19.5, date_created = "2024-02-20T08:00:00", notes = "second"
        })
    )");

    // Export
    lua.run(R"(db:export_csv("Items", "", "roundtrip.csv"))");

    // Delete all elements and re-import
    lua.run(R"(
        db:delete_element("Items", 1)
        db:delete_element("Items", 2)
    )");

    lua.run(R"(db:import_csv("Items", "", "roundtrip.csv"))");

    lua.run(R"(
        local names = db:read_scalar_strings("Items", "name")
        assert(#names == 2, "Expected 2 names, got " .. #names)
        assert(names[1] == "Alpha", "Expected Alpha, got " .. names[1])
        assert(names[2] == "Beta", "Expected Beta, got " .. names[2])
    )");
}

TEST_F(LuaRunner_ImportCSV, VectorGroupRoundTrip) {
    auto csv_schema = VALID_SCHEMA("csv_export.sql");
    auto db = quiver::Database::from_schema(db_path(), csv_schema);
    quiver::LuaRunner lua(db);

    lua.run(R"(
        local id1 = db:create_element("Items", { label = "Item1", name = "Alpha" })
        db:update_element("Items", id1, { measurement = {1.1, 2.2, 3.3} })
    )");

    // Export, then re-import (import will overwrite existing values)
    lua.run(R"(db:export_csv("Items", "measurements", "roundtrip.csv"))");
    lua.run(R"(db:import_csv("Items", "measurements", "roundtrip.csv"))");

    auto vals = db.read_vector_floats_by_id("Items", "measurement", 1);
    EXPECT_EQ(vals.size(), 3);
}

TEST_F(LuaRunner_ImportCSV, ScalarHeaderOnlyClearsTable) {
    auto csv_schema = VALID_SCHEMA("csv_export.sql");
    auto db = quiver::Database::from_schema(db_path(), csv_schema);
    quiver::LuaRunner lua(db);

    lua.run(R"(
        db:create_element("Items", { label = "Item1", name = "Alpha" })
    )");

    // Write header-only CSV
    write_lua_csv_file((sandbox / "headeronly.csv").string(), "sep=,\nlabel,name,status,price,date_created,notes\n");

    lua.run(R"(db:import_csv("Items", "", "headeronly.csv"))");

    lua.run(R"(
        local names = db:read_scalar_strings("Items", "name")
        assert(#names == 0, "Expected 0 names, got " .. #names)
    )");
}

TEST_F(LuaRunner_ImportCSV, EnumResolution) {
    auto csv_schema = VALID_SCHEMA("csv_export.sql");
    auto db = quiver::Database::from_schema(db_path(), csv_schema);
    quiver::LuaRunner lua(db);

    write_lua_csv_file((sandbox / "enum.csv").string(),
                       "sep=,\nlabel,name,status,price,date_created,notes\nItem1,Alpha,Active,,,\n");

    lua.run(R"(
        db:import_csv("Items", "", "enum.csv", {
            enum_labels = {
                status = { en = { Active = 1, Inactive = 2 } }
            }
        })
    )");

    lua.run(R"(
        local scalars = db:read_scalars_by_id("Items", 1)
        assert(scalars.status == 1, "Expected status 1, got " .. tostring(scalars.status))
    )");
}

TEST_F(LuaRunner_ImportCSV, DateTimeFormat) {
    auto csv_schema = VALID_SCHEMA("csv_export.sql");
    auto db = quiver::Database::from_schema(db_path(), csv_schema);
    quiver::LuaRunner lua(db);

    write_lua_csv_file((sandbox / "datetime.csv").string(),
                       "sep=,\nlabel,name,status,price,date_created,notes\nItem1,Alpha,,,2024/01/15,\n");

    lua.run(R"(
        db:import_csv("Items", "", "datetime.csv", {
            date_time_format = "%Y/%m/%d"
        })
    )");

    lua.run(R"(
        local scalars = db:read_scalars_by_id("Items", 1)
        assert(scalars.date_created == "2024-01-15T00:00:00", "Expected 2024-01-15T00:00:00, got " .. tostring(scalars.date_created))
    )");
}

TEST_F(LuaRunner_ImportCSV, ScalarTrailingEmptyColumns) {
    auto csv_schema = VALID_SCHEMA("csv_export.sql");
    auto db = quiver::Database::from_schema(db_path(), csv_schema);
    quiver::LuaRunner lua(db);

    write_lua_csv_file((sandbox / "trailing.csv").string(),
                       "sep=,\n"
                       "label,name,status,price,date_created,notes,,,,\n"
                       "Item1,Alpha,1,9.99,2024-01-15T10:30:00,first,,,,\n");

    lua.run(R"(db:import_csv("Items", "", "trailing.csv"))");

    lua.run(R"(
        local names = db:read_scalar_strings("Items", "name")
        assert(#names == 1, "Expected 1 name, got " .. #names)
        assert(names[1] == "Alpha", "Expected Alpha, got " .. names[1])
    )");
}

TEST_F(LuaRunner_ImportCSV, VectorTrailingEmptyColumns) {
    auto csv_schema = VALID_SCHEMA("csv_export.sql");
    auto db = quiver::Database::from_schema(db_path(), csv_schema);
    quiver::LuaRunner lua(db);

    lua.run(R"(
        db:create_element("Items", { label = "Item1", name = "Alpha" })
    )");

    write_lua_csv_file((sandbox / "vectrailing.csv").string(),
                       "sep=,\n"
                       "id,vector_index,measurement,,,\n"
                       "Item1,1,1.1,,,\n"
                       "Item1,2,2.2,,,\n");

    lua.run(R"(db:import_csv("Items", "measurements", "vectrailing.csv"))");

    auto vals = db.read_vector_floats_by_id("Items", "measurement", 1);
    EXPECT_EQ(vals.size(), 2);
}

TEST_F(LuaRunner_ImportCSV, InsideTransactionThrows) {
    auto csv_schema = VALID_SCHEMA("csv_export.sql");
    auto db = quiver::Database::from_schema(db_path(), csv_schema);
    quiver::LuaRunner lua(db);

    write_lua_csv_file((sandbox / "intx.csv").string(),
                       "sep=,\nlabel,name,status,price,date_created,notes\nItem1,Alpha,,,,\n");

    lua.run("db:begin_transaction()");
    expect_lua_error(lua, R"(db:import_csv("Items", "", "intx.csv"))", "Cannot import_csv: transaction already active");

    // The caller's transaction must survive intact
    lua.run(R"(assert(db:in_transaction(), "expected transaction to survive import_csv failure"))");
    lua.run("db:rollback()");
}

// --- db-directory sandbox ---

TEST_F(LuaRunner_ImportCSV, EscapeThrows) {
    auto csv_schema = VALID_SCHEMA("csv_export.sql");
    auto db = quiver::Database::from_schema(db_path(), csv_schema);
    quiver::LuaRunner lua(db);

    expect_lua_error(lua,
                     R"(db:import_csv("Items", "", "../x.csv"))",
                     "Cannot import_csv: path '../x.csv' escapes the database directory");
}

TEST_F(LuaRunner_ImportCSV, InMemoryThrows) {
    auto csv_schema = VALID_SCHEMA("csv_export.sql");
    auto db = quiver::Database::from_schema(":memory:", csv_schema);
    quiver::LuaRunner lua(db);

    expect_lua_error(lua, R"(db:import_csv("Items", "", "x.csv"))", "Cannot import_csv: database is in-memory");
}
