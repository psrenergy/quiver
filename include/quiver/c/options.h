#ifndef QUIVER_C_OPTIONS_H
#define QUIVER_C_OPTIONS_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    QUIVER_LOG_DEBUG = 0,
    QUIVER_LOG_INFO = 1,
    QUIVER_LOG_WARN = 2,
    QUIVER_LOG_ERROR = 3,
    QUIVER_LOG_OFF = 4,
} quiver_log_level_t;

// NULL on ui_config_dir means "use the <db_dir>/ui/ convention"; NULL on ui_locale means "en".
// An empty string on either field is treated identically to NULL (see convert_database_options).
// Layout is pinned by static_asserts in src/c/options.cpp -- sizeof 24 on 64-bit, offsets
// read_only@0, console_level@4, ui_config_dir@8, ui_locale@16. Every FFI binding hardcodes this
// layout; do not reorder or insert fields.
typedef struct {
    int read_only;
    quiver_log_level_t console_level;
    const char* ui_config_dir;
    const char* ui_locale;
} quiver_database_options_t;

// CSV options for controlling enum resolution and date formatting.
// All pointers are borrowed -- caller owns the memory, function reads during call only.
//
// Enum labels map attribute names to locale-keyed (string_label -> integer_value) pairs.
// Represented as grouped-by-attribute-and-locale parallel arrays:
//   enum_attribute_names[i]   = attribute name for group i
//   enum_locale_names[i]      = locale name for group i (e.g. "en", "pt")
//   enum_entry_counts[i]      = number of entries in group i
//   enum_labels[]             = all string labels, concatenated across groups
//   enum_values[]             = all integer values, concatenated across groups
//   enum_group_count          = total number of (attribute, locale) groups
//
// Example: {"status": {"en": {"Active": 1, "Inactive": 2}, "pt": {"Ativo": 1}}}
//   enum_attribute_names = ["status", "status"]
//   enum_locale_names    = ["en", "pt"]
//   enum_entry_counts    = [2, 1]
//   enum_labels          = ["Active", "Inactive", "Ativo"]
//   enum_values          = [1, 2, 1]
//   enum_group_count     = 2
typedef struct {
    const char* date_time_format;             // strftime format; "" = no formatting
    const char* const* enum_attribute_names;  // [enum_group_count]
    const char* const* enum_locale_names;     // [enum_group_count]
    const size_t* enum_entry_counts;          // [enum_group_count]
    const char* const* enum_labels;           // [sum of enum_entry_counts]
    const int64_t* enum_values;               // [sum of enum_entry_counts]
    size_t enum_group_count;                  // number of (attribute, locale) groups
} quiver_csv_options_t;

QUIVER_C_API quiver_database_options_t quiver_database_options_default(void);
QUIVER_C_API quiver_csv_options_t quiver_csv_options_default(void);

// Native sizeof of quiver_database_options_t. No parameters, plain size_t return -- Bun FFI
// cannot call a struct-by-value function (quiver_database_options_default), so this is the
// shape every binding's load-time layout assertion calls instead (D-07).
QUIVER_C_API size_t quiver_database_options_sizeof(void);

// Native sizeof of quiver_csv_options_t -- the fourth struct a binding hand-allocates a raw
// buffer for (bindings/js/src/csv.ts allocates 56 bytes by hand). Same no-parameter,
// Bun-callable size_t shape as quiver_database_options_sizeof (D-07); every load-time gate
// checks this accessor last, after options/scalar metadata/group metadata.
QUIVER_C_API size_t quiver_csv_options_sizeof(void);

#ifdef __cplusplus
}
#endif

#endif  // QUIVER_C_OPTIONS_H
