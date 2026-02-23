part of 'database.dart';

/// CSV import/export operations for Database.
extension DatabaseCSV on Database {
  /// Exports a collection to a CSV file.
  ///
  /// [collection] is the collection name. [group] is the group name
  /// (empty string "" for scalar export). [path] is the output file path.
  ///
  /// Optional named parameters:
  /// - [enumLabels]: Maps attribute names to (integer value -> string label) pairs.
  ///   Example: `{'status': {1: 'Active', 2: 'Inactive'}}`
  /// - [dateTimeFormat]: strftime format string for DateTime columns.
  ///   Example: `'%Y/%m/%d'`
  void exportCSV(
    String collection,
    String group,
    String path, {
    Map<String, Map<int, String>>? enumLabels,
    String? dateTimeFormat,
  }) {
    _ensureNotClosed();
    final arena = Arena();
    try {
      final optsPtr = arena<quiver_csv_export_options_t>();

      // date_time_format: empty string means no formatting
      optsPtr.ref.date_time_format = (dateTimeFormat ?? '').toNativeUtf8(allocator: arena).cast();

      if (enumLabels != null && enumLabels.isNotEmpty) {
        final attrCount = enumLabels.length;
        final attrNames = arena<Pointer<Char>>(attrCount);
        final entryCounts = arena<Size>(attrCount);

        // Count total entries across all attributes
        var totalEntries = 0;
        for (final entries in enumLabels.values) {
          totalEntries += entries.length;
        }

        final allValues = arena<Int64>(totalEntries);
        final allLabels = arena<Pointer<Char>>(totalEntries);

        var attrIdx = 0;
        var entryIdx = 0;
        for (final attr in enumLabels.entries) {
          attrNames[attrIdx] = attr.key.toNativeUtf8(allocator: arena).cast();
          entryCounts[attrIdx] = attr.value.length;

          for (final entry in attr.value.entries) {
            allValues[entryIdx] = entry.key;
            allLabels[entryIdx] = entry.value.toNativeUtf8(allocator: arena).cast();
            entryIdx++;
          }
          attrIdx++;
        }

        optsPtr.ref.enum_attribute_names = attrNames;
        optsPtr.ref.enum_entry_counts = entryCounts;
        optsPtr.ref.enum_values = allValues;
        optsPtr.ref.enum_labels = allLabels;
        optsPtr.ref.enum_attribute_count = attrCount;
      } else {
        optsPtr.ref.enum_attribute_names = nullptr;
        optsPtr.ref.enum_entry_counts = nullptr;
        optsPtr.ref.enum_values = nullptr;
        optsPtr.ref.enum_labels = nullptr;
        optsPtr.ref.enum_attribute_count = 0;
      }

      check(
        bindings.quiver_database_export_csv(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          group.toNativeUtf8(allocator: arena).cast(),
          path.toNativeUtf8(allocator: arena).cast(),
          optsPtr,
        ),
      );
    } finally {
      arena.releaseAll();
    }
  }

  /// Imports a CSV file into a table.
  void importCSV(String table, String path) {
    _ensureNotClosed();
    final arena = Arena();
    try {
      check(
        bindings.quiver_database_import_csv(
          _ptr,
          table.toNativeUtf8(allocator: arena).cast(),
          path.toNativeUtf8(allocator: arena).cast(),
        ),
      );
    } finally {
      arena.releaseAll();
    }
  }
}
