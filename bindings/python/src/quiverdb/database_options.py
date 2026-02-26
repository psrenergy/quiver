from __future__ import annotations

from quiverdb._c_api import ffi
from quiverdb.metadata import CSVOptions


def _marshal_csv_options(options: CSVOptions) -> tuple:
    """Marshal CSVOptions into C API struct pointer.

    Returns (keepalive, c_opts) where keepalive must stay referenced
    during the C API call.
    """
    keepalive: list = []
    c_opts = ffi.new("quiver_csv_options_t*")

    # date_time_format
    dtf_buf = ffi.new("char[]", options.date_time_format.encode("utf-8"))
    keepalive.append(dtf_buf)
    c_opts.date_time_format = dtf_buf

    if not options.enum_labels:
        c_opts.enum_attribute_names = ffi.NULL
        c_opts.enum_locale_names = ffi.NULL
        c_opts.enum_entry_counts = ffi.NULL
        c_opts.enum_labels = ffi.NULL
        c_opts.enum_values = ffi.NULL
        c_opts.enum_group_count = 0
        return keepalive, c_opts

    # Flatten attribute -> locale -> entries into grouped parallel arrays
    # Each (attribute, locale) pair is one group
    group_attr_names: list[str] = []
    group_locale_names: list[str] = []
    group_entry_counts: list[int] = []
    all_labels: list[str] = []
    all_values: list[int] = []

    for attr_name, locales in options.enum_labels.items():
        for locale_name, entries in locales.items():
            group_attr_names.append(attr_name)
            group_locale_names.append(locale_name)
            group_entry_counts.append(len(entries))
            for label, value in entries.items():
                all_labels.append(label)
                all_values.append(value)

    group_count = len(group_attr_names)
    total_entries = len(all_labels)

    c_attr_names = ffi.new("const char*[]", group_count)
    keepalive.append(c_attr_names)
    c_locale_names = ffi.new("const char*[]", group_count)
    keepalive.append(c_locale_names)
    c_entry_counts = ffi.new("size_t[]", group_count)
    keepalive.append(c_entry_counts)
    c_labels = ffi.new("const char*[]", total_entries)
    keepalive.append(c_labels)
    c_values = ffi.new("int64_t[]", total_entries)
    keepalive.append(c_values)

    for i in range(group_count):
        name_buf = ffi.new("char[]", group_attr_names[i].encode("utf-8"))
        keepalive.append(name_buf)
        c_attr_names[i] = name_buf
        locale_buf = ffi.new("char[]", group_locale_names[i].encode("utf-8"))
        keepalive.append(locale_buf)
        c_locale_names[i] = locale_buf
        c_entry_counts[i] = group_entry_counts[i]

    for i in range(total_entries):
        label_buf = ffi.new("char[]", all_labels[i].encode("utf-8"))
        keepalive.append(label_buf)
        c_labels[i] = label_buf
        c_values[i] = all_values[i]

    c_opts.enum_attribute_names = c_attr_names
    c_opts.enum_locale_names = c_locale_names
    c_opts.enum_entry_counts = c_entry_counts
    c_opts.enum_labels = c_labels
    c_opts.enum_values = c_values
    c_opts.enum_group_count = group_count

    return keepalive, c_opts
