#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <optional>
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
        if (fs::exists(mirror_root())) {
            fs::remove_all(mirror_root());
        }
    }

    void TearDown() override {
        if (fs::exists(root)) {
            fs::remove_all(root);
        }
        if (fs::exists(mirror_root())) {
            fs::remove_all(mirror_root());
        }
    }

    std::string migrations_dir() const { return (fs::path(root) / "migrations").string(); }

    // The sibling `ui/` directory `from_migrations` resolves against the migrations path. Uses
    // weakly_canonical before parent_path, matching src/lua_runner.cpp's resolve_sandboxed_path
    // idiom -- a raw parent_path() misresolves a trailing-slash or bare-relative migrations path
    // (CONTEXT.md "Two resolution traps").
    std::string ui_dir() const { return (fs::weakly_canonical(migrations_dir()).parent_path() / "ui").string(); }

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
        return quiver::Database::from_migrations((fs::path(root) / "study.db").string(),
                                                 migrations_dir(),
                                                 {.read_only = false, .console_level = quiver::LogLevel::Off});
    }

    // A sibling of `root`, never a subdirectory of it -- copying `root` into its own subdirectory
    // would recurse into itself.
    std::string mirror_root() const {
        return (fs::path(root).parent_path() / "quiver_ui_metadata_test_mirror").string();
    }

    // Copies the whole temp tree (migrations/ + ui/, if any) to a sibling directory, deletes that
    // copy's `ui/` sibling, and opens a fresh database there -- the "same tree with the sidecar
    // removed" comparison baseline (plan action text), built by copy-then-remove rather than by
    // mutating the tree under an already-open handle. Call this before open_tree() in the same
    // test so no live sqlite file is mid-copy.
    quiver::Database open_ui_free_mirror() {
        const fs::path mirror = mirror_root();
        if (fs::exists(mirror)) {
            fs::remove_all(mirror);
        }
        fs::copy(root, mirror, fs::copy_options::recursive);
        const fs::path mirror_migrations = mirror / "migrations";
        const fs::path mirror_ui = fs::weakly_canonical(mirror_migrations).parent_path() / "ui";
        if (fs::exists(mirror_ui)) {
            fs::remove_all(mirror_ui);
        }
        return quiver::Database::from_migrations((mirror / "mirror_study.db").string(),
                                                 mirror_migrations.string(),
                                                 {.read_only = false, .console_level = quiver::LogLevel::Off});
    }

    std::string root;
};

// Loader-facing gtest suite name -- reserved for the sidecar-parsing tests of a later plan in
// this phase (see 01-VALIDATION.md's gtest filters).
class UiMetadataTest : public UiTempTreeFixture {};

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
        if (line.starts_with("    - ")) {
            lines.push_back(line);
        }
    }
    return lines;
}

// One collection's section of a full describe() report, delimited by its own "Collection: <name>"
// line and the next one (or end of string). describe()'s first two lines ("Database: <path>" /
// "Version: N") carry the db's own path, which legitimately differs between a main tree and its
// ui-free mirror (different temp directories) -- comparing only the collection section is what
// makes the SAFE-02 byte-identity assertions meaningful rather than failing on an irrelevant path.
std::string extract_collection_section(const std::string& describe_output, const std::string& collection) {
    auto pos = describe_output.find("Collection: " + collection);
    if (pos == std::string::npos) {
        return {};
    }
    auto end = describe_output.find("\nCollection: ", pos + 1);
    return end == std::string::npos ? describe_output.substr(pos) : describe_output.substr(pos, end - pos);
}

