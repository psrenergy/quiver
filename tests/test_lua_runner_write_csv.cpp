#include "test_lua_runner.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

// std::fstream accepts forward slashes on Windows; using them avoids escaping backslashes inside
// embedded Lua string literals (mirrors LuaBinaryTest::lp / test_lua_runner_read_csv.cpp's lp()).
std::string lp(const std::string& p) {
    std::string r = p;
    std::replace(r.begin(), r.end(), '\\', '/');
    return r;
}

// Reads bindings/js/src/lua-api.ts (via quiver::test::path_from) and extracts the fenced ```lua
// block that follows a given `## heading` -- the same drift-proof technique
// bindings/js/test/lua-api-sync.test.ts uses on src/lua_runner.cpp, applied here in the other
// direction (parsing the reference instead of parsing the binding). LUA_DB_API_REFERENCE is a
// TypeScript template literal, so every backtick in it is backslash-escaped in the source (the
// fence markers included) to keep it from terminating the surrounding `...` literal; this function
// un-escapes that before returning. Throws -- loudly, naming the heading -- if the heading, the
// opening fence, the closing fence, or a non-empty block cannot be found, so a reformat of the
// reference cannot make the caller's test pass vacuously.
std::string extract_lua_example(const std::string& file_contents, const std::string& heading) {
    const auto heading_pos = file_contents.find(heading);
    if (heading_pos == std::string::npos) {
        throw std::runtime_error("extract_lua_example: heading not found: " + heading);
    }

    // The literal bytes in the .ts source are backslash + backtick, repeated three times, then
    // "lua" -- NOT a raw ``` sequence, which never appears unescaped inside the template literal.
    const std::string open_fence = "\\`\\`\\`lua";
    const auto fence_start = file_contents.find(open_fence, heading_pos);
    if (fence_start == std::string::npos) {
        throw std::runtime_error("extract_lua_example: opening ```lua fence not found after heading: " + heading);
    }

    const auto block_start = fence_start + open_fence.size();
    const std::string close_fence = "\\`\\`\\`";
    const auto block_end = file_contents.find(close_fence, block_start);
    if (block_end == std::string::npos) {
        throw std::runtime_error("extract_lua_example: closing ``` fence not found for heading: " + heading);
    }

    const std::string block = file_contents.substr(block_start, block_end - block_start);
    if (block.find_first_not_of(" \t\r\n") == std::string::npos) {
        throw std::runtime_error("extract_lua_example: extracted block is empty for heading: " + heading);
    }

    // Undo the template-literal escaping: every "\`" becomes "`". No other escape sequence (e.g.
    // "\${") appears in this section, but the loop only ever touches a backslash-backtick pair, so
    // it cannot mangle anything else even if one were added later.
    std::string unescaped;
    unescaped.reserve(block.size());
    for (std::size_t i = 0; i < block.size(); ++i) {
        if (block[i] == '\\' && i + 1 < block.size() && block[i + 1] == '`') {
            unescaped += '`';
            ++i;
        } else {
            unescaped += block[i];
        }
    }
    return unescaped;
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
// through double first, so a value past double's 53-bit mantissa survives exactly. This is the
// single value that distinguishes the integer path from the double path: 9007199254740993 is the
// first odd integer that cannot be represented as a double, so a regression that routes it through
// the double branch produces the digit string one lower, "9007199254740992" (the digit that
// disappears the moment an int64 is coerced through a double's 53-bit mantissa).
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

// TEST-07: INT64_MIN/INT64_MAX -- the buffer-size boundary for append_number's 32-byte array --
// spelled via math.mininteger/math.maxinteger, the robust way to reach them from Lua source (a
// bare -9223372036854775808 literal is unary minus applied to a positive literal that itself
// overflows int64, which Lua would instead read as a float).
TEST_F(LuaRunner_WriteCsv, MinIntegerAndMaxIntegerRoundTripExactDecimalText) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "int_bounds.csv").string());

    lua.run(R"(
        local w = db:write_csv(")" +
            path + R"(")
        w:write_row({ math.mininteger, math.maxinteger })
        w:close()

        local csv = db:read_csv(")" +
            path + R"(", { header_row = 0 })
        assert(csv.rows[1][1] == "-9223372036854775808",
            "expected INT64_MIN exact text, got " .. tostring(csv.rows[1][1]))
        assert(csv.rows[1][2] == "9223372036854775807",
            "expected INT64_MAX exact text, got " .. tostring(csv.rows[1][2]))
    )");
}

