#include "test_sandbox.h"

#include <fstream>
#include <iterator>
#include <memory>
#include <utility>

namespace {

quiver::Database output_database(const std::string& path) {
    return quiver::Database::from_schema(
        path,
        VALID_SCHEMA("collections.sql"),
        {.read_only = false, .console_level = quiver::LogLevel::Off}
    );
}

std::string read_log(const std::filesystem::path& path) {
    std::ifstream file(path);
    return {std::istreambuf_iterator<char>(file), {}};
}

}  // namespace

TEST_F(LuaSandboxTest, OutputSharesTheDatabaseLogAndPreservesPrintFormatting) {
    auto db = output_database(db_path());
    quiver::Sandbox sandbox(db);
    EXPECT_EQ(
        sandbox.run(R"(
        print('hello', 42, 1.5, true, false, nil)
        print()
        print(setmetatable({}, {__tostring = function() return 'custom {}' end}))
        return {ok = true}
    )"),
        R"({"ok":true})"
    );
    const auto log = read_log(sandbox_path / "quiver_database.log");
    EXPECT_NE(log.find("Opening database:"), std::string::npos);
    EXPECT_NE(log.find("[info] hello\t42\t1.5\ttrue\tfalse\tnil\n"), std::string::npos);
    EXPECT_NE(log.find("[info] \n"), std::string::npos);
    EXPECT_NE(log.find("[info] custom {}\n"), std::string::npos);
}

TEST_F(LuaSandboxTest, OutputUsesTheHostConfiguredLogPath) {
    auto db = quiver::Database::from_schema(
        db_path(),
        VALID_SCHEMA("collections.sql"),
        {.console_level = quiver::LogLevel::Off, .log_path = "logs/custom.log"}
    );
    quiver::Sandbox sandbox(db);
    EXPECT_EQ(sandbox.run("print('custom destination'); return 42"), "42");
    EXPECT_NE(read_log(sandbox_path / "logs" / "custom.log").find("[info] custom destination\n"), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(sandbox_path / "quiver_database.log"));
}

TEST_F(LuaSandboxTest, OutputIsFlushedBeforeErrorsAndTimeout) {
    auto db = output_database(db_path());
    quiver::Sandbox sandbox(db);
    expect_sandbox_error(sandbox, "print('before error')\nerror('original failure')", ":2: original failure");
    expect_sandbox_error(sandbox, "print('before encoding'); return function() end", "unsupported Lua type");
    expect_sandbox_error(
        sandbox,
        R"(
        local value = setmetatable({}, {__tostring = function() error('conversion failed', 0) end})
        print('incomplete print', value)
    )",
        "conversion failed"
    );
    try {
        sandbox.run("print('before timeout'); while true do end", 100);
        FAIL() << "expected script to time out";
    } catch (const std::exception& e) {
        EXPECT_EQ(std::string(e.what()), "Cannot run: script exceeded its time limit of 100 ms");
    }
    const auto log = read_log(sandbox_path / "quiver_database.log");
    EXPECT_NE(log.find("[info] before error\n"), std::string::npos);
    EXPECT_NE(log.find("[info] before encoding\n"), std::string::npos);
    EXPECT_NE(log.find("[info] before timeout\n"), std::string::npos);
    EXPECT_EQ(log.find("incomplete print"), std::string::npos);
    EXPECT_EQ(sandbox.run("return 42"), "42");
}

TEST_F(SandboxTest, OutputRespectsConsoleLevelAndNeverWritesStdout) {
    for (const auto level : {quiver::LogLevel::Info, quiver::LogLevel::Warn, quiver::LogLevel::Off}) {
        // Windows console sinks retain the HANDLE they receive at construction.
        testing::internal::CaptureStdout();
        testing::internal::CaptureStderr();
        {
            auto db = quiver::Database::from_schema(
                ":memory:",
                collections_schema,
                {.read_only = false, .console_level = level}
            );
            quiver::Sandbox sandbox(db);
            EXPECT_EQ(sandbox.run("print('console message', nil); return 42"), "42");
        }
        const auto error_output = testing::internal::GetCapturedStderr();
        EXPECT_EQ(testing::internal::GetCapturedStdout(), "");
        if (level == quiver::LogLevel::Info) {
            EXPECT_NE(error_output.find("[info] console message\tnil"), std::string::npos);
        } else {
            EXPECT_EQ(error_output.find("console message"), std::string::npos);
        }
    }
}

TEST_F(LuaSandboxTest, OutputReadOnlyDoesNotChangeOrCreateDatabaseLogs) {
    const auto path = sandbox_path / "quiver_database.log";
    {
        auto db = output_database(db_path());
        quiver::Sandbox sandbox(db);
        sandbox.run("print('writer history')");
    }
    const auto original = read_log(path);
    testing::internal::CaptureStderr();
    {
        quiver::Database db(db_path(), {.read_only = true, .console_level = quiver::LogLevel::Info});
        quiver::Sandbox sandbox(db);
        sandbox.run("print('read-only message')");
    }
    EXPECT_NE(testing::internal::GetCapturedStderr().find("[info] read-only message"), std::string::npos);
    EXPECT_EQ(read_log(path), original);
    std::filesystem::remove(path);
    {
        quiver::Database db(db_path(), {.read_only = true, .console_level = quiver::LogLevel::Off});
        quiver::Sandbox sandbox(db);
        sandbox.run("print('no file')");
    }
    EXPECT_FALSE(std::filesystem::exists(path));
}

TEST_F(LuaSandboxTest, OutputFollowsSandboxMovesAndTheCurrentDatabaseLogger) {
    const auto first_path = sandbox_path / "quiver_database.log";
    const auto replacement_dir = sandbox_path / "replacement";
    std::filesystem::create_directory(replacement_dir);
    const auto replacement_path = replacement_dir / "quiver_database.log";
    auto db = output_database(db_path());
    {
        auto source = std::make_unique<quiver::Sandbox>(db);
        source->run("print('original logger')");
        quiver::Sandbox moved(std::move(*source));
        source.reset();
        quiver::Sandbox target(db);
        target.run("finalizer = setmetatable({}, {__gc = function() print('target closed') end})");
        target = std::move(moved);
        EXPECT_NE(read_log(first_path).find("[info] target closed"), std::string::npos);

        auto replacement = output_database((replacement_dir / "test.db").string());
        db = std::move(replacement);
        target.run("print('current logger'); finalizer = setmetatable({}, {__gc = function() print('collected') end})");
    }
    EXPECT_NE(read_log(first_path).find("[info] original logger"), std::string::npos);
    EXPECT_EQ(read_log(first_path).find("current logger"), std::string::npos);
    EXPECT_NE(read_log(replacement_path).find("[info] current logger"), std::string::npos);
    EXPECT_NE(read_log(replacement_path).find("[info] collected"), std::string::npos);
}
