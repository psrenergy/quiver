#include "test_lua_runner.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

static std::string read_csv_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    std::string content = ss.str();

    // Canonicalize to comma-delimited, '.'-decimal form so byte-level assertions
    // stay independent of the host locale: export_csv emits a ';'-delimited,
    // ','-decimal file on a comma-locale machine (matching Excel). Safe only for
    // values free of spaces and embedded commas — true of every test that uses it.
    if (content.rfind("sep=;", 0) == 0) {
        std::replace(content.begin(), content.end(), ',', '.');  // ',' decimals -> '.'
        std::replace(content.begin(), content.end(), ';', ',');  // ';' delimiters -> ','
    }
    return content;
}

static std::filesystem::path lua_csv_temp(const std::string& test_name) {
    return std::filesystem::temp_directory_path() / ("quiver_lua_test_" + test_name + ".csv");
}

// Convert path to forward slashes for safe Lua string embedding
static std::string lua_safe_path(const std::filesystem::path& p) {
    auto s = p.string();
    std::replace(s.begin(), s.end(), '\\', '/');
    return s;
}

TEST(LuaRunner_ExportCSV, ScalarDefaults) {
    auto csv_schema = VALID_SCHEMA("csv_export.sql");
    auto db = quiver::Database::from_schema(":memory:", csv_schema);
    quiver::LuaRunner lua(db);

    auto csv_path = lua_csv_temp("ScalarDefaults");

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

    lua.run(R"(db:export_csv("Items", "", ")" + lua_safe_path(csv_path) + "\")");

    auto content = read_csv_file(csv_path.string());
    EXPECT_NE(content.find("label,name,status,price,date_created,notes\n"), std::string::npos);
    EXPECT_NE(content.find("Item1,Alpha,1,9.99,2024-01-15T10:30:00,first\n"), std::string::npos);
    EXPECT_NE(content.find("Item2,Beta,2,19.5,2024-02-20T08:00:00,second\n"), std::string::npos);

    std::filesystem::remove(csv_path);
}

TEST(LuaRunner_ExportCSV, GroupExport) {
    auto csv_schema = VALID_SCHEMA("csv_export.sql");
    auto db = quiver::Database::from_schema(":memory:", csv_schema);
    quiver::LuaRunner lua(db);

    auto csv_path = lua_csv_temp("GroupExport");

    lua.run(R"(
        local id1 = db:create_element("Items", { label = "Item1", name = "Alpha" })
        local id2 = db:create_element("Items", { label = "Item2", name = "Beta" })
        db:update_element("Items", id1, { measurement = {1.1, 2.2, 3.3} })
        db:update_element("Items", id2, { measurement = {4.4, 5.5} })
    )");

    lua.run(R"(db:export_csv("Items", "measurements", ")" + lua_safe_path(csv_path) + "\")");

    auto content = read_csv_file(csv_path.string());
    EXPECT_NE(content.find("sep=,\nid,vector_index,measurement\n"), std::string::npos);
    EXPECT_NE(content.find("Item1,1,1.1\n"), std::string::npos);
    EXPECT_NE(content.find("Item1,2,2.2\n"), std::string::npos);
    EXPECT_NE(content.find("Item1,3,3.3\n"), std::string::npos);
    EXPECT_NE(content.find("Item2,1,4.4\n"), std::string::npos);
    EXPECT_NE(content.find("Item2,2,5.5\n"), std::string::npos);

    std::filesystem::remove(csv_path);
}

TEST(LuaRunner_ExportCSV, EnumLabels) {
    auto csv_schema = VALID_SCHEMA("csv_export.sql");
    auto db = quiver::Database::from_schema(":memory:", csv_schema);
    quiver::LuaRunner lua(db);

    auto csv_path = lua_csv_temp("EnumLabels");

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

    lua.run(R"(db:export_csv("Items", "", ")" + lua_safe_path(csv_path) +
            "\", {\n"
            "    enum_labels = {\n"
            "        status = { en = { Active = 1, Inactive = 2 } }\n"
            "    }\n"
            "})");

    auto content = read_csv_file(csv_path.string());
    EXPECT_NE(content.find("Item1,Alpha,Active,9.99,2024-01-15T10:30:00,first\n"), std::string::npos);
    EXPECT_NE(content.find("Item2,Beta,Inactive,19.5,2024-02-20T08:00:00,second\n"), std::string::npos);

    std::filesystem::remove(csv_path);
}

TEST(LuaRunner_ExportCSV, DateTimeFormat) {
    auto csv_schema = VALID_SCHEMA("csv_export.sql");
    auto db = quiver::Database::from_schema(":memory:", csv_schema);
    quiver::LuaRunner lua(db);

    auto csv_path = lua_csv_temp("DateTimeFormat");

    lua.run(R"(
        db:create_element("Items", {
            label = "Item1", name = "Alpha", status = 1,
            price = 9.99, date_created = "2024-01-15T10:30:00", notes = "first"
        })
    )");

    lua.run(R"(db:export_csv("Items", "", ")" + lua_safe_path(csv_path) +
            "\", {\n"
            "    date_time_format = \"%Y/%m/%d\"\n"
            "})");

    auto content = read_csv_file(csv_path.string());
    EXPECT_NE(content.find("Item1,Alpha,1,9.99,2024/01/15,first\n"), std::string::npos);

    std::filesystem::remove(csv_path);
}

TEST(LuaRunner_ExportCSV, CombinedOptions) {
    auto csv_schema = VALID_SCHEMA("csv_export.sql");
    auto db = quiver::Database::from_schema(":memory:", csv_schema);
    quiver::LuaRunner lua(db);

    auto csv_path = lua_csv_temp("CombinedOptions");

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

    lua.run(R"(db:export_csv("Items", "", ")" + lua_safe_path(csv_path) +
            "\", {\n"
            "    enum_labels = {\n"
            "        status = { en = { Active = 1, Inactive = 2 } }\n"
            "    },\n"
            "    date_time_format = \"%Y/%m/%d\"\n"
            "})");

    auto content = read_csv_file(csv_path.string());
    EXPECT_NE(content.find("label,name,status,price,date_created,notes\n"), std::string::npos);
    EXPECT_NE(content.find("Item1,Alpha,Active,9.99,2024/01/15,first\n"), std::string::npos);
    EXPECT_NE(content.find("Item2,Beta,Inactive,19.5,2024/02/20,second\n"), std::string::npos) << "Actual content:\n"
                                                                                               << content;

    std::filesystem::remove(csv_path);
}
