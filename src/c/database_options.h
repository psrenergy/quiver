#ifndef QUIVER_C_DATABASE_OPTIONS_H
#define QUIVER_C_DATABASE_OPTIONS_H

#include "quiver/c/options.h"
#include "quiver/options.h"

#include <string>

inline quiver::CSVOptions convert_options(const quiver_csv_options_t* opts) {
    quiver::CSVOptions cpp_opts;
    cpp_opts.date_time_format = opts->date_time_format ? opts->date_time_format : "";

    size_t offset = 0;
    for (size_t i = 0; i < opts->enum_group_count; ++i) {
        std::string attr_name = opts->enum_attribute_names[i];
        std::string locale_name = opts->enum_locale_names[i];
        auto& locale_map = cpp_opts.enum_labels[attr_name][locale_name];
        for (size_t j = 0; j < opts->enum_entry_counts[i]; ++j) {
            locale_map[opts->enum_labels[offset + j]] = opts->enum_values[offset + j];
        }
        offset += opts->enum_entry_counts[i];
    }

    return cpp_opts;
}

inline quiver_csv_options_t csv_options_default() {
    quiver_csv_options_t opts = {};
    opts.date_time_format = "";
    return opts;
}

#endif
