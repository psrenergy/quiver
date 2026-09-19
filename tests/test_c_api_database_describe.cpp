#include "test_utils.h"

#include <gtest/gtest.h>
#include <quiver/c/database.h>
#include <quiver/c/element.h>
#include <string>

// Proves the enum/unit/hidden/label/header rendering built in plans 01-01..01-04 crosses the
// C API's `char**` marshalling boundary byte-for-byte (DESC-07). Every database here is built
// through the C API itself, with the sqlite file written inside the shared
// tests/schemas/ui/<fixture>/ directory (D-28: the `ui/` sidecar must sit beside the db file) --
// never through tests/test_ui_fixture.h, which returns a C++ quiver::Database, not a C handle.
// Every literal asserted below is quoted verbatim from tests/schemas/ui/README.md's
// `## Rendered literals` table (L1, L3, L4, L5, L6, L9, L10).

namespace {

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

// The em dash (U+2014) as explicit UTF-8 bytes -- mirrors src/database_describe.cpp's kEmDash
// and tests/test_database_ui_describe.cpp's kEmDash, so this file never risks the literal
// character being mangled by a compiler's default source charset (no /utf-8 flag for MSVC).
const std::string kEmDash = "\xE2\x80\x94";

// Fixture directory under tests/schemas/ui/<name>/, per D-28: the sqlite file is written inside
// this same directory so its `ui/` sidecar sibling is found.
std::string fixture_dir(const std::string& name) {
    return quiver::test::path_from(__FILE__, "schemas/ui/" + name);
}

}  // namespace

