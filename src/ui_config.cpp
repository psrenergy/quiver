#include "ui_config.h"

#include "database_impl.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <toml++/toml.hpp>

namespace quiver {

namespace {

// Hub's LocalizationString.ofLocale chain, copied verbatim except the final leg: Hub throws
// when the chain is exhausted, Quiver returns "" instead (PARSE-05/PARSE-11 forbid the throw).
// A bare string wins outright and ignores locale entirely; a table is resolved exact-locale,
// then "en", then the first key in the table's own (std::map, alphabetical) order -- see
// plan 01-04's MixedLocaleFormsResolveInOneVocabulary, which pins that ordering as deterministic.
std::string resolve_localizable(const toml::node* node, const std::string& locale) {
    if (!node) {
        return "";
    }
    if (auto bare = node->value<std::string>()) {
        return *bare;
    }
    const auto* table = node->as_table();
    if (!table) {
        return "";
    }
    if (const auto* exact = table->get(locale)) {
        if (auto value = exact->value<std::string>()) {
            return *value;
        }
    }
    if (const auto* english = table->get("en")) {
        if (auto value = english->value<std::string>()) {
            return *value;
        }
    }
    if (!table->empty()) {
        const auto& first_entry = *table->begin();
        if (auto first = first_entry.second.value<std::string>()) {
            return *first;
        }
    }
    return "";
}

// Shared by [[attribute]] and [[attribute_group]] blocks -- both are `UIMetadata` records with
// the same per-attribute keys (D-09's one-record-for-three-levels design).
UIMetadata parse_attribute_or_group(const toml::table& tbl, const std::string& locale) {
    UIMetadata meta;
    meta.configured = true;
    meta.label = resolve_localizable(tbl.get("label"), locale);
    meta.tooltip = resolve_localizable(tbl.get("tooltip"), locale);
    if (const auto* unit = tbl.get("unit")) {
        if (auto value = unit->value<std::string>()) {
            meta.unit = *value;
        }
    }
    // format is read verbatim as a single string for now -- its 4-key table form is plan
    // 01-04's tolerance (PARSE-06), not built here.
    if (const auto* format = tbl.get("format")) {
        if (auto value = format->value<std::string>()) {
            meta.format = *value;
        }
    }
    if (const auto* hide = tbl.get("hide")) {
        if (auto value = hide->value<bool>()) {
            meta.hidden = *value;
        }
    }
    if (const auto* enum_ref = tbl.get("enum")) {
        if (auto value = enum_ref->value<std::string>()) {
            meta.vocabulary = *value;
        }
    }
    return meta;
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

}  // namespace

std::map<std::string, std::vector<UIEnumEntry>> UIConfigSet::parse_enum_content(const std::string& content) {
    std::map<std::string, std::vector<UIEnumEntry>> vocabularies;

    toml::table tbl = toml::parse(content);
    // Vocabulary names are not known ahead of time -- iterate the table's own keys rather than
    // looking up fixed ones (unlike BinaryMetadata's fixed-key content).
    for (auto&& [name, value] : tbl) {
        const auto* entries_array = value.as_array();
        if (!entries_array) {
            continue;
        }
        std::vector<UIEnumEntry> entries;
        for (const auto& element : *entries_array) {
            const auto* entry_table = element.as_table();
            if (!entry_table) {
                continue;
            }
            UIEnumEntry entry;
            if (const auto* id = entry_table->get("id")) {
                if (auto value_id = id->value<int64_t>()) {
                    entry.code = *value_id;
                }
            }
            // Phase 1's locale is hardcoded "en" everywhere (D-06); enum labels resolve at "en"
            // regardless of any future caller-supplied locale (OPT-02 is Phase 2).
            entry.label = resolve_localizable(entry_table->get("label"), "en");
            entries.push_back(std::move(entry));
        }
        vocabularies[std::string(name.str())] = std::move(entries);
    }

    return vocabularies;
}

UICollectionConfig
UIConfigSet::parse_collection_content(const std::string& content, const std::string& locale, std::string& out_table_id) {
    UICollectionConfig config;
    out_table_id.clear();

    toml::table tbl = toml::parse(content);

    if (const auto* id = tbl.get("id")) {
        if (auto value = id->value<std::string>()) {
            out_table_id = *value;
        }
    }
    config.meta.configured = !out_table_id.empty();
    config.meta.label = resolve_localizable(tbl.get("label"), locale);
    config.meta.tooltip = resolve_localizable(tbl.get("tooltip"), locale);
    if (const auto* icon = tbl.get("icon")) {
        if (auto value = icon->value<std::string>()) {
            config.meta.icon = *value;
        }
    }

    if (const auto* attribute_node = tbl.get("attribute")) {
        if (const auto* attribute_array = attribute_node->as_array()) {
            for (const auto& element : *attribute_array) {
                const auto* attribute_table = element.as_table();
                if (!attribute_table) {
                    continue;
                }
                std::string attribute_id;
                if (const auto* id = attribute_table->get("id")) {
                    if (auto value = id->value<std::string>()) {
                        attribute_id = *value;
                    }
                }
                if (attribute_id.empty()) {
                    continue;
                }
                config.attributes[attribute_id] = parse_attribute_or_group(*attribute_table, locale);
            }
        }
    }

    if (const auto* group_node = tbl.get("attribute_group")) {
        if (const auto* group_array = group_node->as_array()) {
            for (const auto& element : *group_array) {
                const auto* group_table = element.as_table();
                if (!group_table) {
                    continue;
                }
                std::string group_id;
                if (const auto* id = group_table->get("id")) {
                    if (auto value = id->value<std::string>()) {
                        group_id = *value;
                    }
                }
                if (group_id.empty()) {
                    continue;
                }
                config.groups[group_id] = parse_attribute_or_group(*group_table, locale);
            }
        }
    }

    return config;
}

UIConfigSet UIConfigSet::from_directory(const std::string& ui_dir, const std::string& locale) {
    namespace fs = std::filesystem;

    UIConfigSet config;
    config.source_directory = ui_dir;
    config.locale = locale;

    const fs::path dir(ui_dir);

    // main.toml drives what loads (PARSE-01: never scan the directory to decide this).
    toml::table main_tbl = toml::parse(read_file(dir / "main.toml"));
    std::vector<std::string> collection_files;
    if (const auto* collections = main_tbl.get("collections")) {
        if (const auto* collections_array = collections->as_array()) {
            for (const auto& element : *collections_array) {
                if (auto value = element.value<std::string>()) {
                    collection_files.push_back(*value);
                }
            }
        }
    }

    // enum.toml is optional; a missing file leaves vocabularies empty without error (PARSE-09).
    const auto enum_path = dir / "enum.toml";
    if (fs::exists(enum_path)) {
        config.vocabularies = parse_enum_content(read_file(enum_path));
    }

    for (size_t index = 0; index < collection_files.size(); ++index) {
        const auto& filename = collection_files[index];
        const auto collection_path = dir / (filename + ".toml");
        if (!fs::exists(collection_path)) {
            continue;
        }
        std::string table_id;
        auto collection_config = parse_collection_content(read_file(collection_path), locale, table_id);
        if (table_id.empty()) {
            // No usable `id` -- skip rather than fail the whole config (htd_like's
            // "missing collection id" tolerance).
            continue;
        }
        collection_config.meta.display_order = static_cast<int64_t>(index);
        config.collections[table_id] = std::move(collection_config);
    }

    // Enumerate the directory only to record unlisted files (D-23) -- PARSE-01 forbids loading
    // by directory scan, not listing it; recording is what makes Phase 5's VALID-05 reachable.
    std::set<std::string> listed(collection_files.begin(), collection_files.end());
    if (fs::exists(dir) && fs::is_directory(dir)) {
        for (const auto& entry : fs::directory_iterator(dir)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".toml") {
                continue;
            }
            auto stem = entry.path().stem().string();
            if (stem == "main" || stem == "enum" || listed.count(stem)) {
                continue;
            }
            config.unlisted_files.push_back(stem);
        }
        std::sort(config.unlisted_files.begin(), config.unlisted_files.end());
    }

