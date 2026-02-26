part of 'database.dart';

/// Helper to populate a [quiver_csv_options_t] from Dart maps.
void _fillCSVOptions(
  Pointer<quiver_csv_options_t> optionsPtr,
  Arena arena, {
  Map<String, Map<String, Map<String, int>>>? enumLabels,
  String? dateTimeFormat,
}) {
  // date_time_format: empty string means no formatting
  optionsPtr.ref.date_time_format = (dateTimeFormat ?? '')
      .toNativeUtf8(allocator: arena)
      .cast();

  if (enumLabels != null && enumLabels.isNotEmpty) {
    // Flatten attribute -> locale -> entries into grouped parallel arrays
    final groupAttrNames = <String>[];
    final groupLocaleNames = <String>[];
    final groupEntryCounts = <int>[];
    final allLabels = <String>[];
    final allValues = <int>[];

    for (final attr in enumLabels.entries) {
      for (final locale in attr.value.entries) {
        groupAttrNames.add(attr.key);
        groupLocaleNames.add(locale.key);
        groupEntryCounts.add(locale.value.length);
        for (final entry in locale.value.entries) {
          allLabels.add(entry.key);
          allValues.add(entry.value);
        }
      }
    }

    final groupCount = groupAttrNames.length;
    final cAttrNames = arena<Pointer<Char>>(groupCount);
    final cLocaleNames = arena<Pointer<Char>>(groupCount);
    final cEntryCounts = arena<Size>(groupCount);

    for (var i = 0; i < groupCount; i++) {
      cAttrNames[i] = groupAttrNames[i].toNativeUtf8(allocator: arena).cast();
      cLocaleNames[i] = groupLocaleNames[i]
          .toNativeUtf8(allocator: arena)
          .cast();
      cEntryCounts[i] = groupEntryCounts[i];
    }

    final totalEntries = allLabels.length;
    final cLabels = arena<Pointer<Char>>(totalEntries);
    final cValues = arena<Int64>(totalEntries);
    for (var i = 0; i < totalEntries; i++) {
      cLabels[i] = allLabels[i].toNativeUtf8(allocator: arena).cast();
      cValues[i] = allValues[i];
    }

    optionsPtr.ref.enum_attribute_names = cAttrNames;
    optionsPtr.ref.enum_locale_names = cLocaleNames;
    optionsPtr.ref.enum_entry_counts = cEntryCounts;
    optionsPtr.ref.enum_labels = cLabels;
    optionsPtr.ref.enum_values = cValues;
    optionsPtr.ref.enum_group_count = groupCount;
  } else {
    optionsPtr.ref.enum_attribute_names = nullptr;
    optionsPtr.ref.enum_locale_names = nullptr;
    optionsPtr.ref.enum_entry_counts = nullptr;
    optionsPtr.ref.enum_labels = nullptr;
    optionsPtr.ref.enum_values = nullptr;
    optionsPtr.ref.enum_group_count = 0;
  }
}
