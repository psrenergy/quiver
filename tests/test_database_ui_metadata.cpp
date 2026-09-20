#include "test_ui_fixture.h"
#include "test_utils.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <quiver/database.h>
#include <string>
#include <vector>

// Phase 3 (META-01/META-02/META-05, D-36/D-41): the three UI metadata getters --
// get_attribute_ui_metadata, list_ui_vocabularies, get_ui_vocabulary.

namespace {

void write_file(const std::filesystem::path& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary);
    out << content;
}

// A scratch schema + ui/ sidecar written under a fresh directory relative to the test binary's
// cwd (build/bin -- gtest_discover_tests's WORKING_DIRECTORY) and removed in the destructor.
// Copied from tests/test_database_ui_describe.cpp's ScratchSidecarDir (the codebase already
// duplicates this class per test file rather than promoting it to a shared header). Used here for
// the one corpus gap no tracked fixture covers: a vocabulary declared with a name but zero
// entries, and proving list_ui_vocabularies' ordering claim on more than one vocabulary.
class ScratchSidecarDir {
public:
    explicit ScratchSidecarDir(const std::string& name) : dir_(name) {
        std::filesystem::remove_all(dir_);
        std::filesystem::create_directories(dir_ / "ui");
    }
    ~ScratchSidecarDir() { std::filesystem::remove_all(dir_); }

    ScratchSidecarDir(const ScratchSidecarDir&) = delete;
    ScratchSidecarDir& operator=(const ScratchSidecarDir&) = delete;

    void write_schema(const std::string& content) const { write_file(dir_ / "schema.sql", content); }
    void write_ui_file(const std::string& filename, const std::string& content) const {
        write_file(dir_ / "ui" / filename, content);
    }

    quiver::Database open(const std::string& stem) const {
        return quiver::Database::from_schema((dir_ / (stem + ".sqlite")).string(), (dir_ / "schema.sql").string(),
                                             {.read_only = false, .console_level = quiver::LogLevel::Off});
    }

private:
    std::filesystem::path dir_;
};

// Writes a minimal schema plus an enum.toml declaring three vocabularies in deliberately
// non-alphabetical source order -- shared by the two tests below (declared-but-empty, and the
// list_ui_vocabularies ordering claim), each against its own ScratchSidecarDir instance.
// UIConfigSet::parse_enum_content (src/ui_config.cpp:149-180) maps a zero-length TOML array to
// vocabularies[name] = {} -- a present key holding an empty vector, structurally different from
// empty_enum's zero-byte-file case (an absent map entry, which throws like any undeclared name).
// No fixture in tests/schemas/ui/ exercises this, so it is covered here with a scratch sidecar.
void write_three_vocab_sidecar(const ScratchSidecarDir& scratch) {
    scratch.write_schema(R"sql(
CREATE TABLE Configuration (
    id INTEGER PRIMARY KEY,
    label TEXT UNIQUE NOT NULL
) STRICT;
)sql");
    scratch.write_ui_file("main.toml", R"toml(
collections = []
)toml");
    // TOML requires bare root-level key/value pairs to precede any [[table]] header, so `alpha`
    // (a plain empty-array key) must be written first on disk -- the declaration order asserted
    // against is still non-alphabetical (alpha, zebra, middle), since alphabetical order would be
    // alpha, middle, zebra.
    scratch.write_ui_file("enum.toml", R"toml(
alpha = []

[[zebra]]
id = 0
label = "Z"

[[middle]]
id = 0
label = "M"
)toml");
}

}  // namespace

// -- get_attribute_ui_metadata, enum_basic ------------------------------------------------------

TEST(DatabaseUiMetadata, ConfiguredAttributeCarriesLabelAndVocabulary) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "enum_basic", "cpp_ui_metadata_configured");

    auto meta = db.get_attribute_ui_metadata("Storage", "has_commitment");
    EXPECT_TRUE(meta.configured);
    EXPECT_EQ(meta.label, "Has Commitment");
    EXPECT_EQ(meta.vocabulary, "bool");
    // D-31: display_order is unpopulated on an attribute record until Phase 4.
    EXPECT_EQ(meta.display_order, -1);
}

