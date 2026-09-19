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

// A scratch schema + ui/ sidecar written under a fresh directory relative to the test binary's
// cwd (build/bin -- gtest_discover_tests's WORKING_DIRECTORY) and removed in the destructor.
// Task 3 is scoped to this test file only (no fixture corpus edit), and no fixture under
// tests/schemas/ui/ isolates "unit present, label absent" (every unit-bearing attribute there
// also carries a label) -- so that one clause combination is proven here instead of against the
// tracked corpus.
class ScratchSidecarDir {
public:
    explicit ScratchSidecarDir(const std::string& name) : dir_(name) {
        std::filesystem::remove_all(dir_);
        std::filesystem::create_directories(dir_ / "ui");
    }
    ~ScratchSidecarDir() { std::filesystem::remove_all(dir_); }

    ScratchSidecarDir(const ScratchSidecarDir&) = delete;
    ScratchSidecarDir& operator=(const ScratchSidecarDir&) = delete;

    void write_schema(const std::string& content) const { write_file(dir_ / "schema.sql", content); }
    void write_ui_file(const std::string& filename, const std::string& content) const {
        write_file(dir_ / "ui" / filename, content);
    }

    quiver::Database open(const std::string& stem) const {
        return quiver::Database::from_schema((dir_ / (stem + ".sqlite")).string(), (dir_ / "schema.sql").string(),
                                             {.read_only = false, .console_level = quiver::LogLevel::Off});
    }

private:
    std::filesystem::path dir_;
};

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

// WR-01 (review fix): a `ui/` directory that exists but has no `main.toml` -- e.g. a typo'd
// filename, a partial deployment, a case-sensitivity slip -- must be treated as malformed
// (D-24/D-25), not silently accepted as a valid, empty sidecar. Before the fix, read_file()
// returned "" for the missing path and toml::parse("") succeeded as an empty table, so
// has_ui_config() reported true with nothing to show for it. Schema is an exact copy of
// tests/schemas/ui_golden/schema.sql, so the byte-for-byte comparison below proves the output is
// identical to the no-sidecar golden baseline, not merely "doesn't throw".
TEST(DatabaseUiDescribe, DirectoryWithoutMainTomlIsTreatedAsMalformed) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "no_main_toml", "cpp_no_main_toml");
    EXPECT_FALSE(db.has_ui_config());

    db.create_element(
        "Items",
        quiver::Element().set("label", std::string("a")).set("priority", static_cast<int64_t>(1)).set("weight", 1.5));
    db.create_element(
        "Items",
        quiver::Element().set("label", std::string("b")).set("priority", static_cast<int64_t>(2)).set("weight", 2.5));

    std::ifstream describe_collection_in(SCHEMA_PATH("schemas/ui_golden/describe_collection.txt"), std::ios::binary);
    std::string describe_collection_golden((std::istreambuf_iterator<char>(describe_collection_in)),
                                           std::istreambuf_iterator<char>());
    EXPECT_EQ(db.describe_collection("Items"), describe_collection_golden);

    std::ifstream summarize_collection_in(SCHEMA_PATH("schemas/ui_golden/summarize_collection.txt"), std::ios::binary);
    std::string summarize_collection_golden((std::istreambuf_iterator<char>(summarize_collection_in)),
                                            std::istreambuf_iterator<char>());
    EXPECT_EQ(db.summarize_collection("Items"), summarize_collection_golden);

    // describe()'s first line is "Database: <path>", which differs between this file-backed
    // fixture and the :memory: golden -- compare everything after it, mirroring
    // NoSidecarOutputStillMatchesGolden below.
    std::ifstream describe_in(SCHEMA_PATH("schemas/ui_golden/describe.txt"), std::ios::binary);
    std::string golden_describe((std::istreambuf_iterator<char>(describe_in)), std::istreambuf_iterator<char>());
    const auto file_backed_describe = db.describe();
    const auto golden_rest = golden_describe.substr(golden_describe.find('\n') + 1);
    const auto file_backed_rest = file_backed_describe.substr(file_backed_describe.find('\n') + 1);
    EXPECT_EQ(file_backed_rest, golden_rest);
}

