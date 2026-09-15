#include "test_lua_runner.h"

#include <filesystem>
#include <fstream>

namespace {

void write_lua_csv_file(const std::filesystem::path& path, const std::string& content) {
    std::ofstream f(path, std::ios::binary);
    f << content;
}

}  // namespace

// db:read_csv / db:read_csv_stream paths are sandboxed: relative paths resolve against the
// database directory, same as every other file-touching Lua operation.
class LuaRunner_ReadCsv : public LuaSandboxTest {};

TEST_F(LuaRunner_ReadCsv, CleanFileReturnsHeaderAndRows) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    write_lua_csv_file(sandbox / "two_col.csv", "name,value\nAlpha,1\nBeta,2\n");

    lua.run(R"(
        local csv = db:read_csv("two_col.csv")
        assert(csv.header[1] == "name", "expected header[1] == name, got " .. tostring(csv.header[1]))
        assert(csv.header[2] == "value", "expected header[2] == value, got " .. tostring(csv.header[2]))
        assert(#csv.rows == 2, "expected 2 rows, got " .. #csv.rows)
        assert(csv.rows[1][1] == "Alpha", "expected Alpha, got " .. tostring(csv.rows[1][1]))
        assert(csv.rows[1][2] == "1", "expected '1', got " .. tostring(csv.rows[1][2]))
        assert(csv.rows[2][1] == "Beta", "expected Beta, got " .. tostring(csv.rows[2][1]))
        assert(csv.rows[2][2] == "2", "expected '2', got " .. tostring(csv.rows[2][2]))
    )");
}

TEST_F(LuaRunner_ReadCsv, ExactJsonRoundTrip) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    write_lua_csv_file(sandbox / "small.csv", "a,b\n1,2\n");

    auto json = lua.run(R"(return db:read_csv("small.csv"))");
    EXPECT_EQ(json, R"({"header":["a","b"],"rows":[["1","2"]]})");
}

TEST_F(LuaRunner_ReadCsv, RaggedRowsSurviveShort) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    // 3-column header; one 2-column row (short) and one 4-column row (long).
    write_lua_csv_file(sandbox / "ragged.csv", "a,b,c\n1,2\n1,2,3,4\n1,2,3\n");

    lua.run(R"(
        local csv = db:read_csv("ragged.csv")
        assert(#csv.rows == 3, "expected 3 rows, got " .. #csv.rows)
        assert(#csv.rows[1] == 2, "expected row 1 to have 2 fields, got " .. #csv.rows[1])
        assert(csv.rows[1][3] == nil, "expected row 1 field 3 to be nil")
    )");
}

TEST_F(LuaRunner_ReadCsv, PreambleLineNotEaten) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    // A one-cell title line above the real header.
    write_lua_csv_file(sandbox / "preamble.csv", "Title Only\na,b\n1,2\n3,4\n");

    lua.run(R"(
        local csv = db:read_csv("preamble.csv")
        assert(#csv.header == 1, "expected 1 header column, got " .. #csv.header)
        assert(csv.header[1] == "Title Only", "expected 'Title Only', got " .. tostring(csv.header[1]))
        assert(#csv.rows == 3, "expected 3 rows, got " .. #csv.rows)
        assert(csv.rows[1][1] == "a", "expected the real header line to survive as a row, got " .. tostring(csv.rows[1][1]))
        assert(csv.rows[1][2] == "b", "expected the real header line to survive as a row, got " .. tostring(csv.rows[1][2]))
    )");
}

TEST_F(LuaRunner_ReadCsv, StringCellsNoInference) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    write_lua_csv_file(sandbox / "strings.csv", "code,date\n0012,2024-01-15\n");

    lua.run(R"(
        local csv = db:read_csv("strings.csv")
        assert(type(csv.rows[1][1]) == "string", "expected string type for code cell")
        assert(csv.rows[1][1] == "0012", "expected '0012', got " .. tostring(csv.rows[1][1]))
        assert(type(csv.rows[1][2]) == "string", "expected string type for date cell")
        assert(csv.rows[1][2] == "2024-01-15", "expected '2024-01-15', got " .. tostring(csv.rows[1][2]))
    )");
}

TEST_F(LuaRunner_ReadCsv, WhitespaceAndEmptyCellsDistinctFromNil) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    // Row 1 has 3 fields (whitespace-only, empty, non-empty); row 2 is short (1 field), so field
    // 2 is genuinely absent (nil), not an empty string.
    write_lua_csv_file(sandbox / "whitespace.csv", "a,b,c\n\" \",,x\nonly\n");

    lua.run(R"(
        local csv = db:read_csv("whitespace.csv")
        assert(csv.rows[1][1] == " ", "expected untrimmed single space, got '" .. tostring(csv.rows[1][1]) .. "'")
        assert(csv.rows[1][2] == "", "expected empty string, got " .. tostring(csv.rows[1][2]))
        assert(csv.rows[2][1] == "only", "expected 'only', got " .. tostring(csv.rows[2][1]))
        assert(csv.rows[2][2] == nil, "expected nil past the short row's end")
    )");
}

