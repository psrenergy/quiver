#include "test_utils.h"

#include <algorithm>
#include <filesystem>
#include <gtest/gtest.h>
#include <quiver/database.h>
#include <quiver/lua_runner.h>
#include <string>

namespace fs = std::filesystem;

// Lua bindings for the binary subsystem (quiver.metadata / quiver.open_file / quiver.bin_to_csv).
// Mirrors the Julia coverage in bindings/julia/test/test_binary_file.jl + test_binary_metadata.jl.
class LuaBinaryTest : public ::testing::Test {
protected:
    void SetUp() override {
        schema = VALID_SCHEMA("collections.sql");
        path_a = (fs::temp_directory_path() / "quiver_lua_bin_a").string();
        path_b = (fs::temp_directory_path() / "quiver_lua_bin_b").string();
        cleanup();
    }
    void TearDown() override { cleanup(); }

    void cleanup() {
        for (const auto& p : {path_a, path_b}) {
            for (auto ext : {".qvr", ".toml", ".csv"}) {
                auto full = p + ext;
                if (fs::exists(full))
                    fs::remove(full);
            }
        }
    }

    // .qvr I/O goes through std::fstream, which accepts forward slashes on Windows; using them
    // avoids escaping backslashes inside the embedded Lua string literals.
    static std::string lp(const std::string& p) {
        std::string r = p;
        std::replace(r.begin(), r.end(), '\\', '/');
        return r;
    }

    std::string schema, path_a, path_b;
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
    auto db = quiver::Database::from_schema(":memory:", schema);
    quiver::LuaRunner lua(db);
    const std::string a = lp(path_a);
    lua.run("local md = quiver.metadata{ initial_datetime='2025-01-01T00:00:00', unit='MW',"
            " labels={'v1','v2'}, dimensions={'row','col'}, dimension_sizes={3,2} }\n"
            "local f = quiver.open_file('" +
            a +
            "', 'w', md)\n"
            "for row=1,3 do for col=1,2 do f:write({row*10+col, row*100+col}, {row=row, col=col}) end end\n"
            "f:close()\n"
            "local r = quiver.open_file('" +
            a +
            "', 'r')\n"
            "assert(r:is_open(), 'should be open')\n"
            "for row=1,3 do for col=1,2 do\n"
            "  local cell = r:read({row=row, col=col})\n"
            "  assert(#cell == 2, 'cell size')\n"
            "  assert(cell[1] == row*10+col, 'v1 at '..row..','..col)\n"
            "  assert(cell[2] == row*100+col, 'v2 at '..row..','..col)\n"
            "end end\n"
            "local md2 = r:get_metadata()\n"
            "assert(md2:get_unit() == 'MW', 'roundtrip unit')\n"
            "r:close()\n");
}

TEST_F(LuaBinaryTest, AllowNullsReadsNaN) {
    auto db = quiver::Database::from_schema(":memory:", schema);
    quiver::LuaRunner lua(db);
    const std::string a = lp(path_a);
    lua.run("local md = quiver.metadata{ initial_datetime='2025-01-01T00:00:00', unit='x',"
            " labels={'v'}, dimensions={'row'}, dimension_sizes={3} }\n"
            "local f = quiver.open_file('" +
            a +
            "', 'w', md)\n"
            "f:write({42.0}, {row=1})\n"
            "f:close()\n"
            "local r = quiver.open_file('" +
            a +
            "', 'r')\n"
            "assert(r:read({row=1})[1] == 42.0, 'written value')\n"
            "local nullcell = r:read({row=2}, true)\n"
            "assert(nullcell[1] ~= nullcell[1], 'unwritten cell should be NaN')\n"
            "r:close()\n");
}

TEST_F(LuaBinaryTest, ReadNullWithoutAllowThrows) {
    auto db = quiver::Database::from_schema(":memory:", schema);
    quiver::LuaRunner lua(db);
    const std::string a = lp(path_a);
    EXPECT_THROW(lua.run("local md = quiver.metadata{ initial_datetime='2025-01-01T00:00:00', unit='x',"
                         " labels={'v'}, dimensions={'row'}, dimension_sizes={3} }\n"
                         "local f = quiver.open_file('" +
                         a +
                         "', 'w', md)\n"
                         "f:write({42.0}, {row=1})\n"
                         "f:close()\n"
                         "local r = quiver.open_file('" +
                         a +
                         "', 'r')\n"
                         "r:read({row=2})\n"),
                 std::exception);
}

TEST_F(LuaBinaryTest, TimeDimensionWriteRead) {
    auto db = quiver::Database::from_schema(":memory:", schema);
    quiver::LuaRunner lua(db);
    const std::string a = lp(path_a);
    // 2 monthly stages x 28 daily blocks (Feb has 28 days from 2025-02-01); one label.
    lua.run("local md = quiver.metadata{ initial_datetime='2025-02-01T00:00:00', unit='MW',"
            " labels={'v'}, dimensions={'stage','block'}, dimension_sizes={2,28},"
            " time_dimensions={'stage','block'}, frequencies={'monthly','daily'} }\n"
            "local f = quiver.open_file('" +
            a +
            "', 'w', md)\n"
            "for stage=1,2 do for block=1,28 do f:write({stage*1000+block}, {stage=stage, block=block}) end end\n"
            "f:close()\n"
            "local r = quiver.open_file('" +
            a +
            "', 'r')\n"
            "assert(r:read({stage=1, block=1})[1] == 1001, 'first cell')\n"
            "assert(r:read({stage=2, block=28})[1] == 2028, 'last cell')\n"
            "r:close()\n");
}

TEST_F(LuaBinaryTest, CsvRoundTrip) {
    auto db = quiver::Database::from_schema(":memory:", schema);
    quiver::LuaRunner lua(db);
    const std::string a = lp(path_a);
    lua.run("local md = quiver.metadata{ initial_datetime='2025-01-01T00:00:00', unit='MW',"
            " labels={'v1','v2'}, dimensions={'row','col'}, dimension_sizes={3,2} }\n"
            "local f = quiver.open_file('" +
            a +
            "', 'w', md)\n"
            "for row=1,3 do for col=1,2 do f:write({row*10+col, row+col}, {row=row, col=col}) end end\n"
            "f:close()\n"
            "quiver.bin_to_csv('" +
            a +
            "')\n"
            "quiver.csv_to_bin('" +
            a +
            "')\n"
            "local r = quiver.open_file('" +
            a +
            "', 'r')\n"
            "for row=1,3 do for col=1,2 do\n"
            "  local cell = r:read({row=row, col=col})\n"
            "  assert(cell[1] == row*10+col and cell[2] == row+col, 'csv roundtrip at '..row..','..col)\n"
            "end end\n"
            "r:close()\n");
    EXPECT_TRUE(fs::exists(path_a + ".csv"));
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
    auto db = quiver::Database::from_schema(":memory:", schema);
    quiver::LuaRunner lua(db);
    const std::string a = lp(path_a);
    EXPECT_THROW(lua.run("quiver.open_file('" + a + "', 'x')\n"), std::exception);
}
