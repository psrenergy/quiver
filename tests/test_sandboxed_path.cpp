#include "lua_runner/path_policy.h"
#include "test_lua_runner.h"

#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>

namespace fs = std::filesystem;
using quiver::lua_internal::resolve_sandboxed_path;

// Calls the gate directly, without Lua. The suite name stays outside the Lua* filter.
class SandboxedPathTest : public LuaSandboxTest {
protected:
    // What the gate prints: on macOS the temp dir sits behind the /var symlink, on Windows it can
    // be an 8.3 short name.
    fs::path root() const {
        return fs::weakly_canonical(sandbox);
    }

    std::string escapes(const std::string& operation, const std::string& path) const {
        return "Cannot " + operation + ": path '" + path + "' escapes the database directory '" + root().string() + "'";
    }
};

namespace {

quiver::DatabaseOptions quiet() {
    return {.read_only = false, .console_level = quiver::LogLevel::Off};
}

std::string error_of(const quiver::Database& db, const std::string& operation, const std::string& path) {
    try {
        resolve_sandboxed_path(db, operation, path);
    } catch (const std::runtime_error& e) {
        return e.what();
    }
    return "<no throw>";
}

}  // namespace

TEST_F(SandboxedPathTest, RelativePathResolvesInsideTheDatabaseDirectory) {
    quiver::Database db(db_path(), quiet());
    EXPECT_EQ(resolve_sandboxed_path(db, "read_csv", "data.csv"), (root() / "data.csv").string());
}
