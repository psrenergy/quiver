#ifndef QUIVER_UI_CONFIG_H
#define QUIVER_UI_CONFIG_H

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace spdlog {
class logger;
}  // namespace spdlog

namespace quiver {

// Private parser output for the PSR `<db_dir>/ui/` TOML sidecar (D-18: never under include/,
// never exported from the shared library -- it aggregates std::map members that would otherwise
// put std::map in the ABI, and it is a second route to the same data the Phase-3 public getters
// expose).

// One entry in an enum vocabulary (ui/enum.toml's [[<name>]] array-of-tables).
struct UIEnumEntry {
    int64_t code = 0;
    std::string label;
};

// One record answers for a collection, an attribute, or a group (D-09) -- the same field set as
// the public Phase-3 type, so that phase is a header move, not a redesign. `configured` is the
// discriminator between "the sidecar declares this" and "nothing declared" -- `label` is never
// back-filled from an id (D-12).
struct UIMetadata {
    bool configured = false;
    std::string label;
    std::string tooltip;  // Phase 3 (META-01): unread in Phase 1, carried for the header-move design.
    std::string unit;
    // A plain TOML string is stored verbatim. A 4-key TOML table (`element_view`/
    // `collection_view`/`edit`/`data`) collapses to the first present of, in order, `data`,
    // `element_view`, `collection_view`, `edit` -- verbatim, unclassified (PARSE-06, D-14). Phase
    // 1's record carries one string; whether META-01 needs all four keys is Phase 3's call.
    std::string format;
    std::string icon;  // Phase 3 (META-01): unread in Phase 1, carried for the header-move design.
    bool hidden = false;
    std::string vocabulary;      // unresolved name; resolved via find_vocabulary()
    int64_t display_order = -1;  // Phase 4 (GROUP-02): unread in Phase 1, carried for the header-move design.
};

// Two separate maps because `degradation` can legally be both an attribute id and a group id
// (PARSE-07) -- one map per namespace keeps that legal.
struct UICollectionConfig {
    UIMetadata meta;
    std::map<std::string, UIMetadata> attributes;
    std::map<std::string, UIMetadata> groups;
};

// Parsed <db_dir>/ui/ sidecar.
struct UIConfigSet {
    std::string source_directory;
    std::string locale;
    std::map<std::string, UICollectionConfig> collections;         // keyed by PascalCase SQL table name (PARSE-10)
    std::map<std::string, std::vector<UIEnumEntry>> vocabularies;  // vector: enum.toml declaration order survives
    std::vector<std::string>
        unlisted_files;  // Phase 5 (VALID-05): unread in Phase 1, carried for the header-move design.

    // Reads <ui_dir>/main.toml (its `collections` array, PARSE-01) plus the optional
    // <ui_dir>/enum.toml and every listed collection file. May throw (toml::parse_error or a
    // filesystem error); Impl::require_ui_config is what catches and swallows so a malformed
    // sidecar publishes nothing (D-25). Mirrors BinaryMetadata::from_toml_file (D-19). `logger`
    // carries the once-per-file unknown-key debug line (PARSE-05, D-26) and the missing-`id`
    // skip notice (PARSE-10); it is the same per-database logger require_ui_config already holds.
    static UIConfigSet
    from_directory(const std::string& ui_dir, const std::string& locale, const std::shared_ptr<spdlog::logger>& logger);

    // Content-level halves, testable without touching disk (D-19's from_toml_file/from_toml_content
    // split). quiver_tests has no include path into src/ (D-18 keeps UIConfigSet private), so these
    // are exercised only indirectly through Database's public surface in Phase 1 -- the seam is
    // what would let a future public wrapper reuse this split without re-deriving it.
    static std::map<std::string, std::vector<UIEnumEntry>> parse_enum_content(const std::string& content);
    // `out_unknown_keys` accumulates every key this parse does not consume, at both the
    // collection level and within every [[attribute]]/[[attribute_group]] block in `content` --
    // the caller (from_directory) owns turning that list into one debug line per file (PARSE-05).
    static UICollectionConfig parse_collection_content(const std::string& content,
                                                       const std::string& locale,
                                                       std::string& out_table_id,
                                                       std::vector<std::string>& out_unknown_keys);
};

// Lookup helpers used by the renderer. Both return nullptr when absent -- neither throws.
const UIMetadata*
find_attribute(const UIConfigSet& config, const std::string& collection, const std::string& attribute);
const std::vector<UIEnumEntry>* find_vocabulary(const UIConfigSet& config, const std::string& name);

}  // namespace quiver

#endif  // QUIVER_UI_CONFIG_H
