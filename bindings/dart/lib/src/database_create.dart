part of 'database.dart';

/// Create operations for Database.
extension DatabaseCreate on Database {
  /// Creates a new element in the specified collection.
  int createElement(String collection, Map<String, Object?> values) {
    _ensureNotClosed();

    final element = Element();
    try {
      for (final entry in values.entries) {
        element.set(entry.key, entry.value);
      }
      return createElementFromBuilder(collection, element);
    } finally {
      element.dispose();
    }
  }

  /// Creates a new element in the specified collection using an Element builder.
  int createElementFromBuilder(String collection, Element element) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outId = arena<Int64>();

      check(
        bindings.quiver_database_create_element(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          element.ptr.cast(),
          outId,
        ),
      );

      return outId.value;
    } finally {
      arena.releaseAll();
    }
  }
}