// TEST-07: a float re-write identity check. Write a float, read the cell back as a string, write
// THAT string as a second file's cell, read it back, and assert the two read-back strings are
// identical. This catches a 5-decimal truncation or a 6-significant-digit cut without the test
// needing to know append_number's exact output text -- that contract belongs to
// src/utils/number.h, not to this test.
TEST_F(LuaRunner_WriteCsv, FloatReWriteIdentityRoundTripsForManySignificantDigitValues) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path1 = lp((sandbox / "float_identity_1.csv").string());
    const auto path2 = lp((sandbox / "float_identity_2.csv").string());

    lua.run(R"(
        local function reWriteIdentity(value)
            local w1 = db:write_csv(")" +
            path1 + R"(")
            w1:write_row({ value })
            w1:close()
            local first = db:read_csv(")" +
            path1 + R"(", { header_row = 0 }).rows[1][1]

            local w2 = db:write_csv(")" +
            path2 + R"(")
            w2:write_row({ first })
            w2:close()
            local second = db:read_csv(")" +
            path2 + R"(", { header_row = 0 }).rows[1][1]

            assert(first == second, "re-write identity failed for " .. tostring(value) ..
                ": " .. tostring(first) .. " vs " .. tostring(second))
        end

        -- 0.1 (not exactly representable in binary), 1e-7 (tiny magnitude), and the largest finite
        -- double (DBL_MAX) -- the widest spread of significant-digit shapes this writer can see.
        reWriteIdentity(0.1)
        reWriteIdentity(1e-7)
        reWriteIdentity(1.7976931348623157e308)
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

// FMT-07 / ROADMAP criterion 1: a row shorter than a 3-name header pads with empty cells before
// Writer::write_row ever sees it, so the file round-trips through db:read_csv (header_row = 1, so
// csv.header names the columns) with 3 fields per row, the script's two values under the columns
// it meant and the third an empty string.
TEST_F(LuaRunner_WriteCsv, ShortRowPadsToHeaderWidthAndRoundTripsAligned) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "short_row.csv").string());

    lua.run(R"(
        local w = db:write_csv(")" +
            path + R"(", { header = { "a", "b", "c" } })
        w:write_row({ "1", "2" })
        w:close()

        local csv = db:read_csv(")" +
            path + R"(")
        assert(#csv.header == 3, "expected 3 header columns, got " .. #csv.header)
        assert(csv.header[1] == "a" and csv.header[2] == "b" and csv.header[3] == "c",
            "unexpected header contents")
        assert(#csv.rows == 1, "expected 1 data row, got " .. #csv.rows)
        assert(csv.rows[1][1] == "1", "expected col a == 1, got " .. tostring(csv.rows[1][1]))
        assert(csv.rows[1][2] == "2", "expected col b == 2, got " .. tostring(csv.rows[1][2]))
        assert(csv.rows[1][3] == "", "expected col c padded to empty, got " .. tostring(csv.rows[1][3]))
    )");
}

// FMT-07 / ROADMAP criterion 2: a row wider than the header throws a Pattern 1 error naming the
// 1-based data-row ordinal and both counts. Two good rows precede the bad one, so the ordinal is 3.
TEST_F(LuaRunner_WriteCsv, RowLongerThanHeaderThrowsNamingOrdinalAndCounts) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "too_long.csv").string());

    expect_prefixed_error(lua,
                          R"(
        local w = db:write_csv(")" +
                              path + R"(", { header = { "a", "b", "c" } })
        w:write_row({ "1", "2", "3" })
        w:write_row({ "4", "5", "6" })
        w:write_row({ "7", "8", "9", "10" })
    )",
                          "Cannot write_row: ",
                          "row 3 has 4 cells but header declares 3");
}

