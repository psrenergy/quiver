#ifndef QUIVER_C_DATABASE_OPTIONS_H
#define QUIVER_C_DATABASE_OPTIONS_H

#include "quiver/c/options.h"
#include "quiver/options.h"

#include <string>

inline quiver::DatabaseOptions convert_database_options(const quiver_database_options_t& c_opts) {
    // A NULL *or* empty ui_config_dir means "not specified" (convention path); a NULL *or*
    // empty ui_locale means "not specified" ("en"). Guarding before construction matters here:
    // constructing a std::string from a null const char* is undefined behavior.
    const std::string ui_config_dir = c_opts.ui_config_dir ? c_opts.ui_config_dir : "";
    const std::string ui_locale = (c_opts.ui_locale && c_opts.ui_locale[0] != '\0') ? c_opts.ui_locale : "en";
    return {
        .read_only = c_opts.read_only != 0,
        .console_level = static_cast<quiver::LogLevel>(c_opts.console_level),
        .ui_config_dir = ui_config_dir,
        .ui_locale = ui_locale,
    };
}

inline quiver::CSVOptions convert_csv_options(const quiver_csv_options_t* options) {
    quiver::CSVOptions cpp_options;
    cpp_options.date_time_format = options->date_time_format ? options->date_time_format : "";

    size_t offset = 0;
    for (size_t i = 0; i < options->enum_group_count; ++i) {
        std::string attr_name = options->enum_attribute_names[i];
        std::string locale_name = options->enum_locale_names[i];
        auto& locale_map = cpp_options.enum_labels[attr_name][locale_name];
        for (size_t j = 0; j < options->enum_entry_counts[i]; ++j) {
            locale_map[options->enum_labels[offset + j]] = options->enum_values[offset + j];
        }
        offset += options->enum_entry_counts[i];
    }

    return cpp_options;
}

#endif
