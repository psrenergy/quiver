#include "test_lua_runner.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <vector>

// UnreadableFileReportsParserFailure makes an existing, non-empty file impossible to open: an
// exclusive lock on Windows, chmod 000 on POSIX (where mode bits, unlike on Windows, do gate reads).
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {

void write_lua_csv_file(const std::filesystem::path& path, const std::string& content) {
    std::ofstream f(path, std::ios::binary);
    f << content;
}

// std::fstream accepts forward slashes on Windows; using them avoids escaping backslashes inside
// embedded Lua string literals (mirrors LuaBinaryTest::lp in test_lua_binary.cpp).
std::string lp(const std::string& p) {
    std::string r = p;
    std::replace(r.begin(), r.end(), '\\', '/');
    return r;
}

// Runs `script` and asserts it throws with a message starting with either entry point's own
// Pattern 1 prefix -- the blanket rule behind LUA-08/D-20: no csv-parser, std::filesystem or sol2
// message may reach a script unwrapped.
void expect_prefixed_error(quiver::LuaRunner& lua, const std::string& script) {
    try {
        lua.run(script);
        FAIL() << "expected script to throw: " << script;
    } catch (const std::exception& e) {
        // lua.run() wraps every script error in the root Pattern 3 "Failed to run Lua script: "
        // envelope; strip it to check the Pattern 1 message the binding itself raised.
        static const std::string kWrapper = "Failed to run Lua script: ";
        std::string msg = e.what();
        if (msg.rfind(kWrapper, 0) == 0) {
            msg.erase(0, kWrapper.size());
        }
        EXPECT_TRUE(msg.rfind("Cannot read_csv: ", 0) == 0 || msg.rfind("Cannot read_csv_stream: ", 0) == 0)
            << "message did not start with the expected prefix: " << msg;
    }
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

// --- header_row option (LUA-05, D-20) ---

TEST_F(LuaRunner_ReadCsv, HeaderRowSelectsNamedLineOverJunkAndUnits) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    // Line 1: junk above the header. Line 2: the real header. Line 3: a units row, below the
    // header. Lines 4+: data. Mirrors the real Maranhao Energia file's shape (D-22).
    write_lua_csv_file(sandbox / "junk_header_units.csv", "Title Only\nname,value\nunit,unit\nAlpha,1\nBeta,2\n");

    auto json = lua.run(R"(return db:read_csv("junk_header_units.csv", { header_row = 2 }))");
    EXPECT_EQ(json, R"({"header":["name","value"],"rows":[["unit","unit"],["Alpha","1"],["Beta","2"]]})")
        << "junk line 1 must appear nowhere in the result";
}

TEST_F(LuaRunner_ReadCsv, HeaderRowOneMatchesNoOptionsDefault) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    write_lua_csv_file(sandbox / "junk_header_units.csv", "Title Only\nname,value\nunit,unit\nAlpha,1\nBeta,2\n");

    // Default is header_row = 1 (D-20) -- explicitly asking for it must be byte-identical to
    // omitting the option entirely (the junk line becomes the header, same as PreambleLineNotEaten).
    auto explicit_json = lua.run(R"(return db:read_csv("junk_header_units.csv", { header_row = 1 }))");
    auto implicit_json = lua.run(R"(return db:read_csv("junk_header_units.csv"))");
    EXPECT_EQ(explicit_json, implicit_json);
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

// --- header_row past the end of the file (LUA-08) ---

TEST_F(LuaRunner_ReadCsv, HeaderRowPastEndOfFileThrowsExactMessage) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    write_lua_csv_file(sandbox / "three.csv", "a,b\n1,2\n3,4\n");

    // Asserted in full: csv-parser's actual past-EOF behavior is a SUCCESSFUL read with an empty
    // header and zero rows, which "looks like" a valid empty result under a loose substring check.
    expect_lua_error(lua,
                     R"(db:read_csv("three.csv", { header_row = 99 }))",
                     "Cannot read_csv: header row 99 not found in file 'three.csv'");
}

TEST_F(LuaRunner_ReadCsv, StreamHeaderRowPastEndOfFileNamesTheStreamEntryPoint) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    write_lua_csv_file(sandbox / "three.csv", "a,b\n1,2\n3,4\n");

    expect_lua_error(lua,
                     R"(db:read_csv_stream("three.csv", function() end, { header_row = 99 }))",
                     "Cannot read_csv_stream: header row 99 not found in file 'three.csv'");
}

