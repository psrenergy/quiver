part of 'database.dart';

/// CSV import/export operations for Database.
extension DatabaseCSV on Database {
  /// Exports a table to a CSV file.
  void exportCSV(String table, String path) {
    _ensureNotClosed();
    final arena = Arena();
    try {
      check(
        bindings.quiver_database_export_csv(
          _ptr,
          table.toNativeUtf8(allocator: arena).cast(),
          path.toNativeUtf8(allocator: arena).cast(),
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
