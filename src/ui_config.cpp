#include "ui_config.h"

#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <string>
#include <toml++/toml.hpp>
#include <utility>

namespace quiver {

namespace {

namespace fs = std::filesystem;

// Serves label, tooltip and (task 1-01-02) each enum.toml entry's own label (READ-04): a plain
// string is used as-is; a table is read at its "en" sub-key. Anything else (missing key, wrong
// shape, no "en") degrades to nullopt rather than throwing -- there is no in-repo precedent for
// this exact string-or-table branch (RESEARCH.md Pattern 3), so every read here stays
// optional-checked.
std::optional<std::string> read_localized(const toml::node* node) {
    if (!node) {
        return std::nullopt;
    }
    if (auto s = node->value<std::string>()) {
        return *s;
    }
    if (const auto* t = node->as_table()) {
        if (auto en = (*t)["en"].value<std::string>()) {
            return *en;
        }
    }
    return std::nullopt;
}

// enum.toml's vocabulary pass. Filled in by task 1-01-02 (enum.toml has no wrapper key: each
// top-level key IS itself a vocabulary name, discovered by iterating the whole top-level table --
// RESEARCH.md Pattern 4). Left empty here so the seam exists and every attribute's enum_labels
// stays default-constructed for this task.
std::map<std::string, std::map<int64_t, std::string>> parse_vocabularies(const toml::table&) {
    return {};
}

// One ui/*.toml collection file. Returns nullopt when the file's shape does not self-select as a
// collection file (READ-02/D-17): a non-empty top-level string `id` and an `attribute` array are
// both required -- this is what excludes main.toml (no `id` key) and every theme file, with no
// filename translated into a table name. Reads [[attribute]] only, never [[attribute_group]]
// (D-18). A repeated attribute id resolves to the later entry.
std::optional<std::pair<std::string, std::map<std::string, UiAttribute>>>
parse_collection_file(const toml::table& tbl,
                      const std::map<std::string, std::map<int64_t, std::string>>& /*vocabularies*/) {
    auto id = tbl["id"].value<std::string>();
    const auto* attributes = tbl["attribute"].as_array();
    if (!id || id->empty() || !attributes) {
        return std::nullopt;
    }

    std::map<std::string, UiAttribute> attrs;
    for (auto& elem : *attributes) {
        const auto* attr_tbl = elem.as_table();
        if (!attr_tbl) {
            continue;
        }
        auto attr_id = (*attr_tbl)["id"].value<std::string>();
        if (!attr_id || attr_id->empty()) {
            continue;
        }

        UiAttribute meta;
        if (auto label = read_localized(attr_tbl->get("label"))) {
            meta.label = *label;
        }
        if (auto tooltip = read_localized(attr_tbl->get("tooltip"))) {
            meta.tooltip = *tooltip;
        }
        // Enum join (D-19) is task 1-01-02's seam -- meta.enum_labels stays default-constructed
        // (empty) here.
        attrs[*attr_id] = std::move(meta);
    }
    return std::make_pair(*id, std::move(attrs));
}

}  // namespace

UiConfig load_ui_config(const std::string& migrations_path, spdlog::logger& logger) {
    UiConfig config;
    try {
        const fs::path ui_dir = fs::weakly_canonical(fs::path(migrations_path)).parent_path() / "ui";
        if (!fs::is_directory(ui_dir)) {
            // An absent sidecar is the normal case (SAFE-01), not a degradation -- no warning.
            return config;
        }

        // Vocabulary pass, before the collection pass: enum.toml's own inner try/catch, so a
        // malformed vocabulary file costs only the enum clauses -- the collection pass still runs.
        std::map<std::string, std::map<int64_t, std::string>> vocabularies;
        const fs::path enum_path = ui_dir / "enum.toml";
        if (fs::is_regular_file(enum_path)) {
            try {
                std::ifstream file(enum_path);
                std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                vocabularies = parse_vocabularies(toml::parse(content));
            } catch (const std::exception& ex) {
                logger.warn("Failed to load UI metadata from '{}': {}", enum_path.string(), ex.what());
            }
        }

        // Collection pass: non-recursive scan, each file in its own inner try/catch so one
        // malformed ui/*.toml costs only its own collection.
        for (const auto& dir_entry : fs::directory_iterator(ui_dir)) {
            if (!dir_entry.is_regular_file() || dir_entry.path().extension() != ".toml") {
                continue;
            }
            // Already consumed by the vocabulary pass above -- skipping avoids a second parse and
            // a duplicated warning (it would also be rejected by the shape gate, which stays the
            // only *selector*).
            if (dir_entry.path().filename() == "enum.toml") {
                continue;
            }
            try {
                std::ifstream file(dir_entry.path());
                std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                auto parsed = parse_collection_file(toml::parse(content), vocabularies);
                if (parsed) {
                    config.collections[parsed->first] = std::move(parsed->second);
                }
            } catch (const std::exception& ex) {
                logger.warn("Failed to load UI metadata from '{}': {}", dir_entry.path().string(), ex.what());
            }
        }
    } catch (const std::exception& ex) {
        // Outer catch: never fails from_migrations. Covers directory iteration itself and path
        // resolution -- anything the inner per-file catches above cannot reach. Discards any
        // partial work, following src/database.cpp's warn-and-continue posture (never
        // binary_metadata.cpp's throwing posture).
        logger.warn("Failed to load UI metadata from '{}': {}", migrations_path, ex.what());
        return {};
    }
    return config;
}

}  // namespace quiver
