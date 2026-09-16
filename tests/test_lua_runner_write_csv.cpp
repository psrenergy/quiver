#include "test_lua_runner.h"

#include <algorithm>
#include <filesystem>
#include <string>

namespace {

// std::fstream accepts forward slashes on Windows; using them avoids escaping backslashes inside
// embedded Lua string literals (mirrors LuaBinaryTest::lp / test_lua_runner_read_csv.cpp's lp()).
std::string lp(const std::string& p) {
    std::string r = p;
    std::replace(r.begin(), r.end(), '\\', '/');
    return r;
}

// TEST-12: adapted from test_lua_runner_read_csv.cpp's own expect_prefixed_error. Strips the
// root Pattern 3 "Failed to run Lua script: " envelope, then asserts the Pattern 1 PREFIX and a
// reason substring SEPARATELY -- never one bare substring check, so a write_csv message can never
// satisfy a write_row assertion (or vice versa) and a matching prefix with the wrong reason still
// fails.
void expect_prefixed_error(quiver::LuaRunner& lua,
                           const std::string& script,
                           const std::string& expected_prefix,
                           const std::string& reason_substring) {
    try {
        lua.run(script);
        FAIL() << "expected script to throw: " << script;
    } catch (const std::exception& e) {
        static const std::string kWrapper = "Failed to run Lua script: ";
        std::string msg = e.what();
        if (msg.rfind(kWrapper, 0) == 0) {
            msg.erase(0, kWrapper.size());
        }
        EXPECT_TRUE(msg.rfind(expected_prefix, 0) == 0)
            << "message did not start with expected prefix '" << expected_prefix << "': " << msg;
        EXPECT_NE(msg.find(reason_substring), std::string::npos)
            << "message missing reason substring '" << reason_substring << "': " << msg;
    }
}

}  // namespace

// db:write_csv paths are sandboxed: relative paths resolve against the database directory, same
// as every other file-touching Lua operation. Every correctness assertion in this suite reads the
// emitted file back through db:read_csv and compares cells -- never by reading the raw file.
class LuaRunner_WriteCsv : public LuaSandboxTest {};

TEST_F(LuaRunner_WriteCsv, WriteRowThenReadCsvRoundTripsPlainStrings) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "out.csv").string());

    lua.run(R"(
        local w = db:write_csv(")" +
            path + R"(")
        w:write_row({ "Alpha", "Beta" })
        w:close()

        local csv = db:read_csv(")" +
            path + R"(", { header_row = 0 })
        assert(#csv.rows == 1, "expected 1 row, got " .. #csv.rows)
        assert(csv.rows[1][1] == "Alpha", "expected Alpha, got " .. tostring(csv.rows[1][1]))
        assert(csv.rows[1][2] == "Beta", "expected Beta, got " .. tostring(csv.rows[1][2]))
    )");
}

// FMT-04 / TEST-07: an int64 reaches append_number's std::int64_t overload directly, never routed
// through double first, so a value past double's 53-bit mantissa survives exactly.
TEST_F(LuaRunner_WriteCsv, IntegerCellRoundTripsExactDigitString) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "int.csv").string());

    lua.run(R"(
        local w = db:write_csv(")" +
            path + R"(")
        w:write_row({ 9007199254740993 })
        w:close()

        local csv = db:read_csv(")" +
            path + R"(", { header_row = 0 })
        assert(csv.rows[1][1] == "9007199254740993", "expected exact digit string, got " .. tostring(csv.rows[1][1]))
    )");
}

// D-34: a whole float writes as append_number's to_chars gives it -- no synthetic ".0" -- so a
// float 2014.0 and the integer 2014 are indistinguishable text after the round trip.
TEST_F(LuaRunner_WriteCsv, WholeFloatAndEqualIntegerProduceSameCellText) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "float.csv").string());

    lua.run(R"(
        local w = db:write_csv(")" +
            path + R"(")
        w:write_row({ 2014.0 })
        w:write_row({ 2014 })
        w:close()

        local csv = db:read_csv(")" +
            path + R"(", { header_row = 0 })
        assert(csv.rows[1][1] == csv.rows[2][1],
            "expected equal text, got " .. tostring(csv.rows[1][1]) .. " vs " .. tostring(csv.rows[2][1]))
        assert(csv.rows[1][1] == "2014", "expected no synthetic decimal point, got " .. tostring(csv.rows[1][1]))
    )");
}

