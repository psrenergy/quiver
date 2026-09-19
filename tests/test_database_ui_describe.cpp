#include "test_ui_fixture.h"
#include "test_utils.h"

#include <gtest/gtest.h>
#include <quiver/database.h>
#include <quiver/element.h>
#include <string>

namespace {

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

}  // namespace

// The tracer slice's canonical output: a TOML sidecar on disk reaches
// summarize_collection()'s histogram as "<code>: <count> (<label>)" (D-04, literal L1 in
// plan 01-02's authoritative fixture-literal table).
TEST(DatabaseUiDescribe, EnumLabelsRenderBesideCodes) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "enum_basic", "cpp_enum_basic");

    for (int i = 0; i < 8; ++i) {
        db.create_element("Storage",
                          quiver::Element()
                              .set("label", "disabled_" + std::to_string(i))
                              .set("has_commitment", static_cast<int64_t>(0)));
    }
    for (int i = 0; i < 4; ++i) {
        db.create_element("Storage",
                          quiver::Element()
                              .set("label", "enabled_" + std::to_string(i))
                              .set("has_commitment", static_cast<int64_t>(1)));
    }

    const auto report = db.summarize_collection("Storage");
    EXPECT_TRUE(contains(report, "values {0: 8 (Disabled), 1: 4 (Enabled)}")) << report;
}

// A data code the vocabulary does not declare is marked explicitly (D-04, literal L2) -- never a
// bare code and never a borrowed label.
TEST(DatabaseUiDescribe, UndeclaredCodeRendersMarker) {
    auto db = quiver::test::open_ui_fixture(__FILE__, "enum_basic", "cpp_enum_basic_undeclared");

    db.create_element(
        "Storage", quiver::Element().set("label", std::string("weird")).set("has_commitment", static_cast<int64_t>(2)));

    const auto report = db.summarize_collection("Storage");
    EXPECT_TRUE(contains(report, "2: 1 (undeclared)")) << report;
}
