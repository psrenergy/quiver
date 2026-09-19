#include "test_utils.h"

#include <filesystem>
#include <gtest/gtest.h>
#include <quiver/database.h>
#include <string>

// OPT-01/OPT-02/SAFE-01 (Phase 2, plan 02-01): DatabaseOptions::ui_config_dir/ui_locale threaded
// through the one C++ constructor (Database::Database) and the one lazy loader
// (Impl::require_ui_config). Reuses the foresight_like fixture from Phase 1 (its enum.toml is the
// corpus's only locale-varying data) rather than authoring a new fixture tree -- a database file
// placed in a scratch directory with no `ui/` sibling, plus an explicit ui_config_dir pointing
// into foresight_like/ui, is itself the "config dir anywhere on disk" proof.

namespace {

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

std::string foresight_schema_path() {
    return quiver::test::path_from(__FILE__, "schemas/ui/foresight_like/schema.sql");
}

std::string foresight_ui_dir() {
    return quiver::test::path_from(__FILE__, "schemas/ui/foresight_like/ui");
}

std::string foresight_fixture_dir() {
    return quiver::test::path_from(__FILE__, "schemas/ui/foresight_like");
}

// A fresh directory with no `ui/` sibling of its own -- the negative space the explicit-override
// proof needs. Removed in the destructor; unlike ScratchSidecarDir (test_database_ui_describe.cpp)
// this file never writes its own sidecar, it only needs a database-file location that the
// convention path cannot resolve.
class ScratchDbDir {
public:
    explicit ScratchDbDir(const std::string& name) : dir_(name) {
        std::filesystem::remove_all(dir_);
        std::filesystem::create_directories(dir_);
    }
    ~ScratchDbDir() { std::filesystem::remove_all(dir_); }

    ScratchDbDir(const ScratchDbDir&) = delete;
    ScratchDbDir& operator=(const ScratchDbDir&) = delete;

    std::string db_path(const std::string& stem) const { return (dir_ / (stem + ".sqlite")).string(); }

private:
    std::filesystem::path dir_;
};

}  // namespace

// D-01/OPT-01: a file-backed database in a directory with NO `ui/` sibling still loads the
// sidecar when the caller names the directory explicitly.
TEST(DatabaseUiOptions, ExplicitConfigDirOverridesConvention) {
    ScratchDbDir scratch("cpp_options_override_scratch");
    auto db = quiver::Database::from_schema(
        scratch.db_path("db"),
        foresight_schema_path(),
        {.read_only = false, .console_level = quiver::LogLevel::Off, .ui_config_dir = foresight_ui_dir()});

    EXPECT_TRUE(db.has_ui_config());
    const auto report = db.describe_collection("EconomicDriver");
    EXPECT_TRUE(contains(report, "Seasonal Na\xC3\xAFve")) << report;
}

// D-01: an explicit config directory loads even for a `:memory:` database -- the caller named it
// outright, so the `:memory:` short-circuit (which guards only the convention path) does not
// apply.
TEST(DatabaseUiOptions, ExplicitConfigDirLoadsEvenForMemoryDatabase) {
    auto with_override = quiver::Database::from_schema(
        ":memory:",
        foresight_schema_path(),
        {.read_only = false, .console_level = quiver::LogLevel::Off, .ui_config_dir = foresight_ui_dir()});
    EXPECT_TRUE(with_override.has_ui_config());

    auto without_override = quiver::Database::from_schema(
        ":memory:", foresight_schema_path(), {.read_only = false, .console_level = quiver::LogLevel::Off});
    EXPECT_FALSE(without_override.has_ui_config());
}

// EDGE/adjacency: an explicit ui_config_dir naming exactly <db_dir>/ui/ takes the override branch
// but loads a config identical to the convention-path result -- the two are mutually exclusive
// branches of one `if`, never merged, never double-loaded. The report's first line renders the
// sidecar's own source_directory string verbatim (database_describe.cpp's "UI config: <path>"
// header), which differs only in path separator style between the two ways of naming the same
// directory (native-separator via std::filesystem's parent_path()/"ui" vs. the caller's own
// string) -- not a semantic difference, so it is stripped before comparing, mirroring
// DirectoryWithoutMainTomlLogsAtWarn's "Database: <path>" precedent in test_database_ui_describe.cpp.
TEST(DatabaseUiOptions, ExplicitConfigDirEqualToConventionPathIsIdentical) {
    auto convention_db = quiver::Database::from_schema(foresight_fixture_dir() + "/cpp_options_convention.sqlite",
                                                        foresight_schema_path(),
                                                        {.read_only = false, .console_level = quiver::LogLevel::Off});
    auto explicit_db = quiver::Database::from_schema(
        foresight_fixture_dir() + "/cpp_options_explicit_equal.sqlite",
        foresight_schema_path(),
        {.read_only = false, .console_level = quiver::LogLevel::Off, .ui_config_dir = foresight_ui_dir()});

    EXPECT_TRUE(convention_db.has_ui_config());
    EXPECT_TRUE(explicit_db.has_ui_config());

    const auto convention_report = convention_db.describe_collection("EconomicDriver");
    const auto explicit_report = explicit_db.describe_collection("EconomicDriver");
    const auto convention_rest = convention_report.substr(convention_report.find('\n') + 1);
    const auto explicit_rest = explicit_report.substr(explicit_report.find('\n') + 1);
    EXPECT_EQ(convention_rest, explicit_rest);
}

