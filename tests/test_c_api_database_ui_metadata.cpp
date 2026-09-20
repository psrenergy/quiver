#include "test_utils.h"

#include <cstddef>
#include <gtest/gtest.h>
#include <quiver/c/database.h>
#include <string>

// Phase 3 plan 03-02 (META-01..META-05): the C API's UI-metadata surface, exercised directly
// through quiver_database_from_schema rather than the C++ test_ui_fixture.h helper -- a C API
// test must prove the C boundary itself, not the C++ helper behind it (01-05 precedent, restated
// in test_c_api_database_options.cpp). include/quiver/c/database.h is frozen after this plan.

namespace {

std::string enum_basic_fixture_dir() {
    return quiver::test::path_from(__FILE__, "schemas/ui/enum_basic");
}

std::string enum_basic_schema_path() {
    return enum_basic_fixture_dir() + "/schema.sql";
}

// db_stem is per-TEST (not per-suite) so ctest -j cannot race two cases writing the same file
// inside enum_basic/ -- same reasoning as test_ui_fixture.h's open_ui_fixture. The database file
// lives inside the fixture directory itself so the <db_dir>/ui/ convention resolves the sidecar
// with no explicit ui_config_dir override needed.
quiver_database_t* open_enum_basic(const std::string& db_stem) {
    auto options = quiver::test::quiet_options();
    const auto db_path = enum_basic_fixture_dir() + "/" + db_stem + ".sqlite";
    quiver_database_t* db = nullptr;
    EXPECT_EQ(quiver_database_from_schema(db_path.c_str(), enum_basic_schema_path().c_str(), &options, &db),
              QUIVER_OK);
    EXPECT_NE(db, nullptr);
    return db;
}

}  // namespace

// ============================================================================
// Layout: the 64-byte hole-free struct, all nine offsets
// ============================================================================
//
// The compile-time static_asserts in src/c/database_metadata.cpp are the enforcing gate; these
// are the readable record of the same nine numbers the four hand-written FFI decoders hardcode,
// so a reader of this file need not open the source to see them.

TEST(DatabaseCApiUiMetadata, SizeofAccessorMatchesNativeSixtyFourByteLayout) {
    EXPECT_EQ(quiver_ui_metadata_sizeof(), 64u);
    EXPECT_EQ(quiver_ui_metadata_sizeof(), sizeof(quiver_ui_metadata_t));

    EXPECT_EQ(offsetof(quiver_ui_metadata_t, label), 0u);
    EXPECT_EQ(offsetof(quiver_ui_metadata_t, tooltip), 8u);
    EXPECT_EQ(offsetof(quiver_ui_metadata_t, unit), 16u);
    EXPECT_EQ(offsetof(quiver_ui_metadata_t, format), 24u);
    EXPECT_EQ(offsetof(quiver_ui_metadata_t, icon), 32u);
    EXPECT_EQ(offsetof(quiver_ui_metadata_t, vocabulary), 40u);
    EXPECT_EQ(offsetof(quiver_ui_metadata_t, display_order), 48u);
    EXPECT_EQ(offsetof(quiver_ui_metadata_t, configured), 56u);
    EXPECT_EQ(offsetof(quiver_ui_metadata_t, hidden), 60u);
}

// ROADMAP criterion 4: the two pre-existing metadata structs must be provably untouched by this
// phase -- asserted here, next to the new struct, rather than only in a diff.
TEST(DatabaseCApiUiMetadata, ScalarAndGroupMetadataSizesAreUnaffected) {
    EXPECT_EQ(quiver_scalar_metadata_sizeof(), 56u);
    EXPECT_EQ(quiver_scalar_metadata_sizeof(), sizeof(quiver_scalar_metadata_t));

    EXPECT_EQ(quiver_group_metadata_sizeof(), 32u);
    EXPECT_EQ(quiver_group_metadata_sizeof(), sizeof(quiver_group_metadata_t));
}

// ============================================================================
// Single-record round trip
// ============================================================================

