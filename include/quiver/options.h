#ifndef QUIVER_OPTIONS_H
#define QUIVER_OPTIONS_H

#include "export.h"
#include "quiver/c/options.h"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace quiver {

using DatabaseOptions = quiver_database_options_t;

inline DatabaseOptions default_database_options() {
    return {0, QUIVER_LOG_INFO};
}

struct QUIVER_API CSVExportOptions {
    std::unordered_map<std::string, std::unordered_map<int64_t, std::string>> enum_labels;
    std::string date_time_format;  // strftime format string; empty = no formatting
};

inline CSVExportOptions default_csv_export_options() {
    return {};
}

struct QUIVER_API CSVImportOptions {
    std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_map<std::string, int64_t>>>
        enum_labels;
    std::string date_time_format;  // strftime format string; empty = no formatting
};

inline CSVImportOptions default_csv_import_options() {
    return {};
}

}  // namespace quiver

#endif  // QUIVER_OPTIONS_H
