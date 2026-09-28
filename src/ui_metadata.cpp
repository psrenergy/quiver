#include "ui_metadata.h"

#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <toml++/toml.hpp>
#include <utility>

namespace quiver {

namespace {

namespace fs = std::filesystem;

// The one file read in this translation unit, shared by the vocabulary pass and the collection
// pass so their failure behaviour cannot drift. `ostringstream << rdbuf()` is the house idiom
// (`Migration::up_sql`, src/migration.cpp) and is the reason the stream state is checked *after*
// the read as well as before it: an `istreambuf_iterator` slurp stops on a read error exactly as
// it stops at EOF, and a truncated TOML prefix is usually still syntactically valid -- so a
// half-read sidecar would have parsed as a complete one and silently lost every attribute past
// the cut. Both throws land in the caller's per-file catch and become one warning.
toml::table parse_toml_file(const fs::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open UI metadata file: " + path.string());
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    if (file.bad()) {
        throw std::runtime_error("Failed to read UI metadata file: " + path.string());
    }
    return toml::parse(buffer.str());
}

// Serves label, tooltip and each enum.toml entry's own label (READ-04): a plain string is used
// as-is; a table is read at its "en" sub-key. Anything else (missing key, wrong shape, no "en")
// degrades to nullopt rather than throwing -- there is no in-repo precedent for this exact
// string-or-table branch (RESEARCH.md Pattern 3), so every read here stays optional-checked.
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

// enum.toml has no wrapper key: each top-level key IS itself a vocabulary name ([[bool]],
// [[initial_volume_type]], ...) and its value is an array of {id, label} tables. Discovered by
// iterating the whole top-level table rather than reading one fixed array key (RESEARCH.md
// Pattern 4, which corrects CONTEXT.md's "[[vocab]]" shorthand). A duplicated id inside one
// vocabulary resolves to the later entry (map assignment in file order); an entry with no id, a
// non-integer id, or no readable label is dropped.
std::map<std::string, std::map<int64_t, std::string>> parse_vocabularies(const toml::table& tbl) {
    std::map<std::string, std::map<int64_t, std::string>> vocabularies;
    for (auto&& [key, node] : tbl) {
        const auto* arr = node.as_array();
        if (!arr) {
            continue;
        }
        std::map<int64_t, std::string> entries;
        for (auto& elem : *arr) {
            const auto* entry_tbl = elem.as_table();
            if (!entry_tbl) {
                continue;
            }
            auto id = (*entry_tbl)["id"].value<int64_t>();
            if (!id) {
                continue;
            }
            auto label = read_localized(entry_tbl->get("label"));
            if (!label) {
                continue;
            }
            entries[*id] = *label;
        }
        vocabularies[std::string(key.str())] = std::move(entries);
    }
    return vocabularies;
}

// One ui/*.toml collection file. Returns nullopt when the file's shape does not self-select as a
// collection file (READ-02/D-17): a non-empty top-level string `id` and an `attribute` array are
// both required -- this is what excludes main.toml (no `id` key) and every theme file, with no
// filename translated into a table name. Reads [[attribute]] only, never [[attribute_group]]
// (D-18). A repeated attribute id resolves to the later entry.
std::optional<std::pair<std::string, std::map<std::string, UiAttribute>>>
parse_collection_file(const toml::table& tbl,
                      const std::map<std::string, std::map<int64_t, std::string>>& vocabularies) {
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
        // Join key is the attribute's own `enum` value, never its `id` (D-19) -- 46 corpus
        // attributes share the `bool` vocabulary, so joining by attribute id would give each of
        // them a different, wrong vocabulary or none. An `enum` value naming nothing leaves the
        // map default-constructed (empty).
        if (auto vocab_name = (*attr_tbl)["enum"].value<std::string>()) {
            auto vocab_it = vocabularies.find(*vocab_name);
            if (vocab_it != vocabularies.end()) {
                meta.enum_labels = vocab_it->second;
            }
        }
        attrs[*attr_id] = std::move(meta);
    }
    return std::make_pair(*id, std::move(attrs));
}

}  // namespace

UiMetadata load_ui_metadata(const std::string& migrations_path, spdlog::logger& logger) {
    UiMetadata metadata;
    try {
        const fs::path ui_dir = fs::weakly_canonical(fs::path(migrations_path)).parent_path() / "ui";
        if (!fs::is_directory(ui_dir)) {
            // An absent sidecar is the normal case (SAFE-01), not a degradation -- no warning.
            return metadata;
        }

        // Vocabulary pass, before the collection pass: enum.toml's own inner try/catch, so a
        // malformed vocabulary file costs only the enum clauses -- the collection pass still runs.
        std::map<std::string, std::map<int64_t, std::string>> vocabularies;
        const fs::path enum_path = ui_dir / "enum.toml";
        if (fs::is_regular_file(enum_path)) {
            try {
                vocabularies = parse_vocabularies(parse_toml_file(enum_path));
            } catch (const std::exception& ex) {
                logger.warn("Failed to load UI metadata from '{}': {}", enum_path.string(), ex.what());
            }
        }

        // Collection pass: non-recursive scan, each file in its own inner try/catch so one
        // malformed ui/*.toml costs only its own collection.
        for (const auto& dir_entry : fs::directory_iterator(ui_dir)) {
            if (dir_entry.path().extension() != ".toml") {
                continue;
            }
            // `error_code` overload, not the throwing one: this test sits outside the per-file
            // try below, so a single unstattable entry (a locked or racing file, an EACCES on a
            // component) would otherwise unwind into the *outer* catch and discard every
            // collection already parsed -- the opposite of the per-file degradation this loop
            // exists to provide.
            std::error_code ec;
            if (!dir_entry.is_regular_file(ec) || ec) {
                continue;
            }
            // Already consumed by the vocabulary pass above -- skipping avoids a second parse and
            // a duplicated warning (it would also be rejected by the shape gate, which stays the
            // only *selector*).
            if (dir_entry.path().filename() == "enum.toml") {
                continue;
            }
            try {
                auto parsed = parse_collection_file(parse_toml_file(dir_entry.path()), vocabularies);
                if (parsed) {
                    // Two files may legally declare the same collection `id` -- selection is by
                    // shape, never by filename -- and `directory_iterator` order is unspecified,
                    // so the winner would differ between filesystems with nothing to see it.
                    // Last-in still wins (a stale copy is as likely to be first as last), but the
                    // collision is now diagnosable.
                    auto [it, inserted] = metadata.collections.try_emplace(parsed->first, std::move(parsed->second));
                    if (!inserted) {
                        logger.warn("Duplicate UI metadata for collection '{}' in '{}': replacing the earlier file",
                                    parsed->first,
                                    dir_entry.path().string());
                        it->second = std::move(parsed->second);
                    }
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
    return metadata;
}

}  // namespace quiver
