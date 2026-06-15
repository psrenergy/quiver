#include "test_utils.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <quiver/ui_config.h>
#include <string>

using quiver::UiConfig;

namespace {
std::string ui_dir() {
    return SCHEMA_PATH("schemas/from_hub/ui");
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

TEST(UiConfig, EmptyEnglishUnitDoesNotShadowOtherLocale) {
    const auto ui = UiConfig::from_directory(ui_dir());
    EXPECT_EQ(ui.attribute_unit("Material", "empty_en_unit"), "kg");
}

TEST(UiConfig, ReadsBareStringUnit) {
    const auto ui = UiConfig::from_directory(ui_dir());
    EXPECT_EQ(ui.attribute_unit("Material", "bare_unit"), "bare");
}

TEST(UiConfig, ResolvesUnitWithoutMainToml) {
    // No main.toml -> no declared localizations; a non-English-only unit must still resolve via the
    // any-locale fallback rather than being silently dropped.
    namespace fs = std::filesystem;
    const auto dir = fs::temp_directory_path() / "quiver_ui_no_main";
    fs::remove_all(dir);
    fs::create_directories(dir);
    {
        std::ofstream f((dir / "material.toml").string());
        f << "id = \"Material\"\n\n[[attribute]]\nid = \"demand\"\nunit.pt = \"unidade\"\n";
    }
    const auto ui = UiConfig::from_directory(dir.string());
    EXPECT_EQ(ui.model_name(), "");
    EXPECT_EQ(ui.attribute_unit("Material", "demand"), "unidade");
    fs::remove_all(dir);
}

TEST(UiConfig, UnknownCollectionOrAttributeIsEmpty) {
    const auto ui = UiConfig::from_directory(ui_dir());
    EXPECT_EQ(ui.attribute_unit("Nonexistent", "demand"), "");
    EXPECT_EQ(ui.attribute_unit("Material", "nonexistent"), "");
}

TEST(UiConfig, MissingDirectoryYieldsEmptyConfig) {
    const auto ui = UiConfig::from_directory(SCHEMA_PATH("schemas/from_hub/does_not_exist"));
    EXPECT_EQ(ui.model_name(), "");
    EXPECT_EQ(ui.attribute_unit("Material", "demand"), "");
}
