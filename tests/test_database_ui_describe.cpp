#include "test_ui_fixture.h"
#include "test_utils.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <quiver/database.h>
#include <quiver/element.h>
#include <string>
#include <vector>

namespace {

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> lines;
    std::string::size_type start = 0;
    while (start <= text.size()) {
        const auto newline = text.find('\n', start);
        if (newline == std::string::npos) {
            lines.push_back(text.substr(start));
            break;
        }
        lines.push_back(text.substr(start, newline - start));
        start = newline + 1;
    }
    return lines;
}

// The em dash (U+2014) as explicit UTF-8 bytes -- mirrors src/database_describe.cpp's kEmDash so
// this file never risks a literal character being mangled by a compiler's default source charset.
const std::string kEmDash = "\xE2\x80\x94";

// Returns the first line whose start matches `prefix`, or "" when none does.
std::string find_line(const std::vector<std::string>& lines, const std::string& prefix) {
    for (const auto& line : lines) {
        if (line.rfind(prefix, 0) == 0) {
            return line;
        }
    }
    return "";
}

size_t count_occurrences(const std::string& haystack, const std::string& needle) {
    size_t count = 0;
    size_t pos = 0;
    while ((pos = haystack.find(needle, pos)) != std::string::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
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

// D-05/D-06: all three reports carry `UI config: <path> (locale: en)` when a sidecar loaded --
// describe_collection()/summarize_collection() have it as line 1, describe() has it on line 3
// (after Database:/Version:).
TEST(DatabaseUiDescribe, HeaderLineInAllThreeReports) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "enum_basic", "cpp_header");

    const auto describe_report = db.describe();
    const auto describe_collection_report = db.describe_collection("Storage");
    const auto summarize_collection_report = db.summarize_collection("Storage");

    EXPECT_TRUE(contains(describe_report, " (locale: en)")) << describe_report;
    EXPECT_TRUE(contains(describe_collection_report, " (locale: en)")) << describe_collection_report;
    EXPECT_TRUE(contains(summarize_collection_report, " (locale: en)")) << summarize_collection_report;

    const auto describe_collection_lines = split_lines(describe_collection_report);
    ASSERT_FALSE(describe_collection_lines.empty());
    EXPECT_EQ(0u, describe_collection_lines[0].find("UI config: ")) << describe_collection_lines[0];

    const auto summarize_collection_lines = split_lines(summarize_collection_report);
    ASSERT_FALSE(summarize_collection_lines.empty());
    EXPECT_EQ(0u, summarize_collection_lines[0].find("UI config: ")) << summarize_collection_lines[0];

    const auto describe_lines = split_lines(describe_report);
    ASSERT_GE(describe_lines.size(), 3u);
    EXPECT_EQ(0u, describe_lines[0].find("Database: ")) << describe_lines[0];
    EXPECT_EQ(0u, describe_lines[1].find("Version: ")) << describe_lines[1];
    EXPECT_EQ(0u, describe_lines[2].find("UI config: ")) << describe_lines[2];
}

// D-05: no sidecar, no header line at all, in any of the three reports.
TEST(DatabaseUiDescribe, HeaderAbsentWithoutSidecar) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "no_ui_dir", "cpp_header_none");

    EXPECT_FALSE(contains(db.describe(), "UI config: "));
    EXPECT_FALSE(contains(db.describe_collection("Storage"), "UI config: "));
    EXPECT_FALSE(contains(db.summarize_collection("Storage"), "UI config: "));
}

// D-03/DESC-04: a collection with a UI label renders it on its Collection: line, in all three
// reports, produced by the one shared renderer (literal L6, tests/schemas/ui/README.md).
TEST(DatabaseUiDescribe, CollectionLabelRendered) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "enum_basic", "cpp_collection_label");

    const std::string expected = "Collection: Storage (0 elements) " + kEmDash + " \"Storage Units\"";
    EXPECT_TRUE(contains(db.describe(), expected)) << db.describe();
    EXPECT_TRUE(contains(db.describe_collection("Storage"), expected)) << db.describe_collection("Storage");
    EXPECT_TRUE(contains(db.summarize_collection("Storage"), expected)) << db.summarize_collection("Storage");
}

