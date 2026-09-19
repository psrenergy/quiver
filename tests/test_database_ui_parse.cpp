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

// ---------------------------------------------------------------------------
// Task 2: PascalCase ids, the dual id namespace, interleaved blocks, the orphan file
// ---------------------------------------------------------------------------

// PARSE-10: main.collections names the snake_case filename; the file's own `id` (PascalCase SQL
// table name) is the actual lookup key. The snake_case string is never a valid lookup.
TEST(DatabaseUiParse, PascalCaseIdKeysTheConfig) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "htd_like", "cpp_pascal_case");

    const auto report = db.describe_collection("HydroPlant");
    EXPECT_TRUE(contains(report, "Hydro Plants")) << report;
    EXPECT_TRUE(contains(report, "Measurement Date")) << report;

    EXPECT_THROW(db.describe_collection("hydro_plant"), std::runtime_error);
}

// PARSE-10: a listed collection file with no top-level `id` is skipped with a debug log, not
// fatal -- the rest of the config still publishes. Different rule and different fixture from a
// listed-but-missing file (that case is `malformed`'s job, not this one's).
TEST(DatabaseUiParse, MissingCollectionIdIsSkippedNotFatal) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "htd_like", "cpp_missing_id");
    EXPECT_TRUE(db.has_ui_config());

    const auto hydro_report = db.describe_collection("HydroPlant");
    EXPECT_TRUE(contains(hydro_report, "Hydro Plants")) << hydro_report;

    const auto full_report = db.describe();
    EXPECT_FALSE(contains(full_report, "Thermal Plants")) << full_report;
}

// PARSE-07: `degradation` is legally both an [[attribute]] id and an [[attribute_group]] id in
// one file, with different labels -- the scalar line carries the attribute's label, never the
// group's, while the Vectors: section still names the group.
TEST(DatabaseUiParse, AttributeAndGroupIdsAreSeparateNamespaces) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "bess_like", "cpp_dual_namespace");

    const auto report = db.describe_collection("Storage");
    const auto lines = split_lines(report);

    const auto scalar_line = find_line(lines, "    - degradation (");
    ASSERT_FALSE(scalar_line.empty()) << report;
    EXPECT_TRUE(contains(scalar_line, "Degradation Rate")) << scalar_line;
    EXPECT_FALSE(contains(scalar_line, "Degradation Curve")) << scalar_line;

    const auto group_line = find_line(lines, "    - degradation: ");
    EXPECT_FALSE(group_line.empty()) << report;
}

// PARSE-08: [[attribute]] and [[attribute_group]] blocks may interleave in any order; the two
// attributes declared after an [[attribute_group]] block parse with full fidelity.
TEST(DatabaseUiParse, AttributesAfterAGroupBlockAreStillParsed) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "bess_like", "cpp_after_group");

    const auto report = db.describe_collection("Storage");
    EXPECT_TRUE(contains(report, "\"Installed Capacity\"")) << report;
    EXPECT_TRUE(contains(report, "\"Rated Cycle Count\"")) << report;
}

// WR-02 (review fix): bess_like's enum.toml declares a gapped/negative/int64-max `look_ahead`
// vocabulary (codes -1, 0, 3, 7, 9223372036854775807) as proof the parser/renderer tolerate the
// full int64_t range -- but until this fix it was never bound to any attribute, so only toml++'s
// own parser (already covered upstream) ever touched those values. `look_ahead` is now a real
// Storage column bound via `enum = "look_ahead"`; this asserts the exact rendered line, including
// the negative and int64-max codes, through append_scalar_ui_clauses -- the render path the
// vocabulary was originally meant to prove.
TEST(DatabaseUiParse, Int64ExtremeVocabularyCodesRenderExactly) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "bess_like", "cpp_int64_extremes");

    const auto report = db.describe_collection("Storage");
    const std::string expected =
        "enum look_ahead {-1: Unknown, 0: Immediate, 3: Short Term, 7: Long Term, 9223372036854775807: Unbounded}";
    EXPECT_TRUE(contains(report, expected)) << report;
}

// PARSE-04 adjacency: bess_like's labels are all bare strings -- the bare string wins outright and
// renders unchanged, ahead of any locale key.
TEST(DatabaseUiParse, BareStringLabelsIgnoreLocale) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "bess_like", "cpp_bare_locale");

    const auto report = db.describe_collection("Storage");
    EXPECT_TRUE(contains(report, "\"Installed Capacity\"")) << report;
    EXPECT_TRUE(contains(report, "\"Rated Cycle Count\"")) << report;
    EXPECT_TRUE(contains(report, "\"Degradation Rate\"")) << report;
}

// PARSE-04: one substring covering four locale-resolution legs at once -- exact-locale (2),
// bare-string-wins (3), accented exact-locale (5, literal L9), and first-key-in-map-order
// fallback for an entry declaring only `es`/`pt` (4, literal L10 -- "es" < "pt"). Entry order is
// the fixture's TOML declaration order, deliberately not code order (also pins DESC-03).
TEST(DatabaseUiParse, MixedLocaleFormsResolveInOneVocabulary) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "foresight_like", "cpp_mixed_locale");

    const auto report = db.describe_collection("EconomicDriver");
    // UTF-8 escapes, not literal accented characters: cmake/CompilerOptions.cmake passes no
    // /utf-8 to MSVC, so a literal non-ASCII character in this source is a portability hazard.
    const std::string expected =
        "enum model {2: Local Linear Trend, 3: ARIMA, 5: Seasonal Na\xC3\xAFve, 4: Regresi\xC3\xB3n Lineal}";
    EXPECT_TRUE(contains(report, expected)) << report;
}

// PARSE-01: a fully-formed collection file present in ui/ but absent from main.collections is
// never loaded -- its collection label never renders, even though the file sits right there on
// disk. `Storage` (listed) still carries its label as the contrast.
TEST(DatabaseUiParse, UnlistedCollectionFileIsNeverLoaded) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "orphan_collection", "cpp_orphan");

    const auto report = db.describe();
    EXPECT_TRUE(contains(report, "\"Storage Units\"")) << report;

    const auto lines = split_lines(report);
    const auto agent_line = find_line(lines, "Collection: Agent");
    ASSERT_FALSE(agent_line.empty()) << report;
    EXPECT_FALSE(contains(agent_line, kEmDash)) << agent_line;
}
