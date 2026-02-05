part of 'database.dart';

/// Delete operations for Database.
extension DatabaseDelete on Database {
  /// Deletes an element by ID from a collection.
  /// CASCADE DELETE handles cleanup of related vector/set tables.
  void deleteElementById(String collection, int id) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      check(bindings.quiver_database_delete_element_by_id(
        _ptr,
        collection.toNativeUtf8(allocator: arena).cast(),
        id,
      ));
    } finally {
      arena.releaseAll();
    }
  }
}
