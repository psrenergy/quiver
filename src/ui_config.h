#ifndef QUIVER_SRC_UI_CONFIG_H
#define QUIVER_SRC_UI_CONFIG_H

// Internal reader for the `ui/` TOML sidecar that sits beside a model's migrations directory,
// consumed only by src/database.cpp (populates it, once, in from_migrations) and
// src/database_describe.cpp (renders it into describe/describe_collection). No public
// include/quiver/ counterpart, no QUIVER_API, no C API symbol and no FFI binding: describe*
// already returns a plain std::string through the C API, so there is no FFI consumer for a
// structured getter. tomlplusplus is linked PRIVATE on the `quiver` target (src/CMakeLists.txt),
// so no `toml::` symbol may appear in this header -- the parser is confined to ui_config.cpp.

#include <cstdint>
#include <map>
#include <spdlog/spdlog.h>
#include <string>

namespace quiver {

// One scalar attribute's UI metadata. Empty string means "absent"; an empty enum_labels map means
// "no vocabulary" (or a vocabulary that resolved to nothing).
struct UiAttribute {
    std::string label;
    std::string tooltip;
    std::map<int64_t, std::string> enum_labels;
};

// Parsed sidecar: collection id -> attribute id -> metadata. A default-constructed UiConfig is
// the "no sidecar" state, and find() returning nullptr is what makes an undescribed attribute a
// no-op by construction rather than a special case in the renderer.
struct UiConfig {
    std::map<std::string, std::map<std::string, UiAttribute>> collections;

    const UiAttribute* find(const std::string& collection, const std::string& attribute) const {
        auto coll_it = collections.find(collection);
        if (coll_it == collections.end()) {
            return nullptr;
        }
        auto attr_it = coll_it->second.find(attribute);
        if (attr_it == coll_it->second.end()) {
            return nullptr;
        }
        return &attr_it->second;
    }
};

// Resolves the `ui/` sibling of `migrations_path`, parses every collection file plus `enum.toml`,
// and returns the result. Never throws: any failure (missing directory, unreadable file,
// malformed TOML) is warned via `logger` and degrades to an empty UiConfig, following the
// warn-and-degrade posture in src/database.cpp (never binary_metadata.cpp's throwing posture).
UiConfig load_ui_config(const std::string& migrations_path, spdlog::logger& logger);

}  // namespace quiver

#endif  // QUIVER_SRC_UI_CONFIG_H
