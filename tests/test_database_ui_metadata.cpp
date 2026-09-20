#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <quiver/database.h>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

// Clause openers the ui-config feature (plan 01/02 of this phase) introduces per scalar
// attribute. Named here as `const char*` constants so later plans reuse the exact spelling
// instead of re-deriving it.
const char* kLabelClauseOpener = "; label";
const char* kEnumClauseOpener = "; enum";
const char* kTooltipClauseOpener = "; tooltip";

}  // namespace

// Base fixture: builds a per-test temp dir holding a `migrations/` tree and a sibling `ui/` tree
// from caller-supplied file contents, so no fixture is ever committed under tests/schemas/ui/.
// Copied from MigrationsTestFixture (tests/test_migrations.cpp) and extended with the ui-tree
// helpers per 01-PATTERNS.md's "Temp-dir fixture idiom".
class UiTempTreeFixture : public ::testing::Test {
protected:
    void SetUp() override {
        root = (fs::temp_directory_path() / "quiver_ui_metadata_test").string();
        if (fs::exists(root)) {
            fs::remove_all(root);
        }
    }

    void TearDown() override {
        if (fs::exists(root)) {
            fs::remove_all(root);
        }
    }

    std::string migrations_dir() const {
        return (fs::path(root) / "migrations").string();
    }

    // The sibling `ui/` directory `from_migrations` resolves against the migrations path. Uses
    // weakly_canonical before parent_path, matching src/lua_runner.cpp's resolve_sandboxed_path
    // idiom -- a raw parent_path() misresolves a trailing-slash or bare-relative migrations path
    // (CONTEXT.md "Two resolution traps").
    std::string ui_dir() const {
        return (fs::weakly_canonical(migrations_dir()).parent_path() / "ui").string();
    }

    void write_migration(int version, const std::string& up_sql, const std::string& down_sql) {
        auto dir = fs::path(migrations_dir()) / std::to_string(version);
        fs::create_directories(dir);
        std::ofstream up(dir / "up.sql");
        up << up_sql;
        up.close();
        std::ofstream down(dir / "down.sql");
        down << down_sql;
        down.close();
    }

    // Callers pass raw string literals; this helper never synthesizes TOML content itself.
    void write_ui_file(const std::string& filename, const std::string& contents) {
        fs::create_directories(ui_dir());
        std::ofstream file(fs::path(ui_dir()) / filename);
        file << contents;
        file.close();
    }

    quiver::Database open_tree() {
        return quiver::Database::from_migrations(
            (fs::path(root) / "study.db").string(),
            migrations_dir(),
            {.read_only = false, .console_level = quiver::LogLevel::Off});
    }

    std::string root;
};

// Loader-facing gtest suite name -- reserved for the sidecar-parsing tests of a later plan in
// this phase (see 01-VALIDATION.md's gtest filters).
class UiConfigTest : public UiTempTreeFixture {};

// Render-facing gtest suite name -- describe / describe_collection / summarize_collection
// assertions, including the SAFE-01 baseline below.
class DatabaseUiMetadataTest : public UiTempTreeFixture {};

namespace {

// The mandatory Configuration table plus a HydroPlant collection carrying the columns every
// later test's sidecar describes. Keep the names exactly as written so plan 01 and plan 02 can
// assert against them without re-reading this file.
std::string reservoir_schema() {
    return R"(
CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;

CREATE TABLE HydroPlant (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL,
    hm3_initial REAL,
    initial_volume_type INTEGER NOT NULL,
    reservoir_type INTEGER,
    discount_rate REAL
) STRICT;
)";
}

// Lines beginning with the pinned four-space-dash scalar prefix, in order -- used by the
// prefix-invariant test (D-07) to compare describe()'s per-scalar line against
// describe_collection()'s correspondingly-indexed line.
std::vector<std::string> extract_scalar_lines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream iss(text);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.rfind("    - ", 0) == 0) {
            lines.push_back(line);
        }
    }
    return lines;
}

}  // namespace