// FMT-06: a boolean writes as the one-character text 1 or 0, the project-wide boolean-is-INTEGER
// write policy.
TEST_F(LuaRunner_WriteCsv, BooleanCellWritesOneOrZero) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "bool.csv").string());

    lua.run(R"(
        local w = db:write_csv(")" +
            path + R"(")
        w:write_row({ true, false })
        w:close()

        local csv = db:read_csv(")" +
            path + R"(", { header_row = 0 })
        assert(csv.rows[1][1] == "1", "expected '1', got " .. tostring(csv.rows[1][1]))
        assert(csv.rows[1][2] == "0", "expected '0', got " .. tostring(csv.rows[1][2]))
    )");
}

// FMT-06: a table or function cell is a Pattern 1 error naming write_row and the 1-based cell
// index -- never silently dropped or stringified.
TEST_F(LuaRunner_WriteCsv, TableCellThrowsNamingWriteRowAndCellIndex) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "bad_cell.csv").string());

    expect_lua_error(lua,
                     R"(
        local w = db:write_csv(")" +
                         path + R"(")
        w:write_row({ "ok", {} })
    )",
                     "Cannot write_row: cell #2 has unsupported Lua type");
}

TEST_F(LuaRunner_WriteCsv, FunctionCellThrowsNamingWriteRowAndCellIndex) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "bad_cell_fn.csv").string());

    expect_lua_error(lua,
                     R"(
        local w = db:write_csv(")" +
                         path + R"(")
        w:write_row({ print })
    )",
                     "Cannot write_row: cell #1 has unsupported Lua type");
}

// FMT-08: row width is the MAXIMUM integer key, not the count of present keys -- an interior hole
// (key 2 absent, key 3 present) writes an empty middle cell rather than collapsing the row.
TEST_F(LuaRunner_WriteCsv, RowWidthComesFromMaxIntegerKeyNotKeyCount) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "hole.csv").string());

    lua.run(R"(
        local w = db:write_csv(")" +
            path + R"(")
        w:write_row({ [1] = "a", [3] = "c" })
        w:close()

        local csv = db:read_csv(")" +
            path + R"(", { header_row = 0 })
        assert(csv.rows[1][1] == "a", "expected a, got " .. tostring(csv.rows[1][1]))
        assert(csv.rows[1][2] == "", "expected empty middle cell, got " .. tostring(csv.rows[1][2]))
        assert(csv.rows[1][3] == "c", "expected c, got " .. tostring(csv.rows[1][3]))
    )");
}

// FMT-02 extended to the degenerate zero-cell case: a row with zero integer keys still writes one
// quoted empty cell, so the record survives db:read_csv's KEEP_NON_EMPTY policy instead of being
// discarded as a blank line.
TEST_F(LuaRunner_WriteCsv, RowWithZeroIntegerKeysWritesOneQuotedEmptyCell) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "empty_row.csv").string());

    lua.run(R"(
        local w = db:write_csv(")" +
            path + R"(")
        w:write_row({})
        w:close()

        local csv = db:read_csv(")" +
            path + R"(", { header_row = 0 })
        assert(#csv.rows == 1, "expected 1 row, got " .. #csv.rows)
        assert(csv.rows[1][1] == "", "expected one empty cell, got " .. tostring(csv.rows[1][1]))
    )");
}

// FMT-08: a non-integer row key, or an integer key below 1, is a Pattern 1 error naming write_row
// -- never silently ignored.
TEST_F(LuaRunner_WriteCsv, NonIntegerRowKeyThrows) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "bad_key.csv").string());

    expect_lua_error(lua,
                     R"(
        local w = db:write_csv(")" +
                         path + R"(")
        w:write_row({ x = "y" })
    )",
                     "Cannot write_row:");
}

TEST_F(LuaRunner_WriteCsv, SubOneIntegerRowKeyThrows) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "bad_key_zero.csv").string());

    expect_lua_error(lua,
                     R"(
        local w = db:write_csv(")" +
                         path + R"(")
        w:write_row({ [0] = "y" })
    )",
                     "Cannot write_row:");
}