// Shared SAFE-02 assertion: a collection's describe()/describe_collection()/summarize_collection()
// output is byte-identical between `db` (some sidecar tree, possibly malformed or undescribed) and
// `mirror_db` (the same migrations tree with no ui/ sidecar at all).
void expect_reports_match(quiver::Database& db, quiver::Database& mirror_db, const std::string& collection) {
    EXPECT_EQ(extract_collection_section(db.describe(), collection),
              extract_collection_section(mirror_db.describe(), collection));
    EXPECT_EQ(db.describe_collection(collection), mirror_db.describe_collection(collection));
    EXPECT_EQ(db.summarize_collection(collection), mirror_db.summarize_collection(collection));
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

    const std::string expected_describe_line = "    - hm3_initial (REAL); label \"Initial Storage (hm³)\"\n";
    const std::string expected_describe_collection_line = "    - hm3_initial (REAL); label \"Initial Storage (hm³)\"; "
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
        EXPECT_TRUE(describe_collection_lines[i].starts_with(describe_lines[i]))
            << "describe line:            " << describe_lines[i] << "\n"
            << "describe_collection line: " << describe_collection_lines[i];
    }
}

// ============================================================================
// UiMetadataTest: loader-facing behavior, driven through the public Database API only (D-12)
// ============================================================================

// READ-03: keyed by the file's own top-level id and each [[attribute]]'s own id -- never the
// filename. The file below is named differently from both the collection and the attribute.
TEST_F(UiMetadataTest, LabelTooltipKeyedByFileIdAndAttributeId) {
    write_migration(1, reservoir_schema(), "DROP TABLE HydroPlant; DROP TABLE Configuration;");
    write_ui_file("plant_metadata.toml", R"(
id = "HydroPlant"

[[attribute]]
id = "reservoir_type"
label.en = "Reservoir Kind"
)");

    auto db = open_tree();
    auto describe_collection = db.describe_collection("HydroPlant");

    EXPECT_NE(describe_collection.find("    - reservoir_type (INTEGER); label \"Reservoir Kind\"\n"), std::string::npos)
        << describe_collection;
}