// FMT-07 / ROADMAP criterion 2's second clause: the rows written before the rejected long row are
// still on disk and readable through db:read_csv -- the throw does not truncate or corrupt what
// was already flushed. Same pcall + w:close() shape as RejectedNonFiniteRowLeavesFileIntact... below.
TEST_F(LuaRunner_WriteCsv, RejectedLongRowLeavesEarlierRowsOnDisk) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "too_long_intact.csv").string());

    lua.run(R"(
        local w = db:write_csv(")" +
            path + R"(", { header = { "a", "b", "c" } })
        w:write_row({ "1", "2", "3" })
        w:write_row({ "4", "5", "6" })
        local ok, err = pcall(function() w:write_row({ "7", "8", "9", "10" }) end)
        assert(ok == false, "expected the too-long third write_row to fail")
        assert(err:find("Cannot write_row:", 1, true) ~= nil, "expected Cannot write_row: prefix, got " .. tostring(err))
        w:close()

        local csv = db:read_csv(")" +
            path + R"(")
        assert(#csv.rows == 2, "expected exactly 2 data rows after the rejected third, got " .. #csv.rows)
        assert(csv.rows[1][1] == "1" and csv.rows[1][2] == "2" and csv.rows[1][3] == "3", "expected row1 intact")
        assert(csv.rows[2][1] == "4" and csv.rows[2][2] == "5" and csv.rows[2][3] == "6", "expected row2 intact")
    )");
}

// TEST-10 boundary: against one N=3 header, N-1 pads (covered above), N passes through
// byte-identical, N+1 throws (covered above) -- this test is the exact-width middle case.
TEST_F(LuaRunner_WriteCsv, ExactWidthRowPassesThroughUnchanged) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "exact_width.csv").string());

    lua.run(R"(
        local w = db:write_csv(")" +
            path + R"(", { header = { "a", "b", "c" } })
        w:write_row({ "x", "y", "z" })
        w:close()

        local csv = db:read_csv(")" +
            path + R"(")
        assert(#csv.rows == 1, "expected 1 data row, got " .. #csv.rows)
        assert(csv.rows[1][1] == "x" and csv.rows[1][2] == "y" and csv.rows[1][3] == "z",
            "expected the exact-width row unchanged")
    )");
}

// D-44 / EDGE FMT-07/empty: w:write_row{} under a 3-name header pads to 3 empty cells, emitted as
// 2 bare separators -- a legitimate 3-field row, NOT FMT-02's quoted empty-string spelling -- and
// db:read_csv returns 3 empty cells for it.
TEST_F(LuaRunner_WriteCsv, EmptyRowPadsToMultiColumnHeaderWidth) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "empty_multi.csv").string());

    lua.run(R"(
        local w = db:write_csv(")" +
            path + R"(", { header = { "a", "b", "c" } })
        w:write_row({})
        w:close()

        local csv = db:read_csv(")" +
            path + R"(")
        assert(#csv.rows == 1, "expected 1 data row, got " .. #csv.rows)
        assert(csv.rows[1][1] == "" and csv.rows[1][2] == "" and csv.rows[1][3] == "",
            "expected 3 empty cells, got " .. tostring(csv.rows[1][1]) .. "/" ..
            tostring(csv.rows[1][2]) .. "/" .. tostring(csv.rows[1][3]))
    )");
}

// D-44 / EDGE FMT-07/empty, the 1-column half: the same w:write_row{} call under a 1-name header
// pads to exactly 1 empty cell -- still the TEST-09 shape (FMT-02's blank-line defence still
// applies) -- and still round-trips as one present row, not zero.
TEST_F(LuaRunner_WriteCsv, EmptyRowUnderSingleColumnHeaderStillRoundTrips) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "empty_single.csv").string());

    lua.run(R"(
        local w = db:write_csv(")" +
            path + R"(", { header = { "only" } })
        w:write_row({})
        w:close()

        local csv = db:read_csv(")" +
            path + R"(")
        assert(#csv.rows == 1, "expected exactly 1 present row, got " .. #csv.rows)
        assert(csv.rows[1][1] == "", "expected 1 empty cell, got " .. tostring(csv.rows[1][1]))
    )");
}

