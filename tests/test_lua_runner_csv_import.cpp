#include "test_lua_runner.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

static std::filesystem::path lua_csv_temp(const std::string& test_name) {
    return std::filesystem::temp_directory_path() / ("quiver_lua_test_" + test_name + ".csv");
}

// Convert path to forward slashes for safe Lua string embedding
static std::string lua_safe_path(const std::filesystem::path& p) {
    auto s = p.string();
    std::replace(s.begin(), s.end(), '\\', '/');
    return s;
}

static void write_lua_csv_file(const std::string& path, const std::string& content) {
    std::ofstream f(path, std::ios::binary);
    f << content;
}

TEST(LuaRunner_ImportCSV, ScalarRoundTrip) {
    auto csv_schema = VALID_SCHEMA("csv_export.sql");
    auto db = quiver::Database::from_schema(":memory:", csv_schema);
    quiver::LuaRunner lua(db);

    auto csv_path = lua_csv_temp("ImportScalarRT");

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
    lua.run(R"(db:export_csv("Items", "", ")" + lua_safe_path(csv_path) + "\")");

    // Delete all elements and re-import
    lua.run(R"(
        db:delete_element("Items", 1)
        db:delete_element("Items", 2)
    )");

    lua.run(R"(db:import_csv("Items", "", ")" + lua_safe_path(csv_path) + "\")");

    lua.run(R"(
        local names = db:read_scalar_strings("Items", "name")
        assert(#names == 2, "Expected 2 names, got " .. #names)
        assert(names[1] == "Alpha", "Expected Alpha, got " .. names[1])
        assert(names[2] == "Beta", "Expected Beta, got " .. names[2])
    )");

    std::filesystem::remove(csv_path);
}

TEST(LuaRunner_ImportCSV, VectorGroupRoundTrip) {
    auto csv_schema = VALID_SCHEMA("csv_export.sql");
    auto db = quiver::Database::from_schema(":memory:", csv_schema);
    quiver::LuaRunner lua(db);

    auto csv_path = lua_csv_temp("ImportVectorRT");

    lua.run(R"(
        local id1 = db:create_element("Items", { label = "Item1", name = "Alpha" })
        db:update_element("Items", id1, { measurement = {1.1, 2.2, 3.3} })
    )");

    // Export
    lua.run(R"(db:export_csv("Items", "measurements", ")" + lua_safe_path(csv_path) + R"("))");

    // Re-import (import will overwrite existing values)
    lua.run(R"(db:import_csv("Items", "measurements", ")" + lua_safe_path(csv_path) + R"("))");

    auto vals = db.read_vector_floats_by_id("Items", "measurement", 1);
    EXPECT_EQ(vals.size(), 3);

    std::filesystem::remove(csv_path);
}

TEST(LuaRunner_ImportCSV, ScalarHeaderOnlyClearsTable) {
    auto csv_schema = VALID_SCHEMA("csv_export.sql");
    auto db = quiver::Database::from_schema(":memory:", csv_schema);
    quiver::LuaRunner lua(db);

    auto csv_path = lua_csv_temp("ImportHeaderOnly");

    lua.run(R"(
        db:create_element("Items", { label = "Item1", name = "Alpha" })
    )");

    // Write header-only CSV
    write_lua_csv_file(csv_path.string(), "sep=,\nlabel,name,status,price,date_created,notes\n");

    lua.run(R"(db:import_csv("Items", "", ")" + lua_safe_path(csv_path) + "\")");

    lua.run(R"(
        local names = db:read_scalar_strings("Items", "name")
        assert(#names == 0, "Expected 0 names, got " .. #names)
    )");

    std::filesystem::remove(csv_path);
}

TEST(LuaRunner_ImportCSV, EnumResolution) {
    auto csv_schema = VALID_SCHEMA("csv_export.sql");
    auto db = quiver::Database::from_schema(":memory:", csv_schema);
    quiver::LuaRunner lua(db);

    auto csv_path = lua_csv_temp("ImportEnum");
    write_lua_csv_file(csv_path.string(), "sep=,\nlabel,name,status,price,date_created,notes\nItem1,Alpha,Active,,,\n");

    lua.run(R"(db:import_csv("Items", "", ")" + lua_safe_path(csv_path) +
            "\", {\n"
            "    enum_labels = {\n"
            "        status = { en = { Active = 1, Inactive = 2 } }\n"
            "    }\n"
            "})");

    lua.run(R"(
        local scalars = db:read_scalars_by_id("Items", 1)
        assert(scalars.status == 1, "Expected status 1, got " .. tostring(scalars.status))
    )");

    std::filesystem::remove(csv_path);
}

