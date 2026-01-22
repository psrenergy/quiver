/// Dart FFI bindings for QUIVER Database - SQLite wrapper library.
///
/// This library provides a Dart interface to the QUIVER Database C library.
///
/// ## Usage
///
/// ```dart
/// import 'package:quiver_database/quiver_database.dart';
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
library quiver_database;

export 'src/database.dart' show Database;
export 'src/element.dart' show Element;
export 'src/exceptions.dart';
export 'src/lua_runner.dart' show LuaRunner;
