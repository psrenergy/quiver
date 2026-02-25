#ifndef QUIVER_C_DATABASE_OPTIONS_H
#define QUIVER_C_DATABASE_OPTIONS_H

#include "quiver/c/options.h"
#include "quiver/options.h"

#include <string>

inline quiver::CSVOptions convert_options(const quiver_csv_options_t* options) {
    quiver::CSVOptions cpp_opts;
    cpp_opts.date_time_format = options->date_time_format ? options->date_time_format : "";

    size_t offset = 0;
    for (size_t i = 0; i < options->enum_group_count; ++i) {
        std::string attr_name = options->enum_attribute_names[i];
        std::string locale_name = options->enum_locale_names[i];
        auto& locale_map = cpp_opts.enum_labels[attr_name][locale_name];
        for (size_t j = 0; j < options->enum_entry_counts[i]; ++j) {
            locale_map[options->enum_labels[offset + j]] = options->enum_values[offset + j];
        }
        offset += options->enum_entry_counts[i];
    }

    return cpp_opts;
}

inline quiver_csv_options_t csv_options_default() {
    quiver_csv_options_t options = {};
    options.date_time_format = "";
    return options;
}

#endif
