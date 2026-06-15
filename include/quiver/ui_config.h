#ifndef QUIVER_UI_CONFIG_H
#define QUIVER_UI_CONFIG_H

#include "export.h"

#include <map>
#include <string>
#include <vector>

namespace quiver {

// Reads the UI metadata that ships alongside a model database (the `ui/` folder next to
// `migrations/`). This first slice exposes the model name and per-attribute units; the remaining
// UI information (enums, labels, tooltips, cards, queries) is parsed-as-needed in later slices.
//
// Plain value type (Rule of Zero); toml++ is an implementation detail of ui_config.cpp.
class QUIVER_API UiConfig {
public:
    // Reads `<ui_dir>/main.toml` (model name) and each `<ui_dir>/<collection>.toml` (per-attribute
    // units). A missing ui_dir, or a missing main.toml, yields an empty UiConfig (no throw) — unset
    // values simply read back as empty. Malformed TOML in a file that exists throws (Pattern 3).
    static UiConfig from_directory(const std::string& ui_dir);

    // Model name from main.toml `model`; "" when unset / no UI loaded.
    const std::string& model_name() const;

    // Unit for an attribute, keyed by the collection's canonical id (e.g. "Material") and the
    // attribute id (e.g. "demand"). English-first (see ui_config.cpp). "" when the collection or
    // attribute is unknown, or the attribute has no unit.
    std::string attribute_unit(const std::string& collection, const std::string& attribute) const;

private:
    std::string model_;
    std::string extension_;                    // parsed from main.toml; not yet exposed across FFI
    std::vector<std::string> collections_;     // parsed from main.toml; not yet exposed across FFI
    std::vector<std::string> localizations_;   // drives the English-first unit fallback order
    // collection id -> attribute id -> unit (only attributes that have a non-empty unit)
    std::map<std::string, std::map<std::string, std::string>> units_;
};

}  // namespace quiver

#endif  // QUIVER_UI_CONFIG_H
