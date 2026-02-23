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

typedef struct {
    int read_only;
    quiver_log_level_t console_level;
} quiver_database_options_t;

// CSV export options for controlling enum resolution and date formatting.
// All pointers are borrowed -- caller owns the memory, function reads during call only.
//
// Enum labels map attribute names to (integer_value -> string_label) pairs.
// Represented as grouped-by-attribute parallel arrays:
//   enum_attribute_names[i]  = attribute name for group i
//   enum_entry_counts[i]     = number of entries in group i
//   enum_values[]            = all integer values, concatenated across groups
//   enum_labels[]            = all string labels, concatenated across groups
//   enum_attribute_count     = number of attribute groups (0 = no enum mapping)
//
// Example: {"status": {1: "Active", 2: "Inactive"}, "priority": {0: "Low"}}
//   enum_attribute_names = ["status", "priority"]
//   enum_entry_counts    = [2, 1]
//   enum_values          = [1, 2, 0]
//   enum_labels          = ["Active", "Inactive", "Low"]
//   enum_attribute_count = 2
typedef struct {
    const char* date_time_format;             // strftime format; "" = no formatting
    const char* const* enum_attribute_names;  // [enum_attribute_count]
    const size_t* enum_entry_counts;          // [enum_attribute_count]
    const int64_t* enum_values;               // [sum of enum_entry_counts]
    const char* const* enum_labels;           // [sum of enum_entry_counts]
    size_t enum_attribute_count;              // number of attributes with enum mappings
} quiver_csv_export_options_t;

// Returns default options (no enum mapping, no date formatting).
QUIVER_C_API quiver_csv_export_options_t quiver_csv_export_options_default(void);

// CSV import options for controlling enum resolution and date formatting during import.
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
} quiver_csv_import_options_t;

QUIVER_C_API quiver_csv_import_options_t quiver_csv_import_options_default(void);

#ifdef __cplusplus
}
#endif

#endif  // QUIVER_C_OPTIONS_H
