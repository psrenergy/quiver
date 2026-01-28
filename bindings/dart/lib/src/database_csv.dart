part of 'database.dart';

/// CSV import/export operations for Database.
extension DatabaseCSV on Database {
  /// Exports a table to a CSV file.
  void exportToCsv(String table, String path) {
    _ensureNotClosed();
    final arena = Arena();
    try {
      final err = bindings.quiver_database_export_to_csv(
        _ptr,
        table.toNativeUtf8(allocator: arena).cast(),
        path.toNativeUtf8(allocator: arena).cast(),
      );
      if (err != quiver_error_t.QUIVER_OK) {
        throw DatabaseException.fromError(err, "Failed to export table '$table' to '$path'");
      }
    } finally {
      arena.releaseAll();
    }
  }

  /// Imports a CSV file into a table.
  void importCsv(String table, String path) {
    _ensureNotClosed();
    final arena = Arena();
    try {
      final err = bindings.quiver_database_import_csv(
        _ptr,
        table.toNativeUtf8(allocator: arena).cast(),
        path.toNativeUtf8(allocator: arena).cast(),
      );
      if (err != quiver_error_t.QUIVER_OK) {
        throw DatabaseException.fromError(err, "Failed to import CSV from '$path' into table '$table'");
      }
    } finally {
      arena.releaseAll();
    }
  }
}
