#include "test_sandbox.h"
#include "utils/string.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {

void write_json_file(const std::filesystem::path& path, const std::string& content) {
    std::ofstream file(path, std::ios::binary);
    file << content;
}

}  // namespace

class Sandbox_ReadJson : public LuaSandboxTest {};

TEST_F(Sandbox_ReadJson, NestedValuesKeepNativeTypesAndExactIntegers) {
    auto db = quiver::Database::from_schema(db_path(), VALID_SCHEMA("basic.sql"));
    quiver::Sandbox sandbox(db);
    const std::string source = R"({
        "name": "  ação € 🎯  ", "enabled": true, "disabled": false,
        "min": -9223372036854775808, "max": 9223372036854775807,
        "exact": 9007199254740993, "fraction": 1.5, "exponent": 1e3,
        "unsigned": 18446744073709551615, "large": 18446744073709551616,
        "items": [{"value": 7}, ["01", false]], "1": "string key",
        "escaped": "quote: \" slash: \\ newline: \n tab: \t nul: \u0000",
        "a\u0000b": "nul key", "unicode": "\u00e7\ud83c\udfaf"
    })";
    write_json_file(sandbox_path / "data.json", source);

    sandbox.run(R"(
        local data = db:read_json("data.json")
        assert(data.name == "  ação € 🎯  ")
        assert(type(data.enabled) == "boolean" and data.enabled)
        assert(type(data.disabled) == "boolean" and not data.disabled)
        assert(math.type(data.min) == "integer" and data.min == math.mininteger)
        assert(math.type(data.max) == "integer" and data.max == math.maxinteger)
        assert(math.type(data.exact) == "integer" and data.exact == 9007199254740993)
        assert(math.type(data.fraction) == "float" and data.fraction == 1.5)
        assert(math.type(data.exponent) == "float" and data.exponent == 1000)
        assert(math.type(data.unsigned) == "float" and data.unsigned == 18446744073709551616.0)
        assert(math.type(data.large) == "float" and data.large == 18446744073709551616.0)
        assert(data.items[1].value == 7 and data.items[2][1] == "01" and data.items[2][2] == false)
        assert(data["1"] == "string key" and data[1] == nil)
        assert(data.escaped == 'quote: " slash: \\ newline: \n tab: \t nul: \0')
        assert(data["a\0b"] == "nul key" and data.unicode == "ç🎯")
    )");
    std::ifstream file(sandbox_path / "data.json", std::ios::binary);
    EXPECT_EQ(std::string(std::istreambuf_iterator<char>(file), {}), source);
}

TEST_F(Sandbox_ReadJson, PrimitiveRootsAndEmptyContainersUseExistingReturnEncoding) {
    auto db = quiver::Database::from_schema(db_path(), VALID_SCHEMA("basic.sql"));
    quiver::Sandbox sandbox(db);
    const std::vector<std::pair<std::string, std::string>> cases{
        {"null", "null"},
        {"true", "true"},
        {"false", "false"},
        {"0", "0"},
        {"-12", "-12"},
        {"1.5", "1.5"},
        {R"("hello")", R"("hello")"},
        {"[]", "[]"},
        {"{}", "[]"},
        {R"({"1":"one"})", R"({"1":"one"})"},
        {R"([1,"two",false])", R"([1,"two",false])"},
        {R"({"same":1,"same":2})", R"({"same":2})"},
    };
    for (const auto& [input, expected] : cases) {
        SCOPED_TRACE(input);
        write_json_file(sandbox_path / "data.json", input);
        EXPECT_EQ(sandbox.run(R"(return db:read_json("data.json"))"), expected);
    }
}

TEST_F(Sandbox_ReadJson, NullsLeaveArrayHolesWithoutShiftingIndices) {
    auto db = quiver::Database::from_schema(db_path(), VALID_SCHEMA("basic.sql"));
    quiver::Sandbox sandbox(db);
    write_json_file(
        sandbox_path / "data.json",
        R"({"missing":null,"values":[null,10,null,30,null],"empty":[null,null]})"
    );
    EXPECT_EQ(
        sandbox.run(R"(
            local data = db:read_json("data.json")
            assert(data.missing == nil and data.values[1] == nil)
            assert(data.values[2] == 10 and data.values[3] == nil)
            assert(data.values[4] == 30 and data.values[5] == nil)
            assert(next(data.empty) == nil)
            return data.values
        )"),
        R"({"2":10,"4":30})"
    );
}

