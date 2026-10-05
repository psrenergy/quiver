#include "test_sandbox.h"

#include <filesystem>
#include <fstream>
#include <sstream>

static std::string read_csv_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// db:export_csv paths are sandboxed: relative paths resolve against the database directory.
class Sandbox_ExportCSV : public SandboxTest {};

TEST_F(Sandbox_ExportCSV, ScalarDefaults) {
    auto csv_schema = VALID_SCHEMA("csv_export.sql");
    auto db = quiver::Database::from_schema(db_path(), csv_schema);
    quiver::Sandbox sandbox(db);

    sandbox.run(R"(
        db:create_element("Items", {
            label = "Item1", name = "Alpha", status = 1,
            price = 9.99, date_created = "2024-01-15T10:30:00", notes = "first"
        })
        db:create_element("Items", {
            label = "Item2", name = "Beta", status = 2,
            price = 19.5, date_created = "2024-02-20T08:00:00", notes = "second"
        })
    )");

    sandbox.run(R"(db:export_csv("Items", "", "out.csv"))");

    auto content = read_csv_file((sandbox / "out.csv").string());
    EXPECT_NE(content.find("label,name,status,price,date_created,notes\n"), std::string::npos);
    EXPECT_NE(content.find("Item1,Alpha,1,9.99,2024-01-15T10:30:00,first\n"), std::string::npos);
    EXPECT_NE(content.find("Item2,Beta,2,19.5,2024-02-20T08:00:00,second\n"), std::string::npos);
}

TEST_F(Sandbox_ExportCSV, GroupExport) {
    auto csv_schema = VALID_SCHEMA("csv_export.sql");
    auto db = quiver::Database::from_schema(db_path(), csv_schema);
    quiver::Sandbox sandbox(db);

    sandbox.run(R"(
        local id1 = db:create_element("Items", { label = "Item1", name = "Alpha" })
        local id2 = db:create_element("Items", { label = "Item2", name = "Beta" })
        db:update_element("Items", id1, { measurement = {1.1, 2.2, 3.3} })
        db:update_element("Items", id2, { measurement = {4.4, 5.5} })
    )");

    sandbox.run(R"(db:export_csv("Items", "measurements", "out.csv"))");

    auto content = read_csv_file((sandbox / "out.csv").string());
    EXPECT_NE(content.find("sep=,\nid,vector_index,measurement\n"), std::string::npos);
    EXPECT_NE(content.find("Item1,1,1.1\n"), std::string::npos);
    EXPECT_NE(content.find("Item1,2,2.2\n"), std::string::npos);
    EXPECT_NE(content.find("Item1,3,3.3\n"), std::string::npos);
    EXPECT_NE(content.find("Item2,1,4.4\n"), std::string::npos);
    EXPECT_NE(content.find("Item2,2,5.5\n"), std::string::npos);
}

TEST_F(Sandbox_ExportCSV, EnumLabels) {
    auto csv_schema = VALID_SCHEMA("csv_export.sql");
    auto db = quiver::Database::from_schema(db_path(), csv_schema);
    quiver::Sandbox sandbox(db);

    sandbox.run(R"(
        db:create_element("Items", {
            label = "Item1", name = "Alpha", status = 1,
            price = 9.99, date_created = "2024-01-15T10:30:00", notes = "first"
        })
        db:create_element("Items", {
            label = "Item2", name = "Beta", status = 2,
            price = 19.5, date_created = "2024-02-20T08:00:00", notes = "second"
        })
    )");

    sandbox.run(R"(
        db:export_csv("Items", "", "out.csv", {
            enum_labels = {
                status = { en = { Active = 1, Inactive = 2 } }
            }
        })
    )");

    auto content = read_csv_file((sandbox / "out.csv").string());
    EXPECT_NE(content.find("Item1,Alpha,Active,9.99,2024-01-15T10:30:00,first\n"), std::string::npos);
    EXPECT_NE(content.find("Item2,Beta,Inactive,19.5,2024-02-20T08:00:00,second\n"), std::string::npos);
}

TEST_F(Sandbox_ExportCSV, DateTimeFormat) {
    auto csv_schema = VALID_SCHEMA("csv_export.sql");
    auto db = quiver::Database::from_schema(db_path(), csv_schema);
    quiver::Sandbox sandbox(db);

    sandbox.run(R"(
        db:create_element("Items", {
            label = "Item1", name = "Alpha", status = 1,
            price = 9.99, date_created = "2024-01-15T10:30:00", notes = "first"
        })
    )");

    sandbox.run(R"(
        db:export_csv("Items", "", "out.csv", {
            date_time_format = "%Y/%m/%d"
        })
    )");

    auto content = read_csv_file((sandbox / "out.csv").string());
    EXPECT_NE(content.find("Item1,Alpha,1,9.99,2024/01/15,first\n"), std::string::npos);
}