// FMT-02 is narrow by design: a multi-column row with an empty middle field stays unquoted and its
// neighbours are unaffected.
TEST_F(LuaRunner_WriteCsv, MultiColumnRowWithEmptyMiddleFieldLeavesNeighborsIntact) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "empty_middle.csv").string());

    lua.run(R"(
        local w = db:write_csv(")" +
            path + R"(")
        w:write_row({ "a", "", "b" })
        w:close()

        local csv = db:read_csv(")" +
            path + R"(", { header_row = 0 })
        assert(csv.rows[1][1] == "a", "expected a, got " .. tostring(csv.rows[1][1]))
        assert(csv.rows[1][2] == "", "expected empty, got " .. tostring(csv.rows[1][2]))
        assert(csv.rows[1][3] == "b", "expected b, got " .. tostring(csv.rows[1][3]))
    )");
}

// TEST-09 / FMT-02: a single-column file with an empty cell in the first, a middle, and the last
// row round-trips with every row present -- none deleted as a blank line -- and a nil cell and an
// empty-string cell produce byte-identical records (D-40): rows[1] (nil) and rows[2] ("") compare
// equal.
TEST_F(LuaRunner_WriteCsv, SingleColumnFileWithNilAndEmptyCellsRoundTripsEveryRow) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "single_column.csv").string());

    lua.run(R"(
        local w = db:write_csv(")" +
            path + R"(")
        w:write_row({})
        w:write_row({ "" })
        w:write_row({})
        w:close()

        local csv = db:read_csv(")" +
            path + R"(", { header_row = 0 })
        assert(#csv.rows == 3, "expected 3 rows, got " .. #csv.rows)
        assert(csv.rows[1][1] == "", "expected empty first row, got " .. tostring(csv.rows[1][1]))
        assert(csv.rows[2][1] == "", "expected empty middle row, got " .. tostring(csv.rows[2][1]))
        assert(csv.rows[3][1] == "", "expected empty last row, got " .. tostring(csv.rows[3][1]))
        assert(csv.rows[1][1] == csv.rows[2][1], "nil cell and empty-string cell must be indistinguishable")
    )");
}

// TEST-06 dirty-cell suite. Every fixture below writes with db:write_csv/w:write_row/w:close, then
// reads the SAME path back with db:read_csv in the same script and compares cells positionally --
// never by opening the file with an ifstream or searching it for a quote character, which would
// prove the emitter emitted, not that the file is readable (this project's third encounter with
// that trap; see export_csv's 118 export-only tests).

// A single cell carrying all four quote-trigger bytes at once: the configured separator, a bare
// quote character, a CR and an LF. A regression that mishandles any one of them corrupts this
// cell.
TEST_F(LuaRunner_WriteCsv, CellWithSeparatorQuoteCrAndLfTogetherRoundTrips) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "dirty_cell.csv").string());

    lua.run(R"(
        local dirty = ',' .. '"' .. '\r' .. '\n'
        local w = db:write_csv(")" +
            path + R"(")
        w:write_row({ dirty })
        w:close()

        local csv = db:read_csv(")" +
            path + R"(", { header_row = 0 })
        assert(#csv.rows == 1, "expected 1 row, got " .. #csv.rows)
        assert(csv.rows[1][1] == dirty, "dirty cell did not round-trip byte-identically")
    )");
}

// Same fixture as above, repeated under a non-comma separator, so the quote trigger tracks the
// CONFIGURED separator rather than a hardcoded comma.
TEST_F(LuaRunner_WriteCsv, CellWithSeparatorQuoteCrAndLfTogetherRoundTripsWithSemicolonSeparator) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "dirty_cell_semicolon.csv").string());

    lua.run(R"(
        local dirty = ';' .. '"' .. '\r' .. '\n'
        local w = db:write_csv(")" +
            path + R"(", { separator = ";" })
        w:write_row({ dirty })
        w:close()

        local csv = db:read_csv(")" +
            path + R"(", { header_row = 0, separator = ";" })
        assert(#csv.rows == 1, "expected 1 row, got " .. #csv.rows)
        assert(csv.rows[1][1] == dirty, "dirty cell did not round-trip byte-identically under ';'")
    )");
}