// The two offset-distinguishing records. Without these, tooltip@8/unit@16/format@24/icon@32 (all
// empty on has_commitment) could be silently transposed, and configured@56/hidden@60 (both `int`,
// both true on a hide=true configured record) could be swapped -- every other assertion in this
// phase would still pass. Do not delete these as "redundant" with ConfiguredAttribute... above.

TEST(DatabaseUiMetadata, UnitDistinguishesOffset16FromNeighbours) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "enum_basic", "cpp_ui_metadata_unit");

    auto meta = db.get_attribute_ui_metadata("Storage", "max_generation");
    EXPECT_EQ(meta.unit, "MW");
}

TEST(DatabaseUiMetadata, HiddenDistinguishesOffset60FromConfigured) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "enum_basic", "cpp_ui_metadata_hidden");

    auto meta = db.get_attribute_ui_metadata("Storage", "internal_code");
    EXPECT_TRUE(meta.hidden);
    EXPECT_EQ(meta.label, "Internal Code");
}

TEST(DatabaseUiMetadata, DeclaredBlankLabelIsConfiguredNotAbsent) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "enum_basic", "cpp_ui_metadata_blank");

    // D-12: notes declares label = "" -- declared-blank is not unconfigured.
    auto meta = db.get_attribute_ui_metadata("Storage", "notes");
    EXPECT_TRUE(meta.configured);
    EXPECT_EQ(meta.label, "");
}

TEST(DatabaseUiMetadata, RealUnconfiguredColumnReturnsDefaultWithoutThrowing) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "enum_basic", "cpp_ui_metadata_unconfigured");

    // META-02: `label` is a real Storage column enum_basic/ui/storage.toml never mentions.
    auto meta = db.get_attribute_ui_metadata("Storage", "label");
    EXPECT_FALSE(meta.configured);
    EXPECT_EQ(meta.label, "");
    EXPECT_EQ(meta.tooltip, "");
    EXPECT_EQ(meta.unit, "");
    EXPECT_EQ(meta.format, "");
    EXPECT_EQ(meta.icon, "");
    EXPECT_EQ(meta.vocabulary, "");
    EXPECT_EQ(meta.display_order, -1);
}

TEST(DatabaseUiMetadata, NonexistentColumnThrowsExactPattern2Message) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "enum_basic", "cpp_ui_metadata_no_such_column");

    EXPECT_THROW(
        {
            try {
                db.get_attribute_ui_metadata("Storage", "no_such_column");
            } catch (const std::runtime_error& e) {
                EXPECT_STREQ(e.what(), "Scalar attribute not found: 'no_such_column' in collection 'Storage'");
                throw;
            }
        },
        std::runtime_error);
}

// -- format round-trip (ROADMAP criterion 3, D-32) ------------------------------------------------

TEST(DatabaseUiMetadata, FormatTableCollapsesToWinningKeyVerbatim) {
    // format_table already holds cpp_format_table / cpp_empty_collection -- a fresh db_stem is
    // required so ctest -j cannot race two cases writing the same file.
    auto db = quiver::test::open_ui_fixture(__FILE__, "format_table", "cpp_ui_metadata_format_table");

    // capacity declares all four format keys (element_view=0.0, collection_view=0.00, edit=0.000,
    // data=0.0000); `data` wins over the other three per D-32's first-present-of[data, element_view,
    // collection_view, edit] precedence -- the three discarded keys are unreachable through any
    // Quiver surface, and this is the only place in the phase that fact is observable.
    auto meta = db.get_attribute_ui_metadata("Storage", "capacity");
    EXPECT_EQ(meta.format, "0.0000");
}

// -- get_ui_vocabulary / list_ui_vocabularies, enum_basic -----------------------------------------

