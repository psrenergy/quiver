#include "quiver/ui_config.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <toml++/toml.hpp>

namespace quiver {

namespace {

toml::table parse_toml_file(const std::string& path) {
    std::ifstream file(path);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    try {
        return toml::parse(content);
    } catch (const toml::parse_error& e) {
        throw std::runtime_error("Failed to parse UI config: " + std::string(e.description()) + " (" + path + ")");
    }
}

// Picks a localized string from a {en = "...", pt = "..."} sub-table: English first, then each
// declared localization in order, then any remaining locale. Empty strings are treated as absent so
// an explicit `unit.en = ""` does not shadow a real value in another locale.
std::string pick_localized(const toml::table& values, const std::vector<std::string>& localizations) {
    if (auto en = values["en"].value<std::string>(); en && !en->empty()) {
        return *en;
    }
    for (const auto& localization : localizations) {
        if (auto value = values[localization].value<std::string>(); value && !value->empty()) {
            return *value;
        }
    }
    for (const auto& [key, value] : values) {
        if (auto str = value.value<std::string>(); str && !str->empty()) {
            return *str;
        }
    }
    return "";
}

}  // namespace

UiConfig UiConfig::from_directory(const std::string& ui_dir) {
    namespace fs = std::filesystem;
    UiConfig config;

    if (!fs::exists(ui_dir) || !fs::is_directory(ui_dir)) {
        return config;
    }

    // main.toml: model name, extension, collection list, and the localization order.
    const auto main_path = fs::path(ui_dir) / "main.toml";
    if (fs::exists(main_path)) {
        const toml::table tbl = parse_toml_file(main_path.string());
        if (auto v = tbl["model"].value<std::string>()) {
            config.model_ = *v;
        }
        if (auto* arr = tbl["localizations"].as_array()) {
            for (auto& elem : *arr) {
                if (auto v = elem.value<std::string>()) {
                    config.localizations_.push_back(*v);
                }
            }
        }
    }

    // One <collection>.toml per collection: read its canonical id and per-attribute units.
    for (const auto& entry : fs::directory_iterator(ui_dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const auto& file_path = entry.path();
        if (file_path.extension() != ".toml") {
            continue;
        }
        const auto file_name = file_path.filename().string();
        if (file_name == "main.toml" || file_name == "enum.toml") {
            continue;
        }

        const toml::table tbl = parse_toml_file(file_path.string());
        const auto collection_id = tbl["id"].value<std::string>();
        if (!collection_id) {
            continue;  // not a collection file (no canonical id) — nothing to key units by
        }

        const auto* attributes = tbl["attribute"].as_array();
        if (!attributes) {
            continue;
        }
        for (auto& elem : *attributes) {
            const auto* attribute = elem.as_table();
            if (!attribute) {
                continue;
            }
            const auto attribute_id = (*attribute)["id"].value<std::string>();
            if (!attribute_id) {
                continue;
            }

            // Unit may be a localized sub-table (unit.en/unit.pt/...) or a bare string. English-first.
            std::string unit;
            const auto unit_node = (*attribute)["unit"];
            if (const auto* unit_table = unit_node.as_table()) {
                unit = pick_localized(*unit_table, config.localizations_);
            } else if (auto bare = unit_node.value<std::string>()) {
                unit = *bare;
            }

            if (!unit.empty()) {
                config.units_[*collection_id][*attribute_id] = unit;
            }
        }
    }

    return config;
}

const std::string& UiConfig::model_name() const {
    return model_;
}

std::string UiConfig::attribute_unit(const std::string& collection, const std::string& attribute) const {
    const auto collection_it = units_.find(collection);
    if (collection_it == units_.end()) {
        return "";
    }
    const auto attribute_it = collection_it->second.find(attribute);
    if (attribute_it == collection_it->second.end()) {
        return "";
    }
    return attribute_it->second;
}

}  // namespace quiver
