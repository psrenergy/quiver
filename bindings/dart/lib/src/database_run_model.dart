part of 'database.dart';

/// Model execution operations for Database.
extension DatabaseRunModel on Database {
  /// Runs the fixed external model on this database's file.
  ///
  /// Returns the model process exit code. Throws on an in-memory database, on an
  /// active transaction, or if the model script is missing/unlaunchable.
  int runModel() {
    _ensureNotClosed();
    final arena = Arena();
    try {
      final outExitCode = arena<Int>();
      check(bindings.quiver_database_run_model(_ptr, outExitCode));
      return outExitCode.value;
    } finally {
      arena.releaseAll();
    }
  }
}