// TEST-08: a field that is exactly one quote character serializes to four quote characters and
// round-trips as a one-character string.
TEST_F(LuaRunner_WriteCsv, LoneQuoteCharacterCellRoundTripsAsLengthOne) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "lone_quote.csv").string());

    lua.run(R"(
        local w = db:write_csv(")" +
            path + R"(")
        w:write_row({ '"' })
        w:close()

        local csv = db:read_csv(")" +
            path + R"(", { header_row = 0 })
        assert(#csv.rows[1][1] == 1, "expected length 1, got " .. #csv.rows[1][1])
        assert(csv.rows[1][1] == '"', "expected a single quote character, got " .. tostring(csv.rows[1][1]))
    )");
}

// TEST-08: a field that is exactly two quote characters serializes to six quote characters and
// round-trips as a two-character string.
TEST_F(LuaRunner_WriteCsv, TwoQuoteCharacterCellRoundTripsAsLengthTwo) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "two_quotes.csv").string());

    lua.run(R"(
        local w = db:write_csv(")" +
            path + R"(")
        w:write_row({ '""' })
        w:close()

        local csv = db:read_csv(")" +
            path + R"(", { header_row = 0 })
        assert(#csv.rows[1][1] == 2, "expected length 2, got " .. #csv.rows[1][1])
        assert(csv.rows[1][1] == '""', "expected two quote characters, got " .. tostring(csv.rows[1][1]))
    )");
}

// A cell whose FIRST byte is the separator, a cell whose LAST byte is the separator, and a cell
// that is NOTHING BUT the separator -- three separate cells in one row, so an off-by-one in the
// quote-trigger scan cannot hide behind only one of the three shapes.
TEST_F(LuaRunner_WriteCsv, LeadingTrailingAndSeparatorOnlyCellsRoundTripPositionally) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "separator_positions.csv").string());

    lua.run(R"(
        local leading = ',lead'
        local trailing = 'trail,'
        local sep_only = ','
        local w = db:write_csv(")" +
            path + R"(")
        w:write_row({ leading, trailing, sep_only })
        w:close()

        local csv = db:read_csv(")" +
            path + R"(", { header_row = 0 })
        assert(csv.rows[1][1] == leading, "expected leading-separator cell intact, got " .. tostring(csv.rows[1][1]))
        assert(csv.rows[1][2] == trailing, "expected trailing-separator cell intact, got " .. tostring(csv.rows[1][2]))
        assert(csv.rows[1][3] == sep_only, "expected separator-only cell intact, got " .. tostring(csv.rows[1][3]))
    )");
}

// A cell containing CR immediately followed by LF round-trips as that exact two-byte sequence --
// neither collapsed to one byte, nor merged with the record terminator, nor split into two rows.
// Neighbouring rows prove the row count: a regression that treats the embedded CRLF as a record
// terminator would turn this into 4 rows instead of 3.
TEST_F(LuaRunner_WriteCsv, CrThenLfCellRoundTripsAsTwoByteSequenceWithoutSplittingRows) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "cr_then_lf.csv").string());

    lua.run(R"(
        local w = db:write_csv(")" +
            path + R"(")
        w:write_row({ "before" })
        w:write_row({ "\r\n" })
        w:write_row({ "after" })
        w:close()

        local csv = db:read_csv(")" +
            path + R"(", { header_row = 0 })
        assert(#csv.rows == 3, "expected exactly 3 rows, got " .. #csv.rows)
        assert(csv.rows[1][1] == "before", "expected 'before', got " .. tostring(csv.rows[1][1]))
        assert(#csv.rows[2][1] == 2, "expected the CR-LF cell to keep length 2, got " .. #csv.rows[2][1])
        assert(csv.rows[2][1] == "\r\n", "expected the exact two-byte CR-LF sequence, got " .. tostring(csv.rows[2][1]))
        assert(csv.rows[3][1] == "after", "expected 'after', got " .. tostring(csv.rows[3][1]))
    )");
}

// FMT-02's narrowness: an empty cell sitting next to a cell that DOES need quoting (because it
// contains the separator) still round-trips as empty and does not disturb its neighbours --
// asserting the presence AND the boundary, not merely that SOME empty cell survives somewhere.
TEST_F(LuaRunner_WriteCsv, EmptyCellAdjacentToAQuotedCellRoundTripsWithNeighborsIntact) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "empty_next_to_quoted.csv").string());

    lua.run(R"(
        local w = db:write_csv(")" +
            path + R"(")
        w:write_row({ "a,b", "", "c" })
        w:close()

        local csv = db:read_csv(")" +
            path + R"(", { header_row = 0 })
        assert(csv.rows[1][1] == "a,b", "expected the comma-carrying cell intact, got " .. tostring(csv.rows[1][1]))
        assert(csv.rows[1][2] == "", "expected the adjacent cell empty, got " .. tostring(csv.rows[1][2]))
        assert(csv.rows[1][3] == "c", "expected the trailing neighbour intact, got " .. tostring(csv.rows[1][3]))
    )");
}