    return config;
}

const UIMetadata*
find_attribute(const UIConfigSet& config, const std::string& collection, const std::string& attribute) {
    auto collection_it = config.collections.find(collection);
    if (collection_it == config.collections.end()) {
        return nullptr;
    }
    auto attribute_it = collection_it->second.attributes.find(attribute);
    if (attribute_it == collection_it->second.attributes.end()) {
        return nullptr;
    }
    return &attribute_it->second;
}

const std::vector<UIEnumEntry>* find_vocabulary(const UIConfigSet& config, const std::string& name) {
    auto it = config.vocabularies.find(name);
    if (it == config.vocabularies.end()) {
        return nullptr;
    }
    return &it->second;
}

// Database::Impl::require_ui_config() -- the one non-inline Impl method (declared in
// database_impl.h). Lazily loads and caches the UI sidecar exactly like require_schema/
// load_schema_metadata (D-20/D-21), except it swallows a failure instead of propagating it: a
// malformed sidecar must never turn a describe() call into a throwing one (D-25/PARSE-12).
void Database::Impl::require_ui_config() const {
    if (ui_load_attempted) {
        return;
    }
    ui_load_attempted = true;

    // Checked before any directory computation: an in-memory path has no filesystem directory
    // to derive one from, and any cwd-fallback copied from create_database_logger's would make
    // every in-memory describe test sensitive to the process's working directory (see
    // anti-pattern in 01-RESEARCH.md).
    if (path == ":memory:") {
        logger->debug("No UI config for in-memory database");
        return;
    }

    namespace fs = std::filesystem;
    const auto ui_dir = fs::path(path).parent_path() / "ui";
    if (!fs::exists(ui_dir) || !fs::is_directory(ui_dir)) {
        logger->debug("No UI config at {}", ui_dir.string());
        return;
    }

    try {
        // The assignment is the last statement inside the try, so a throw anywhere in the walk
        // publishes nothing (D-25): ui_config stays nullopt rather than half-loaded.
        ui_config = UIConfigSet::from_directory(ui_dir.string(), "en");
    } catch (const std::exception& e) {
        logger->warn("Failed to load UI config at {}: {}", ui_dir.string(), e.what());
    }
}

}  // namespace quiver
