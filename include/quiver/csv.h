#ifndef QUIVER_CSV_H
#define QUIVER_CSV_H

#include "export.h"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace quiver {

struct QUIVER_API CSVExportOptions {
    std::unordered_map<std::string, std::unordered_map<int64_t, std::string>> enum_labels;
    std::string date_time_format;  // strftime format string; empty = no formatting
};

inline CSVExportOptions default_csv_export_options() {
    return {};
}

}  // namespace quiver

#endif  // QUIVER_CSV_H