// WR-01 companion: the same missing-main.toml directory logs at warn (the malformed path),
// mirroring MalformedDirectoryLogsAtWarn below -- not at debug (the absent-directory path).
TEST(DatabaseUiDescribe, DirectoryWithoutMainTomlLogsAtWarn) {
    testing::internal::CaptureStderr();
    auto db = quiver::Database::from_schema(SCHEMA_PATH("schemas/ui/no_main_toml") + "/cpp_stderr_no_main_toml.sqlite",
                                            SCHEMA_PATH("schemas/ui/no_main_toml/schema.sql"),
                                            {.read_only = false, .console_level = quiver::LogLevel::Warn});
    db.has_ui_config();
    const auto output = testing::internal::GetCapturedStderr();
    EXPECT_FALSE(output.empty());
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

// DESC-02/03/04 adjacency edge: unit/label/vocabulary are each independently optional -- every
// combination renders as exactly one well-formed line, with no doubled or trailing separator.
// Four of the five combinations (unit only, label only, both, neither) are proven against a
// scratch sidecar (see ScratchSidecarDir); the corpus has no attribute isolating "unit present,
// label absent" alone.
TEST(DatabaseUiDescribe, ClauseCombinationsHaveNoDoubledOrTrailingSeparator) {
    ScratchSidecarDir scratch("cpp_combinations_scratch");
    scratch.write_schema(R"(PRAGMA foreign_keys = ON;

CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE Storage (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL,
    unit_only REAL,
    label_only REAL,
    both_present REAL,
    neither_present REAL
) STRICT;
)");
    scratch.write_ui_file("main.toml", "model = \"Combinations\"\ncollections = [\"storage\"]\n");
    scratch.write_ui_file("storage.toml", R"(id = "Storage"

[[attribute]]
id = "unit_only"
unit = "MW"

[[attribute]]
id = "label_only"
label = "Label Only"

[[attribute]]
id = "both_present"
unit = "MW"
label = "Both Present"

[[attribute]]
id = "neither_present"
)");

    auto db = scratch.open("db");
    const auto lines = split_lines(db.describe_collection("Storage"));

    EXPECT_EQ("    - unit_only (REAL) [MW]", find_line(lines, "    - unit_only"));
    EXPECT_EQ("    - label_only (REAL) " + kEmDash + " \"Label Only\"", find_line(lines, "    - label_only"));
    EXPECT_EQ("    - both_present (REAL) [MW] " + kEmDash + " \"Both Present\"", find_line(lines, "    - both_present"));
    EXPECT_EQ("    - neither_present (REAL)", find_line(lines, "    - neither_present"));
}

// Fifth combination: a resolved vocabulary (htd_like's has_commitment, which also carries a
// label) versus an unresolved one (no_enum's has_commitment, same shape) -- exact full lines.
TEST(DatabaseUiDescribe, VocabularyPresentWithAndWithoutResolution) {
    auto htd_db = quiver::test::open_ui_fixture(__FILE__, "htd_like", "cpp_combinations_vocab_resolved");
    const auto htd_lines = split_lines(htd_db.describe_collection("HydroPlant"));
    const std::string htd_expected =
        "    - has_commitment (INTEGER) " + kEmDash + " \"Unit Commitment\" enum bool {0: Disable, 1: Enable}";
    EXPECT_EQ(htd_expected, find_line(htd_lines, "    - has_commitment"));

    auto no_enum_db = quiver::test::open_ui_fixture(__FILE__, "no_enum", "cpp_combinations_vocab_unresolved");
    const auto no_enum_lines = split_lines(no_enum_db.describe_collection("Storage"));
    const std::string no_enum_expected =
        "    - has_commitment (INTEGER) " + kEmDash + " \"Has Commitment\" enum bool (undeclared vocabulary)";
    EXPECT_EQ(no_enum_expected, find_line(no_enum_lines, "    - has_commitment"));
}

// Cardinality boundary: kMaxDistributionCardinality (64) gates the histogram, and the new label
// clause lives inside that branch. At exactly 64 distinct values the histogram (and its labels)
// still renders.
TEST(DatabaseUiDescribe, CardinalityAt64RendersLabels) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "enum_basic", "cpp_card_64");
    for (int64_t i = 0; i < 64; ++i) {
        db.create_element(
            "Storage", quiver::Element().set("label", "card64_" + std::to_string(i)).set("has_commitment", i));
    }

    const auto report = db.summarize_collection("Storage");
    EXPECT_TRUE(contains(report, "values {")) << report;
    EXPECT_TRUE(contains(report, "(Disabled)")) << report;
    EXPECT_TRUE(contains(report, "(undeclared)")) << report;
}

