#include "test_utils.h"

#include <gtest/gtest.h>
#include <quiver/ui_config.h>
#include <string>

using quiver::UiConfig;

namespace {
std::string ui_dir() {
    return SCHEMA_PATH("schemas/from_database/ui");
}
}  // namespace

TEST(UiConfig, ReadsModelName) {
    const auto ui = UiConfig::from_directory(ui_dir());
    EXPECT_EQ(ui.model_name(), "demo_model");
}

TEST(UiConfig, ReadsAttributeUnit) {
    const auto ui = UiConfig::from_directory(ui_dir());
    EXPECT_EQ(ui.attribute_unit("Material", "demand"), "unit/year");
}

TEST(UiConfig, AttributeWithoutUnitIsEmpty) {
    const auto ui = UiConfig::from_directory(ui_dir());
    EXPECT_EQ(ui.attribute_unit("Material", "label"), "");
}

TEST(UiConfig, EnglishFirstFallsBackToFirstLocalization) {
    const auto ui = UiConfig::from_directory(ui_dir());
    EXPECT_EQ(ui.attribute_unit("Material", "pt_only_unit"), "unidade");
}

TEST(UiConfig, UnknownCollectionOrAttributeIsEmpty) {
    const auto ui = UiConfig::from_directory(ui_dir());
    EXPECT_EQ(ui.attribute_unit("Nonexistent", "demand"), "");
    EXPECT_EQ(ui.attribute_unit("Material", "nonexistent"), "");
}

TEST(UiConfig, MissingDirectoryYieldsEmptyConfig) {
    const auto ui = UiConfig::from_directory(SCHEMA_PATH("schemas/from_database/does_not_exist"));
    EXPECT_EQ(ui.model_name(), "");
    EXPECT_EQ(ui.attribute_unit("Material", "demand"), "");
}
