#include "test_ui_fixture.h"
#include "test_utils.h"

#include <fstream>
#include <gtest/gtest.h>
#include <quiver/database.h>
#include <quiver/element.h>
#include <string>

namespace {

// The two Items elements every golden report (tests/schemas/ui_golden/*.txt) was captured
// against. Both byte-identity cases below must seed exactly these values, or the comparison
// proves nothing.
void seed_items(quiver::Database& db) {
    db.create_element(
        "Items",
        quiver::Element().set("label", std::string("a")).set("priority", static_cast<int64_t>(1)).set("weight", 1.5));
    db.create_element(
        "Items",
        quiver::Element().set("label", std::string("b")).set("priority", static_cast<int64_t>(2)).set("weight", 2.5));
}

}  // namespace

// DESC-05: with no ui/ sidecar, describe()/describe_collection()/summarize_collection() must be
// byte-identical to the pre-change baseline captured from unmodified code (whole-string equality,
// not `contains`). Every golden read is std::ios::binary -- a text-mode read on Windows would
// silently collapse a CRLF the .gitattributes eol=lf pin is supposed to have prevented, hiding
// the very failure this guard exists to catch.
TEST(DatabaseUiGolden, MemoryDescribeByteIdentical) {
    auto db = quiver::Database::from_schema(":memory:",
                                            SCHEMA_PATH("schemas/ui_golden/schema.sql"),
                                            {.read_only = false, .console_level = quiver::LogLevel::Off});
    seed_items(db);

    std::ifstream describe_in(SCHEMA_PATH("schemas/ui_golden/describe.txt"), std::ios::binary);
    std::string describe_golden((std::istreambuf_iterator<char>(describe_in)), std::istreambuf_iterator<char>());
    EXPECT_EQ(db.describe(), describe_golden);

    std::ifstream describe_collection_in(SCHEMA_PATH("schemas/ui_golden/describe_collection.txt"), std::ios::binary);
    std::string describe_collection_golden((std::istreambuf_iterator<char>(describe_collection_in)),
                                           std::istreambuf_iterator<char>());
    EXPECT_EQ(db.describe_collection("Items"), describe_collection_golden);

    std::ifstream summarize_collection_in(SCHEMA_PATH("schemas/ui_golden/summarize_collection.txt"), std::ios::binary);
    std::string summarize_collection_golden((std::istreambuf_iterator<char>(summarize_collection_in)),
                                            std::istreambuf_iterator<char>());
    EXPECT_EQ(db.summarize_collection("Items"), summarize_collection_golden);
}

TEST(DatabaseUiGolden, FileBackedWithoutSidecarByteIdentical) {
    auto db = quiver::test::open_ui_fixture_at(__FILE__, "ui_golden", "cpp_golden");
    seed_items(db);

    // describe_collection / summarize_collection carry no machine-specific content -- exact match.
    std::ifstream describe_collection_in(SCHEMA_PATH("schemas/ui_golden/describe_collection.txt"), std::ios::binary);
    std::string describe_collection_golden((std::istreambuf_iterator<char>(describe_collection_in)),
                                           std::istreambuf_iterator<char>());
    EXPECT_EQ(db.describe_collection("Items"), describe_collection_golden);

    std::ifstream summarize_collection_in(SCHEMA_PATH("schemas/ui_golden/summarize_collection.txt"), std::ios::binary);
    std::string summarize_collection_golden((std::istreambuf_iterator<char>(summarize_collection_in)),
                                            std::istreambuf_iterator<char>());
    EXPECT_EQ(db.summarize_collection("Items"), summarize_collection_golden);

    // describe()'s first line is "Database: <path>", which differs between the :memory: golden
    // and this file-backed database; every line from the second onward must still match exactly.
    std::ifstream describe_in(SCHEMA_PATH("schemas/ui_golden/describe.txt"), std::ios::binary);
    std::string golden_describe((std::istreambuf_iterator<char>(describe_in)), std::istreambuf_iterator<char>());
    const auto file_backed_describe = db.describe();
    const auto golden_rest = golden_describe.substr(golden_describe.find('\n') + 1);
    const auto file_backed_rest = file_backed_describe.substr(file_backed_describe.find('\n') + 1);
    EXPECT_EQ(file_backed_rest, golden_rest);
}