TEST(DatabaseCApiUiMetadata, ConfiguredAttributeRoundTripsAllNineFields) {
    quiver_database_t* db = open_enum_basic("capi_ui_metadata_configured");

    quiver_ui_metadata_t meta = {};
    auto err = quiver_database_get_attribute_ui_metadata(db, "Storage", "has_commitment", &meta);
    EXPECT_EQ(err, QUIVER_OK);

    // D-13: absence is spelled empty string, never NULL -- assert every one of the six const
    // char* fields is non-NULL, including the three that are empty on this record (tooltip,
    // format, icon).
    ASSERT_NE(meta.label, nullptr);
    ASSERT_NE(meta.tooltip, nullptr);
    ASSERT_NE(meta.unit, nullptr);
    ASSERT_NE(meta.format, nullptr);
    ASSERT_NE(meta.icon, nullptr);
    ASSERT_NE(meta.vocabulary, nullptr);

    EXPECT_STREQ(meta.label, "Has Commitment");
    EXPECT_STREQ(meta.tooltip, "");
    EXPECT_STREQ(meta.unit, "");
    EXPECT_STREQ(meta.format, "");
    EXPECT_STREQ(meta.icon, "");
    EXPECT_STREQ(meta.vocabulary, "bool");
    EXPECT_EQ(meta.display_order, -1);  // D-31: unpopulated on an attribute record until Phase 4.
    EXPECT_NE(meta.configured, 0);
    EXPECT_EQ(meta.hidden, 0);

    quiver_database_free_ui_metadata(&meta);
    quiver_database_close(db);
}

// D-12: notes declares label = "" -- a declared-blank string field is still a non-NULL pointer,
// distinct from an unconfigured attribute (also empty, also non-NULL, but configured == 0).
TEST(DatabaseCApiUiMetadata, DeclaredBlankLabelIsNonNullPointer) {
    quiver_database_t* db = open_enum_basic("capi_ui_metadata_blank");

    quiver_ui_metadata_t meta = {};
    auto err = quiver_database_get_attribute_ui_metadata(db, "Storage", "notes", &meta);
    EXPECT_EQ(err, QUIVER_OK);
    ASSERT_NE(meta.label, nullptr);
    EXPECT_STREQ(meta.label, "");
    EXPECT_NE(meta.configured, 0);

    quiver_database_free_ui_metadata(&meta);
    quiver_database_close(db);
}

// ============================================================================
// The offset-distinguishing records -- see the plan's rationale: enum_basic carries no
// tooltip/format/icon anywhere, so on has_commitment alone offsets 8/16/24/32 (and 56/60) all
// read as indistinguishable values. These two records are what makes nine separate offset
// assertions actually prove something instead of four transposable no-ops.
// ============================================================================

TEST(DatabaseCApiUiMetadata, UnitDistinguishesOffsetSixteenFromNeighbours) {
    quiver_database_t* db = open_enum_basic("capi_ui_metadata_unit");

    quiver_ui_metadata_t meta = {};
    auto err = quiver_database_get_attribute_ui_metadata(db, "Storage", "max_generation", &meta);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_STREQ(meta.unit, "MW");

    quiver_database_free_ui_metadata(&meta);
    quiver_database_close(db);
}

TEST(DatabaseCApiUiMetadata, HiddenDistinguishesOffsetSixtyFromConfigured) {
    quiver_database_t* db = open_enum_basic("capi_ui_metadata_hidden");

    quiver_ui_metadata_t meta = {};
    auto err = quiver_database_get_attribute_ui_metadata(db, "Storage", "internal_code", &meta);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_NE(meta.hidden, 0);
    EXPECT_STREQ(meta.label, "Internal Code");

    quiver_database_free_ui_metadata(&meta);
    quiver_database_close(db);
}

// ============================================================================
// Both D-36 polarities at the C boundary
// ============================================================================

TEST(DatabaseCApiUiMetadata, UnconfiguredRealColumnReturnsOkWithConfiguredFalse) {
    quiver_database_t* db = open_enum_basic("capi_ui_metadata_unconfigured");

    // `label` is a real Storage column enum_basic/ui/storage.toml never mentions.
    quiver_ui_metadata_t meta = {};
    auto err = quiver_database_get_attribute_ui_metadata(db, "Storage", "label", &meta);
    EXPECT_EQ(err, QUIVER_OK);
    EXPECT_EQ(meta.configured, 0);
    ASSERT_NE(meta.label, nullptr);
    EXPECT_STREQ(meta.label, "");

    quiver_database_free_ui_metadata(&meta);
    quiver_database_close(db);
}

