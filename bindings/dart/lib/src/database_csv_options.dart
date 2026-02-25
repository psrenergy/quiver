part of 'database.dart';

/// Helper to populate a [quiver_csv_options_t] from Dart maps.
void _fillCSVOptions(
  Pointer<quiver_csv_options_t> optsPtr,
  Arena arena, {
  Map<String, Map<String, Map<String, int>>>? enumLabels,
  String? dateTimeFormat,
}) {
  // date_time_format: empty string means no formatting
  optsPtr.ref.date_time_format = (dateTimeFormat ?? '').toNativeUtf8(allocator: arena).cast();

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
      cLocaleNames[i] = groupLocaleNames[i].toNativeUtf8(allocator: arena).cast();
      cEntryCounts[i] = groupEntryCounts[i];
    }

    final totalEntries = allLabels.length;
    final cLabels = arena<Pointer<Char>>(totalEntries);
    final cValues = arena<Int64>(totalEntries);
    for (var i = 0; i < totalEntries; i++) {
      cLabels[i] = allLabels[i].toNativeUtf8(allocator: arena).cast();
      cValues[i] = allValues[i];
    }

    optsPtr.ref.enum_attribute_names = cAttrNames;
    optsPtr.ref.enum_locale_names = cLocaleNames;
    optsPtr.ref.enum_entry_counts = cEntryCounts;
    optsPtr.ref.enum_labels = cLabels;
    optsPtr.ref.enum_values = cValues;
    optsPtr.ref.enum_group_count = groupCount;
  } else {
    optsPtr.ref.enum_attribute_names = nullptr;
    optsPtr.ref.enum_locale_names = nullptr;
    optsPtr.ref.enum_entry_counts = nullptr;
    optsPtr.ref.enum_labels = nullptr;
    optsPtr.ref.enum_values = nullptr;
    optsPtr.ref.enum_group_count = 0;
  }
}