TEST_F(LuaRunner_ReadCsv, HeaderRowOnLastLineSucceedsWithEmptyRows) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    // The header row is found (non-empty) with zero data rows following it -- a legitimate
    // header-only file, distinct from Finding 1's genuinely-not-found case (LUA-08 research).
    write_lua_csv_file(sandbox / "headerlast.csv", "1,2\n3,4\na,b\n");

    lua.run(R"(
        local csv = db:read_csv("headerlast.csv", { header_row = 3 })
        assert(#csv.header == 2, "expected 2 header columns, got " .. #csv.header)
        assert(csv.header[1] == "a" and csv.header[2] == "b", "unexpected header contents")
        assert(#csv.rows == 0, "expected 0 rows, got " .. #csv.rows)
    )");
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

    // Phase 2 legitimately added `header_row`, not `header` -- this key stays unknown forever.
    // Kept as its own test rather than folded into the sibling below so a future reader isn't
    // misled into thinking `header_row`'s arrival would make it obsolete.
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

// --- TEST-03: the five sandbox negatives, for both entry points, plus the subdirectory positive
// control. Every negative pins the full `Cannot <op>: ...` prefix plus the distinguishing text --
// never a lone keyword -- per the TEST-03 prohibition: a test that only asserts "it threw" goes
// green when the call fails for an unrelated reason and certifies containment it never exercised.
//
// LuaSandboxTest's per-test temp directory (named after the current suite + test name) is what
// keeps these negatives from colliding with each other or with the other CSV suites when the
// binary is re-run (TEST-03/concurrency) -- see test_lua_runner.h.

TEST_F(LuaRunner_ReadCsv, EscapingPathThrowsForReadCsv) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    expect_lua_error(lua,
                     R"(db:read_csv("../outside.csv"))",
                     "Cannot read_csv: path '../outside.csv' escapes the database directory");

    const std::string outside =
        lp((std::filesystem::temp_directory_path() / "quiver_lua_read_csv_outside" / "x.csv").string());
    expect_lua_error(lua,
                     "db:read_csv('" + outside + "')",
                     "Cannot read_csv: path '" + outside + "' escapes the database directory");
}

TEST_F(LuaRunner_ReadCsv, EscapingPathThrowsForReadCsvStream) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    expect_lua_error(lua,
                     R"(db:read_csv_stream("../outside.csv", function() end))",
                     "Cannot read_csv_stream: path '../outside.csv' escapes the database directory");

    const std::string outside =
        lp((std::filesystem::temp_directory_path() / "quiver_lua_read_csv_outside" / "x.csv").string());
    expect_lua_error(lua,
                     "db:read_csv_stream('" + outside + "', function() end)",
                     "Cannot read_csv_stream: path '" + outside + "' escapes the database directory");
}

TEST_F(LuaRunner_ReadCsv, InMemoryDatabaseThrowsForReadCsv) {
    // A separate in-memory Database + LuaRunner -- cannot share the sandbox fixture's file-backed
    // database, since an in-memory db has no directory to sandbox against.
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(":memory:", schema);
    quiver::LuaRunner lua(db);

    expect_lua_error(lua,
                     R"(db:read_csv("anything.csv"))",
                     "Cannot read_csv: database is in-memory, file operations are unavailable");
}

TEST_F(LuaRunner_ReadCsv, InMemoryDatabaseThrowsForReadCsvStream) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(":memory:", schema);
    quiver::LuaRunner lua(db);

    expect_lua_error(lua,
                     R"(db:read_csv_stream("anything.csv", function() end))",
                     "Cannot read_csv_stream: database is in-memory, file operations are unavailable");
}

TEST_F(LuaRunner_ReadCsv, MissingFileThrowsForReadCsv) {
    // The case resolve_sandboxed_path does not catch on its own: weakly_canonical tolerates a
    // missing path, so a green here is the evidence the separate existence check is present.
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    expect_lua_error(lua, R"(db:read_csv("missing.csv"))", "Cannot read_csv: file not found: missing.csv");
}

TEST_F(LuaRunner_ReadCsv, MissingFileThrowsForReadCsvStream) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    expect_lua_error(lua,
                     R"(db:read_csv_stream("missing.csv", function() end))",
                     "Cannot read_csv_stream: file not found: missing.csv");
}

TEST_F(LuaRunner_ReadCsv, DirectoryAsPathThrowsForReadCsv) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    std::filesystem::create_directories(sandbox / "adir");
    expect_lua_error(lua, R"(db:read_csv("adir"))", "Cannot read_csv: path is a directory: adir");
    EXPECT_TRUE(std::filesystem::remove(sandbox / "adir"));
}

