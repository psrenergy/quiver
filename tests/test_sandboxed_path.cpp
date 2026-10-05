#include "sandbox/path_policy.h"
#include "test_sandbox.h"

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

TEST_F(SandboxedPathTest, SubdirectoryIsAllowed) {
    quiver::Database db(db_path(), quiet());
    EXPECT_EQ(resolve_sandboxed_path(db, "write_csv", "sub/data.csv"), (root() / "sub" / "data.csv").string());
}

TEST_F(SandboxedPathTest, AbsolutePathInsideIsAllowed) {
    quiver::Database db(db_path(), quiet());
    const auto inside = (sandbox / "abs.csv").string();  // not canonical on purpose
    EXPECT_EQ(resolve_sandboxed_path(db, "open_file", inside), (root() / "abs.csv").string());
}

TEST_F(SandboxedPathTest, DotDotThatStaysInsideIsAllowed) {
    quiver::Database db(db_path(), quiet());
    EXPECT_EQ(resolve_sandboxed_path(db, "read_csv", "sub/../data.csv"), (root() / "data.csv").string());
}

TEST_F(SandboxedPathTest, DotDotEscapeIsRejected) {
    quiver::Database db(db_path(), quiet());
    EXPECT_EQ(error_of(db, "read_csv", "../outside.csv"), escapes("read_csv", "../outside.csv"));
}

TEST_F(SandboxedPathTest, NormalisedEscapeIsRejected) {
    // "sub" does not exist; weakly_canonical still normalises the ".." lexically.
    quiver::Database db(db_path(), quiet());
    EXPECT_EQ(error_of(db, "export_csv", "sub/../../outside.csv"), escapes("export_csv", "sub/../../outside.csv"));
}

TEST_F(SandboxedPathTest, AbsolutePathOutsideIsRejected) {
    quiver::Database db(db_path(), quiet());
    const auto outside = (sandbox.parent_path() / "quiver_sandboxed_path_outside.csv").string();
    EXPECT_EQ(error_of(db, "import_csv", outside), escapes("import_csv", outside));
}

TEST_F(SandboxedPathTest, RootItselfIsRejected) {
    // The binary subsystem appends ".qvr" by concatenation, so the root would write "<root>.qvr" outside.
    quiver::Database db(db_path(), quiet());
    for (const std::string& path : {std::string("."), std::string("sub/.."), sandbox.string()}) {
        EXPECT_EQ(error_of(db, "open_file", path), escapes("open_file", path));
    }
}

TEST_F(SandboxedPathTest, SymlinkPointingOutsideIsRejected) {
    // The target is a sibling of the sandbox, so TearDown's remove_all(sandbox) only removes the link.
    const fs::path outside(sandbox.string() + "_outside");
    fs::remove_all(outside);
    fs::create_directories(outside);
    std::error_code ec;
    fs::create_directory_symlink(outside, sandbox / "link", ec);
    if (ec) {
        fs::remove_all(outside);
        GTEST_SKIP() << "cannot create a directory symlink here: " << ec.message();
    }
    {
        quiver::Database db(db_path(), quiet());
        EXPECT_EQ(error_of(db, "write_csv", "link/x.csv"), escapes("write_csv", "link/x.csv"));
    }
    fs::remove_all(outside);
}

TEST_F(SandboxedPathTest, InMemoryDatabaseIsRejectedBeforeContainment) {
    quiver::Database db(":memory:", quiet());
    const std::string in_memory = "Cannot save: database is in-memory, file operations are unavailable";
    EXPECT_EQ(error_of(db, "save", "out"), in_memory);
    EXPECT_EQ(error_of(db, "save", "../out"), in_memory);
}

#ifdef _WIN32
// A device name makes weakly_canonical throw; the OS reason after the prefix is localized.
TEST_F(SandboxedPathTest, DeviceNameIsReportedWithPrefix) {
    quiver::Database db(db_path(), quiet());
    for (const std::string& device : {std::string("NUL"), std::string("nul")}) {
        const auto prefix = "Cannot open_file: cannot resolve path '" + device + "': ";
        const auto message = error_of(db, "open_file", device);
        EXPECT_EQ(message.rfind(prefix, 0), 0U) << message;
        EXPECT_GT(message.size(), prefix.size()) << message;
    }
}
#endif
