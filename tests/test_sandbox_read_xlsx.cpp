#include "test_sandbox.h"

#include <fstream>
#include <iterator>
#include <memory>
#include <vector>

class Sandbox_ReadXlsx : public LuaSandboxTest {
protected:
    void SetUp() override {
        LuaSandboxTest::SetUp();
        for (const auto* name : {"xlsx_read.xlsx", "xlsx_bad_sheet.xlsx", "xlsx_bad_workbook.xlsx"}) {
            std::filesystem::copy_file(SCHEMA_PATH(std::string("fixtures/") + name), sandbox_path / name);
        }
        db = std::make_unique<quiver::Database>(quiver::Database::from_schema(db_path(), VALID_SCHEMA("basic.sql")));
        sandbox = std::make_unique<quiver::Sandbox>(*db);
    }

    void TearDown() override {
        sandbox.reset();
        db.reset();
        LuaSandboxTest::TearDown();
    }

    void error_both(const std::string& path, const std::string& options, const std::string& reason) {
        for (const auto* operation : {"read_xlsx", "read_xlsx_stream"}) {
            const std::string callback = std::string(operation) == "read_xlsx_stream" ? ", function() end" : "";
            const auto script = std::string("db:") + operation + "(\"" + path + "\"" + callback + ", " + options + ")";
            expect_sandbox_error(*sandbox, script, std::string("Cannot ") + operation + ": ");
            expect_sandbox_error(*sandbox, script, reason);
        }
    }

    std::unique_ptr<quiver::Database> db;
    std::unique_ptr<quiver::Sandbox> sandbox;
};