// READ-04: a localizable value is read either as a bare string or from a table's `en` sub-key.
TEST_F(UiMetadataTest, LocalizedStringOrTableEn) {
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
TEST_F(UiMetadataTest, LocalizedNewlineCollapse) {
    write_migration(1, reservoir_schema(), "DROP TABLE HydroPlant; DROP TABLE Configuration;");
    write_ui_file("hydro_plant.toml", R"(
id = "HydroPlant"

[[attribute]]
id = "hm3_initial"
label.en = "Mean\nProduction\nFactor"
)");

    auto db = open_tree();
    auto describe_collection = db.describe_collection("HydroPlant");

    EXPECT_NE(describe_collection.find("; label \"Mean Production Factor\""), std::string::npos) << describe_collection;
}

// READ-04/D-03: every C0 control byte (tab, CR, ESC, ...) is normalized to a space, not just the
// \r/\n/\t named in D-03's prose -- a deliberate superset that also neutralizes ESC.
TEST_F(UiMetadataTest, LocalizedControlCharacterCollapse) {
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
TEST_F(UiMetadataTest, LocalizedUtf8Passthrough) {
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
    EXPECT_NE(describe_collection.find("; tooltip \"Measured in °C\""), std::string::npos) << describe_collection;
}

// ============================================================================
// Task 1-01-02: enum.toml vocabularies and the enum clause (TDD)
// ============================================================================

// D-06/D-19: a gapped vocabulary ([0, 2]) renders its real codes verbatim, joined by the
// attribute's own `enum` value.
TEST_F(UiMetadataTest, EnumGappedCodesRenderVerbatim) {
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
TEST_F(UiMetadataTest, EnumOneBasedCodesRenderVerbatim) {
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
TEST_F(UiMetadataTest, EnumJoinedByEnumValueNotAttributeId) {
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
TEST_F(UiMetadataTest, EnumUnknownVocabularyRendersNoClause) {
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
TEST_F(UiMetadataTest, EnumEmptyVocabularyRendersNoClause) {
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
TEST_F(UiMetadataTest, EnumEntryMissingIdOrLabelIsDropped) {
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
TEST_F(UiMetadataTest, EnumEntriesRenderInAscendingCodeOrder) {
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
TEST_F(UiMetadataTest, EnumTopLevelKeyIsTheVocabularyName) {
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

// ============================================================================
// Task 1-02-01: path resolution (READ-01)
// ============================================================================

// A trailing separator on the migrations path resolves to the same ui/ sibling as the same path
// without one -- raw parent_path() would instead land on "<migrations>/ui", which never exists.
TEST_F(UiMetadataTest, PathResolutionTrailingSeparator) {
    write_migration(1, reservoir_schema(), "DROP TABLE HydroPlant; DROP TABLE Configuration;");
    write_ui_file("hydro_plant.toml", R"(
id = "HydroPlant"

[[attribute]]
id = "hm3_initial"
label.en = "Initial Storage"
)");

    auto db_no_slash = quiver::Database::from_migrations((fs::path(root) / "study_no_slash.db").string(),
                                                         migrations_dir(),
                                                         {.read_only = false, .console_level = quiver::LogLevel::Off});
    auto db_trailing_slash =
        quiver::Database::from_migrations((fs::path(root) / "study_trailing_slash.db").string(),
                                          migrations_dir() + "/",
                                          {.read_only = false, .console_level = quiver::LogLevel::Off});

    auto report_no_slash = db_no_slash.describe_collection("HydroPlant");
    auto report_trailing_slash = db_trailing_slash.describe_collection("HydroPlant");

    EXPECT_EQ(report_no_slash, report_trailing_slash);
    EXPECT_NE(report_no_slash.find(kLabelClauseOpener), std::string::npos) << report_no_slash;
}

// A bare relative migrations path finds the sibling ui/ next to it, never a ui/ under the process
// CWD -- raw parent_path() on a bare relative path yields "./ui" against whatever the CWD happens
// to be at call time.
TEST_F(UiMetadataTest, PathResolutionRelativeMigrationsPath) {
    write_migration(1, reservoir_schema(), "DROP TABLE HydroPlant; DROP TABLE Configuration;");
    write_ui_file("hydro_plant.toml", R"(
id = "HydroPlant"

[[attribute]]
id = "hm3_initial"
label.en = "Initial Storage"
)");

    auto expected = open_tree().describe_collection("HydroPlant");

    // Scope guard restores the process-wide CWD even if an assertion below fails (T-01-07): this
    // mutates global state every other test in the binary shares.
    const fs::path saved_cwd = fs::current_path();
    struct CwdGuard {
        fs::path saved;
        ~CwdGuard() { fs::current_path(saved); }
    } guard{saved_cwd};
    fs::current_path(root);

    auto db = quiver::Database::from_migrations(
        "relative_study.db", "migrations", {.read_only = false, .console_level = quiver::LogLevel::Off});
    auto actual = db.describe_collection("HydroPlant");

    EXPECT_EQ(actual, expected);
}

// The decoy at migrations/ui/ (a live sibling of every numbered version directory) is never read
// -- only the resolved sibling of the migrations path itself is. This is the exact misresolution
// raw parent_path() produces on a trailing-separator or relative path.
TEST_F(UiMetadataTest, PathResolutionNeverReadsUiUnderMigrations) {
    write_migration(1, reservoir_schema(), "DROP TABLE HydroPlant; DROP TABLE Configuration;");
    write_ui_file("hydro_plant.toml", R"(
id = "HydroPlant"

[[attribute]]
id = "hm3_initial"
label.en = "Sibling Label"
)");

    const fs::path decoy_dir = fs::path(migrations_dir()) / "ui";
    fs::create_directories(decoy_dir);
    std::ofstream decoy(decoy_dir / "hydro_plant.toml");
    decoy << R"(
id = "HydroPlant"

[[attribute]]
id = "hm3_initial"
label.en = "Decoy Label"
)";
    decoy.close();

    auto db = open_tree();
    auto describe_collection = db.describe_collection("HydroPlant");

    EXPECT_NE(describe_collection.find("; label \"Sibling Label\""), std::string::npos) << describe_collection;
    EXPECT_EQ(describe_collection.find("Decoy Label"), std::string::npos) << describe_collection;
}

// ============================================================================
// Task 1-02-01: shape selection (READ-02)
// ============================================================================

// main.toml (flat keys, no top-level id), a theme-shaped file (id but no attribute array), a plain
// text file, and a themes/ subdirectory (non-recursive scan) contribute nothing and never throw --
// even though the themes/ file would otherwise self-select as a collection file.
TEST_F(UiMetadataTest, ShapeSelectionIgnoresNonCollectionFiles) {
    write_migration(1, reservoir_schema(), "DROP TABLE HydroPlant; DROP TABLE Configuration;");
    write_ui_file("main.toml", R"(
model = "HydroThermalDispatch"
collections = ["HydroPlant"]
)");
    write_ui_file("theme.toml", R"(
id = "SomeTheme"
)");
    write_ui_file("notes.txt", "just a note, not toml at all");

    const fs::path themes_dir = fs::path(ui_dir()) / "themes";
    fs::create_directories(themes_dir);
    std::ofstream themed_file(themes_dir / "hydro_plant.toml");
    themed_file << R"(
id = "HydroPlant"

[[attribute]]
id = "hm3_initial"
label.en = "Should Never Render"
)";
    themed_file.close();

    auto mirror_db = open_ui_free_mirror();
    auto mirror_report = mirror_db.describe_collection("HydroPlant");

    std::optional<quiver::Database> db;
    EXPECT_NO_THROW(db.emplace(open_tree()));
    ASSERT_TRUE(db.has_value());
    auto report = db->describe_collection("HydroPlant");

    EXPECT_EQ(report, mirror_report);
    EXPECT_EQ(report.find("Should Never Render"), std::string::npos) << report;
}

// A collection file self-selects by its own top-level `id`, never its filename.
TEST_F(UiMetadataTest, ShapeSelectionUsesFileIdNotFilename) {
    write_migration(1, reservoir_schema(), "DROP TABLE HydroPlant; DROP TABLE Configuration;");
    write_ui_file("dc_line.toml", R"(
id = "HydroPlant"

[[attribute]]
id = "hm3_initial"
label.en = "Initial Storage"
)");

    auto db = open_tree();
    auto describe_collection = db.describe_collection("HydroPlant");

    EXPECT_NE(describe_collection.find("; label \"Initial Storage\""), std::string::npos) << describe_collection;
}

// ============================================================================
// Task 1-02-01: undescribed cases (RENDER-03)
// ============================================================================

// A collection named by no ui/*.toml file at all (while ui/ itself exists and describes something
// else) renders every scalar exactly as a no-sidecar run does.
TEST_F(DatabaseUiMetadataTest, UndescribedCollectionRendersUnchanged) {
    write_migration(1, reservoir_schema(), "DROP TABLE HydroPlant; DROP TABLE Configuration;");
    write_ui_file("enum.toml", R"(
[[bool]]
id = 0
label.en = "No"
)");

    auto mirror_report = open_ui_free_mirror().describe_collection("HydroPlant");
    auto report = open_tree().describe_collection("HydroPlant");

    EXPECT_EQ(report, mirror_report);
}

// An attribute absent from its collection's [[attribute]] array renders exactly as a no-sidecar
// run, while a sibling attribute present in the same file still renders its clause.
TEST_F(DatabaseUiMetadataTest, UndescribedAttributeRendersUnchanged) {
    write_migration(1, reservoir_schema(), "DROP TABLE HydroPlant; DROP TABLE Configuration;");
    write_ui_file("hydro_plant.toml", R"(
id = "HydroPlant"

[[attribute]]
id = "hm3_initial"
label.en = "Initial Storage"
)");
    // discount_rate has no [[attribute]] entry in this file.

    auto mirror_lines = extract_scalar_lines(open_ui_free_mirror().describe_collection("HydroPlant"));
    auto lines = extract_scalar_lines(open_tree().describe_collection("HydroPlant"));

    ASSERT_EQ(lines.size(), mirror_lines.size());
    bool checked_discount_rate = false;
    bool checked_hm3_initial = false;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (lines[i].starts_with("    - discount_rate ")) {
            EXPECT_EQ(lines[i], mirror_lines[i]) << lines[i];
            checked_discount_rate = true;
        }
        if (lines[i].starts_with("    - hm3_initial ")) {
            EXPECT_NE(lines[i], mirror_lines[i]) << "sibling in the same file should still render its clause";
            EXPECT_NE(lines[i].find(kLabelClauseOpener), std::string::npos) << lines[i];
            checked_hm3_initial = true;
        }
    }
    EXPECT_TRUE(checked_discount_rate);
    EXPECT_TRUE(checked_hm3_initial);
}

// A ui entry naming a column the schema does not have changes nothing and warns nothing.
TEST_F(DatabaseUiMetadataTest, UndescribedDanglingUiColumnRendersUnchanged) {
    write_migration(1, reservoir_schema(), "DROP TABLE HydroPlant; DROP TABLE Configuration;");
    write_ui_file("hydro_plant.toml", R"(
id = "HydroPlant"

[[attribute]]
id = "does_not_exist_column"
label.en = "Ghost Column"
)");

    auto mirror_report = open_ui_free_mirror().describe_collection("HydroPlant");
    auto report = open_tree().describe_collection("HydroPlant");

    EXPECT_EQ(report, mirror_report);
}

// D-20: an attribute carrying hide = true still renders its clauses -- describe describes the
// schema, not the UI.
TEST_F(DatabaseUiMetadataTest, HiddenAttributeStillRenders) {
    write_migration(1, reservoir_schema(), "DROP TABLE HydroPlant; DROP TABLE Configuration;");
    write_ui_file("hydro_plant.toml", R"(
id = "HydroPlant"

[[attribute]]
id = "hm3_initial"
label.en = "Initial Storage"
hide = true
)");

    auto db = open_tree();
    auto describe_collection = db.describe_collection("HydroPlant");

    EXPECT_NE(describe_collection.find("; label \"Initial Storage\""), std::string::npos) << describe_collection;
}

// ============================================================================
// Task 1-02-01: malformed-sidecar degradation (SAFE-02)
// ============================================================================

// An empty ui/ directory (present, but holding no files at all) opens successfully and renders
// exactly as a no-sidecar run.
TEST_F(DatabaseUiMetadataTest, MalformedEmptyUiDirRendersIdentical) {
    write_migration(1, reservoir_schema(), "DROP TABLE HydroPlant; DROP TABLE Configuration;");
    fs::create_directories(ui_dir());

    auto mirror_db = open_ui_free_mirror();

    std::optional<quiver::Database> db;
    EXPECT_NO_THROW(db.emplace(open_tree()));
    ASSERT_TRUE(db.has_value());

    expect_reports_match(*db, mirror_db, "HydroPlant");
}

// A zero-byte enum.toml opens successfully and renders exactly as a no-sidecar run.
TEST_F(DatabaseUiMetadataTest, MalformedZeroByteEnumRendersIdentical) {
    write_migration(1, reservoir_schema(), "DROP TABLE HydroPlant; DROP TABLE Configuration;");
    write_ui_file("enum.toml", "");

    auto mirror_db = open_ui_free_mirror();

    std::optional<quiver::Database> db;
    EXPECT_NO_THROW(db.emplace(open_tree()));
    ASSERT_TRUE(db.has_value());

    expect_reports_match(*db, mirror_db, "HydroPlant");
}

// A syntactically invalid .toml collection file opens successfully (the inner per-file catch
// warns and degrades) and renders exactly as a no-sidecar run.
TEST_F(DatabaseUiMetadataTest, MalformedInvalidSyntaxRendersIdentical) {
    write_migration(1, reservoir_schema(), "DROP TABLE HydroPlant; DROP TABLE Configuration;");
    write_ui_file("hydro_plant.toml", "this is not valid toml {{{");

    auto mirror_db = open_ui_free_mirror();

    std::optional<quiver::Database> db;
    EXPECT_NO_THROW(db.emplace(open_tree()));
    ASSERT_TRUE(db.has_value());

    expect_reports_match(*db, mirror_db, "HydroPlant");
}

// A collection file whose `attribute` value is a string rather than an array fails the shape gate
// (never a filename, never a fixed key) and renders exactly as a no-sidecar run.
TEST_F(DatabaseUiMetadataTest, MalformedWrongTypeAttributeRendersIdentical) {
    write_migration(1, reservoir_schema(), "DROP TABLE HydroPlant; DROP TABLE Configuration;");
    write_ui_file("hydro_plant.toml", R"(
id = "HydroPlant"
attribute = "not_an_array"
)");

    auto mirror_db = open_ui_free_mirror();

    std::optional<quiver::Database> db;
    EXPECT_NO_THROW(db.emplace(open_tree()));
    ASSERT_TRUE(db.has_value());

    expect_reports_match(*db, mirror_db, "HydroPlant");
}

// D-09: two collection files, one unparseable -- the good collection's clauses still render, and
// nothing throws. This is what the inner per-file catch buys over a single outer catch.
TEST_F(UiMetadataTest, MalformedOneFileKeepsOtherCollections) {
    write_migration(1,
                    reservoir_schema() + R"(
CREATE TABLE ThermalPlant (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    label TEXT UNIQUE NOT NULL,
    capacity_mw REAL
) STRICT;
)",
                    "DROP TABLE ThermalPlant; DROP TABLE HydroPlant; DROP TABLE Configuration;");

    write_ui_file("hydro_plant.toml", "this is not valid toml {{{");
    write_ui_file("thermal_plant.toml", R"TOML(
id = "ThermalPlant"

[[attribute]]
id = "capacity_mw"
label.en = "Installed Capacity (MW)"
)TOML");

    std::optional<quiver::Database> db;
    EXPECT_NO_THROW(db.emplace(open_tree()));
    ASSERT_TRUE(db.has_value());

    auto thermal_report = db->describe_collection("ThermalPlant");
    EXPECT_NE(thermal_report.find("; label \"Installed Capacity (MW)\""), std::string::npos) << thermal_report;

    EXPECT_NO_THROW(db->describe_collection("HydroPlant"));
}

// ============================================================================
// Plan 02-01: summarize_collection's histogram annotates observed codes with enum labels
// ============================================================================

// D2-01/D2-02/D2-03/D2-04/D2-05/D2-07: the label rides on the key (`code SP "Label": count`), an
// uncovered code (1) stays bare, and an unobserved vocabulary code (2) never appears at all.
TEST_F(DatabaseUiMetadataTest, SummarizeHistogramAnnotatesCodesWithEnumLabels) {
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
    db.create_element(
        "HydroPlant",
        quiver::Element().set("label", std::string("a")).set("initial_volume_type", static_cast<int64_t>(0)));
    db.create_element(
        "HydroPlant",
        quiver::Element().set("label", std::string("b")).set("initial_volume_type", static_cast<int64_t>(0)));
    db.create_element(
        "HydroPlant",
        quiver::Element().set("label", std::string("c")).set("initial_volume_type", static_cast<int64_t>(1)));

    auto report = db.summarize_collection("HydroPlant");

    EXPECT_TRUE(report.find(R"(values {0 "Per Unit": 2, 1: 1})") != std::string::npos) << report;
    EXPECT_FALSE(report.find("\"Volume\"") != std::string::npos) << report;
}

// D-09: a label that normalizes to empty (here, all-whitespace) drops only the annotation and
// keeps the histogram entry -- deliberate divergence from D-06's `enum {}` clause, where an
// empty-normalizing label drops the whole vocabulary entry.
TEST_F(DatabaseUiMetadataTest, SummarizeHistogramKeepsEntryWhenLabelNormalizesToEmpty) {
    write_migration(1, reservoir_schema(), "DROP TABLE HydroPlant; DROP TABLE Configuration;");
    write_ui_file("enum.toml", R"(
[[initial_volume_type]]
id = 0
label.en = "   "
)");
    write_ui_file("hydro_plant.toml", R"(
id = "HydroPlant"

[[attribute]]
id = "initial_volume_type"
enum = "initial_volume_type"
)");

    auto db = open_tree();
    db.create_element(
        "HydroPlant",
        quiver::Element().set("label", std::string("a")).set("initial_volume_type", static_cast<int64_t>(0)));

    auto report = db.summarize_collection("HydroPlant");

    // The entry survives with a bare code -- only the annotation is dropped.
    EXPECT_NE(report.find(R"(values {0: 1})"), std::string::npos) << report;
}