TEST_F(Sandbox_ReadJson, Utf8BomAndSurroundingWhitespaceAreAccepted) {
    auto db = quiver::Database::from_schema(db_path(), VALID_SCHEMA("basic.sql"));
    quiver::Sandbox sandbox(db);
    write_json_file(sandbox_path / "data.json", std::string("\xEF\xBB\xBF") + " \r\n [1, 2] \t\r\n");
    EXPECT_EQ(sandbox.run(R"(return db:read_json("data.json"))"), "[1,2]");
}

TEST_F(Sandbox_ReadJson, InvalidJsonUtf8AndOverflowReportPathAndOperation) {
    auto db = quiver::Database::from_schema(db_path(), VALID_SCHEMA("basic.sql"));
    quiver::Sandbox sandbox(db);
    const std::vector<std::string> cases{
        "",
        " \n\t",
        "{",
        R"({"x":})",
        "true false",
        "[1,]",
        "/*comment*/ null",
        "NaN",
        "Infinity",
        "01",
        "1e400",
        R"("\ud800")",
        R"("unterminated)",
        std::string("{\"x\":\"") + '\xE9' + "\"}",
        std::string("null\0", 5),
        std::string("null\0false", 10),
    };
    for (const auto& input : cases) {
        SCOPED_TRACE(input);
        write_json_file(sandbox_path / "invalid.json", input);
        expect_sandbox_error(
            sandbox,
            R"(db:read_json("invalid.json"))",
            "Cannot read_json: cannot parse file 'invalid.json':"
        );
    }
    write_json_file(sandbox_path / "invalid.json", "{");
    expect_sandbox_error(sandbox, R"(db:read_json("invalid.json"))", "line 1");
}

TEST_F(Sandbox_ReadJson, InvalidUtf8StillProducesUtf8ErrorTextForHostBindings) {
    auto db = quiver::Database::from_schema(db_path(), VALID_SCHEMA("basic.sql"));
    quiver::Sandbox sandbox(db);
    write_json_file(sandbox_path / "invalid.json", std::string("{\"x\":\"") + '\xE9' + "\"}");
    try {
        sandbox.run(R"(db:read_json("invalid.json"))");
        FAIL() << "expected invalid UTF-8 to throw";
    } catch (const std::exception& e) {
        const std::string message = e.what();
        EXPECT_NE(message.find("Cannot read_json: cannot parse file 'invalid.json':"), std::string::npos);
        EXPECT_NE(message.find("byte"), std::string::npos);
        EXPECT_TRUE(quiver::string::is_valid_utf8(message));
    }
}

TEST_F(Sandbox_ReadJson, NestingLimitIsCheckedBeforeLuaConversion) {
    auto db = quiver::Database::from_schema(db_path(), VALID_SCHEMA("basic.sql"));
    quiver::Sandbox sandbox(db);
    const auto allowed = std::string(32, '[') + "0" + std::string(32, ']');
    write_json_file(sandbox_path / "deep.json", allowed);
    EXPECT_EQ(sandbox.run(R"(return db:read_json("deep.json"))"), allowed);

    for (const std::size_t depth : {33U, 1024U}) {
        write_json_file(sandbox_path / "deep.json", std::string(depth, '[') + "0" + std::string(depth, ']'));
        expect_sandbox_error(
            sandbox,
            R"(db:read_json("deep.json"))",
            "Cannot read_json: file 'deep.json' nests deeper than 32 levels"
        );
    }
    EXPECT_TRUE(std::filesystem::remove(sandbox_path / "deep.json"));
}

TEST_F(Sandbox_ReadJson, InputSizeLimitAcceptsExactBoundaryAndRejectsOneMoreByte) {
    auto db = quiver::Database::from_schema(db_path(), VALID_SCHEMA("basic.sql"));
    quiver::Sandbox sandbox(db);
    constexpr std::size_t kLimit = 64UL * 1024UL * 1024UL;
    std::string content(kLimit, ' ');
    content.replace(0, 4, "null");
    write_json_file(sandbox_path / "large.json", content);
    EXPECT_EQ(sandbox.run(R"(return db:read_json("large.json"))"), "null");
    {
        std::ofstream file(sandbox_path / "large.json", std::ios::binary | std::ios::app);
        file.put(' ');
    }
    expect_sandbox_error(
        sandbox,
        R"(db:read_json("large.json"))",
        "Cannot read_json: file 'large.json' exceeds 67108864 bytes of JSON"
    );
    EXPECT_TRUE(std::filesystem::remove(sandbox_path / "large.json"));
}