TEST_F(LuaRunner_ReadCsv, DirectoryAsPathThrowsForReadCsvStream) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    std::filesystem::create_directories(sandbox / "adir");
    expect_lua_error(
        lua, R"(db:read_csv_stream("adir", function() end))", "Cannot read_csv_stream: path is a directory: adir");
    EXPECT_TRUE(std::filesystem::remove(sandbox / "adir"));
}

TEST_F(LuaRunner_ReadCsv, SubdirectoryPathReadsSuccessfullyForBothEntryPoints) {
    // The positive control: without this, the four negatives above would all pass equally well
    // against an implementation that rejects every path.
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    std::filesystem::create_directories(sandbox / "sub");
    write_lua_csv_file(sandbox / "sub" / "data.csv", "a,b\n1,2\n");

    lua.run(R"(
        local whole = db:read_csv("sub/data.csv")
        assert(whole.header[1] == "a" and whole.header[2] == "b", "header mismatch")
        assert(#whole.rows == 1, "expected 1 row, got " .. #whole.rows)
        assert(whole.rows[1][1] == "1" and whole.rows[1][2] == "2", "row mismatch")

        local streamed = {}
        local n = db:read_csv_stream("sub/data.csv", function(row)
            streamed[#streamed + 1] = row
        end)
        assert(n == 1, "expected 1 streamed row, got " .. tostring(n))
        assert(streamed[1][1] == "1" and streamed[1][2] == "2", "streamed row mismatch")
    )");
}

// --- the remaining D-22 catalogue entries: empty file (extended to the stream form) and the
// csv-parser wrapper ---

TEST_F(LuaRunner_ReadCsv, EmptyFileThrowsForReadCsvStream) {
    // EmptyFileThrows (above) covers db:read_csv; extend to db:read_csv_stream so both forms
    // agree on an empty file rather than one throwing and the other reporting zero rows.
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    write_lua_csv_file(sandbox / "empty_stream.csv", "");

    expect_lua_error(lua,
                     R"(db:read_csv_stream("empty_stream.csv", function() end))",
                     "Cannot read_csv_stream: file 'empty_stream.csv' is empty");
    EXPECT_TRUE(std::filesystem::remove(sandbox / "empty_stream.csv"));
}

// D-22 entry 10 (the csv-parser wrapper: "cannot read file '<p>': <reason>") fires when a file
// passes all three preconditions -- it exists, is not a directory, is non-empty -- but still cannot
// be opened. An exclusive lock produces exactly that: the metadata queries below are answered from
// the directory entry and succeed, while opening the file for reading fails. No elevation and no
// second process needed, so this runs anywhere the suite runs.
//
// The two platforms need different levers, because they disagree about what an ACL protects:
// on Windows a DENY ACE blocks the open but NOT the metadata queries, and chmod is a no-op for
// read access; on POSIX chmod 000 blocks the open while stat still succeeds. Both land in the same
// place. (POSIX skips the check when running as root, for whom mode bits are advisory.)
TEST_F(LuaRunner_ReadCsv, UnreadableFileReportsParserFailure) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto target = sandbox / "unreadable.csv";
    write_lua_csv_file(target, "a,b\n1,2\n");

#ifdef _WIN32
    // dwShareMode 0 == no sharing: every later open fails with ERROR_SHARING_VIOLATION.
    HANDLE lock =
        CreateFileW(target.wstring().c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    ASSERT_NE(lock, INVALID_HANDLE_VALUE) << "could not take an exclusive lock (GetLastError=" << GetLastError() << ")";
#else
    if (::geteuid() == 0) {
        GTEST_SKIP() << "running as root: mode bits do not restrict access";
    }
    ASSERT_EQ(::chmod(target.c_str(), 0), 0);
#endif

    // Guard against a false positive: the preconditions must genuinely still pass, or this test
    // would be asserting one of the three earlier catalogue messages instead of entry 10.
    std::error_code ec;
    EXPECT_TRUE(std::filesystem::exists(target, ec)) << ec.message();
    EXPECT_FALSE(std::filesystem::is_directory(target, ec)) << ec.message();
    EXPECT_GT(std::filesystem::file_size(target, ec), 0U) << ec.message();

    expect_lua_error(lua, R"(db:read_csv("unreadable.csv"))", "Cannot read_csv: cannot read file 'unreadable.csv': ");
    expect_lua_error(lua,
                     R"(db:read_csv_stream("unreadable.csv", function() end))",
                     "Cannot read_csv_stream: cannot read file 'unreadable.csv': ");

#ifdef _WIN32
    CloseHandle(lock);
#else
    ASSERT_EQ(::chmod(target.c_str(), 0600), 0);  // restore so TearDown can delete it
#endif
}

#ifdef _WIN32
// A Windows device name ("NUL", "CON", "COM1", in any case, in any directory) is not a filesystem
// path, and weakly_canonical throws std::filesystem_error on it rather than reporting a missing
// file. That call lives in resolve_sandboxed_path -- the choke point every file-touching Lua
// operation shares -- so before it was wrapped, a script got the raw
// "weakly_canonical: The parameter is incorrect.: ..." with no Pattern 1 prefix, breaking LUA-08.
// Guarded to _WIN32 because no POSIX path is reserved this way.
TEST_F(LuaRunner_ReadCsv, DeviceNamePathIsReportedWithPrefix) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    // Lowercase "nul" too: the reservation is case-insensitive, and only the spellings that reach
    // weakly_canonical exercise the wrapped path.
    for (const char* device : {"NUL", "nul"}) {
        expect_prefixed_error(lua, std::string(R"(db:read_csv(")") + device + R"("))");
        expect_prefixed_error(lua, std::string(R"(db:read_csv_stream(")") + device + R"(", function() end))");
    }
}
#endif

// --- catalogue ordering + adjacency ---

TEST_F(LuaRunner_ReadCsv, InMemoryDatabaseReportsBeforeBadOptions) {
    // Two conditions trip at once (in-memory db, a positionally-passed separator): the earlier
    // catalogue entry (in-memory, #1) must be the one reported, not the options error (#3).
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(":memory:", schema);
    quiver::LuaRunner lua(db);

    expect_lua_error(
        lua, R"(db:read_csv("x.csv", ";"))", "Cannot read_csv: database is in-memory, file operations are unavailable");
}

TEST_F(LuaRunner_ReadCsv, EscapingPathReportsBeforeMissingFile) {
    // The path both escapes the sandbox AND does not exist: the escape error (#2) must win over
    // file-not-found (#7).
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    expect_lua_error(lua,
                     R"(db:read_csv("../does_not_exist.csv"))",
                     "Cannot read_csv: path '../does_not_exist.csv' escapes the database directory");
}

TEST_F(LuaRunner_ReadCsv, UnknownKeyReportsBeforeBadSeparatorValue) {
    // Both problems present at once: an unknown key and a non-string separator. Unknown-key
    // (D-22 #4) must be reported ahead of separator-type (D-22 #5), regardless of the table's
    // iteration order.
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    expect_lua_error(
        lua, R"(db:read_csv("f.csv", { delim = ";", separator = 59 }))", "Cannot read_csv: unknown option 'delim'");
}

// --- the blanket rule: no unwrapped csv-parser / std::filesystem / sol2 message reaches Lua ---

TEST_F(LuaRunner_ReadCsv, EveryNegativeCaseStartsWithItsOwnEntryPointPrefix) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    std::filesystem::create_directories(sandbox / "adir");
    write_lua_csv_file(sandbox / "blank.csv", "");

    const std::vector<std::string> negatives = {
        R"(db:read_csv("../outside.csv"))",
        R"(db:read_csv_stream("../outside.csv", function() end))",
        R"(db:read_csv("missing.csv"))",
        R"(db:read_csv_stream("missing.csv", function() end))",
        R"(db:read_csv("adir"))",
        R"(db:read_csv_stream("adir", function() end))",
        R"(db:read_csv("blank.csv"))",
        R"(db:read_csv_stream("blank.csv", function() end))",
        R"(db:read_csv("f.csv", ";"))",
        R"(db:read_csv_stream("f.csv", function() end, ";"))",
        R"(db:read_csv("f.csv", { delim = ";" }))",
        R"(db:read_csv_stream("f.csv", function() end, { delim = ";" }))",
        R"(db:read_csv("f.csv", { separator = 59 }))",
        R"(db:read_csv("f.csv", { separator = "" }))",
        R"(db:read_csv("f.csv", { separator = ";;" }))",
    };
    for (const auto& script : negatives) {
        expect_prefixed_error(lua, script);
    }

    auto mem_db = quiver::Database::from_schema(":memory:", schema);
    quiver::LuaRunner mem_lua(mem_db);
    expect_prefixed_error(mem_lua, R"(db:read_csv("x.csv"))");
    expect_prefixed_error(mem_lua, R"(db:read_csv_stream("x.csv", function() end))");

    EXPECT_TRUE(std::filesystem::remove_all(sandbox / "adir"));
    EXPECT_TRUE(std::filesystem::remove(sandbox / "blank.csv"));
}
