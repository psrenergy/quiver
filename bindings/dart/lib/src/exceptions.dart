/// Base exception for Margaux errors.
sealed class DatabaseException implements Exception {
  final String message;
  const DatabaseException(this.message);

  @override
  String toString() => 'DatabaseException: $message';

  /// Creates the appropriate exception from an error code.
  factory DatabaseException.fromError(int errorCode, [String? context]) {
    return switch (errorCode) {
      -1 => InvalidArgumentException(context ?? 'Invalid argument'),
      -2 => DatabaseOperationException(context ?? 'Database error'),
      -3 => MigrationException(context ?? 'Migration error'),
      -4 => SchemaException(context ?? 'Schema error'),
      -5 => CreateElementException(context ?? 'Failed to create element'),
      -6 => NotFoundException(context ?? 'Not found'),
      _ => UnknownDatabaseException('Unknown error: $errorCode'),
    };
  }
}

class InvalidArgumentException extends DatabaseException {
  const InvalidArgumentException(super.message);
}

class DatabaseOperationException extends DatabaseException {
  const DatabaseOperationException(super.message);
}

class MigrationException extends DatabaseException {
  const MigrationException(super.message);
}

class SchemaException extends DatabaseException {
  const SchemaException(super.message);
}

class CreateElementException extends DatabaseException {
  const CreateElementException(super.message);
}

class NotFoundException extends DatabaseException {
  const NotFoundException(super.message);
}

class UnknownDatabaseException extends DatabaseException {
  const UnknownDatabaseException(super.message);
}

class LuaException extends DatabaseException {
  const LuaException(super.message);
}
