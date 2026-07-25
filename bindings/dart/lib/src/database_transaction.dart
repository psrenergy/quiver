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
      final outActive = arena<Int>();
      check(bindings.quiver_database_in_transaction(_ptr, outActive));
      return outActive.value != 0;
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

  /// Begins a dry run: a transaction that [endDryRun] always rolls back.
  ///
  /// While it is active, [beginTransaction]/[commit]/[rollback] are absorbed (no-ops), so code
  /// that manages its own transactions composes instead of erroring on a nested BEGIN. A nested
  /// rollback is therefore not partial -- everything is undone when the dry run ends.
  void beginDryRun() {
    _ensureNotClosed();
    check(bindings.quiver_database_begin_dry_run(_ptr));
  }

  /// Ends the active dry run, rolling back everything it covered.
  void endDryRun() {
    _ensureNotClosed();
    check(bindings.quiver_database_end_dry_run(_ptr));
  }

  /// Returns true if a dry run is currently active.
  bool inDryRun() {
    _ensureNotClosed();
    final arena = Arena();
    try {
      final outActive = arena<Int>();
      check(bindings.quiver_database_in_dry_run(_ptr, outActive));
      return outActive.value != 0;
    } finally {
      arena.releaseAll();
    }
  }

  /// Executes [fn] inside a transaction that is always rolled back, and returns its result.
  T dryRun<T>(T Function(Database) fn) {
    _ensureNotClosed();
    beginDryRun();
    try {
      return fn(this);
    } finally {
      try {
        endDryRun();
      } catch (_) {
        // Best-effort; ignore so the original exception survives
      }
    }
  }
}