TEST_F(LuaRunner_ReadCsv, EmptyFileThrows) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    write_lua_csv_file(sandbox / "empty.csv", "");

    expect_lua_error(lua, R"(db:read_csv("empty.csv"))", "Cannot read_csv: file 'empty.csv' is empty");
}

TEST_F(LuaRunner_ReadCsv, HeaderOnlyFileYieldsEmptyRows) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    write_lua_csv_file(sandbox / "headeronly.csv", "a,b,c\n");

    lua.run(R"(
        local csv = db:read_csv("headeronly.csv")
        assert(#csv.header == 3, "expected 3 header columns, got " .. #csv.header)
        assert(#csv.rows == 0, "expected 0 rows, got " .. #csv.rows)
    )");

    auto json = lua.run(R"(return db:read_csv("headeronly.csv").rows)");
    EXPECT_EQ(json, "[]");
}

TEST_F(LuaRunner_ReadCsv, TwoConsecutiveReadsReturnIdenticalContents) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    write_lua_csv_file(sandbox / "repeat.csv", "a,b\n1,2\n3,4\n");

    lua.run(R"(
        local first = db:read_csv("repeat.csv")
        local second = db:read_csv("repeat.csv")
        assert(#first.rows == #second.rows, "row counts differ")
        for i = 1, #first.rows do
            for j = 1, #first.rows[i] do
                assert(first.rows[i][j] == second.rows[i][j], "cell mismatch at " .. i .. "," .. j)
            end
        end
        assert(first.header[1] == second.header[1] and first.header[2] == second.header[2], "header mismatch")
    )");
}

// --- db:read_csv_stream ---

TEST_F(LuaRunner_ReadCsv, StreamFiresOncePerRowWithIndexAndHeader) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    write_lua_csv_file(sandbox / "three_rows.csv", "a,b\n1,2\n3,4\n5,6\n");

    auto json = lua.run(R"(
        local seen = {}
        local n = db:read_csv_stream("three_rows.csv", function(row, index, header)
            assert(header[1] == "a", "expected header[1] == a, got " .. tostring(header[1]))
            assert(header[2] == "b", "expected header[2] == b, got " .. tostring(header[2]))
            seen[#seen + 1] = { index, row[1], row[2] }
        end)
        assert(n == 3, "expected 3 rows fed, got " .. tostring(n))
        return seen
    )");
    EXPECT_EQ(json, R"([[1,"1","2"],[2,"3","4"],[3,"5","6"]])");
}

TEST_F(LuaRunner_ReadCsv, StreamEarlyStopReturnsPartialCount) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    write_lua_csv_file(sandbox / "three_rows.csv", "a,b\n1,2\n3,4\n5,6\n");

    lua.run(R"(
        local seen = 0
        local n = db:read_csv_stream("three_rows.csv", function(row, index, header)
            seen = seen + 1
            if index == 2 then
                return false
            end
        end)
        assert(n == 2, "expected early stop to return 2, got " .. tostring(n))
        assert(seen == 2, "expected the third row to never be delivered, got " .. tostring(seen))
    )");
}

TEST_F(LuaRunner_ReadCsv, StreamCallbackReturningNothingRunsToCompletion) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    write_lua_csv_file(sandbox / "three_rows.csv", "a,b\n1,2\n3,4\n5,6\n");

    lua.run(R"(
        local n = db:read_csv_stream("three_rows.csv", function(row, index, header)
            if index == 999 then
                return false
            end
        end)
        assert(n == 3, "expected a no-return callback to process every row, got " .. tostring(n))
    )");
}

TEST_F(LuaRunner_ReadCsv, StreamComparisonAsLastStatementTruncates) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    write_lua_csv_file(sandbox / "three_rows.csv", "a,b\n1,2\n3,4\n5,6\n");

    // The documented hazard: a callback whose last statement is a plain comparison that
    // evaluates false truncates the stream, exactly as `return false` would.
    lua.run(R"(
        local n = db:read_csv_stream("three_rows.csv", function(row, index, header)
            return row[1] ~= "3"
        end)
        assert(n == 2, "expected the false comparison on row 2 to truncate the stream, got " .. tostring(n))
    )");
}