// At 65 distinct values the histogram (and therefore its labels) is suppressed entirely, while
// describe_collection() still lists the full declared vocabulary -- the declared list is
// independent of the data.
TEST(DatabaseUiDescribe, CardinalityAt65SuppressesHistogram) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "enum_basic", "cpp_card_65");
    for (int64_t i = 0; i < 65; ++i) {
        db.create_element(
            "Storage", quiver::Element().set("label", "card65_" + std::to_string(i)).set("has_commitment", i));
    }

    const auto summarize_report = db.summarize_collection("Storage");
    EXPECT_FALSE(contains(summarize_report, "values {")) << summarize_report;

    const auto describe_report = db.describe_collection("Storage");
    EXPECT_TRUE(contains(describe_report, "enum bool {0: Disabled, 1: Enabled}")) << describe_report;
}

// DESC-05, re-asserted at the end of the render work: a database with no ui/ sidecar renders
// byte-identical to the pre-change golden baselines, so a render regression from this plan's own
// work surfaces here rather than in a later plan.
TEST(DatabaseUiDescribe, NoSidecarOutputStillMatchesGolden) {
    auto db = quiver::test::open_ui_fixture_at(__FILE__, "ui_golden", "cpp_no_sidecar_still_golden");
    db.create_element(
        "Items",
        quiver::Element().set("label", std::string("a")).set("priority", static_cast<int64_t>(1)).set("weight", 1.5));
    db.create_element(
        "Items",
        quiver::Element().set("label", std::string("b")).set("priority", static_cast<int64_t>(2)).set("weight", 2.5));

    std::ifstream describe_collection_in(SCHEMA_PATH("schemas/ui_golden/describe_collection.txt"), std::ios::binary);
    std::string describe_collection_golden((std::istreambuf_iterator<char>(describe_collection_in)),
                                           std::istreambuf_iterator<char>());
    EXPECT_EQ(db.describe_collection("Items"), describe_collection_golden);

    std::ifstream summarize_collection_in(SCHEMA_PATH("schemas/ui_golden/summarize_collection.txt"), std::ios::binary);
    std::string summarize_collection_golden((std::istreambuf_iterator<char>(summarize_collection_in)),
                                            std::istreambuf_iterator<char>());
    EXPECT_EQ(db.summarize_collection("Items"), summarize_collection_golden);

    std::ifstream describe_in(SCHEMA_PATH("schemas/ui_golden/describe.txt"), std::ios::binary);
    std::string golden_describe((std::istreambuf_iterator<char>(describe_in)), std::istreambuf_iterator<char>());
    const auto file_backed_describe = db.describe();
    const auto golden_rest = golden_describe.substr(golden_describe.find('\n') + 1);
    const auto file_backed_rest = file_backed_describe.substr(file_backed_describe.find('\n') + 1);
    EXPECT_EQ(file_backed_rest, golden_rest);
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
