#include "test_ui_fixture.h"
#include "test_utils.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <quiver/database.h>
#include <quiver/element.h>
#include <string>

namespace {

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

void write_file(const std::filesystem::path& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary);
    out << content;
}

}  // namespace

// The tracer slice's canonical output: a TOML sidecar on disk reaches
// summarize_collection()'s histogram as "<code>: <count> (<label>)" (D-04, literal L1 in
// plan 01-02's authoritative fixture-literal table).
TEST(DatabaseUiDescribe, EnumLabelsRenderBesideCodes) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "enum_basic", "cpp_enum_basic");

    for (int i = 0; i < 8; ++i) {
        db.create_element("Storage",
                          quiver::Element()
                              .set("label", "disabled_" + std::to_string(i))
                              .set("has_commitment", static_cast<int64_t>(0)));
    }
    for (int i = 0; i < 4; ++i) {
        db.create_element("Storage",
                          quiver::Element()
                              .set("label", "enabled_" + std::to_string(i))
                              .set("has_commitment", static_cast<int64_t>(1)));
    }

    const auto report = db.summarize_collection("Storage");
    EXPECT_TRUE(contains(report, "values {0: 8 (Disabled), 1: 4 (Enabled)}")) << report;
}

// A data code the vocabulary does not declare is marked explicitly (D-04, literal L2) -- never a
// bare code and never a borrowed label.
TEST(DatabaseUiDescribe, UndeclaredCodeRendersMarker) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "enum_basic", "cpp_enum_basic_undeclared");

    db.create_element(
        "Storage", quiver::Element().set("label", std::string("weird")).set("has_commitment", static_cast<int64_t>(2)));

    const auto report = db.summarize_collection("Storage");
    EXPECT_TRUE(contains(report, "2: 1 (undeclared)")) << report;
}

// D-22: has_ui_config() answers in C++ only in this phase.
TEST(DatabaseUiDescribe, HasUiConfigTrueWhenSidecarLoads) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "enum_basic", "cpp_has_ui");
    EXPECT_TRUE(db.has_ui_config());
}

TEST(DatabaseUiDescribe, HasUiConfigFalseWithoutDirectory) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "no_ui_dir", "cpp_no_ui");
    EXPECT_FALSE(db.has_ui_config());

    // Absence is never an error: every report still succeeds, and none carries a label clause.
    EXPECT_NO_THROW(db.describe());
    EXPECT_NO_THROW(db.describe_collection("Storage"));
    db.create_element(
        "Storage", quiver::Element().set("label", std::string("a")).set("has_commitment", static_cast<int64_t>(0)));
    const auto report = db.summarize_collection("Storage");
    EXPECT_TRUE(contains(report, "values {0: 1}")) << report;
    EXPECT_FALSE(contains(report, "(undeclared)")) << report;
    EXPECT_FALSE(contains(report, "(Disabled)")) << report;
}

// D-25/PARSE-12: a single broken collection file fails the whole config -- nothing partial is
// published, even though the enum.toml beside the broken file is itself perfectly valid.
TEST(DatabaseUiDescribe, MalformedSidecarPublishesNothing) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "malformed", "cpp_malformed");
    EXPECT_FALSE(db.has_ui_config());

    EXPECT_NO_THROW(db.describe());
    EXPECT_NO_THROW(db.describe_collection("Storage"));
    db.create_element(
        "Storage", quiver::Element().set("label", std::string("a")).set("has_commitment", static_cast<int64_t>(0)));
    const auto report = db.summarize_collection("Storage");
    EXPECT_TRUE(contains(report, "values {0: 1}")) << report;
    EXPECT_FALSE(contains(report, "(undeclared)")) << report;
    EXPECT_FALSE(contains(report, "(Disabled)")) << report;
}

