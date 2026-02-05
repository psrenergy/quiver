part of 'database.dart';

/// CSV import/export operations for Database.
extension DatabaseCSV on Database {
  /// Exports a table to a CSV file.
  void exportToCSV(String table, String path) {
    _ensureNotClosed();
    final arena = Arena();
    try {
      check(bindings.quiver_database_export_to_csv(
        _ptr,
        table.toNativeUtf8(allocator: arena).cast(),
        path.toNativeUtf8(allocator: arena).cast(),
      ));
    } finally {
      arena.releaseAll();
    }
  }

  /// Imports a CSV file into a table.
  void importFromCSV(String table, String path) {
    _ensureNotClosed();
    final arena = Arena();
    try {
      check(bindings.quiver_database_import_from_csv(
        _ptr,
        table.toNativeUtf8(allocator: arena).cast(),
        path.toNativeUtf8(allocator: arena).cast(),
      ));
    } finally {
      arena.releaseAll();
    }
  }
}
