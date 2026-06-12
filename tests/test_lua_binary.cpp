#include "test_lua_runner.h"

#include <algorithm>
#include <filesystem>
#include <gtest/gtest.h>
#include <quiver/database.h>
#include <quiver/lua_runner.h>
#include <string>

namespace fs = std::filesystem;

// Lua bindings for the binary subsystem (quiver.metadata / db:open_file / db:bin_to_csv) and the
// db-directory sandbox on every file-touching operation.
// Mirrors the Julia coverage in bindings/julia/test/test_binary_file.jl + test_binary_metadata.jl.
class LuaBinaryTest : public LuaSandboxTest {
protected:
    void SetUp() override {
        LuaSandboxTest::SetUp();
        schema = VALID_SCHEMA("collections.sql");
    }

    // .qvr I/O goes through std::fstream, which accepts forward slashes on Windows; using them
    // avoids escaping backslashes inside the embedded Lua string literals.
    static std::string lp(const std::string& p) {
        std::string r = p;
        std::replace(r.begin(), r.end(), '\\', '/');
        return r;
    }

    // Single-label 1x3 metadata for the sandbox tests that just need a writable file.
    static std::string md1() {
        return "local md = quiver.metadata{ initial_datetime='2025-01-01T00:00:00', unit='x',"
               " labels={'v'}, dimensions={'row'}, dimension_sizes={3} }\n";
    }

    std::string schema;
};

TEST_F(LuaBinaryTest, MetadataBuilderAndAccessors) {
    auto db = quiver::Database::from_schema(":memory:", schema);
    quiver::LuaRunner lua(db);
    lua.run(R"(
        local md = quiver.metadata{
            initial_datetime = "2025-01-01T00:00:00",
            unit = "MW",
            labels = {"plant_1", "plant_2"},
            dimensions = {"stage", "block"},
            dimension_sizes = {4, 31},
            time_dimensions = {"stage", "block"},
            frequencies = {"monthly", "daily"},
        }
        assert(md:get_unit() == "MW", "unit mismatch")
        assert(md:get_version() == "1", "version mismatch")

        local labels = md:get_labels()
        assert(#labels == 2, "label count")
        assert(labels[1] == "plant_1" and labels[2] == "plant_2", "label values")

        assert(md:get_number_of_time_dimensions() == 2, "time dim count")

        local dims = md:get_dimensions()
        assert(#dims == 2, "dim count")
        assert(dims[1].name == "stage" and dims[1].size == 4, "dim1 name/size")
        assert(dims[1].is_time_dimension == true, "dim1 is time")
        assert(dims[1].frequency == "monthly", "dim1 frequency")
        assert(dims[1].initial_value == 1, "dim1 initial value")
        assert(dims[2].name == "block" and dims[2].frequency == "daily", "dim2")
        assert(dims[2].initial_value == 1, "dim2 initial value")

        local toml = md:to_toml()
        assert(type(toml) == "string" and #toml > 0, "toml non-empty")
    )");
}

TEST_F(LuaBinaryTest, WriteReadRoundTrip) {
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);
    lua.run(R"(
        local md = quiver.metadata{ initial_datetime='2025-01-01T00:00:00', unit='MW',
            labels={'v1','v2'}, dimensions={'row','col'}, dimension_sizes={3,2} }
        local f = db:open_file('bin_a', 'w', md)
        for row=1,3 do for col=1,2 do f:write({row*10+col, row*100+col}, {row=row, col=col}) end end
        f:close()
        local r = db:open_file('bin_a', 'r')
        assert(r:is_open(), 'should be open')
        for row=1,3 do for col=1,2 do
          local cell = r:read({row=row, col=col})
          assert(#cell == 2, 'cell size')
          assert(cell[1] == row*10+col, 'v1 at '..row..','..col)
          assert(cell[2] == row*100+col, 'v2 at '..row..','..col)
        end end
        local md2 = r:get_metadata()
        assert(md2:get_unit() == 'MW', 'roundtrip unit')
        r:close()
    )");
    EXPECT_TRUE(fs::exists(sandbox / "bin_a.qvr"));
}

TEST_F(LuaBinaryTest, AllowNullsReadsNaN) {
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);
    lua.run(md1() + R"(
        local f = db:open_file('bin_a', 'w', md)
        f:write({42.0}, {row=1})
        f:close()
        local r = db:open_file('bin_a', 'r')
        assert(r:read({row=1})[1] == 42.0, 'written value')
        local nullcell = r:read({row=2}, true)
        assert(nullcell[1] ~= nullcell[1], 'unwritten cell should be NaN')
        r:close()
    )");
}

TEST_F(LuaBinaryTest, ReadNullWithoutAllowThrows) {
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);
    lua.run(md1() + R"(
        local f = db:open_file('bin_a', 'w', md)
        f:write({42.0}, {row=1})
        f:close()
    )");
    EXPECT_THROW(lua.run("local r = db:open_file('bin_a', 'r')\nr:read({row=2})\n"), std::exception);
}

TEST_F(LuaBinaryTest, TimeDimensionWriteRead) {
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);
    // 2 monthly stages x 28 daily blocks (Feb has 28 days from 2025-02-01); one label.
    lua.run(R"(
        local md = quiver.metadata{ initial_datetime='2025-02-01T00:00:00', unit='MW',
            labels={'v'}, dimensions={'stage','block'}, dimension_sizes={2,28},
            time_dimensions={'stage','block'}, frequencies={'monthly','daily'} }
        local f = db:open_file('bin_a', 'w', md)
        for stage=1,2 do for block=1,28 do f:write({stage*1000+block}, {stage=stage, block=block}) end end
        f:close()
        local r = db:open_file('bin_a', 'r')
        assert(r:read({stage=1, block=1})[1] == 1001, 'first cell')
        assert(r:read({stage=2, block=28})[1] == 2028, 'last cell')
        r:close()
    )");
}