// SAFE-01 baseline: with no `ui/` sibling, a from_migrations tree renders describe(),
// describe_collection() and summarize_collection() exactly as it does today -- no "; label",
// "; enum" or "; tooltip" clause anywhere, and write_collection_section's existing scalar line
// (shared by describe()/describe_collection()) is untouched. This test is green against today's
// build with zero production changes, and stays green after the feature lands -- it is the
// SAFE-01 anchor, not a scaffold placeholder.
TEST_F(DatabaseUiMetadataTest, NoUiDirReportsUnchanged) {
    write_migration(1, reservoir_schema(), "DROP TABLE HydroPlant; DROP TABLE Configuration;");

    auto db = open_tree();

    auto describe = db.describe();
    auto describe_collection = db.describe_collection("HydroPlant");
    auto summarize_collection = db.summarize_collection("HydroPlant");

    // describe() and describe_collection() share write_collection_section, which emits this exact
    // scalar line (src/database_describe.cpp) -- present verbatim today, and must stay so when no
    // ui/ sidecar exists.
    const std::string expected_scalar_line = "    - initial_volume_type (INTEGER) NOT NULL\n";
    EXPECT_NE(describe.find(expected_scalar_line), std::string::npos) << describe;
    EXPECT_NE(describe_collection.find(expected_scalar_line), std::string::npos) << describe_collection;

    // summarize_collection has its own scalar loop (null/non-null counts, not the "(TYPE) ..."
    // declaration form), so it is checked for its own unchanged shape instead of the line above.
    EXPECT_NE(summarize_collection.find("    - initial_volume_type: "), std::string::npos) << summarize_collection;

    for (const auto* output : {&describe, &describe_collection, &summarize_collection}) {
        EXPECT_EQ(output->find(kLabelClauseOpener), std::string::npos) << *output;
        EXPECT_EQ(output->find(kEnumClauseOpener), std::string::npos) << *output;
        EXPECT_EQ(output->find(kTooltipClauseOpener), std::string::npos) << *output;
    }
}

// ============================================================================
// Task 1-01-01: label + tooltip render, one path through every layer
// ============================================================================

// D-01/D-08 worked example: a label renders in both reports, a tooltip renders only in
// describe_collection() and sits after the label clause.
TEST_F(DatabaseUiMetadataTest, RenderLabelAndTooltipClauses) {
    write_migration(1, reservoir_schema(), "DROP TABLE HydroPlant; DROP TABLE Configuration;");
    write_ui_file("hydro_plant.toml", R"TOML(
id = "HydroPlant"

[[attribute]]
id = "hm3_initial"
label.en = "Initial Storage (hm³)"
tooltip.en = "Reservoir volume at the start of the study."
)TOML");

    auto db = open_tree();

    const std::string expected_describe_line =
        "    - hm3_initial (REAL); label \"Initial Storage (hm³)\"\n";
    const std::string expected_describe_collection_line =
        "    - hm3_initial (REAL); label \"Initial Storage (hm³)\"; "
        "tooltip \"Reservoir volume at the start of the study.\"\n";

    auto describe = db.describe();
    auto describe_collection = db.describe_collection("HydroPlant");

    EXPECT_NE(describe.find(expected_describe_line), std::string::npos) << describe;
    EXPECT_EQ(describe.find(kTooltipClauseOpener), std::string::npos) << describe;
    EXPECT_NE(describe_collection.find(expected_describe_collection_line), std::string::npos) << describe_collection;
}

// D-04: a label whose squash equals the attribute name's squash emits no label clause.
TEST_F(DatabaseUiMetadataTest, RenderSuppressesRedundantLabel) {
    write_migration(1, reservoir_schema(), "DROP TABLE HydroPlant; DROP TABLE Configuration;");
    write_ui_file("hydro_plant.toml", R"(
id = "HydroPlant"

[[attribute]]
id = "initial_volume_type"
label.en = "Initial Volume Type"
)");

    auto db = open_tree();
    auto describe_collection = db.describe_collection("HydroPlant");

    EXPECT_NE(describe_collection.find("    - initial_volume_type (INTEGER) NOT NULL\n"), std::string::npos)
        << describe_collection;
    EXPECT_EQ(describe_collection.find(kLabelClauseOpener), std::string::npos) << describe_collection;
}

