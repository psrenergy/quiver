#ifndef QUIVER_UI_METADATA_H
#define QUIVER_UI_METADATA_H

#include "export.h"

#include <cstdint>
#include <string>

namespace quiver {

// Public record for the PSR `<db_dir>/ui/` TOML sidecar (Phase 3, META-01). Moved verbatim out of
// the private src/ui_config.h -- see get_attribute_ui_metadata / list_ui_vocabularies /
// get_ui_vocabulary on Database (include/quiver/database.h).

// One entry in an enum vocabulary (ui/enum.toml's [[<name>]] array-of-tables). Returned by
// Database::get_ui_vocabulary.
struct QUIVER_API UIEnumEntry {
    int64_t code = 0;
    std::string label;
};

// One record answers for a collection, an attribute, or a group (D-09) -- the same field set
// used privately by src/ui_config.h before the Phase 3 move, so publishing it is a header move,
// not a redesign. `configured` is the discriminator between "the sidecar declares this" and
// "nothing declared" -- `label` is never back-filled from an id (D-12). Returned by
// Database::get_attribute_ui_metadata.
struct QUIVER_API UIMetadata {
    bool configured = false;
    std::string label;
    std::string tooltip;  // Read by get_attribute_ui_metadata (Phase 3, META-01).
    std::string unit;
    // A plain TOML string is stored verbatim. A 4-key TOML table (`element_view`/
    // `collection_view`/`edit`/`data`) collapses to the first present of, in order, `data`,
    // `element_view`, `collection_view`, `edit` -- verbatim, unclassified (PARSE-06, D-14/D-32).
    // The other three keys, when present, are discarded irrecoverably -- there is no Quiver
    // surface that can read them back.
    std::string format;
    std::string icon;  // Read by get_attribute_ui_metadata (Phase 3, META-01).
    bool hidden = false;
    std::string vocabulary;      // unresolved name; resolved via Database::get_ui_vocabulary()
    int64_t display_order = -1;  // Phase 4 (GROUP-02): unread until then, carried for the header-move design.
};

}  // namespace quiver

#endif  // QUIVER_UI_METADATA_H