TEST(DatabaseUiMetadata, GetUiVocabularyReturnsOrderedEntries) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "enum_basic", "cpp_ui_metadata_vocab");

    auto entries = db.get_ui_vocabulary("bool");
    ASSERT_EQ(entries.size(), 2u);
    EXPECT_EQ(entries[0].code, 0);
    EXPECT_EQ(entries[0].label, "Disabled");
    EXPECT_EQ(entries[1].code, 1);
    EXPECT_EQ(entries[1].label, "Enabled");
}

TEST(DatabaseUiMetadata, GetUiVocabularyUndeclaredNameThrows) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "enum_basic", "cpp_ui_metadata_vocab_missing");

    EXPECT_THROW(
        {
            try {
                db.get_ui_vocabulary("no_such_vocabulary");
            } catch (const std::runtime_error& e) {
                EXPECT_STREQ(e.what(), "Vocabulary not found: 'no_such_vocabulary'");
                throw;
            }
        },
        std::runtime_error);
}

// -- Disengaged std::optional -- a database with no ui/ sidecar at all (D-41) ---------------------
//
// This is the normal state of every database outside tests/schemas/ui/, so all three getters are
// exercised here or two of them ship untested against their most common input. `:memory:` never
// resolves a directory, so it never engages Impl::ui_config.

TEST(DatabaseUiMetadata, NoSidecarGetAttributeReturnsDefaultRecord) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    auto meta = db.get_attribute_ui_metadata("Configuration", "integer_attribute");
    EXPECT_FALSE(meta.configured);
    EXPECT_EQ(meta.label, "");
}

TEST(DatabaseUiMetadata, NoSidecarListUiVocabulariesReturnsEmpty) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    EXPECT_TRUE(db.list_ui_vocabularies().empty());
}

TEST(DatabaseUiMetadata, NoSidecarGetUiVocabularyThrowsSameAsUndeclaredName) {
    auto db = quiver::Database::from_schema(
        ":memory:", VALID_SCHEMA("basic.sql"), {.read_only = false, .console_level = quiver::LogLevel::Off});

    // Same outcome as an engaged config lacking the name -- the implementation collapses both
    // into one branch on purpose (there is no sidecar-shaped reason to distinguish them).
    EXPECT_THROW(
        {
            try {
                db.get_ui_vocabulary("bool");
            } catch (const std::runtime_error& e) {
                EXPECT_STREQ(e.what(), "Vocabulary not found: 'bool'");
                throw;
            }
        },
        std::runtime_error);
}

// -- The one confirmed corpus gap: a vocabulary declared with a name but zero entries --------------
// Both tests share write_three_vocab_sidecar's fixture (declared in non-alphabetical source
// order) against their own ScratchSidecarDir instance.

TEST(DatabaseUiMetadata, DeclaredButEmptyVocabularyReturnsEmptyVectorWithoutThrowing) {
    ScratchSidecarDir scratch("scratch_ui_metadata_empty_vocab");
    write_three_vocab_sidecar(scratch);
    auto db = scratch.open("declared_but_empty");

    auto entries = db.get_ui_vocabulary("alpha");
    EXPECT_TRUE(entries.empty());
}

TEST(DatabaseUiMetadata, ListUiVocabulariesReturnsAlphabeticalOrderFromNonAlphabeticalSource) {
    // The only place in the phase that can fail the ordering claim, since every tracked fixture
    // declares at most one vocabulary and a one-element list is sorted under any ordering. Do not
    // mistake the other six plans' single-name order assertions as redundant copies of this one.
    ScratchSidecarDir scratch("scratch_ui_metadata_vocab_order");
    write_three_vocab_sidecar(scratch);
    auto db = scratch.open("vocab_order");

    auto names = db.list_ui_vocabularies();
    ASSERT_EQ(names.size(), 3u);
    EXPECT_EQ(names[0], "alpha");
    EXPECT_EQ(names[1], "middle");
    EXPECT_EQ(names[2], "zebra");
}