TEST(LuaRunner_ImportCSV, DateTimeFormat) {
    auto csv_schema = VALID_SCHEMA("csv_export.sql");
    auto db = quiver::Database::from_schema(":memory:", csv_schema);
    quiver::LuaRunner lua(db);

    auto csv_path = lua_csv_temp("ImportDateTime");
    write_lua_csv_file(csv_path.string(),
                       "sep=,\nlabel,name,status,price,date_created,notes\nItem1,Alpha,,,2024/01/15,\n");

    lua.run(R"(db:import_csv("Items", "", ")" + lua_safe_path(csv_path) +
            "\", {\n"
            "    date_time_format = \"%Y/%m/%d\"\n"
            "})");

    lua.run(R"(
        local scalars = db:read_scalars_by_id("Items", 1)
        assert(scalars.date_created == "2024-01-15T00:00:00", "Expected 2024-01-15T00:00:00, got " .. tostring(scalars.date_created))
    )");

    std::filesystem::remove(csv_path);
}

TEST(LuaRunner_ImportCSV, ScalarTrailingEmptyColumns) {
    auto csv_schema = VALID_SCHEMA("csv_export.sql");
    auto db = quiver::Database::from_schema(":memory:", csv_schema);
    quiver::LuaRunner lua(db);

    auto csv_path = lua_csv_temp("ImportScalarTrailing");
    write_lua_csv_file(csv_path.string(),
                       "sep=,\n"
                       "label,name,status,price,date_created,notes,,,,\n"
                       "Item1,Alpha,1,9.99,2024-01-15T10:30:00,first,,,,\n");

    lua.run(R"(db:import_csv("Items", "", ")" + lua_safe_path(csv_path) + "\")");

    lua.run(R"(
        local names = db:read_scalar_strings("Items", "name")
        assert(#names == 1, "Expected 1 name, got " .. #names)
        assert(names[1] == "Alpha", "Expected Alpha, got " .. names[1])
    )");

    std::filesystem::remove(csv_path);
}

TEST(LuaRunner_ImportCSV, VectorTrailingEmptyColumns) {
    auto csv_schema = VALID_SCHEMA("csv_export.sql");
    auto db = quiver::Database::from_schema(":memory:", csv_schema);
    quiver::LuaRunner lua(db);

    lua.run(R"(
        db:create_element("Items", { label = "Item1", name = "Alpha" })
    )");

    auto csv_path = lua_csv_temp("ImportVectorTrailing");
    write_lua_csv_file(csv_path.string(),
                       "sep=,\n"
                       "id,vector_index,measurement,,,\n"
                       "Item1,1,1.1,,,\n"
                       "Item1,2,2.2,,,\n");

    lua.run(R"(db:import_csv("Items", "measurements", ")" + lua_safe_path(csv_path) + "\")");

    auto vals = db.read_vector_floats_by_id("Items", "measurement", 1);
    EXPECT_EQ(vals.size(), 2);

    std::filesystem::remove(csv_path);
}

TEST(LuaRunner_ImportCSV, InsideTransactionThrows) {
    auto csv_schema = VALID_SCHEMA("csv_export.sql");
    auto db = quiver::Database::from_schema(":memory:", csv_schema);
    quiver::LuaRunner lua(db);

    auto csv_path = lua_csv_temp("ImportInTransaction");
    {
        std::ofstream f(csv_path, std::ios::binary);
        f << "sep=,\nlabel,name,status,price,date_created,notes\nItem1,Alpha,,,,\n";
    }

    lua.run("db:begin_transaction()");
    EXPECT_THROW({ lua.run(R"(db:import_csv("Items", "", ")" + lua_safe_path(csv_path) + "\")"); }, std::runtime_error);

    // The caller's transaction must survive intact
    lua.run(R"(assert(db:in_transaction(), "expected transaction to survive import_csv failure"))");
    lua.run("db:rollback()");

    std::filesystem::remove(csv_path);
}