TEST_F(Sandbox_ExportCSV, CombinedOptions) {
    auto csv_schema = VALID_SCHEMA("csv_export.sql");
    auto db = quiver::Database::from_schema(db_path(), csv_schema);
    quiver::Sandbox sandbox(db);

    sandbox.run(R"(
        db:create_element("Items", {
            label = "Item1", name = "Alpha", status = 1,
            price = 9.99, date_created = "2024-01-15T10:30:00", notes = "first"
        })
        db:create_element("Items", {
            label = "Item2", name = "Beta", status = 2,
            price = 19.5, date_created = "2024-02-20T08:00:00", notes = "second"
        })
    )");

    sandbox.run(R"(
        db:export_csv("Items", "", "out.csv", {
            enum_labels = {
                status = { en = { Active = 1, Inactive = 2 } }
            },
            date_time_format = "%Y/%m/%d"
        })
    )");

    auto content = read_csv_file((sandbox / "out.csv").string());
    EXPECT_NE(content.find("label,name,status,price,date_created,notes\n"), std::string::npos);
    EXPECT_NE(content.find("Item1,Alpha,Active,9.99,2024/01/15,first\n"), std::string::npos);
    EXPECT_NE(content.find("Item2,Beta,Inactive,19.5,2024/02/20,second\n"), std::string::npos) << "Actual content:\n"
                                                                                               << content;
}

// A misspelled key, a wrong-typed value and a non-table options argument used to be ignored.
TEST_F(Sandbox_ExportCSV, OptionsAreStrict) {
    auto csv_schema = VALID_SCHEMA("csv_export.sql");
    auto db = quiver::Database::from_schema(db_path(), csv_schema);
    quiver::Sandbox sandbox(db);

    expect_sandbox_error(
        sandbox,
        R"(db:export_csv("Items", "", "out.csv", { date_format = "%Y" }))",
        "Cannot export_csv: unknown option 'date_format'"
    );
    expect_sandbox_error(
        sandbox,
        R"(db:export_csv("Items", "", "out.csv", "x"))",
        "Cannot export_csv: options must be a table"
    );
    expect_sandbox_error(
        sandbox,
        R"(db:export_csv("Items", "", "out.csv", { date_time_format = 5 }))",
        "Cannot export_csv: option 'date_time_format' must be a string"
    );
}

// --- db-directory sandbox ---

TEST_F(Sandbox_ExportCSV, RelativeResolvesAgainstDbDir) {
    auto csv_schema = VALID_SCHEMA("csv_export.sql");
    auto db = quiver::Database::from_schema(db_path(), csv_schema);
    quiver::Sandbox sandbox(db);

    sandbox.run(R"(db:create_element("Items", { label = "Item1", name = "Alpha" }))");
    sandbox.run(R"(db:export_csv("Items", "", "out.csv"))");

    EXPECT_TRUE(std::filesystem::exists(sandbox / "out.csv"));
    EXPECT_FALSE(std::filesystem::exists(std::filesystem::current_path() / "out.csv"));
}

TEST_F(Sandbox_ExportCSV, EscapeThrows) {
    auto csv_schema = VALID_SCHEMA("csv_export.sql");
    auto db = quiver::Database::from_schema(db_path(), csv_schema);
    quiver::Sandbox sandbox(db);

    expect_sandbox_error(
        sandbox,
        R"(db:export_csv("Items", "", "../x.csv"))",
        "Cannot export_csv: path '../x.csv' escapes the database directory"
    );
}

// The path is resolved before the options are decoded, so a non-table options argument cannot mask an escaping path.
TEST_F(Sandbox_ExportCSV, EscapeIsReportedBeforeNonTableOptions) {
    auto csv_schema = VALID_SCHEMA("csv_export.sql");
    auto db = quiver::Database::from_schema(db_path(), csv_schema);
    quiver::Sandbox sandbox(db);

    expect_sandbox_error(
        sandbox,
        R"(db:export_csv("Items", "", "../x.csv", 5))",
        "Cannot export_csv: path '../x.csv' escapes the database directory"
    );
}

TEST_F(Sandbox_ExportCSV, InMemoryThrows) {
    auto csv_schema = VALID_SCHEMA("csv_export.sql");
    auto db = quiver::Database::from_schema(":memory:", csv_schema);
    quiver::Sandbox sandbox(db);

    expect_sandbox_error(sandbox, R"(db:export_csv("Items", "", "out.csv"))", "Cannot export_csv: database is in-memory");
}