// D-03/D-04: a NULL/empty ui_config_dir means "convention path"; a NULL/empty ui_locale means
// "en". Proven at the C++ layer with explicit empty strings (the C API's NULL guard is Task 2).
TEST(DatabaseUiOptions, EmptyStringConfigDirAndLocaleBehaveAsUnset) {
    auto db = quiver::Database::from_schema(
        foresight_fixture_dir() + "/cpp_options_empty_unset.sqlite",
        foresight_schema_path(),
        {.read_only = false, .console_level = quiver::LogLevel::Off, .ui_config_dir = "", .ui_locale = ""});

    EXPECT_TRUE(db.has_ui_config());
    const auto report = db.describe_collection("EconomicDriver");
    EXPECT_TRUE(contains(report, "Seasonal Na\xC3\xAFve")) << report;
}

// OPT-02/D-06/D-17: `ui_locale = "es"` renders the Spanish labels from foresight_like/ui/enum.toml
// (new literals L18/L19) and not their `en` counterparts. This is the load-bearing proof that
// parse_enum_content's locale threading actually works -- economic_driver.toml's own labels are
// bare strings and would render identically regardless of locale.
TEST(DatabaseUiOptions, LocaleAffectsRenderedLabel) {
    auto db = quiver::Database::from_schema(
        foresight_fixture_dir() + "/cpp_options_locale_es.sqlite",
        foresight_schema_path(),
        {.read_only = false, .console_level = quiver::LogLevel::Off, .ui_locale = "es"});

    const auto report = db.describe_collection("EconomicDriver");
    EXPECT_TRUE(contains(report, "Ingenuo Estacional")) << report;
    EXPECT_TRUE(contains(report, "Tendencia Lineal Local")) << report;
    EXPECT_FALSE(contains(report, "Seasonal Na\xC3\xAFve")) << report;
    EXPECT_FALSE(contains(report, "Local Linear Trend")) << report;
}

// The `en` mirror of LocaleAffectsRenderedLabel: no ui_locale given, the Phase 1 default is
// preserved.
TEST(DatabaseUiOptions, DefaultLocaleIsEnglish) {
    auto db = quiver::Database::from_schema(foresight_fixture_dir() + "/cpp_options_locale_default.sqlite",
                                            foresight_schema_path(),
                                            {.read_only = false, .console_level = quiver::LogLevel::Off});

    const auto report = db.describe_collection("EconomicDriver");
    EXPECT_TRUE(contains(report, "Seasonal Na\xC3\xAFve")) << report;
    EXPECT_TRUE(contains(report, "Local Linear Trend")) << report;
    EXPECT_FALSE(contains(report, "Ingenuo Estacional")) << report;
    EXPECT_FALSE(contains(report, "Tendencia Lineal Local")) << report;
}

// D-05: an explicit directory that is absent logs at warn (a caller-typed path gone wrong); the
// convention path's absence logs at debug (the normal state for every non-PSR database). Neither
// throws and both report has_ui_config() false -- proven behaviorally via captured stderr rather
// than by inspecting spdlog internals.
TEST(DatabaseUiOptions, ExplicitConfigDirMissingLogsWarnNotDebug) {
    const auto missing_dir = quiver::test::path_from(__FILE__, "schemas/ui/foresight_like/does_not_exist");

    ScratchDbDir explicit_scratch("cpp_options_explicit_missing_scratch");
    testing::internal::CaptureStderr();
    auto explicit_db = quiver::Database::from_schema(
        explicit_scratch.db_path("db"),
        foresight_schema_path(),
        {.read_only = false, .console_level = quiver::LogLevel::Warn, .ui_config_dir = missing_dir});
    EXPECT_FALSE(explicit_db.has_ui_config());
    const auto explicit_output = testing::internal::GetCapturedStderr();
    EXPECT_NE(explicit_output.find(missing_dir), std::string::npos) << explicit_output;

    ScratchDbDir convention_scratch("cpp_options_convention_missing_scratch");
    testing::internal::CaptureStderr();
    auto convention_db = quiver::Database::from_schema(
        convention_scratch.db_path("db"),
        foresight_schema_path(),
        {.read_only = false, .console_level = quiver::LogLevel::Warn});
    EXPECT_FALSE(convention_db.has_ui_config());
    const auto convention_output = testing::internal::GetCapturedStderr();
    EXPECT_TRUE(convention_output.empty()) << convention_output;
}