TEST_F(Sandbox_ReadJson, PathsResolveInsideDatabaseDirectoryAndRejectEscapes) {
    auto db = quiver::Database::from_schema(db_path(), VALID_SCHEMA("basic.sql"));
    quiver::Sandbox sandbox(db);
    std::filesystem::create_directories(sandbox_path / "nested");
    const auto path = sandbox_path / "nested" / "data.json";
    write_json_file(path, R"({"ok":true})");
    EXPECT_EQ(sandbox.run(R"(return db:read_json("nested/data.json"))"), R"({"ok":true})");
    EXPECT_EQ(sandbox.run("return db:read_json([[" + path.generic_string() + "]])"), R"({"ok":true})");
    expect_sandbox_error(
        sandbox,
        R"(db:read_json("../outside.json"))",
        "Cannot read_json: path '../outside.json' escapes"
    );
    const auto outside = (sandbox_path.parent_path() / "outside.json").generic_string();
    expect_sandbox_error(sandbox, "db:read_json([[" + outside + "]])", "escapes the database directory");
}

TEST_F(Sandbox_ReadJson, MissingFilesDirectoriesAndWrongPathTypesAreDiagnosed) {
    auto db = quiver::Database::from_schema(db_path(), VALID_SCHEMA("basic.sql"));
    quiver::Sandbox sandbox(db);
    expect_sandbox_error(sandbox, R"(db:read_json("missing.json"))", "Cannot read_json: file not found: missing.json");
    std::filesystem::create_directories(sandbox_path / "directory");
    expect_sandbox_error(sandbox, R"(db:read_json("directory"))", "Cannot read_json: path is a directory: directory");
    for (const std::string value : {"nil", "false", "42", "{}"}) {
        expect_sandbox_error(sandbox, "db:read_json(" + value + ")", "Cannot read_json: path must be a string");
    }
}

TEST_F(Sandbox_ReadJson, InMemoryDatabasesRejectFileOperations) {
    auto db = quiver::Database::from_schema(":memory:", VALID_SCHEMA("basic.sql"));
    quiver::Sandbox sandbox(db);
    expect_sandbox_error(
        sandbox,
        R"(db:read_json("data.json"))",
        "Cannot read_json: database is in-memory, file operations are unavailable"
    );
}

TEST_F(Sandbox_ReadJson, UnreadableFileIsReportedAndHandlesCloseOnEveryExit) {
    auto db = quiver::Database::from_schema(db_path(), VALID_SCHEMA("basic.sql"));
    quiver::Sandbox sandbox(db);
    const auto path = sandbox_path / "locked.json";
    write_json_file(path, "null");
#ifdef _WIN32
    HANDLE lock =
        CreateFileW(path.wstring().c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    ASSERT_NE(lock, INVALID_HANDLE_VALUE);
#else
    if (::geteuid() == 0) {
        GTEST_SKIP() << "running as root: mode bits do not restrict access";
    }
    ASSERT_EQ(::chmod(path.c_str(), 0), 0);
#endif
    expect_sandbox_error(sandbox, R"(db:read_json("locked.json"))", "Cannot read_json: cannot read file 'locked.json'");
#ifdef _WIN32
    EXPECT_TRUE(CloseHandle(lock));
#else
    EXPECT_EQ(::chmod(path.c_str(), 0600), 0);
#endif
    EXPECT_EQ(sandbox.run(R"(return db:read_json("locked.json"))"), "null");
    EXPECT_TRUE(std::filesystem::remove(path));
    write_json_file(path, "{");
    expect_sandbox_error(
        sandbox,
        R"(db:read_json("locked.json"))",
        "Cannot read_json: cannot parse file 'locked.json'"
    );
    EXPECT_TRUE(std::filesystem::remove(path));
}
