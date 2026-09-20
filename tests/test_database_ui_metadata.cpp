#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <quiver/database.h>

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