// The single riskiest regression in the phase: if the :memory: short-circuit in
// Impl::require_ui_config were ever deleted, every existing :memory:-based describe test would
// become sensitive to whatever happens to be sitting in the process's working directory
// (gtest_discover_tests sets WORKING_DIRECTORY to build/bin -- tests/CMakeLists.txt). This test
// plants a valid ui/ directory in that exact cwd and proves has_ui_config() still reports false,
// with the goldens still byte-equal -- not merely "false" on a directory that never existed.
class MemoryDatabaseCwdProbe {
public:
    MemoryDatabaseCwdProbe() {
        std::filesystem::remove_all("ui");
        std::filesystem::create_directories("ui");
        write_file("ui/main.toml", "model = \"Probe\"\ncollections = [\"storage\"]\n");
        write_file("ui/enum.toml", "[[bool]]\nid = 0\nlabel = \"Disabled\"\n\n[[bool]]\nid = 1\nlabel = \"Enabled\"\n");
        write_file("ui/storage.toml",
                  "id = \"Storage\"\n\n[[attribute]]\nid = \"has_commitment\"\nenum = \"bool\"\nlabel = \"Has "
                  "Commitment\"\n");
    }
    ~MemoryDatabaseCwdProbe() { std::filesystem::remove_all("ui"); }

    MemoryDatabaseCwdProbe(const MemoryDatabaseCwdProbe&) = delete;
    MemoryDatabaseCwdProbe& operator=(const MemoryDatabaseCwdProbe&) = delete;
};

TEST(DatabaseUiDescribe, MemoryDatabaseNeverLoadsUiConfig) {
    MemoryDatabaseCwdProbe probe;

    // Leg 1: a :memory: database opened from a schema whose sibling directory *also* has a real
    // ui/ (enum_basic/schema.sql) -- covers the schema-path probe.
    {
        auto db = quiver::Database::from_schema(
            ":memory:", SCHEMA_PATH("schemas/ui/enum_basic/schema.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});
        EXPECT_FALSE(db.has_ui_config());
    }

    // Leg 2: a :memory: database opened from the golden schema -- covers the cwd probe with byte
    // proof, using the only schema the goldens describe.
    {
        auto db = quiver::Database::from_schema(
            ":memory:", SCHEMA_PATH("schemas/ui_golden/schema.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});
        db.create_element(
            "Items",
            quiver::Element().set("label", std::string("a")).set("priority", static_cast<int64_t>(1)).set("weight", 1.5));
        db.create_element(
            "Items",
            quiver::Element().set("label", std::string("b")).set("priority", static_cast<int64_t>(2)).set("weight", 2.5));

        EXPECT_FALSE(db.has_ui_config());

        std::ifstream describe_in(SCHEMA_PATH("schemas/ui_golden/describe.txt"), std::ios::binary);
        std::string describe_golden((std::istreambuf_iterator<char>(describe_in)), std::istreambuf_iterator<char>());
        EXPECT_EQ(db.describe(), describe_golden);

        std::ifstream describe_collection_in(SCHEMA_PATH("schemas/ui_golden/describe_collection.txt"), std::ios::binary);
        std::string describe_collection_golden((std::istreambuf_iterator<char>(describe_collection_in)),
                                               std::istreambuf_iterator<char>());
        EXPECT_EQ(db.describe_collection("Items"), describe_collection_golden);

        std::ifstream summarize_collection_in(SCHEMA_PATH("schemas/ui_golden/summarize_collection.txt"), std::ios::binary);
        std::string summarize_collection_golden((std::istreambuf_iterator<char>(summarize_collection_in)),
                                                std::istreambuf_iterator<char>());
        EXPECT_EQ(db.summarize_collection("Items"), summarize_collection_golden);
    }
}

// D-24: absence logs at debug (silent at Warn); malformation logs at warn (visible at Warn).
TEST(DatabaseUiDescribe, AbsentDirectoryLogsAtDebugNotWarn) {
    testing::internal::CaptureStderr();
    auto db = quiver::Database::from_schema(SCHEMA_PATH("schemas/ui/no_ui_dir") + "/cpp_stderr_absent.sqlite",
                                            SCHEMA_PATH("schemas/ui/no_ui_dir/schema.sql"),
                                            {.read_only = false, .console_level = quiver::LogLevel::Warn});
    db.has_ui_config();
    const auto output = testing::internal::GetCapturedStderr();
    EXPECT_TRUE(output.empty()) << output;
}

TEST(DatabaseUiDescribe, MalformedDirectoryLogsAtWarn) {
    testing::internal::CaptureStderr();
    auto db = quiver::Database::from_schema(SCHEMA_PATH("schemas/ui/malformed") + "/cpp_stderr_malformed.sqlite",
                                            SCHEMA_PATH("schemas/ui/malformed/schema.sql"),
                                            {.read_only = false, .console_level = quiver::LogLevel::Warn});
    db.has_ui_config();
    const auto output = testing::internal::GetCapturedStderr();
    EXPECT_FALSE(output.empty());
}