TEST_F(Sandbox_ReadXlsx, CellStringsPreserveValuesAndSparseColumns) {
    EXPECT_NO_THROW(sandbox->run(R"lua(
        local x = db:read_xlsx("xlsx_read.xlsx")
        assert(#x.header == 8 and x.header[1] == "name" and x.header[8] == "large")
        assert(#x.rows == 2)
        local a, b = x.rows[1], x.rows[2]
        assert(#a == 8 and #b == 8) -- formatting-only XFD1048576 did not inflate the range
        for _, r in ipairs(x.rows) do
            for _, v in ipairs(r) do assert(type(v) == "string") end
        end
        assert(a[1] == "São Paulo" and a[2] == "1.25" and a[3] == "1")
        assert(a[4] == "45292.5" and a[5] == "2.5")
        assert(a[6] == " Olá 🌍\nnext" and a[7] == "#DIV/0!")
        assert(a[8] == "9007199254740993") -- no round trip through double
        assert(b[1] == "Beta" and b[2] == "-1.25E+03" and b[3] == "0")
        assert(b[4] == "60" and b[5] == "" and b[6] == "0012")
        assert(b[7] == "" and b[8] == "")
    )lua"));
}

TEST_F(Sandbox_ReadXlsx, SheetSelectionSkipsChartSheets) {
    EXPECT_NO_THROW(sandbox->run(R"lua(
        for _, sheet in ipairs({2, "Second"}) do
            local x = db:read_xlsx("xlsx_read.xlsx", {sheet = sheet})
            assert(#x.rows == 1 and x.rows[1][1] == "other" and x.rows[1][2] == "42")
        end
        assert(db:read_xlsx("xlsx_read.xlsx", {sheet = 1}).rows[1][1] == "São Paulo")
    )lua"));
    error_both("xlsx_read.xlsx", "{sheet = 'Chart'}", "worksheet not found: 'Chart'");
    error_both("xlsx_read.xlsx", "{sheet = 'second'}", "worksheet not found: 'second'");
    error_both("xlsx_read.xlsx", "{sheet = 99}", "worksheet index not found: 99");
}

TEST_F(Sandbox_ReadXlsx, HeadersUsePhysicalWorksheetRows) {
    EXPECT_NO_THROW(sandbox->run(R"lua(
        local x = db:read_xlsx("xlsx_read.xlsx", {sheet = "Preamble", header_row = 3})
        assert(#x.header == 3 and x.header[1] == "name" and x.header[2] == "" and x.header[3] == "value")
        assert(#x.rows == 2 and #x.rows[1] == 3)
        assert(x.rows[1][1] == "first" and x.rows[1][2] == "" and x.rows[1][3] == "10")
        assert(x.rows[2][1] == "last" and x.rows[2][3] == "")
        local all = db:read_xlsx("xlsx_read.xlsx", {sheet = "Preamble", header_row = 0})
        assert(all.header == nil and #all.rows == 4 and all.rows[1][1] == "Title")
        assert(all.rows[2][1] == "name")
    )lua"));
    error_both("xlsx_read.xlsx", "{sheet = 'Preamble', header_row = 2}", "header row 2 not found");
    error_both("xlsx_read.xlsx", "{header_row = 9223372036854775807}", "header row 9223372036854775807 not found");
}

TEST_F(Sandbox_ReadXlsx, CallbackAndWholeSheetAgree) {
    EXPECT_NO_THROW(sandbox->run(R"lua(
        for _, options in ipairs({{}, {sheet = 2}, {sheet = "Preamble", header_row = 3},
                                  {sheet = "Preamble", header_row = 0}}) do
            local whole = db:read_xlsx("xlsx_read.xlsx", options)
            local seen = 0
            local n = db:read_xlsx_stream("xlsx_read.xlsx", function(row, index, header)
                seen = seen + 1
                assert(index == seen and #row == #whole.rows[index])
                for c, value in ipairs(row) do assert(value == whole.rows[index][c]) end
                if whole.header then
                    assert(#header == #whole.header)
                    for c, name in ipairs(header) do assert(name == whole.header[c]) end
                else assert(header == nil) end
            end, options)
            assert(n == #whole.rows and seen == n)
        end
    )lua"));
}

TEST_F(Sandbox_ReadXlsx, CallbackStopsOnlyOnExactFalseAndCountsTheStoppingRow) {
    EXPECT_NO_THROW(sandbox->run(R"lua(
        for _, value in ipairs({true, 0, "stop", {}}) do
            assert(db:read_xlsx_stream("xlsx_read.xlsx", function() return value end) == 2)
        end
        assert(db:read_xlsx_stream("xlsx_read.xlsx", function() return nil end) == 2)
        assert(db:read_xlsx_stream("xlsx_read.xlsx", function() end) == 2)
        assert(db:read_xlsx_stream("xlsx_read.xlsx", function() return false end) == 1)
        assert(db:read_xlsx_stream("xlsx_read.xlsx", function(_, index) return index < 2 end) == 2)
        -- A later missing cache is not read after the callback stops.
        assert(db:read_xlsx_stream("xlsx_read.xlsx", function() return false end,
                                  {sheet = "MissingCache"}) == 1)
    )lua"));
}

TEST_F(Sandbox_ReadXlsx, EmptyAndHeaderOnlySheets) {
    EXPECT_NO_THROW(sandbox->run(R"lua(
        local header = db:read_xlsx("xlsx_read.xlsx", {sheet = "HeaderOnly"})
        assert(#header.header == 1 and #header.rows == 0)
        assert(db:read_xlsx_stream("xlsx_read.xlsx", function() error("should not run") end,
                                  {sheet = "HeaderOnly"}) == 0)
        local empty = db:read_xlsx("xlsx_read.xlsx", {sheet = "Empty", header_row = 0})
        assert(empty.header == nil and #empty.rows == 0)
        assert(db:read_xlsx_stream("xlsx_read.xlsx", function() error("should not run") end,
                                  {sheet = "Empty", header_row = 0}) == 0)
    )lua"));
    error_both("xlsx_read.xlsx", "{sheet = 'Empty'}", "header row 1 not found");
}

TEST_F(Sandbox_ReadXlsx, MissingFormulaCachesNameSheetAndCell) {
    error_both("xlsx_read.xlsx", "{sheet = 'MissingCache'}", "worksheet 'MissingCache' cell 'A3' has no cached result");
    error_both("xlsx_read.xlsx", "{sheet = 'AbsentCache'}", "worksheet 'AbsentCache' cell 'A2' has no cached result");
}

TEST_F(Sandbox_ReadXlsx, InvalidOptionsAreRejected) {
    const std::vector<std::string> options = {
        "false",
        "'Second'",
        "db",
        "{unknown = 1}",
        "{[true] = 1}",
        "{[1] = 'Second'}",
        "{sheet = false}",
        "{sheet = 2.0}",
        "{sheet = 2.5}",
        "{sheet = {}}",
        "{sheet = 0}",
        "{sheet = -1}",
        "{header_row = true}",
        "{header_row = '1'}",
        "{header_row = 1.0}",
        "{header_row = 1.5}",
        "{header_row = -1}",
    };
    for (const auto& option : options) {
        SCOPED_TRACE(option);
        error_both("xlsx_read.xlsx", option, "Cannot read_xlsx");
    }
    EXPECT_NO_THROW(sandbox->run(R"lua(
        assert(#db:read_xlsx("xlsx_read.xlsx", nil).rows == 2)
        assert(#db:read_xlsx("xlsx_read.xlsx", {}).rows == 2)
    )lua"));
    for (const auto* callback : {"nil", "false", "{}", "'callback'", "db"}) {
        expect_sandbox_error(
            *sandbox,
            std::string("db:read_xlsx_stream('xlsx_read.xlsx', ") + callback + ")",
            "Cannot read_xlsx_stream: on_row must be a function"
        );
    }
}

TEST_F(Sandbox_ReadXlsx, FileAndXmlErrorsCarryOperationAndOriginalPath) {
    std::ofstream(sandbox_path / "empty.xlsx");
    std::ofstream(sandbox_path / "invalid.xlsx") << "This is not a ZIP file";
    std::filesystem::create_directory(sandbox_path / "directory.xlsx");
    error_both("missing.xlsx", "nil", "file not found: missing.xlsx");
    error_both("empty.xlsx", "nil", "file 'empty.xlsx' is empty");
    error_both("directory.xlsx", "nil", "path is not a regular file: directory.xlsx");
    error_both("invalid.xlsx", "nil", "cannot read file 'invalid.xlsx'");
    error_both("xlsx_bad_sheet.xlsx", "nil", "invalid XML in 'xl/worksheets/sheet1.xml'");
    error_both("xlsx_bad_workbook.xlsx", "nil", "invalid XML in 'xl/workbook.xml'");
}

TEST_F(Sandbox_ReadXlsx, PathsStayInsideDatabaseDirectory) {
    error_both("../outside.xlsx", "false", "escapes the database directory");
    std::filesystem::create_directory(sandbox_path / "sub");
    std::filesystem::copy_file(sandbox_path / "xlsx_read.xlsx", sandbox_path / "sub" / "data.xlsx");
    EXPECT_NO_THROW(sandbox->run(R"(assert(#db:read_xlsx("sub/data.xlsx").rows == 2))"));
    auto memory = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"));
    quiver::Sandbox memory_sandbox(memory);
    expect_sandbox_error(memory_sandbox, R"(db:read_xlsx("x.xlsx", false))", "Cannot read_xlsx: database is in-memory");
    expect_sandbox_error(
        memory_sandbox,
        R"(db:read_xlsx_stream("x.xlsx", function() end, false))",
        "Cannot read_xlsx_stream: database is in-memory"
    );
}

TEST_F(Sandbox_ReadXlsx, CallbackErrorPropagatesAndAllReadPathsCloseWithoutChangingInput) {
    const auto contents = [](const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(file), {});
    };
    const auto path = sandbox_path / "xlsx_read.xlsx";
    const auto before = contents(path);
    EXPECT_NO_THROW(sandbox->run(R"(db:read_xlsx("xlsx_read.xlsx"))"));
    expect_sandbox_error(
        *sandbox,
        R"lua(
        db:read_xlsx_stream("xlsx_read.xlsx", function() error("callback-marker") end)
    )lua",
        "callback-marker"
    );
    EXPECT_NO_THROW(sandbox->run(R"lua(
        local ok, message = pcall(function()
            db:read_xlsx_stream("xlsx_read.xlsx", function() error("callback-marker") end)
        end)
        assert(not ok and not message:find("Cannot read_xlsx_stream", 1, true))
        assert(#db:read_xlsx("xlsx_read.xlsx").rows == 2)
    )lua"));
    error_both("xlsx_read.xlsx", "{sheet = 'MissingCache'}", "has no cached result");
    EXPECT_EQ(contents(path), before);
    // Windows refuses this rename if a reader left the archive handle open.
    EXPECT_NO_THROW(std::filesystem::rename(path, sandbox_path / "closed.xlsx"));
    EXPECT_NO_THROW(std::filesystem::rename(sandbox_path / "closed.xlsx", path));
}
