#include "test_ui_fixture.h"
#include "test_utils.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <quiver/database.h>
#include <quiver/lua_runner.h>
#include <sstream>
#include <string>
#include <vector>

// OPT-01/OPT-02/OPT-04 at the Lua layer (Phase 2, plan 02-06). D-14: LuaRunner takes an
// already-configured Database& and has no options channel of its own, so Lua's host is
// quiver_cli -- the config-path/locale surface is two CLI flags, proven here by shelling out to
// the real built binary (never a manual probe), while db:has_ui_config() is proven directly
// against LuaRunner.

namespace {

std::string foresight_schema_path() {
    return quiver::test::path_from(__FILE__, "schemas/ui/foresight_like/schema.sql");
}

std::string foresight_ui_dir() {
    return quiver::test::path_from(__FILE__, "schemas/ui/foresight_like/ui");
}

// A scratch directory with no `ui/` sibling of its own. The database file, the Lua script, and
// the captured CLI output all live here, so every path handed to std::system is self-contained
// and disposable, and the convention path can never accidentally resolve.
class ScratchDir {
public:
    explicit ScratchDir(const std::string& name)
        : dir_(std::filesystem::temp_directory_path() / ("quiver_cli_ui_options_" + name)) {
        std::filesystem::remove_all(dir_);
        std::filesystem::create_directories(dir_);
    }
    ~ScratchDir() { std::filesystem::remove_all(dir_); }

    ScratchDir(const ScratchDir&) = delete;
    ScratchDir& operator=(const ScratchDir&) = delete;

    std::string path(const std::string& name) const { return (dir_ / name).string(); }

private:
    std::filesystem::path dir_;
};

void write_file(const std::string& path, const std::string& content) {
    std::ofstream file(path, std::ios::binary);
    file << content;
}

std::string read_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    std::ostringstream out;
    out << file.rdbuf();
    return out.str();
}

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

// `path_from` (test_utils.h) joins a backslash-native __FILE__ parent directory with a
// forward-slash relative literal ("schemas/ui/..."), so its result mixes separators on Windows.
// cmd.exe's own path resolution (both for the program name and for a quoted argument) can fail
// on a mixed-separator path -- confirmed independently of this suite's code (`cmd /c dir
// "C:\...\schemas/ui/x"` reports "File Not Found" for a file that exists). Normalizing to the
// native separator before it ever reaches std::system sidesteps that shell quirk entirely; it
// changes nothing the program under test sees; QUIVER_CLI_PATH itself is also forward-slash
// (baked in via CMake's generator-expression path style) and needs the same normalization.
std::string native(const std::string& p) {
    return std::filesystem::path(p).make_preferred().string();
}

struct CliResult {
    int status;
    std::string output;
};

// Runs quiver_cli via the absolute path baked in at build time (QUIVER_CLI_PATH,
// tests/CMakeLists.txt) -- never a bare "quiver_cli" name. This suite's own <verify> command runs
// `./build/bin/quiver_tests.exe` from the repo root, where quiver_cli is not on PATH, so the
// compiled-in absolute path is what makes the Cli* cases pass identically there and under ctest.
// stdout+stderr are redirected into a scratch file since std::system gives no capture of its own.
CliResult run_cli(const ScratchDir& scratch, const std::vector<std::string>& args) {
    const auto out_path = scratch.path("cli_output.txt");
    std::ostringstream cmd;
    cmd << '"' << native(QUIVER_CLI_PATH) << '"';
    for (const auto& arg : args) {
        cmd << " \"" << native(arg) << "\"";
    }
    cmd << " > \"" << native(out_path) << "\" 2>&1";
#ifdef _WIN32
    // cmd.exe's own /c parsing (what std::system shells out through on Windows) breaks when the
    // command line both starts with a quoted token (the exe path) and contains a `>` redirection
    // operator -- confirmed independently of this suite's code: the identical command fails with
    // "The filename, directory name, or volume label syntax is incorrect" when passed as cmd.exe's
    // single /c argument, and succeeds once wrapped in one more, outermost pair of quotes (the
    // standard cmd.exe workaround for a quoted-program-name-plus-redirection command line).
    const std::string command = "\"" + cmd.str() + "\"";
#else
    const std::string command = cmd.str();
#endif
    const int status = std::system(command.c_str());
    return {status, read_file(out_path)};
}

}  // namespace

TEST(LuaRunnerUiOptions, LuaHasUiConfigTrueWhenLoaded) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "foresight_like", "lua_ui_options_true");
    quiver::LuaRunner lua(db);
    lua.run(R"LUA(assert(db:has_ui_config() == true, "expected has_ui_config() to be true"))LUA");
}

TEST(LuaRunnerUiOptions, LuaHasUiConfigFalseWhenAbsent) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "no_ui_dir", "lua_ui_options_false");
    quiver::LuaRunner lua(db);
    lua.run(R"LUA(assert(db:has_ui_config() == false, "expected has_ui_config() to be false"))LUA");
}

TEST(LuaRunnerUiOptions, CliUiLocaleFlagRendersSpanishLabel) {
    ScratchDir scratch("cli_locale_es");
    const auto db_path = scratch.path("db.sqlite");
    const auto script_path = scratch.path("script.lua");
    write_file(script_path, R"LUA(return db:describe_collection("EconomicDriver"))LUA");

    const auto result = run_cli(scratch,
                                {"--schema",
                                 foresight_schema_path(),
                                 "--ui-config-dir",
                                 foresight_ui_dir(),
                                 "--ui-locale",
                                 "es",
                                 db_path,
                                 script_path});

    EXPECT_EQ(result.status, 0) << result.output;
    EXPECT_TRUE(contains(result.output, "Ingenuo Estacional")) << result.output;
    EXPECT_FALSE(contains(result.output, "Seasonal Na\xC3\xAFve")) << result.output;
}

TEST(LuaRunnerUiOptions, CliDefaultLocaleRendersEnglishLabel) {
    ScratchDir scratch("cli_locale_default");
    const auto db_path = scratch.path("db.sqlite");
    const auto script_path = scratch.path("script.lua");
    write_file(script_path, R"LUA(return db:describe_collection("EconomicDriver"))LUA");

    const auto result = run_cli(
        scratch, {"--schema", foresight_schema_path(), "--ui-config-dir", foresight_ui_dir(), db_path, script_path});

    EXPECT_EQ(result.status, 0) << result.output;
    EXPECT_TRUE(contains(result.output, "Seasonal Na\xC3\xAFve")) << result.output;
    EXPECT_FALSE(contains(result.output, "Ingenuo Estacional")) << result.output;
}

TEST(LuaRunnerUiOptions, CliMissingUiConfigDirStillSucceeds) {
    ScratchDir scratch("cli_missing_ui_dir");
    const auto db_path = scratch.path("db.sqlite");
    const auto script_path = scratch.path("script.lua");
    write_file(script_path, R"LUA(return db:describe_collection("EconomicDriver"))LUA");
    const auto missing_dir = scratch.path("does_not_exist");

    const auto result =
        run_cli(scratch, {"--schema", foresight_schema_path(), "--ui-config-dir", missing_dir, db_path, script_path});

    EXPECT_EQ(result.status, 0) << result.output;
}