// ROADMAP criterion 3 / D-42: with no header given -- option omitted entirely, and separately
// header = {} -- rows of differing widths (1, 2, 3 cells) are written as-is and no width error is
// raised (header_width == 0 means no enforcement).
TEST_F(LuaRunner_WriteCsv, NoHeaderMeansNoWidthCheck) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path1 = lp((sandbox / "no_header_omitted.csv").string());
    const auto path2 = lp((sandbox / "no_header_empty_table.csv").string());

    lua.run(R"(
        local function check(path, opts)
            local w = db:write_csv(path, opts)
            w:write_row({ "1" })
            w:write_row({ "1", "2" })
            w:write_row({ "1", "2", "3" })
            w:close()

            local csv = db:read_csv(path, { header_row = 0 })
            assert(#csv.rows == 3, "expected 3 rows, got " .. #csv.rows)
            assert(#csv.rows[1] == 1, "expected row 1 to have 1 cell, got " .. #csv.rows[1])
            assert(#csv.rows[2] == 2, "expected row 2 to have 2 cells, got " .. #csv.rows[2])
            assert(#csv.rows[3] == 3, "expected row 3 to have 3 cells, got " .. #csv.rows[3])
        end

        check(")" +
            path1 + R"(", nil)
        check(")" +
            path2 + R"(", { header = {} })
    )");
}

// EDGE FMT-07/encoding: the width comparison counts CELLS, never characters or bytes -- a 3-name
// header whose names and whose row values are multi-byte UTF-8 still pads a 2-cell row to 3 and
// still rejects a 4-cell row, with the reported counts unchanged by the encoding.
TEST_F(LuaRunner_WriteCsv, MultiByteUtf8CellsDoNotChangeCellCounts) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto pad_path = lp((sandbox / "utf8_pad.csv").string());

    lua.run(R"(
        local w = db:write_csv(")" +
            pad_path + R"(", { header = { "名前", "値", "c" } })
        w:write_row({ "アルファ", "42" })
        w:close()

        local csv = db:read_csv(")" +
            pad_path + R"(")
        assert(#csv.header == 3, "expected 3 header columns, got " .. #csv.header)
        assert(#csv.rows == 1, "expected 1 data row, got " .. #csv.rows)
        assert(csv.rows[1][1] == "アルファ", "expected multi-byte cell 1 intact")
        assert(csv.rows[1][2] == "42", "expected cell 2 intact")
        assert(csv.rows[1][3] == "", "expected padded cell 3 to be empty")
    )");

    const auto reject_path = lp((sandbox / "utf8_reject.csv").string());
    expect_prefixed_error(lua,
                          R"(
        local w = db:write_csv(")" +
                              reject_path + R"(", { header = { "名前", "値", "c" } })
        w:write_row({ "アルファ", "42", "余分", "余分2" })
    )",
                          "Cannot write_row: ",
                          "row 1 has 4 cells but header declares 3");
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

// D-39/DOC-05: the worked example shipped in bindings/js/src/lua-api.ts's "## CSV file writing"
// section is EXTRACTED FROM THE REFERENCE FILE AT TEST TIME and executed, never transcribed into
// this test -- a pasted copy is a second copy that drifts, exactly what lua-api-sync.test.ts
// exists to prevent on the binding side. The example itself supplies no `path` variable (it is
// meant to be read as prose over a caller-supplied path), so this test defines one before running
// the extracted body. Assertions below are positional against the example's OWN data table
// (Alpha/first/true/42, Beta/nil/false/3.5): the Beta row's nil is INTERIOR (04-01 task 3 pins
// this), so it must round-trip as an empty cell at FULL row width, not a shortened row -- a future
// edit that moves the nil to the end must make this assertion fail (FMT-08), not be accommodated.
TEST_F(LuaRunner_WriteCsv, ReferenceWorkedExampleRunsAndRoundTripsItsOwnData) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const std::string reference_path = quiver::test::path_from(__FILE__, "../bindings/js/src/lua-api.ts");
    std::ifstream reference_file(reference_path, std::ios::binary);
    ASSERT_TRUE(reference_file.is_open()) << "could not open reference file: " << reference_path;
    std::ostringstream buffer;
    buffer << reference_file.rdbuf();
    const std::string reference_contents = buffer.str();

    const std::string example = extract_lua_example(reference_contents, "## CSV file writing");

    const auto path = lp((sandbox / "reference_example.csv").string());
    const std::string script = "local path = \"" + path + "\"\n" + example + R"(
        local csv = db:read_csv(path, { header_row = 0 })
        assert(#csv.rows == 3, "expected a header record plus 2 data rows, got " .. #csv.rows)

        -- record 1: the header, written because the example passes `header` -- nothing else here
        -- would notice a header that was decoded and never actually written.
        assert(csv.rows[1][1] == "name", "expected header col1 'name', got " .. tostring(csv.rows[1][1]))
        assert(csv.rows[1][2] == "note", "expected header col2 'note', got " .. tostring(csv.rows[1][2]))
        assert(csv.rows[1][3] == "active", "expected header col3 'active', got " .. tostring(csv.rows[1][3]))
        assert(csv.rows[1][4] == "score", "expected header col4 'score', got " .. tostring(csv.rows[1][4]))

        -- record 2: { "Alpha", "first", true, 42 }
        assert(csv.rows[2][1] == "Alpha", "expected Alpha, got " .. tostring(csv.rows[2][1]))
        assert(csv.rows[2][2] == "first", "expected first, got " .. tostring(csv.rows[2][2]))
        assert(csv.rows[2][3] == "1", "expected boolean true written as '1', got " .. tostring(csv.rows[2][3]))
        assert(csv.rows[2][4] == "42", "expected 42, got " .. tostring(csv.rows[2][4]))

        -- record 3: { "Beta", nil, false, 3.5 } -- the INTERIOR nil at position 2 must produce an
        -- empty cell at FULL row width (4 cells), not a row shortened to 3 (FMT-08).
        assert(#csv.rows[3] == 4, "expected the Beta row at full width (4 cells), got " .. #csv.rows[3])
        assert(csv.rows[3][1] == "Beta", "expected Beta, got " .. tostring(csv.rows[3][1]))
        assert(csv.rows[3][2] == "", "expected the interior nil to round-trip as an empty cell, got " ..
            tostring(csv.rows[3][2]))
        assert(csv.rows[3][3] == "0", "expected boolean false written as '0', got " .. tostring(csv.rows[3][3]))
        assert(csv.rows[3][4] == "3.5", "expected 3.5, got " .. tostring(csv.rows[3][4]))
    )";
    lua.run(script);
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

// Two writers open on one path each open with ios::trunc and write from offset 0, so the second
// silently discarded everything the first had buffered (proven: only the second writer's row
// survived). Refused now, the way db:open_file's write registry already refuses it. Reopening a
// path whose previous writer was CLOSED stays legal -- that is WRITE-08, pinned by
// ReopeningSamePathTruncatesExistingContent, which is why this guard checks is_closed().
TEST_F(LuaRunner_WriteCsvErrors, SecondWriterOnAnAlreadyOpenPathIsRefused) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "concurrent.csv").string());

    expect_prefixed_error(lua,
                          R"(
        local a = db:write_csv(")" +
                              path + R"(")
        local b = db:write_csv(")" +
                              path + R"(")
    )",
                          "Cannot write_csv: ",
                          "file is already open for writing");
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

// WRITE-06 / TEST-11 (ROADMAP criterion 4): a script that returns without calling w:close() still
// leaves a complete, re-readable file. This is the two-separate-lua.run()-calls shape RESEARCH.md
// Q4 confirms has no precedent in this suite -- the second run() proves the flush happened BETWEEN
// script executions, with the LuaRunner never destroyed, moved from, or reset in between (the
// ROADMAP criterion 4 trap). The fixture is deliberately tiny (one column, one short row) per
// Pitfall 2: a payload large enough to spill std::filebuf's own buffer would put bytes on disk
// without the fix and make the RED accidental. The byte count below is diagnostic evidence
// attached to the FAIL() message only (D-49) -- the pass/fail decision is always the db:read_csv
// round trip in the second run(), never a raw-byte assertion.
TEST_F(LuaRunner_WriteCsv, UnclosedWriterIsFlushedWhenRunReturns) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "unclosed.csv").string());

    lua.run(R"(
        local w = db:write_csv(")" +
            path + R"(", { header = { "a" } })
        w:write_row({ "x" })
        -- deliberately no w:close() -- WRITE-06 must flush this when run() returns
    )");

    // Diagnostic only (D-49): the actual observed byte count, not the ROADMAP's unverified "zero
    // bytes" claim. Streamed into the failure message below; never the assertion itself.
    const auto observed_bytes =
        std::filesystem::exists(path) ? std::filesystem::file_size(path) : static_cast<std::uintmax_t>(0);

    try {
        lua.run(R"(
            local csv = db:read_csv(")" +
                path + R"(")
            assert(#csv.rows == 1, "expected 1 flushed row, got " .. #csv.rows)
            assert(csv.rows[1][1] == "x", "expected 'x', got " .. tostring(csv.rows[1][1]))
        )");
    } catch (const std::exception& e) {
        FAIL() << "unclosed writer was not readable back through db:read_csv (WRITE-06 not yet "
                  "implemented): "
               << e.what() << " -- observed on-disk file size after the first run() returned: " << observed_bytes
               << " bytes";
    }
}