TEST(DatabaseCApiDescribe, DeclaredVocabularyListCrossesTheBoundary) {
    const auto dir = fixture_dir("enum_basic");
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema((dir + "/capi_declared.sqlite").c_str(), (dir + "/schema.sql").c_str(),
                                          &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    // Zero elements created -- the declared list can only have come from the vocabulary, never
    // from data.
    char* report = nullptr;
    ASSERT_EQ(quiver_database_describe_collection(db, "Storage", &report), QUIVER_OK);
    ASSERT_NE(report, nullptr);
    EXPECT_TRUE(contains(report, "enum bool {0: Disabled, 1: Enabled}")) << report;

    quiver_database_free_string(report);
    quiver_database_close(db);
}

TEST(DatabaseCApiDescribe, HistogramLabelsCrossTheBoundary) {
    const auto dir = fixture_dir("enum_basic");
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema((dir + "/capi_histogram.sqlite").c_str(), (dir + "/schema.sql").c_str(),
                                          &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    for (int i = 0; i < 8; ++i) {
        quiver_element_t* element = nullptr;
        ASSERT_EQ(quiver_element_create(&element), QUIVER_OK);
        quiver_element_set_string(element, "label", ("disabled_" + std::to_string(i)).c_str());
        quiver_element_set_integer(element, "has_commitment", 0);
        int64_t id = 0;
        EXPECT_EQ(quiver_database_create_element(db, "Storage", element, &id), QUIVER_OK);
        quiver_element_destroy(element);
    }
    for (int i = 0; i < 4; ++i) {
        quiver_element_t* element = nullptr;
        ASSERT_EQ(quiver_element_create(&element), QUIVER_OK);
        quiver_element_set_string(element, "label", ("enabled_" + std::to_string(i)).c_str());
        quiver_element_set_integer(element, "has_commitment", 1);
        int64_t id = 0;
        EXPECT_EQ(quiver_database_create_element(db, "Storage", element, &id), QUIVER_OK);
        quiver_element_destroy(element);
    }

    char* report = nullptr;
    ASSERT_EQ(quiver_database_summarize_collection(db, "Storage", &report), QUIVER_OK);
    ASSERT_NE(report, nullptr);
    EXPECT_TRUE(contains(report, "values {0: 8 (Disabled), 1: 4 (Enabled)}")) << report;

    quiver_database_free_string(report);
    quiver_database_close(db);
}

TEST(DatabaseCApiDescribe, UnitHiddenAndLabelCrossTheBoundary) {
    const auto dir = fixture_dir("enum_basic");
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema((dir + "/capi_unit_hidden.sqlite").c_str(), (dir + "/schema.sql").c_str(),
                                          &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    char* report = nullptr;
    ASSERT_EQ(quiver_database_describe_collection(db, "Storage", &report), QUIVER_OK);
    ASSERT_NE(report, nullptr);
    EXPECT_TRUE(contains(report, "- max_generation (REAL) [MW] " + kEmDash + " \"Maximum Generation\"")) << report;
    EXPECT_TRUE(contains(report, "- internal_code (INTEGER) [hidden] " + kEmDash + " \"Internal Code\"")) << report;

    quiver_database_free_string(report);
    quiver_database_close(db);
}

TEST(DatabaseCApiDescribe, HeaderLineCrossesTheBoundary) {
    const auto dir = fixture_dir("enum_basic");
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema((dir + "/capi_header.sqlite").c_str(), (dir + "/schema.sql").c_str(),
                                          &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    char* report = nullptr;
    ASSERT_EQ(quiver_database_describe(db, &report), QUIVER_OK);
    ASSERT_NE(report, nullptr);
    EXPECT_TRUE(contains(report, "UI config: ")) << report;
    EXPECT_TRUE(contains(report, " (locale: en)")) << report;

    quiver_database_free_string(report);
    quiver_database_close(db);
}

// The assertion the C++ core suite cannot substitute for: the only place a truncating or
// re-encoding `char**` bug would surface. Both labels are literal L9 and L10 from
// tests/schemas/ui/README.md, resolved at locale "en" (L9 via its own label.en, L10 via the
// first-key-in-map-order fallback leg) -- neither depends on a locale Phase 1 never resolves.
TEST(DatabaseCApiDescribe, NonAsciiLabelBytesSurviveMarshalling) {
    const auto dir = fixture_dir("foresight_like");
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema((dir + "/capi_unicode.sqlite").c_str(), (dir + "/schema.sql").c_str(),
                                          &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    char* report = nullptr;
    ASSERT_EQ(quiver_database_describe_collection(db, "EconomicDriver", &report), QUIVER_OK);
    ASSERT_NE(report, nullptr);
    EXPECT_TRUE(contains(report, "Seasonal Na\xC3\xAFve")) << report;
    EXPECT_TRUE(contains(report, "Regresi\xC3\xB3n Lineal")) << report;

    quiver_database_free_string(report);
    quiver_database_close(db);
}

TEST(DatabaseCApiDescribe, NoHeaderWithoutSidecar) {
    const auto dir = fixture_dir("no_ui_dir");
    auto options = quiver::test::quiet_options();
    quiver_database_t* db = nullptr;
    ASSERT_EQ(quiver_database_from_schema((dir + "/capi_no_sidecar.sqlite").c_str(), (dir + "/schema.sql").c_str(),
                                          &options, &db),
              QUIVER_OK);
    ASSERT_NE(db, nullptr);

    char* describe_report = nullptr;
    ASSERT_EQ(quiver_database_describe(db, &describe_report), QUIVER_OK);
    ASSERT_NE(describe_report, nullptr);
    EXPECT_FALSE(contains(describe_report, "UI config: ")) << describe_report;
    quiver_database_free_string(describe_report);

    char* collection_report = nullptr;
    ASSERT_EQ(quiver_database_describe_collection(db, "Storage", &collection_report), QUIVER_OK);
    ASSERT_NE(collection_report, nullptr);
    EXPECT_FALSE(contains(collection_report, "UI config: ")) << collection_report;
    quiver_database_free_string(collection_report);

    char* summarize_report = nullptr;
    ASSERT_EQ(quiver_database_summarize_collection(db, "Storage", &summarize_report), QUIVER_OK);
    ASSERT_NE(summarize_report, nullptr);
    EXPECT_FALSE(contains(summarize_report, "UI config: ")) << summarize_report;
    quiver_database_free_string(summarize_report);

    quiver_database_close(db);
}