TEST_F(LuaRunner_ReadCsv, StreamCallbackErrorPropagatesAndClosesFile) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    auto csv_path = sandbox / "erroring.csv";
    write_lua_csv_file(csv_path, "a,b\n1,2\n3,4\n");

    expect_lua_error(
        lua, R"(db:read_csv_stream("erroring.csv", function(row, index, header) error("boom") end))", "boom");

    // On Windows an open reader (a lingering CSVReader from a sol::function/longjmp regression)
    // would block the delete -- this assertion is what fails if that regression is reintroduced.
    EXPECT_TRUE(std::filesystem::remove(csv_path));
}

TEST_F(LuaRunner_ReadCsv, StreamAndWholeFileReadYieldSameRows) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    write_lua_csv_file(sandbox / "compare.csv", "a,b\n1,2\n3,4\n5,6\n");

    lua.run(R"(
        local whole = db:read_csv("compare.csv")
        local streamed = {}
        db:read_csv_stream("compare.csv", function(row)
            streamed[#streamed + 1] = row
        end)
        assert(#whole.rows == #streamed, "row counts differ")
        for i = 1, #whole.rows do
            for j = 1, #whole.rows[i] do
                assert(whole.rows[i][j] == streamed[i][j], "cell mismatch at " .. i .. "," .. j)
            end
        end
    )");
}

// --- options table: separator (D-14 through D-18) ---

TEST_F(LuaRunner_ReadCsv, SemicolonSeparatorReadsCorrectly) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    write_lua_csv_file(sandbox / "semi.csv", "a;b\n1;2\n");

    lua.run(R"(
        local csv = db:read_csv("semi.csv", { separator = ";" })
        assert(csv.header[1] == "a", "expected header[1] == a, got " .. tostring(csv.header[1]))
        assert(csv.header[2] == "b", "expected header[2] == b, got " .. tostring(csv.header[2]))
        assert(#csv.rows == 1, "expected 1 row, got " .. #csv.rows)
        assert(csv.rows[1][1] == "1", "expected '1', got " .. tostring(csv.rows[1][1]))
        assert(csv.rows[1][2] == "2", "expected '2', got " .. tostring(csv.rows[1][2]))
    )");
}

TEST_F(LuaRunner_ReadCsv, StreamSemicolonSeparatorYieldsSameRowsAsWholeFileRead) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    write_lua_csv_file(sandbox / "semi.csv", "a;b\n1;2\n3;4\n");

    lua.run(R"(
        local whole = db:read_csv("semi.csv", { separator = ";" })
        local streamed = {}
        db:read_csv_stream("semi.csv", function(row)
            streamed[#streamed + 1] = row
        end, { separator = ";" })
        assert(#whole.rows == #streamed, "row counts differ")
        for i = 1, #whole.rows do
            for j = 1, #whole.rows[i] do
                assert(whole.rows[i][j] == streamed[i][j], "cell mismatch at " .. i .. "," .. j)
            end
        end
    )");
}

TEST_F(LuaRunner_ReadCsv, DefaultSeparatorMatchesEmptyOptionsTable) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    write_lua_csv_file(sandbox / "comma.csv", "a,b\n1,2\n3,4\n");

    lua.run(R"(
        local no_opts = db:read_csv("comma.csv")
        local empty_opts = db:read_csv("comma.csv", {})
        assert(no_opts.header[1] == empty_opts.header[1] and no_opts.header[2] == empty_opts.header[2],
            "header mismatch between no-options and empty-options reads")
        assert(#no_opts.rows == #empty_opts.rows, "row count mismatch")
        for i = 1, #no_opts.rows do
            for j = 1, #no_opts.rows[i] do
                assert(no_opts.rows[i][j] == empty_opts.rows[i][j], "cell mismatch at " .. i .. "," .. j)
            end
        end
    )");
}

TEST_F(LuaRunner_ReadCsv, TabSeparatorProvesOptionIsNotSpecialCased) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    write_lua_csv_file(sandbox / "tab.csv", "a\tb\n1\t2\n");

    lua.run(R"(
        local csv = db:read_csv("tab.csv", { separator = "\t" })
        assert(csv.header[1] == "a", "expected header[1] == a, got " .. tostring(csv.header[1]))
        assert(csv.header[2] == "b", "expected header[2] == b, got " .. tostring(csv.header[2]))
        assert(csv.rows[1][1] == "1", "expected '1', got " .. tostring(csv.rows[1][1]))
        assert(csv.rows[1][2] == "2", "expected '2', got " .. tostring(csv.rows[1][2]))
    )");
}

// --- options table: the rejection matrix (D-22 entries 3 through 6) ---
//
// Every case here asserts the call throws. `f.csv` deliberately does not exist on disk in most
// of these -- options are decoded before the file is ever opened (D-22 evaluation order), so a
// bad options table throws before a missing-file check could otherwise mask the real failure.

TEST_F(LuaRunner_ReadCsv, PositionalSeparatorStringThrowsOptionsMustBeATable) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    // The likeliest user mistake: a separator passed positionally instead of in a table.
    expect_lua_error(lua, R"(db:read_csv("f.csv", ";"))", "Cannot read_csv: options must be a table");
}