// WRITE-06, the reachable-writer half: the same two-run() shape as above, but the script assigns
// the writer to a GLOBAL (`w = ...`, with no `local` -- Lua's default spelling and the most common
// slip). A global is a GC root, so a flush that relies on collect_garbage() finalizing an
// unreachable object cannot fire here and the file stays at 0 bytes; only an explicit close of
// every writer run() handed out covers it. Deliberately the same tiny payload as the `local` case,
// so the buffer never spills on its own.
TEST_F(LuaRunner_WriteCsv, UnclosedWriterHeldInAGlobalIsAlsoFlushedWhenRunReturns) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "unclosed_global.csv").string());

    lua.run(R"(
        w = db:write_csv(")" +
            path + R"(", { header = { "a" } })
        w:write_row({ "x" })
        -- no `local`, and deliberately no w:close(): w is still reachable from _G when run() returns
    )");

    const auto observed_bytes =
        std::filesystem::exists(path) ? std::filesystem::file_size(path) : static_cast<std::uintmax_t>(0);

    try {
        lua.run(R"(
            local csv = db:read_csv(")" +
                path + R"(")
            assert(#csv.rows == 1, "expected 1 flushed row, got " .. #csv.rows)
            assert(csv.rows[1][1] == "x", "expected 'x', got " .. tostring(csv.rows[1][1]))
        )");
    } catch (const std::exception& e) {
        FAIL() << "a writer left reachable in a Lua global was not flushed when run() returned: " << e.what()
               << " -- observed on-disk file size after the first run() returned: " << observed_bytes << " bytes";
    }
}

