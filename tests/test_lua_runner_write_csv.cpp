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
        local w = db:write_csv(")" + path + R"(")
        w:write_row({ "Alpha", "Beta" })
        w:close()

        local csv = db:read_csv(")" + path + R"(", { header_row = 0 })
        assert(#csv.rows == 1, "expected 1 row, got " .. #csv.rows)
        assert(csv.rows[1][1] == "Alpha", "expected Alpha, got " .. tostring(csv.rows[1][1]))
        assert(csv.rows[1][2] == "Beta", "expected Beta, got " .. tostring(csv.rows[1][2]))
    )");
}