// A multi-byte UTF-8 cell containing no quote byte (no 0x22 anywhere in it) round-trips
// byte-identically and unmodified. Built via string.char so the assertion never depends on this
// .cpp file's own source encoding -- compared against the same Lua variable that was written, not
// a C++ string literal.
TEST_F(LuaRunner_WriteCsv, MultiByteUtf8CellWithNoQuoteByteRoundTripsUnmodified) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "utf8_cell.csv").string());

    lua.run(R"(
        -- "caf" .. U+00E9 ('e' with acute accent, UTF-8 bytes 0xC3 0xA9) .. U+65E5 U+672C U+8A9E
        -- (the three UTF-8-encoded kanji of "Japanese", bytes 0xE6 0x97 0xA5 0xE6 0x9C 0xAC 0xE8
        -- 0xAA 0x9E) -- none of these bytes is 0x22 (the ASCII quote byte).
        local original = "caf" .. string.char(0xC3, 0xA9) .. " " ..
            string.char(0xE6, 0x97, 0xA5, 0xE6, 0x9C, 0xAC, 0xE8, 0xAA, 0x9E)
        local w = db:write_csv(")" +
            path + R"(")
        w:write_row({ original })
        w:close()

        local csv = db:read_csv(")" +
            path + R"(", { header_row = 0 })
        assert(csv.rows[1][1] == original, "expected the UTF-8 cell unmodified, got " .. tostring(csv.rows[1][1]))
    )");
}

// LUA-09: separator and header are the only accepted option keys.
TEST_F(LuaRunner_WriteCsv, UnknownOptionKeyThrows) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "unknown_opt.csv").string());

    expect_lua_error(lua, R"(db:write_csv(")" + path + R"(", { foo = 1 }))", "Cannot write_csv: unknown option");
}

TEST_F(LuaRunner_WriteCsv, NonStringSeparatorThrows) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "bad_sep_type.csv").string());

    expect_lua_error(lua,
                     R"(db:write_csv(")" + path + R"(", { separator = 5 }))",
                     "Cannot write_csv: option 'separator' must be a string");
}

TEST_F(LuaRunner_WriteCsv, MultiCharacterSeparatorThrows) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "bad_sep.csv").string());

    expect_lua_error(lua,
                     R"(db:write_csv(")" + path + R"(", { separator = ";;" }))",
                     "option 'separator' must be a single character");
}

TEST_F(LuaRunner_WriteCsv, NonTableHeaderThrows) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "bad_header_type.csv").string());

    expect_lua_error(lua,
                     R"(db:write_csv(")" + path + R"(", { header = "x" }))",
                     "Cannot write_csv: option 'header' must be a table");
}

TEST_F(LuaRunner_WriteCsv, NonStringHeaderEntryThrows) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "bad_header_entry.csv").string());

    expect_lua_error(lua,
                     R"(db:write_csv(")" + path + R"(", { header = { 1, 2 } }))",
                     "Cannot write_csv: option 'header' entry must be a string");
}

// D-14/D-20-style defaults: an absent options argument, an explicit nil, an empty table, and
// header set to an empty table all mean comma separator and no header row.
TEST_F(LuaRunner_WriteCsv, AbsentNilAndEmptyOptionsAllMeanDefaults) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "defaults.csv").string());

    lua.run(R"(
        local function check(opts)
            local w = db:write_csv(")" +
            path + R"(", opts)
            w:write_row({ "a", "b" })
            w:close()

            local csv = db:read_csv(")" +
            path + R"(", { header_row = 0 })
            assert(csv.header == nil, "expected no header")
            assert(#csv.rows == 1, "expected 1 row, got " .. #csv.rows)
            assert(csv.rows[1][1] == "a" and csv.rows[1][2] == "b", "expected default comma separator")
        end

        check(nil)
        check()
        check({})
        check({ header = {} })
    )");
}

