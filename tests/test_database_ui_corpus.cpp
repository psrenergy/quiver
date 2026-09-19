#include "test_ui_fixture.h"
#include "test_utils.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <quiver/database.h>
#include <string>
#include <utility>
#include <vector>

// DatabaseUiCorpus is the fixture corpus's own guard suite: it walks tests/schemas/ui/ so a
// half-written fixture directory fails loudly the moment it lands, cross-checks
// tests/schemas/ui/README.md against the directory listing and against 01-02-PLAN.md's
// authoritative fixture-literal table, and proves no fixture file has been copied into any
// binding (CORPUS-03).

namespace {
namespace fs = std::filesystem;

fs::path ui_corpus_dir() {
    return fs::path(SCHEMA_PATH("schemas/ui"));
}

// Sorted so the reported fixture set is platform-stable (directory_iterator order is not
// guaranteed to match across filesystems/platforms).
std::vector<std::string> sorted_fixture_dir_names() {
    std::vector<std::string> names;
    for (const auto& entry : fs::directory_iterator(ui_corpus_dir())) {
        if (entry.is_directory()) {
            names.push_back(entry.path().filename().string());
        }
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::string read_binary(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

}  // namespace

// Every directory under tests/schemas/ui/ -- except no_ui_dir (deliberately has no ui/ at all)
// and no_main_toml (deliberately has a ui/ with no main.toml inside it, WR-01) -- must hold
// schema.sql and ui/main.toml. Fails naming the specific directory that is broken.
TEST(DatabaseUiCorpus, EveryFixtureIsStructurallyComplete) {
    const auto ui_dir = ui_corpus_dir();
    for (const auto& name : sorted_fixture_dir_names()) {
        if (name == "no_ui_dir" || name == "no_main_toml") {
            continue;
        }
        const auto dir = ui_dir / name;
        EXPECT_TRUE(fs::exists(dir / "schema.sql")) << "missing schema.sql in fixture '" << name << "'";
        EXPECT_TRUE(fs::exists(dir / "ui" / "main.toml")) << "missing ui/main.toml in fixture '" << name << "'";
    }
}

// Every fixture's SQL compiles and passes SchemaValidator, and no fixture -- including the
// deliberately malformed one -- can make describe() throw out of a report (T-01-06).
TEST(DatabaseUiCorpus, EveryFixtureSchemaLoads) {
    for (const auto& name : sorted_fixture_dir_names()) {
        auto db = quiver::test::open_ui_fixture(__FILE__, name, "cpp_corpus_" + name);
        EXPECT_FALSE(db.describe().empty()) << "describe() returned empty report for fixture '" << name << "'";
    }
}

// Keeps README.md from rotting as fixtures are added in later phases: every directory name the
// walk finds must appear somewhere in the README text.
TEST(DatabaseUiCorpus, ReadmeNamesEveryFixture) {
    const auto readme = read_binary(ui_corpus_dir() / "README.md");
    for (const auto& name : sorted_fixture_dir_names()) {
        EXPECT_NE(readme.find(name), std::string::npos) << "README.md does not mention fixture '" << name << "'";
    }
}

// The drift gate for 01-02-PLAN.md's "## Fixture literals (authoritative)" table: every
// source-side literal that table names must exist, byte-for-byte, in its fixture file AND in
// tests/schemas/ui/README.md's verbatim mirror of the table. An edit that renames or drops a
// literal fails here, naming the file, instead of silently un-asserting the downstream plans
// (01-03/01-04/01-05/01-06) that quote it.
TEST(DatabaseUiCorpus, FixtureLiteralsArePinned) {
    static const std::vector<std::pair<std::string, std::string>> literals = {
        {"foresight_like/ui/enum.toml", "Seasonal Na\xC3\xAFve"},
        {"foresight_like/ui/enum.toml", "Regresi\xC3\xB3n Lineal"},
        {"foresight_like/ui/economic_driver.toml", "enum = \"model\""},
        {"bess_like/ui/storage.toml", "Degradation Rate"},
        {"bess_like/ui/storage.toml", "Degradation Curve"},
        {"bess_like/ui/enum.toml", "9223372036854775807"},
        {"htd_like/ui/hydro_plant.toml", "Measurement Date"},
        {"enum_basic/ui/enum.toml", "Disabled"},
        {"enum_basic/ui/enum.toml", "Enabled"},
        {"htd_like/ui/enum.toml", "Disable"},
        {"htd_like/ui/enum.toml", "Enable"},
    };

    const auto readme = read_binary(ui_corpus_dir() / "README.md");

    for (const auto& [relative_path, literal] : literals) {
        const auto content = read_binary(ui_corpus_dir() / relative_path);
        EXPECT_NE(content.find(literal), std::string::npos)
            << "literal '" << literal << "' missing from tests/schemas/ui/" << relative_path;
        EXPECT_NE(readme.find(literal), std::string::npos)
            << "literal '" << literal << "' missing from tests/schemas/ui/README.md";
    }
}

// CORPUS-03: the fixtures are referenced by every binding suite, never copied into one. Walk
// bindings/ and fail if any main.toml or enum.toml sits inside a directory literally named "ui".
TEST(DatabaseUiCorpus, FixturesAreNeverCopiedIntoABinding) {
    const auto bindings_dir = fs::path(quiver::test::path_from(__FILE__, "../bindings"));
    ASSERT_TRUE(fs::exists(bindings_dir)) << bindings_dir.string();

    for (auto it = fs::recursive_directory_iterator(bindings_dir, fs::directory_options::skip_permission_denied);
         it != fs::recursive_directory_iterator();
         ++it) {
        const auto& entry = *it;
        if (entry.is_directory()) {
            const auto dirname = entry.path().filename().string();
            if (dirname == "build" || dirname == ".dart_tool" || dirname == "node_modules" || dirname == "target") {
                it.disable_recursion_pending();
            }
            continue;
        }

        if (!entry.is_regular_file()) {
            continue;
        }

        const auto filename = entry.path().filename().string();
        if (filename != "main.toml" && filename != "enum.toml") {
            continue;
        }

        const auto parent_name = entry.path().parent_path().filename().string();
        EXPECT_NE(parent_name, "ui") << "UI fixture file copied into a binding: " << entry.path().string();
    }
}
