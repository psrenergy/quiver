part of 'database.dart';

/// Relation operations for Database.
extension DatabaseRelations on Database {
  /// Sets a scalar relation (foreign key) between two elements by their labels.
  void updateScalarRelation(String collection, String attribute, String fromLabel, String toLabel) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      check(
        bindings.quiver_database_update_scalar_relation(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          attribute.toNativeUtf8(allocator: arena).cast(),
          fromLabel.toNativeUtf8(allocator: arena).cast(),
          toLabel.toNativeUtf8(allocator: arena).cast(),
        ),
      );
    } finally {
      arena.releaseAll();
    }
  }

  /// Reads scalar relation values (target labels) for a FK attribute.
  /// Returns null for elements with no relation set.
  List<String?> readScalarRelation(String collection, String attribute) {
    _ensureNotClosed();

    final arena = Arena();
    try {
      final outValues = arena<Pointer<Pointer<Char>>>();
      final outCount = arena<Size>();

      check(
        bindings.quiver_database_read_scalar_relation(
          _ptr,
          collection.toNativeUtf8(allocator: arena).cast(),
          attribute.toNativeUtf8(allocator: arena).cast(),
          outValues,
          outCount,
        ),
      );

      final count = outCount.value;
      if (count == 0 || outValues.value == nullptr) {
        return [];
      }

      final result = <String?>[];
      for (var i = 0; i < count; i++) {
        final ptr = outValues.value[i];
        if (ptr == nullptr) {
          result.add(null);
        } else {
          final s = ptr.cast<Utf8>().toDartString();
          result.add(s.isEmpty ? null : s);
        }
      }
      bindings.quiver_database_free_string_array(outValues.value, count);
      return result;
    } finally {
      arena.releaseAll();
    }
  }
}