// WRITE-03: header is WRITTEN (not merely decoded), in order, ahead of the first data row; one
// header name contains the configured separator and comes back intact, proving the header is
// quoted by the same record emitter a data row uses.
TEST_F(LuaRunner_WriteCsv, HeaderIsWrittenAheadOfDataAndQuotedLikeARow) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "header.csv").string());

    lua.run(R"(
        local w = db:write_csv(")" +
            path + R"(", { separator = ";", header = { "a;b", "c" } })
        w:write_row({ "1", "2" })
        w:close()

        local csv = db:read_csv(")" +
            path + R"(", { header_row = 0, separator = ";" })
        assert(#csv.rows == 2, "expected 2 rows (header + data), got " .. #csv.rows)
        assert(csv.rows[1][1] == "a;b", "expected header col 1 intact, got " .. tostring(csv.rows[1][1]))
        assert(csv.rows[1][2] == "c", "expected header col 2, got " .. tostring(csv.rows[1][2]))
        assert(csv.rows[2][1] == "1", "expected data col 1, got " .. tostring(csv.rows[2][1]))
        assert(csv.rows[2][2] == "2", "expected data col 2, got " .. tostring(csv.rows[2][2]))
    )");
}

// FMT-05: a non-finite number cell (NaN or +/-infinity) is a Pattern 1 error naming write_row,
// the 1-based data-row ordinal, and the 1-based cell index -- never a platform-specific token
// (MSVC's "-nan(ind)"/"nan"/"inf" vs. glibc's "nan"/"inf") reaching the file.
TEST_F(LuaRunner_WriteCsv, NonFiniteNumberCellThrowsNamingWriteRowAndRowOrdinal) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "nan_row.csv").string());

    expect_lua_error(lua,
                     R"(
        local w = db:write_csv(")" +
                         path + R"(")
        w:write_row({ "a" })
        w:write_row({ "b" })
        w:write_row({ 0 / 0 })
    )",
                     "Cannot write_row:");

    // Re-run in a fresh script so the row-ordinal/cell-index assertion is isolated from the
    // pcall/file-intact proof below.
    const auto path2 = lp((sandbox / "inf_row.csv").string());
    try {
        lua.run(R"(
            local w = db:write_csv(")" +
                path2 + R"(")
            w:write_row({ "a" })
            w:write_row({ "b" })
            w:write_row({ 1 / 0 })
        )");
        FAIL() << "expected script to throw";
    } catch (const std::exception& e) {
        const std::string msg = e.what();
        EXPECT_NE(msg.find("3"), std::string::npos) << "expected row ordinal 3 in: " << msg;
        EXPECT_NE(msg.find("#1"), std::string::npos) << "expected cell index 1 in: " << msg;
    }
}

// FMT-05 + the file-intact guarantee: the record is assembled into a buffer first, so a rejected
// row leaves the file exactly as it was before the failing w:write_row call. The explicit
// w:close() after the pcall is load-bearing -- close() is this phase's only flush (WRITE-06 is
// Phase 5's), so an abandoned writer's data would still be sitting in the ofstream buffer and the
// read-back would hit the reader's empty-file error instead of returning 2 rows.
TEST_F(LuaRunner_WriteCsv, RejectedNonFiniteRowLeavesFileIntactAfterPcallAndClose) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "intact.csv").string());

    lua.run(R"(
        local w = db:write_csv(")" +
            path + R"(")
        w:write_row({ "row1" })
        w:write_row({ "row2" })
        local ok, err = pcall(function() w:write_row({ 0 / 0 }) end)
        assert(ok == false, "expected the third write_row to fail")
        assert(err:find("Cannot write_row:", 1, true) ~= nil, "expected Cannot write_row: prefix, got " .. tostring(err))
        w:close()

        local csv = db:read_csv(")" +
            path + R"(", { header_row = 0 })
        assert(#csv.rows == 2, "expected exactly 2 rows after the rejected third, got " .. #csv.rows)
        assert(csv.rows[1][1] == "row1", "expected row1, got " .. tostring(csv.rows[1][1]))
        assert(csv.rows[2][1] == "row2", "expected row2, got " .. tostring(csv.rows[2][1]))
    )");
}

