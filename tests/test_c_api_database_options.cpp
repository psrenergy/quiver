#include "test_utils.h"

#include <gtest/gtest.h>
#include <quiver/c/database.h>
#include <string>

// SAFE-01/OPT-04 (Phase 2, plan 02-01, extended 02-08): the four *_sizeof accessors and
// quiver_database_has_ui_config through the C boundary itself -- every database here is built
// through quiver_database_from_schema directly, never through the C++ test_ui_fixture.h helper
// (the Phase 1 precedent: a C API test must prove the C boundary, not the C++ helper behind it).
// 02-08 promoted the rule from "the three structs the gates check" to the general one: every C
// struct a binding hand-allocates a raw buffer for gets a *_sizeof accessor here, covering
// quiver_csv_options_t (bindings/js/src/csv.ts hand-allocates 56 bytes for it) as the fourth.

namespace {

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

std::string foresight_fixture_dir() {
    return quiver::test::path_from(__FILE__, "schemas/ui/foresight_like");
}

std::string foresight_schema_path() {
    return foresight_fixture_dir() + "/schema.sql";
}

std::string foresight_ui_dir() {
    return foresight_fixture_dir() + "/ui";
}

}  // namespace

// T-02-01: the accessor and the header must never diverge -- asserted against both the literal
// number every Wave-2 binding hardcodes AND the local `sizeof` of the struct declared in this
// translation unit.
TEST(DatabaseCApiOptions, SizeofAccessorsMatchNativeLayout) {
    EXPECT_EQ(quiver_database_options_sizeof(), 24u);
    EXPECT_EQ(quiver_database_options_sizeof(), sizeof(quiver_database_options_t));

    EXPECT_EQ(quiver_scalar_metadata_sizeof(), 56u);
    EXPECT_EQ(quiver_scalar_metadata_sizeof(), sizeof(quiver_scalar_metadata_t));

    EXPECT_EQ(quiver_group_metadata_sizeof(), 32u);
    EXPECT_EQ(quiver_group_metadata_sizeof(), sizeof(quiver_group_metadata_t));

    EXPECT_EQ(quiver_csv_options_sizeof(), 56u);
    EXPECT_EQ(quiver_csv_options_sizeof(), sizeof(quiver_csv_options_t));
}

TEST(DatabaseCApiOptions, HasUiConfigTrueWhenLoaded) {
    const auto ui_dir = foresight_ui_dir();
    const auto db_path = foresight_fixture_dir() + "/capi_options_loaded.sqlite";
    const auto schema_path = foresight_schema_path();

    auto options = quiver::test::quiet_options();
    options.ui_config_dir = ui_dir.c_str();

    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(db_path.c_str(), schema_path.c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    int has_config = 0;
    EXPECT_EQ(quiver_database_has_ui_config(db, &has_config), QUIVER_OK);
    EXPECT_EQ(has_config, 1);

    quiver_database_close(db);
}

TEST(DatabaseCApiOptions, HasUiConfigFalseWhenDirectoryAbsent) {
    auto options = quiver::test::quiet_options();

    quiver_database_t* db = nullptr;
    ASSERT_EQ(
        quiver_database_from_schema(":memory:", foresight_schema_path().c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    int has_config = 0;
    EXPECT_EQ(quiver_database_has_ui_config(db, &has_config), QUIVER_OK);
    EXPECT_EQ(has_config, 0);

    quiver_database_close(db);
}

// OPT-01/OPT-02: populate all four option fields through the C struct and assert the Spanish
// literal in the describe_collection output -- proof that ui_config_dir and ui_locale both cross
// the C ABI boundary, not merely the C++ layer underneath it.
TEST(DatabaseCApiOptions, ExplicitConfigDirAndLocaleCrossTheCBoundary) {
    const auto ui_dir = foresight_ui_dir();
    const auto db_path = foresight_fixture_dir() + "/capi_options_locale_es.sqlite";
    const auto schema_path = foresight_schema_path();

    quiver_database_options_t options = quiver_database_options_default();
    options.console_level = QUIVER_LOG_OFF;
    options.ui_config_dir = ui_dir.c_str();
    options.ui_locale = "es";

    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema(db_path.c_str(), schema_path.c_str(), &options, &db), QUIVER_OK);
    ASSERT_NE(db, nullptr);

    char* report = nullptr;
    ASSERT_EQ(quiver_database_describe_collection(db, "EconomicDriver", &report), QUIVER_OK);
    ASSERT_NE(report, nullptr);
    EXPECT_TRUE(contains(report, "Ingenuo Estacional")) << report;
    EXPECT_TRUE(contains(report, "Tendencia Lineal Local")) << report;

    quiver_database_free_string(report);
    quiver_database_close(db);
}
