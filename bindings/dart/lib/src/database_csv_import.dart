part of 'database.dart';

/// CSV import operations for Database.
extension DatabaseCSVImport on Database {
  /// Imports a CSV file into a collection.
  ///
  /// [collection] is the collection name. [group] is the group name
  /// (empty string "" for scalar import). [path] is the input file path.
  ///
  /// Optional named parameters:
  /// - [enumLabels]: Maps attribute names to locale-keyed (label -> value) pairs.
  ///   Example: `{'status': {'en': {'Active': 1, 'Inactive': 2}}}`
  /// - [dateTimeFormat]: strftime format string for DateTime columns.
  ///   Example: `'%Y/%m/%d'`
  void importCSV(
    String collection,
    String group,
    String path, {
    Map<String, Map<String, Map<String, int>>>? enumLabels,
    String? dateTimeFormat,
  }) {
    _ensureNotClosed();
    final arena = Arena();
    try {
      final optionsPtr = arena<quiver_csv_options_t>();
      _fillCSVOptions(
        optionsPtr,
        arena,
        enumLabels: enumLabels,
        dateTimeFormat: dateTimeFormat,
      );

      check(
        bindings.quiver_database_import_csv(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          group.toNativeUtf8(allocator: arena).cast(),
          path.toNativeUtf8(allocator: arena).cast(),
          optionsPtr,
        ),
      );
    } finally {
      arena.releaseAll();
    }
  }
}