TEST_F(LuaRunner_ReadCsv, NumberInOptionsSlotThrowsOptionsMustBeATable) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    expect_lua_error(lua, R"(db:read_csv("f.csv", 59))", "Cannot read_csv: options must be a table");
}

TEST_F(LuaRunner_ReadCsv, BooleanInOptionsSlotThrowsOptionsMustBeATable) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    expect_lua_error(lua, R"(db:read_csv("f.csv", true))", "Cannot read_csv: options must be a table");
}

TEST_F(LuaRunner_ReadCsv, UnknownOptionKeyThrowsNamingTheKey) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    // The plausible abbreviation a script might try instead of `separator`.
    expect_lua_error(lua, R"(db:read_csv("f.csv", { delim = ";" }))", "Cannot read_csv: unknown option 'delim'");
}

TEST_F(LuaRunner_ReadCsv, FutureHeaderKeyIsAnUnknownOptionToday) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    // A key Phase 2 will legitimately add -- today it must throw, so that change is a deliberate
    // edit rather than a silent behaviour surprise.
    expect_lua_error(lua, R"(db:read_csv("f.csv", { header = false }))", "Cannot read_csv: unknown option 'header'");
}

TEST_F(LuaRunner_ReadCsv, ValidKeyDoesNotExcuseAnInvalidSibling) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    expect_lua_error(
        lua, R"(db:read_csv("f.csv", { separator = ";", delim = ";" }))", "Cannot read_csv: unknown option 'delim'");
}

TEST_F(LuaRunner_ReadCsv, SeparatorAsNumberThrowsMustBeAString) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    expect_lua_error(
        lua, R"(db:read_csv("f.csv", { separator = 59 }))", "Cannot read_csv: option 'separator' must be a string");
}

TEST_F(LuaRunner_ReadCsv, SeparatorAsBooleanThrowsMustBeAString) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    expect_lua_error(
        lua, R"(db:read_csv("f.csv", { separator = true }))", "Cannot read_csv: option 'separator' must be a string");
}

TEST_F(LuaRunner_ReadCsv, EmptySeparatorThrowsMustBeASingleCharacter) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    expect_lua_error(lua,
                     R"(db:read_csv("f.csv", { separator = "" }))",
                     "Cannot read_csv: option 'separator' must be a single character");
}

TEST_F(LuaRunner_ReadCsv, TwoCharacterSeparatorThrowsMustBeASingleCharacter) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    expect_lua_error(lua,
                     R"(db:read_csv("f.csv", { separator = ";;" }))",
                     "Cannot read_csv: option 'separator' must be a single character");
}

TEST_F(LuaRunner_ReadCsv, StreamUnknownOptionKeyNamesTheStreamEntryPoint) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    // Same bad table, but through db:read_csv_stream -- the operation name must follow the entry
    // point the script actually called (D-19), not a single shared literal.
    expect_lua_error(lua,
                     R"(db:read_csv_stream("f.csv", function() end, { delim = ";" }))",
                     "Cannot read_csv_stream: unknown option 'delim'");
}

TEST_F(LuaRunner_ReadCsv, StreamPositionalSeparatorStringThrowsOptionsMustBeATable) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    expect_lua_error(
        lua, R"(db:read_csv_stream("f.csv", function() end, ";"))", "Cannot read_csv_stream: options must be a table");
}

TEST_F(LuaRunner_ReadCsv, BothFormsAgreeOnAValidTableAndNeitherLeavesTheFileOpen) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    auto csv_path = sandbox / "agree.csv";
    write_lua_csv_file(csv_path, "a;b\n1;2\n3;4\n");

    lua.run(R"(
        local whole = db:read_csv("agree.csv", { separator = ";" })
        local streamed = {}
        db:read_csv_stream("agree.csv", function(row)
            streamed[#streamed + 1] = row
        end, { separator = ";" })
        assert(#whole.rows == #streamed, "row counts differ")
        for i = 1, #whole.rows do
            for j = 1, #whole.rows[i] do
                assert(whole.rows[i][j] == streamed[i][j], "cell mismatch at " .. i .. "," .. j)
            end
        end
    )");

    EXPECT_TRUE(std::filesystem::remove(csv_path));
}