// WRITE-05: write_row after close is a Pattern 1 error naming write_row; close is idempotent.
TEST_F(LuaRunner_WriteCsv, WriteRowAfterCloseThrowsNamingWriteRow) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "after_close.csv").string());

    expect_lua_error(lua,
                     R"(
        local w = db:write_csv(")" +
                         path + R"(")
        w:close()
        w:write_row({ "x" })
    )",
                     "Cannot write_row:");
}

TEST_F(LuaRunner_WriteCsv, CloseCalledTwiceDoesNotThrow) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "double_close.csv").string());

    lua.run(R"(
        local w = db:write_csv(")" +
            path + R"(")
        w:write_row({ "x" })
        w:close()
        w:close()
    )");
}

// WRITE-07: a missing parent directory fails the open with a Pattern 1 error naming write_csv and
// the caller's own path spelling; the directory is not created.
TEST_F(LuaRunner_WriteCsv, MissingParentDirectoryThrowsAndDoesNotCreateIt) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto missing_dir = sandbox / "does_not_exist";
    const auto path = lp((missing_dir / "nested.csv").string());

    expect_lua_error(lua, R"(db:write_csv(")" + path + R"("))", "Cannot write_csv:");
    ASSERT_FALSE(std::filesystem::exists(missing_dir)) << "constructor must not create the missing directory";
}

// LUA-10: a path escaping the database directory takes precedence over an invalid separator --
// the sandbox resolves before the options table is decoded, so the path error is the one raised.
TEST_F(LuaRunner_WriteCsv, EscapingPathTakesPrecedenceOverInvalidSeparator) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    expect_lua_error(lua, R"(db:write_csv("../escape.csv", { separator = ";;" }))", "escapes the database directory");
}

// WRITE-08: db:write_csv truncates an existing target at open. Two rows written and closed, then
// the SAME path reopened and one row written, reads back as exactly one row.
TEST_F(LuaRunner_WriteCsv, ReopeningSamePathTruncatesExistingContent) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "truncate.csv").string());

    lua.run(R"(
        local w1 = db:write_csv(")" +
            path + R"(")
        w1:write_row({ "1" })
        w1:write_row({ "2" })
        w1:close()

        local w2 = db:write_csv(")" +
            path + R"(")
        w2:write_row({ "3" })
        w2:close()

        local csv = db:read_csv(")" +
            path + R"(", { header_row = 0 })
        assert(#csv.rows == 1, "expected 1 row after truncate-at-open, got " .. #csv.rows)
        assert(csv.rows[1][1] == "3", "expected '3', got " .. tostring(csv.rows[1][1]))
    )");
}

// TEST-12: the catalogue suite. Every assertion below checks a Pattern 1 PREFIX and a reason
// substring separately (expect_prefixed_error above) -- never a bare substring -- so a write_csv
// message can never satisfy a write_row assertion and vice versa.
class LuaRunner_WriteCsvErrors : public LuaSandboxTest {};

TEST_F(LuaRunner_WriteCsvErrors, NonFiniteNumberCellIsPrefixedWriteRowError) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "catalogue_nan.csv").string());

    expect_prefixed_error(lua,
                          R"(
        local w = db:write_csv(")" +
                              path + R"(")
        w:write_row({ 0 / 0 })
    )",
                          "Cannot write_row: ",
                          "is not a finite number");
}

TEST_F(LuaRunner_WriteCsvErrors, TableCellIsPrefixedWriteRowError) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "catalogue_table.csv").string());

    expect_prefixed_error(lua,
                          R"(
        local w = db:write_csv(")" +
                              path + R"(")
        w:write_row({ {} })
    )",
                          "Cannot write_row: ",
                          "has unsupported Lua type");
}

