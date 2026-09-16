#include "test_lua_runner.h"

#include <algorithm>
#include <string>

namespace {

// std::fstream accepts forward slashes on Windows; using them avoids escaping backslashes inside
// embedded Lua string literals (mirrors LuaBinaryTest::lp / test_lua_runner_read_csv.cpp's lp()).
std::string lp(const std::string& p) {
    std::string r = p;
    std::replace(r.begin(), r.end(), '\\', '/');
    return r;
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
