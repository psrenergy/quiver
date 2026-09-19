#include "test_ui_fixture.h"
#include "test_utils.h"

#include <gtest/gtest.h>
#include <quiver/database.h>
#include <stdexcept>
#include <string>
#include <vector>

// PARSE-01, PARSE-05 through PARSE-10: the parser tolerances built by plans 01-01/01-02 exercised
// entirely through Database's public surface -- quiver_tests links no toml++ and has no include
// path into src/ (D-18). Every fixture used here was authored by plan 01-02 and is never mutated
// by this file; the exact bytes quoted come from tests/schemas/ui/README.md's
// `## Rendered literals` table.

namespace {

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
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

// The em dash (U+2014) as explicit UTF-8 bytes -- mirrors src/database_describe.cpp's kEmDash so
// this file never risks a literal character being mangled by a compiler's default source charset.
const std::string kEmDash = "\xE2\x80\x94";

}  // namespace

// ---------------------------------------------------------------------------
// Task 1: unknown keys, the `format` table form, absent/zero-byte enum.toml
// ---------------------------------------------------------------------------

// PARSE-05: unknown keys at main, collection and attribute level are ignored -- known keys in
// the same file (`label`, `unit`) still read correctly.
TEST(DatabaseUiParse, UnknownKeysIgnoredAndKnownKeysSurvive) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "unknown_keys", "cpp_unknown_keys");
    EXPECT_TRUE(db.has_ui_config());

    const auto report = db.describe_collection("Storage");
    EXPECT_TRUE(contains(report, "\"Storage Units\"")) << report;
    const std::string expected_capacity_line = "    - capacity (REAL) [MW] " + kEmDash + " \"Installed Capacity\"";
    EXPECT_TRUE(contains(report, expected_capacity_line)) << report;
}

// D-26: unknown keys are logged once per file, at debug, never once per key. Captured stderr for
// `unknown_keys` (whose storage.toml carries an unknown key at collection level and five at
// attribute level) names the collection file exactly once.
TEST(DatabaseUiParse, UnknownKeysLoggedOncePerFile) {
    const auto fixture_dir = quiver::test::path_from(__FILE__, "schemas/ui/unknown_keys");
    const auto schema_path = fixture_dir + "/schema.sql";
    const auto db_path = fixture_dir + "/cpp_unknown_keys_debug.sqlite";

    testing::internal::CaptureStderr();
    auto db = quiver::Database::from_schema(
        db_path, schema_path, {.read_only = false, .console_level = quiver::LogLevel::Debug});
    db.describe();
    const auto output = testing::internal::GetCapturedStderr();

    EXPECT_EQ(1u, count_occurrences(output, "storage.toml")) << output;
}

// PARSE-06/D-14: the `format` table form parses and the file survives it. No report renders
// `format` in Phase 1 (verbatim round-trip is Phase 3's META-03), so the only observable proof is
// that the attribute declared after the table-form attribute still renders its own label.
TEST(DatabaseUiParse, FormatTableFormDoesNotAbortTheFile) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "format_table", "cpp_format_table");
    EXPECT_TRUE(db.has_ui_config());

    const auto report = db.describe_collection("Storage");
    const std::string expected_efficiency_line = "    - efficiency (REAL) " + kEmDash + " \"Efficiency\"";
    EXPECT_TRUE(contains(report, expected_efficiency_line)) << report;
    // `code`'s format is table-form-with-only-`data`; it too must not abort the file.
    const std::string expected_code_line = "    - code (TEXT) " + kEmDash + " \"Code\"";
    EXPECT_TRUE(contains(report, expected_code_line)) << report;
}

// PARSE-06: a collection file with zero [[attribute]] blocks registers without error and renders
// today's unadorned scalar lines.
TEST(DatabaseUiParse, EmptyCollectionFileLoads) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "format_table", "cpp_empty_collection");
    EXPECT_TRUE(db.has_ui_config());

    const auto report = db.describe_collection("EmptyCollection");
    EXPECT_TRUE(contains(report, "\"Empty Collection\"")) << report;
    EXPECT_TRUE(contains(report, "  Scalars:\n")) << report;
}

// PARSE-09 (absent half): no enum.toml at all -- the attribute bound to a vocabulary nothing
// declares renders the undeclared marker, never a throw.
TEST(DatabaseUiParse, AbsentEnumFileTolerated) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "no_enum", "cpp_absent_enum");
    EXPECT_TRUE(db.has_ui_config());

    const auto report = db.describe_collection("Storage");
    EXPECT_TRUE(contains(report, "enum bool (undeclared vocabulary)")) << report;
}

// PARSE-09 (zero-byte half): enum.toml exists and is exactly zero bytes -- behaves identically to
// the absent case, proving the zero-length document is not a parse failure.
TEST(DatabaseUiParse, ZeroByteEnumFileTolerated) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "empty_enum", "cpp_zero_byte_enum");
    EXPECT_TRUE(db.has_ui_config());

    const auto report = db.describe_collection("Storage");
    EXPECT_TRUE(contains(report, "enum bool (undeclared vocabulary)")) << report;
}

// PARSE-09: a themes/ directory is never looked for -- its absence must not be a condition the
// parser checks. `no_enum` has no themes/ at all; the config still loads.
TEST(DatabaseUiParse, MissingThemesDirectoryIsNotConsulted) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "no_enum", "cpp_no_themes");
    EXPECT_TRUE(db.has_ui_config());
    EXPECT_NO_THROW(db.describe());
}