TEST(DatabaseCApiUiMetadata, AbsentColumnReturnsErrorWithExactPattern2Message) {
    quiver_database_t* db = open_enum_basic("capi_ui_metadata_no_such_column");

    quiver_ui_metadata_t meta = {};
    auto err = quiver_database_get_attribute_ui_metadata(db, "Storage", "no_such_column", &meta);
    EXPECT_EQ(err, QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Scalar attribute not found: 'no_such_column' in collection 'Storage'");

    quiver_database_close(db);
}

// ============================================================================
// Vocabulary round trip
// ============================================================================

// enum_basic declares exactly one vocabulary, so this asserts the shape (name + count), not
// ordering -- the non-vacuous ordering proof is 03-01 Task 3's three-name C++ scratch sidecar and
// is not repeated here.
TEST(DatabaseCApiUiMetadata, ListUiVocabulariesReturnsFixtureNames) {
    quiver_database_t* db = open_enum_basic("capi_ui_metadata_list_vocab");

    char** names = nullptr;
    size_t count = 0;
    auto err = quiver_database_list_ui_vocabularies(db, &names, &count);
    EXPECT_EQ(err, QUIVER_OK);
    ASSERT_EQ(count, 1u);
    ASSERT_NE(names, nullptr);
    EXPECT_STREQ(names[0], "bool");

    quiver_database_free_string_array(names, count);
    quiver_database_close(db);
}

TEST(DatabaseCApiUiMetadata, GetUiVocabularyReturnsOrderedCodesAndLabels) {
    quiver_database_t* db = open_enum_basic("capi_ui_metadata_get_vocab");

    int64_t* codes = nullptr;
    char** labels = nullptr;
    size_t count = 0;
    auto err = quiver_database_get_ui_vocabulary(db, "bool", &codes, &labels, &count);
    EXPECT_EQ(err, QUIVER_OK);
    ASSERT_EQ(count, 2u);
    ASSERT_NE(codes, nullptr);
    ASSERT_NE(labels, nullptr);
    EXPECT_EQ(codes[0], 0);
    EXPECT_STREQ(labels[0], "Disabled");
    EXPECT_EQ(codes[1], 1);
    EXPECT_STREQ(labels[1], "Enabled");

    quiver_database_free_ui_vocabulary(codes, labels, count);
    quiver_database_close(db);
}

TEST(DatabaseCApiUiMetadata, GetUiVocabularyUnknownNameReturnsErrorWithExactMessage) {
    quiver_database_t* db = open_enum_basic("capi_ui_metadata_vocab_missing");

    int64_t* codes = nullptr;
    char** labels = nullptr;
    size_t count = 0;
    auto err = quiver_database_get_ui_vocabulary(db, "no_such_vocabulary", &codes, &labels, &count);
    EXPECT_EQ(err, QUIVER_ERROR);
    EXPECT_STREQ(quiver_get_last_error(), "Vocabulary not found: 'no_such_vocabulary'");

    quiver_database_close(db);
}

// ============================================================================
// Free-path safety (T-03-03)
// ============================================================================

// quiver_database_free_ui_metadata takes the struct pointer itself and nulls the fields it frees,
// so a second call on the same record is a documented no-op, unlike the vocabulary free below.
TEST(DatabaseCApiUiMetadata, FreeUiMetadataTwiceOnSameRecordIsSafe) {
    quiver_database_t* db = open_enum_basic("capi_ui_metadata_free_twice");

    quiver_ui_metadata_t meta = {};
    auto err = quiver_database_get_attribute_ui_metadata(db, "Storage", "has_commitment", &meta);
    EXPECT_EQ(err, QUIVER_OK);

    EXPECT_EQ(quiver_database_free_ui_metadata(&meta), QUIVER_OK);
    EXPECT_EQ(quiver_database_free_ui_metadata(&meta), QUIVER_OK);

    quiver_database_close(db);
}

// quiver_database_free_ui_vocabulary cannot null the caller's raw pointers, so it is NOT asserted
// safe against a second call on the same pointers (that would be a double free) -- only against a
// zero-count result and NULL arrays, per the plan's explicit instruction not to write that test.
TEST(DatabaseCApiUiMetadata, FreeUiVocabularyToleratesZeroCountAndNullArrays) {
    // Both arrays NULL, count zero -- what get_ui_vocabulary itself returns for a declared-but-
    // empty vocabulary.
    EXPECT_EQ(quiver_database_free_ui_vocabulary(nullptr, nullptr, 0), QUIVER_OK);

    // codes present, labels NULL -- and vice versa -- each array is independently NULL-tolerant.
    auto* codes = new int64_t[1]{0};
    EXPECT_EQ(quiver_database_free_ui_vocabulary(codes, nullptr, 1), QUIVER_OK);

    auto* labels = new char*[1];
    labels[0] = new char[2]{'x', '\0'};
    EXPECT_EQ(quiver_database_free_ui_vocabulary(nullptr, labels, 1), QUIVER_OK);
}
