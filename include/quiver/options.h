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

struct QUIVER_API CSVOptions {
    std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_map<std::string, int64_t>>>
        enum_labels;               // attribute -> locale -> label -> value
    std::string date_time_format;  // strftime format string; empty = no formatting
};

inline CSVOptions default_csv_options() {
    return {};
}

}  // namespace quiver

#endif  // QUIVER_OPTIONS_H