// D-05: a tooltip whose squash equals the raw sidecar label's squash is suppressed, even though
// the label itself (not redundant against the name) is still rendered.
TEST_F(DatabaseUiMetadataTest, RenderSuppressesRedundantTooltip) {
    write_migration(1, reservoir_schema(), "DROP TABLE HydroPlant; DROP TABLE Configuration;");
    write_ui_file("hydro_plant.toml", R"(
id = "HydroPlant"

[[attribute]]
id = "hm3_initial"
label.en = "Storage Volume"
tooltip.en = "Storage Volume"
)");

    auto db = open_tree();
    auto describe_collection = db.describe_collection("HydroPlant");

    EXPECT_NE(describe_collection.find("; label \"Storage Volume\""), std::string::npos) << describe_collection;
    EXPECT_EQ(describe_collection.find(kTooltipClauseOpener), std::string::npos) << describe_collection;
}

// D-02: a label containing a double quote and a backslash arrives escaped, and nothing else is
// escaped. A TOML literal (single-quoted) string keeps the source bytes exactly as written.
TEST_F(DatabaseUiMetadataTest, RenderEscapesQuotesAndBackslashes) {
    write_migration(1, reservoir_schema(), "DROP TABLE HydroPlant; DROP TABLE Configuration;");
    write_ui_file("hydro_plant.toml", R"(
id = "HydroPlant"

[[attribute]]
id = "hm3_initial"
label = 'Say "Hi" and a backslash \ here'
)");

    auto db = open_tree();
    auto describe_collection = db.describe_collection("HydroPlant");

    EXPECT_NE(describe_collection.find(R"(; label "Say \"Hi\" and a backslash \\ here")"), std::string::npos)
        << describe_collection;
}

// D-08: describe() never renders a tooltip clause, even when one is present in the sidecar.
TEST_F(DatabaseUiMetadataTest, RenderDescribeOmitsTooltip) {
    write_migration(1, reservoir_schema(), "DROP TABLE HydroPlant; DROP TABLE Configuration;");
    write_ui_file("hydro_plant.toml", R"(
id = "HydroPlant"

[[attribute]]
id = "discount_rate"
tooltip.en = "Annual discount rate applied to future operating costs, in %."
)");

    auto db = open_tree();
    auto describe = db.describe();
    auto describe_collection = db.describe_collection("HydroPlant");

    EXPECT_EQ(describe.find(kTooltipClauseOpener), std::string::npos) << describe;
    EXPECT_NE(describe_collection.find(kTooltipClauseOpener), std::string::npos) << describe_collection;
}

// D-07: for every scalar, describe()'s line is a strict character-for-character prefix of
// describe_collection()'s line. The cheapest possible anti-drift guarantee -- write it first.
TEST_F(DatabaseUiMetadataTest, PrefixInvariantDescribeIsPrefixOfDescribeCollection) {
    write_migration(1, reservoir_schema(), "DROP TABLE HydroPlant; DROP TABLE Configuration;");
    write_ui_file("enum.toml", R"(
[[reservoir_type]]
id = 0
label.en = "Reservoir"

[[reservoir_type]]
id = 1
label.en = "Run of river"
)");
    write_ui_file("hydro_plant.toml", R"(
id = "HydroPlant"

[[attribute]]
id = "hm3_initial"
label.en = "Storage Volume"
tooltip.en = "Reservoir volume at the start of the study."

[[attribute]]
id = "discount_rate"
tooltip.en = "Annual discount rate."

[[attribute]]
id = "reservoir_type"
label.en = "Reservoir Kind"
enum = "reservoir_type"
tooltip.en = "Operating mode of the plant."
)");

    auto db = open_tree();
    auto describe = db.describe();
    auto describe_collection = db.describe_collection("HydroPlant");

    auto pos = describe.find("Collection: HydroPlant");
    ASSERT_NE(pos, std::string::npos) << describe;
    auto section_end = describe.find("\nCollection: ", pos + 1);
    const std::string hydro_section =
        section_end == std::string::npos ? describe.substr(pos) : describe.substr(pos, section_end - pos);

    auto describe_lines = extract_scalar_lines(hydro_section);
    auto describe_collection_lines = extract_scalar_lines(describe_collection);

    ASSERT_EQ(describe_lines.size(), describe_collection_lines.size());
    ASSERT_FALSE(describe_lines.empty());
    for (size_t i = 0; i < describe_lines.size(); ++i) {
        EXPECT_EQ(describe_collection_lines[i].rfind(describe_lines[i], 0), 0)
            << "describe line:            " << describe_lines[i] << "\n"
            << "describe_collection line: " << describe_collection_lines[i];
    }
}

// ============================================================================
// UiConfigTest: loader-facing behavior, driven through the public Database API only (D-12)
// ============================================================================

// READ-03: keyed by the file's own top-level id and each [[attribute]]'s own id -- never the
// filename. The file below is named differently from both the collection and the attribute.
TEST_F(UiConfigTest, LabelTooltipKeyedByFileIdAndAttributeId) {
    write_migration(1, reservoir_schema(), "DROP TABLE HydroPlant; DROP TABLE Configuration;");
    write_ui_file("plant_metadata.toml", R"(
id = "HydroPlant"

[[attribute]]
id = "reservoir_type"
label.en = "Reservoir Kind"
)");

    auto db = open_tree();
    auto describe_collection = db.describe_collection("HydroPlant");

    EXPECT_NE(describe_collection.find("    - reservoir_type (INTEGER); label \"Reservoir Kind\"\n"),
              std::string::npos)
        << describe_collection;
}

// READ-04: a localizable value is read either as a bare string or from a table's `en` sub-key.
TEST_F(UiConfigTest, LocalizedStringOrTableEn) {
    write_migration(1, reservoir_schema(), "DROP TABLE HydroPlant; DROP TABLE Configuration;");
    write_ui_file("hydro_plant.toml", R"(
id = "HydroPlant"

[[attribute]]
id = "hm3_initial"
label = "Bare String Label"

[[attribute]]
id = "discount_rate"
label.en = "Table En Label"
)");

    auto db = open_tree();
    auto describe_collection = db.describe_collection("HydroPlant");

    EXPECT_NE(describe_collection.find("; label \"Bare String Label\""), std::string::npos) << describe_collection;
    EXPECT_NE(describe_collection.find("; label \"Table En Label\""), std::string::npos) << describe_collection;
}

// READ-04: embedded newlines collapse to a single space so the rendered line stays one line.
TEST_F(UiConfigTest, LocalizedNewlineCollapse) {
    write_migration(1, reservoir_schema(), "DROP TABLE HydroPlant; DROP TABLE Configuration;");
    write_ui_file("hydro_plant.toml", R"(
id = "HydroPlant"

[[attribute]]
id = "hm3_initial"
label.en = "Mean\nProduction\nFactor"
)");

    auto db = open_tree();
    auto describe_collection = db.describe_collection("HydroPlant");

    EXPECT_NE(describe_collection.find("; label \"Mean Production Factor\""), std::string::npos)
        << describe_collection;
}

// READ-04/D-03: every C0 control byte (tab, CR, ESC, ...) is normalized to a space, not just the
// \r/\n/\t named in D-03's prose -- a deliberate superset that also neutralizes ESC.
TEST_F(UiConfigTest, LocalizedControlCharacterCollapse) {
    write_migration(1, reservoir_schema(), "DROP TABLE HydroPlant; DROP TABLE Configuration;");
    write_ui_file("hydro_plant.toml", R"(
id = "HydroPlant"

[[attribute]]
id = "hm3_initial"
label.en = "A\tB\rC\u001bD"
)");

    auto db = open_tree();
    auto describe_collection = db.describe_collection("HydroPlant");

    EXPECT_NE(describe_collection.find("; label \"A B C D\""), std::string::npos) << describe_collection;
}

// READ-04/D-02: non-ASCII UTF-8 passes through byte-for-byte -- squash() may drop it for
// redundancy comparisons, but the rendered text itself is never transcoded.
TEST_F(UiConfigTest, LocalizedUtf8Passthrough) {
    write_migration(1, reservoir_schema(), "DROP TABLE HydroPlant; DROP TABLE Configuration;");
    write_ui_file("hydro_plant.toml", R"(
id = "HydroPlant"

[[attribute]]
id = "hm3_initial"
label.en = "Volume Útil"

[[attribute]]
id = "discount_rate"
tooltip.en = "Measured in °C"
)");

    auto db = open_tree();
    auto describe_collection = db.describe_collection("HydroPlant");

    EXPECT_NE(describe_collection.find("; label \"Volume Útil\""), std::string::npos) << describe_collection;
    EXPECT_NE(describe_collection.find("; tooltip \"Measured in °C\""), std::string::npos)
        << describe_collection;
}

// ============================================================================
// Task 1-01-02: enum.toml vocabularies and the enum clause (TDD)
// ============================================================================

// D-06/D-19: a gapped vocabulary ([0, 2]) renders its real codes verbatim, joined by the
// attribute's own `enum` value.
TEST_F(UiConfigTest, EnumGappedCodesRenderVerbatim) {
    write_migration(1, reservoir_schema(), "DROP TABLE HydroPlant; DROP TABLE Configuration;");
    write_ui_file("enum.toml", R"(
[[initial_volume_type]]
id = 0
label.en = "Per Unit"

[[initial_volume_type]]
id = 2
label.en = "Volume"
)");
    write_ui_file("hydro_plant.toml", R"(
id = "HydroPlant"

[[attribute]]
id = "initial_volume_type"
enum = "initial_volume_type"
)");

    auto db = open_tree();
    auto describe_collection = db.describe_collection("HydroPlant");

    EXPECT_NE(describe_collection.find(R"(; enum {0: "Per Unit", 2: "Volume"})"), std::string::npos)
        << describe_collection;
}

// D-06: a 1-based vocabulary renders with no positional renumbering.
TEST_F(UiConfigTest, EnumOneBasedCodesRenderVerbatim) {
    write_migration(1, reservoir_schema(), "DROP TABLE HydroPlant; DROP TABLE Configuration;");
    write_ui_file("enum.toml", R"(
[[reservoir_type]]
id = 1
label.en = "Reservoir"

[[reservoir_type]]
id = 2
label.en = "Run of river"
)");
    write_ui_file("hydro_plant.toml", R"(
id = "HydroPlant"

[[attribute]]
id = "reservoir_type"
enum = "reservoir_type"
)");

    auto db = open_tree();
    auto describe_collection = db.describe_collection("HydroPlant");

    EXPECT_NE(describe_collection.find(R"(; enum {1: "Reservoir", 2: "Run of river"})"), std::string::npos)
        << describe_collection;
}

// D-19: two attributes with different ids sharing one vocabulary name each render that
// vocabulary -- the join key is the attribute's `enum` value, never its `id`.
TEST_F(UiConfigTest, EnumJoinedByEnumValueNotAttributeId) {
    write_migration(1, reservoir_schema(), "DROP TABLE HydroPlant; DROP TABLE Configuration;");
    write_ui_file("enum.toml", R"(
[[bool]]
id = 0
label.en = "No"

[[bool]]
id = 1
label.en = "Yes"
)");
    write_ui_file("hydro_plant.toml", R"(
id = "HydroPlant"

[[attribute]]
id = "initial_volume_type"
enum = "bool"

[[attribute]]
id = "reservoir_type"
enum = "bool"
)");

    auto db = open_tree();
    auto describe_collection = db.describe_collection("HydroPlant");

    size_t first = describe_collection.find(R"(; enum {0: "No", 1: "Yes"})");
    ASSERT_NE(first, std::string::npos) << describe_collection;
    size_t second = describe_collection.find(R"(; enum {0: "No", 1: "Yes"})", first + 1);
    EXPECT_NE(second, std::string::npos) << describe_collection;
}

// An attribute whose `enum` value names no vocabulary in enum.toml renders no enum clause.
TEST_F(UiConfigTest, EnumUnknownVocabularyRendersNoClause) {
    write_migration(1, reservoir_schema(), "DROP TABLE HydroPlant; DROP TABLE Configuration;");
    write_ui_file("enum.toml", R"(
[[bool]]
id = 0
label.en = "No"
)");
    write_ui_file("hydro_plant.toml", R"(
id = "HydroPlant"

[[attribute]]
id = "reservoir_type"
enum = "does_not_exist"
)");

    auto db = open_tree();
    auto describe_collection = db.describe_collection("HydroPlant");

    EXPECT_EQ(describe_collection.find(kEnumClauseOpener), std::string::npos) << describe_collection;
}

// A vocabulary with zero entries renders no enum clause -- never an empty brace pair.
TEST_F(UiConfigTest, EnumEmptyVocabularyRendersNoClause) {
    write_migration(1, reservoir_schema(), "DROP TABLE HydroPlant; DROP TABLE Configuration;");
    write_ui_file("enum.toml", "reservoir_type = []\n");
    write_ui_file("hydro_plant.toml", R"(
id = "HydroPlant"

[[attribute]]
id = "reservoir_type"
enum = "reservoir_type"
)");

    auto db = open_tree();
    auto describe_collection = db.describe_collection("HydroPlant");

    EXPECT_EQ(describe_collection.find(kEnumClauseOpener), std::string::npos) << describe_collection;
}

// A vocabulary entry with no id, or no readable label, is dropped; the surviving entry still
// renders.
TEST_F(UiConfigTest, EnumEntryMissingIdOrLabelIsDropped) {
    write_migration(1, reservoir_schema(), "DROP TABLE HydroPlant; DROP TABLE Configuration;");
    write_ui_file("enum.toml", R"(
[[reservoir_type]]
label.en = "No Id"

[[reservoir_type]]
id = 1

[[reservoir_type]]
id = 2
label.en = "Valid Entry"
)");
    write_ui_file("hydro_plant.toml", R"(
id = "HydroPlant"

[[attribute]]
id = "reservoir_type"
enum = "reservoir_type"
)");

    auto db = open_tree();
    auto describe_collection = db.describe_collection("HydroPlant");

    EXPECT_NE(describe_collection.find(R"(; enum {2: "Valid Entry"})"), std::string::npos) << describe_collection;
}

// D-06: entries render in ascending code order regardless of file order.
TEST_F(UiConfigTest, EnumEntriesRenderInAscendingCodeOrder) {
    write_migration(1, reservoir_schema(), "DROP TABLE HydroPlant; DROP TABLE Configuration;");
    write_ui_file("enum.toml", R"(
[[initial_volume_type]]
id = 2
label.en = "Volume"

[[initial_volume_type]]
id = 0
label.en = "Per Unit"
)");
    write_ui_file("hydro_plant.toml", R"(
id = "HydroPlant"

[[attribute]]
id = "initial_volume_type"
enum = "initial_volume_type"
)");

    auto db = open_tree();
    auto describe_collection = db.describe_collection("HydroPlant");

    EXPECT_NE(describe_collection.find(R"(; enum {0: "Per Unit", 2: "Volume"})"), std::string::npos)
        << describe_collection;
}

// enum.toml has no wrapper key: each top-level key is discovered by iteration and IS itself a
// vocabulary name. Three differently-named vocabularies in one file, each joined by a different
// attribute, prove discover-by-iteration -- a fixed lookup key could not find any of them.
TEST_F(UiConfigTest, EnumTopLevelKeyIsTheVocabularyName) {
    write_migration(1, reservoir_schema(), "DROP TABLE HydroPlant; DROP TABLE Configuration;");
    write_ui_file("enum.toml", R"(
[[bool]]
id = 0
label.en = "No"

[[initial_volume_type]]
id = 0
label.en = "Per Unit"

[[reservoir_type]]
id = 0
label.en = "Reservoir"
)");
    write_ui_file("hydro_plant.toml", R"(
id = "HydroPlant"

[[attribute]]
id = "initial_volume_type"
enum = "initial_volume_type"

[[attribute]]
id = "reservoir_type"
enum = "reservoir_type"
)");

    auto db = open_tree();
    auto describe_collection = db.describe_collection("HydroPlant");

    EXPECT_NE(describe_collection.find(R"(; enum {0: "Per Unit"})"), std::string::npos) << describe_collection;
    EXPECT_NE(describe_collection.find(R"(; enum {0: "Reservoir"})"), std::string::npos) << describe_collection;
}