TEST_F(LuaBinaryTest, CsvRoundTrip) {
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);
    lua.run(R"(
        local md = quiver.metadata{ initial_datetime='2025-01-01T00:00:00', unit='MW',
            labels={'v1','v2'}, dimensions={'row','col'}, dimension_sizes={3,2} }
        local f = db:open_file('bin_a', 'w', md)
        for row=1,3 do for col=1,2 do f:write({row*10+col, row+col}, {row=row, col=col}) end end
        f:close()
        db:bin_to_csv('bin_a')
        db:csv_to_bin('bin_a')
        local r = db:open_file('bin_a', 'r')
        for row=1,3 do for col=1,2 do
          local cell = r:read({row=row, col=col})
          assert(cell[1] == row*10+col and cell[2] == row+col, 'csv roundtrip at '..row..','..col)
        end end
        r:close()
    )");
    EXPECT_TRUE(fs::exists(sandbox / "bin_a.csv"));
}

TEST_F(LuaBinaryTest, MetadataFromToml) {
    auto db = quiver::Database::from_schema(":memory:", schema);
    quiver::LuaRunner lua(db);
    lua.run(R"(
        local md = quiver.metadata{
            initial_datetime = "2025-01-01T00:00:00", unit = "GWh",
            labels = {"a", "b"}, dimensions = {"row"}, dimension_sizes = {5},
        }
        local toml = md:to_toml()
        local md2 = quiver.metadata_from_toml(toml)
        assert(md2:get_unit() == "GWh", "unit roundtrip")
        local labels = md2:get_labels()
        assert(#labels == 2 and labels[1] == "a" and labels[2] == "b", "labels roundtrip")
    )");
}

TEST_F(LuaBinaryTest, OpenFileInvalidModeThrows) {
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);
    expect_lua_error(lua, "db:open_file('bin_a', 'x')\n", "Cannot open_file: mode must be");
}

// --- db-directory sandbox ---

TEST_F(LuaBinaryTest, RelativePathResolvesAgainstDbDir) {
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);
    lua.run(md1() + "local f = db:open_file('bin_rel', 'w', md)\nf:write({1.0}, {row=1})\nf:close()\n");
    EXPECT_TRUE(fs::exists(sandbox / "bin_rel.qvr"));
    EXPECT_FALSE(fs::exists(fs::current_path() / "bin_rel.qvr"));
}

TEST_F(LuaBinaryTest, SubdirectoryAllowed) {
    fs::create_directories(sandbox / "sub");  // BinaryFile does not create parent directories
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);
    lua.run(md1() + "local f = db:open_file('sub/bin', 'w', md)\nf:write({1.0}, {row=1})\nf:close()\n");
    EXPECT_TRUE(fs::exists(sandbox / "sub" / "bin.qvr"));
}

TEST_F(LuaBinaryTest, AbsoluteInsideAccepted) {
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);
    const std::string abs_inside = lp((sandbox / "abs_inside").string());
    lua.run(md1() + "local f = db:open_file('" + abs_inside + "', 'w', md)\nf:write({1.0}, {row=1})\nf:close()\n");
    EXPECT_TRUE(fs::exists(sandbox / "abs_inside.qvr"));
}

TEST_F(LuaBinaryTest, DotDotEscapeThrows) {
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);
    expect_lua_error(lua,
                     md1() + "db:open_file('../escape', 'w', md)\n",
                     "Cannot open_file: path '../escape' escapes the database directory");
}

TEST_F(LuaBinaryTest, AbsoluteOutsideThrows) {
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);
    const std::string outside = lp((fs::temp_directory_path() / "quiver_lua_outside").string());
    expect_lua_error(lua, md1() + "db:open_file('" + outside + "', 'w', md)\n", "escapes the database directory");
}

TEST_F(LuaBinaryTest, RootItselfRejected) {
    // The root directory itself must be rejected: the subsystem appends ".qvr" by string
    // concatenation, so the root would produce "<root>.qvr" outside the sandbox.
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);
    expect_lua_error(lua, md1() + "db:open_file('.', 'w', md)\n", "escapes the database directory");
}

TEST_F(LuaBinaryTest, ConverterEscapeThrows) {
    auto db = quiver::Database::from_schema(db_path(), schema);
    quiver::LuaRunner lua(db);
    expect_lua_error(lua, "db:bin_to_csv('../x')\n", "Cannot bin_to_csv: path '../x' escapes the database directory");
    expect_lua_error(lua, "db:csv_to_bin('../x')\n", "Cannot csv_to_bin: path '../x' escapes the database directory");
}

TEST_F(LuaBinaryTest, InMemoryThrows) {
    auto db = quiver::Database::from_schema(":memory:", schema);
    quiver::LuaRunner lua(db);
    expect_lua_error(lua, md1() + "db:open_file('bin_a', 'w', md)\n", "Cannot open_file: database is in-memory");
    expect_lua_error(lua, "db:bin_to_csv('bin_a')\n", "Cannot bin_to_csv: database is in-memory");
    expect_lua_error(lua, "db:csv_to_bin('bin_a')\n", "Cannot csv_to_bin: database is in-memory");
}