TEST_F(LuaRunner_WriteCsvErrors, WriteAfterCloseIsPrefixedWriteRowError) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "catalogue_after_close.csv").string());

    expect_prefixed_error(lua,
                          R"(
        local w = db:write_csv(")" +
                              path + R"(")
        w:close()
        w:write_row({ "x" })
    )",
                          "Cannot write_row: ",
                          "already closed");
}

TEST_F(LuaRunner_WriteCsvErrors, EscapingPathIsPrefixedWriteCsvError) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    expect_prefixed_error(
        lua, R"(db:write_csv("../escape.csv"))", "Cannot write_csv: ", "escapes the database directory");
}

TEST_F(LuaRunner_WriteCsvErrors, InMemoryDatabaseIsPrefixedWriteCsvError) {
    // A separate in-memory Database + LuaRunner -- an in-memory db has no directory to sandbox
    // against, so this cannot share the fixture's file-backed database (mirrors
    // test_lua_runner_read_csv.cpp's InMemoryDatabaseThrowsForReadCsv).
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(":memory:", schema);
    quiver::LuaRunner lua(db);

    expect_prefixed_error(lua,
                          R"(db:write_csv("anything.csv"))",
                          "Cannot write_csv: ",
                          "database is in-memory, file operations are unavailable");
}

TEST_F(LuaRunner_WriteCsvErrors, DoubleCloseIsIdempotentNotAnError) {
    // Asserted separately from WriteAfterCloseIsPrefixedWriteRowError above, so a single
    // over-broad "already closed" guard covering both write_row and close cannot satisfy both
    // tests at once: this one asserts NO throw, not merely the absence of one particular message.
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "catalogue_double_close.csv").string());

    EXPECT_NO_THROW(lua.run(R"(
        local w = db:write_csv(")" +
                            path + R"(")
        w:write_row({ "x" })
        w:close()
        w:close()
    )"));
}

// LUA-10: a call passing BOTH an escaping path and an invalid separator receives the path error,
// not the separator error, because the sandbox resolves before the options table is decoded. Both
// bad inputs are present on purpose -- do not "simplify" this fixture down to one bad input, or
// the ordering guarantee this test exists to pin silently stops being checked.
TEST_F(LuaRunner_WriteCsvErrors, EscapingPathBeatsInvalidSeparator) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    expect_prefixed_error(lua,
                          R"(db:write_csv("../escape.csv", { separator = ";;" }))",
                          "Cannot write_csv: ",
                          "escapes the database directory");
}

// FMT-05 + the file-intact guarantee, asserted as a single script (not merely a row count, which
// would pass even if the write had never thrown): two good rows, a pcall-caught non-finite third
// row asserting BOTH the failed pcall and its "Cannot write_row:" prefix, then w:close() and a
// db:read_csv read-back asserting exactly 2 rows.
//
// w:close() here is load-bearing, not ceremony: close() is this phase's only flush (the
// sol::state on LuaRunner::Impl persists across run() calls and nothing calls lua_close), so a
// writer abandoned by an uncaught throw is never finalized and a second-run() read-back would hit
// the reader's empty-file error instead of returning 2 rows. The flush at run()'s return is Phase
// 5's WRITE-06 -- do not delete this w:close() as "redundant" or the test starts failing for a
// reason that has nothing to do with the writer.
TEST_F(LuaRunner_WriteCsvErrors, RejectedRowLeavesFileIntactProvenBothHalves) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "catalogue_intact.csv").string());

    lua.run(R"(
        local w = db:write_csv(")" +
            path + R"(")
        w:write_row({ "row1" })
        w:write_row({ "row2" })
        local ok, err = pcall(function() w:write_row({ 0 / 0 }) end)
        assert(ok == false, "expected the third write_row to fail")
        assert(err:find("Cannot write_row:", 1, true) ~= nil, "expected Cannot write_row: prefix, got " .. tostring(err))
        w:close()  -- load-bearing: this phase's only flush (WRITE-06 is Phase 5's), see comment above

        local csv = db:read_csv(")" +
            path + R"(", { header_row = 0 })
        assert(#csv.rows == 2, "expected exactly 2 rows after the rejected third, got " .. #csv.rows)
        assert(csv.rows[1][1] == "row1", "expected row1, got " .. tostring(csv.rows[1][1]))
        assert(csv.rows[2][1] == "row2", "expected row2, got " .. tostring(csv.rows[2][1]))
    )");
}