// WRITE-06 / TEST-11, the error-path half (D-47): a script that raises mid-write, with the writer
// still open, still leaves the rows written before the error on disk and readable -- the flush
// must fire during stack unwinding too, not only on a normal return. Same tiny-fixture and
// diagnostic-byte-count discipline as the case above.
TEST_F(LuaRunner_WriteCsv, ScriptErrorMidWriteStillLeavesEarlierRowsReadable) {
    auto schema = VALID_SCHEMA("basic.sql");
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);

    const auto path = lp((sandbox / "error_mid_write.csv").string());

    EXPECT_THROW(lua.run(R"(
        local w = db:write_csv(")" +
                         path + R"(", { header = { "a" } })
        w:write_row({ "x" })
        error("boom")
        -- deliberately no w:close() -- WRITE-06's flush must fire during unwinding too (D-47)
    )"),
                 std::exception);

    const auto observed_bytes =
        std::filesystem::exists(path) ? std::filesystem::file_size(path) : static_cast<std::uintmax_t>(0);

    try {
        lua.run(R"(
            local csv = db:read_csv(")" +
                path + R"(")
            assert(#csv.rows == 1, "expected the pre-error row to survive, got " .. #csv.rows)
            assert(csv.rows[1][1] == "x", "expected 'x', got " .. tostring(csv.rows[1][1]))
        )");
    } catch (const std::exception& e) {
        FAIL() << "pre-error row was not readable back through db:read_csv (WRITE-06 not yet "
                  "implemented for the throw path): "
               << e.what() << " -- observed on-disk file size after the throwing run() unwound: " << observed_bytes
               << " bytes";
    }
}
