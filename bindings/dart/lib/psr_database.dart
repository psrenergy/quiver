/// Dart FFI bindings for PSR Database - SQLite wrapper library.
///
/// This library provides a Dart interface to the PSR Database C library.
///
/// ## Usage
///
/// ```dart
/// import 'package:psr_database/psr_database.dart';
///
/// void main() {
///   final db = Database.fromSchema('test.db', 'schema.sql');
///   try {
///     final id = db.createElement('MyCollection', {
///       'name': 'Example',
///       'value': 42,
///     });
///     print('Created element with ID: $id');
///   } finally {
///     db.close();
///   }
/// }
/// ```
library psr_database;

export 'src/database.dart' show Database;
export 'src/element.dart' show Element;
export 'src/exceptions.dart';
