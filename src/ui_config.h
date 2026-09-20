#ifndef QUIVER_UI_CONFIG_H
#define QUIVER_UI_CONFIG_H

#include "quiver/ui_metadata.h"

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
//
// UIEnumEntry / UIMetadata moved to the public quiver/ui_metadata.h in Phase 3 (D-30) -- a header
// move, not a redesign. UIConfigSet stays private (D-18): it aggregates std::map members that
// would otherwise leak into the ABI.

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
    static std::map<std::string, std::vector<UIEnumEntry>>
    parse_enum_content(const std::string& content, const std::string& locale);
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
