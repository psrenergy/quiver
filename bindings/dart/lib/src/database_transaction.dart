part of 'database.dart';

/// Transaction control operations for Database.
extension DatabaseTransaction on Database {
  /// Begins an explicit transaction.
  void beginTransaction() {
    _ensureNotClosed();
    check(bindings.quiver_database_begin_transaction(_ptr));
  }

  /// Commits the current transaction.
  void commit() {
    _ensureNotClosed();
    check(bindings.quiver_database_commit(_ptr));
  }

  /// Rolls back the current transaction.
  void rollback() {
    _ensureNotClosed();
    check(bindings.quiver_database_rollback(_ptr));
  }

  /// Returns true if a transaction is currently active.
  bool inTransaction() {
    _ensureNotClosed();
    final arena = Arena();
    try {
      final outActive = arena<Bool>();
      check(bindings.quiver_database_in_transaction(_ptr, outActive));
      return outActive.value;
    } finally {
      arena.releaseAll();
    }
  }

  /// Executes [fn] within a transaction. Auto-commits on success,
  /// rolls back on exception (best-effort), and rethrows the original exception.
  T transaction<T>(T Function(Database) fn) {
    _ensureNotClosed();
    beginTransaction();
    try {
      final result = fn(this);
      commit();
      return result;
    } catch (e) {
      try {
        rollback();
      } catch (_) {
        // Best-effort rollback; ignore failure
      }
      rethrow;
    }
  }
}
