part of 'database.dart';

/// CSV export operations for Database.
extension DatabaseCSVExport on Database {
  /// Exports a collection to a CSV file.
  ///
  /// [collection] is the collection name. [group] is the group name
  /// (empty string "" for scalar export). [path] is the output file path.
  ///
  /// Optional named parameters:
  /// - [enumLabels]: Maps attribute names to locale-keyed (label -> value) pairs.
  ///   Example: `{'status': {'en': {'Active': 1, 'Inactive': 2}}}`
  /// - [dateTimeFormat]: strftime format string for DateTime columns.
  ///   Example: `'%Y/%m/%d'`
  void exportCSV(
    String collection,
    String group,
    String path, {
    Map<String, Map<String, Map<String, int>>>? enumLabels,
    String? dateTimeFormat,
  }) {
    _ensureNotClosed();
    final arena = Arena();
    try {
      final optsPtr = arena<quiver_csv_options_t>();
      _fillCSVOptions(optsPtr, arena, enumLabels: enumLabels, dateTimeFormat: dateTimeFormat);

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
}