// DESC-02, literal L4 (tests/schemas/ui/README.md): unit and label clauses together, in the fixed
// D-02 order (unit before label).
TEST(DatabaseUiDescribe, UnitAndLabelRendered) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "enum_basic", "cpp_scalar_line");
    const auto report = db.describe_collection("Storage");
    const std::string expected = "    - max_generation (REAL) [MW] " + kEmDash + " \"Maximum Generation\"";
    EXPECT_TRUE(contains(report, expected)) << report;
}

// DESC-06, literal L5: a hidden attribute is tagged, never dropped -- it still appears exactly
// once, decorated with [hidden] and its label.
TEST(DatabaseUiDescribe, HiddenAttributeTaggedNotDropped) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "enum_basic", "cpp_scalar_line_hidden");
    const auto report = db.describe_collection("Storage");
    const std::string expected = "    - internal_code (INTEGER) [hidden] " + kEmDash + " \"Internal Code\"";
    EXPECT_TRUE(contains(report, expected)) << report;
    EXPECT_EQ(1u, count_occurrences(report, "internal_code")) << report;
}

// DESC-03, literal L3: the full declared vocabulary renders even with zero elements in the
// collection -- the list comes from enum.toml, never from the data.
TEST(DatabaseUiDescribe, VocabularyFullValueListRendered) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "enum_basic", "cpp_scalar_line_vocab");
    const auto report = db.describe_collection("Storage");
    EXPECT_TRUE(contains(report, "enum bool {0: Disabled, 1: Enabled}")) << report;
}

// An attribute the sidecar does not configure (id, label -- enum_basic/ui/storage.toml declares
// only has_commitment/max_generation/internal_code/notes) renders today's line unchanged: nothing
// after its type or key flag.
TEST(DatabaseUiDescribe, UnconfiguredAttributeRendersTodaysLine) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "enum_basic", "cpp_scalar_line_unconfigured");
    const auto lines = split_lines(db.describe_collection("Storage"));

    const auto id_line = find_line(lines, "    - id (INTEGER)");
    ASSERT_FALSE(id_line.empty());
    EXPECT_EQ("    - id (INTEGER) PRIMARY KEY", id_line);

    const auto label_line = find_line(lines, "    - label (TEXT)");
    ASSERT_FALSE(label_line.empty());
    EXPECT_EQ("    - label (TEXT) NOT NULL", label_line);
}

// D-12, literal L7 (relocated from plan 01-01 task 3): a configured-with-no-label attribute
// renders today's line exactly -- being present in the sidecar is not by itself a reason to
// decorate a line, and the attribute id is never synthesised into the missing label.
TEST(DatabaseUiDescribe, EmptyLabelRendersTodaysLine) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "enum_basic", "cpp_scalar_line_empty_label");
    const auto lines = split_lines(db.describe_collection("Storage"));

    const auto notes_line = find_line(lines, "    - notes (TEXT)");
    ASSERT_FALSE(notes_line.empty());
    EXPECT_EQ("    - notes (TEXT)", notes_line);
    EXPECT_FALSE(contains(notes_line, kEmDash)) << notes_line;
}

// PARSE-09, literal L15: an attribute bound to a vocabulary the sidecar never declares names the
// binding and the absence -- nothing is invented.
TEST(DatabaseUiDescribe, UndeclaredVocabularyNamed) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "no_enum", "cpp_no_enum");
    const auto report = db.describe_collection("Storage");
    EXPECT_TRUE(contains(report, "enum bool (undeclared vocabulary)")) << report;
}

// D-03: describe() and describe_collection() render the exact same scalar line through the one
// shared renderer -- not two independently-maintained copies.
TEST(DatabaseUiDescribe, DescribeUsesTheSameRenderer) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "enum_basic", "cpp_same_renderer");

    const auto describe_lines = split_lines(db.describe());
    const auto describe_collection_lines = split_lines(db.describe_collection("Storage"));

    const auto describe_line = find_line(describe_lines, "    - has_commitment (INTEGER)");
    const auto describe_collection_line = find_line(describe_collection_lines, "    - has_commitment (INTEGER)");
    ASSERT_FALSE(describe_line.empty());
    EXPECT_EQ(describe_line, describe_collection_line);
}
