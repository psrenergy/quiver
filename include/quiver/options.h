#ifndef QUIVER_OPTIONS_H
#define QUIVER_OPTIONS_H

#include "export.h"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace quiver {

enum class LogLevel {
    Debug = 0,
    Info = 1,
    Warn = 2,
    Error = 3,
    Off = 4,
};

struct QUIVER_API DatabaseOptions {
    bool read_only = false;
    LogLevel console_level = LogLevel::Info;
};

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
