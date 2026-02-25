import 'dart:io';

import 'package:quiverdb/quiverdb.dart';
import 'package:test/test.dart';
import 'package:path/path.dart' as path;

void main() {
  final testsPath = path.join(path.current, '..', '..', 'tests');
  final schemaPath = path.join(testsPath, 'schemas', 'valid', 'basic.sql');
  final migrationsPath = path.join(testsPath, 'schemas', 'migrations');

  group('Database fromSchema', () {
    test('creates database from schema file', () {
      final db = Database.fromSchema(':memory:', schemaPath);
      try {
        // Database is usable
        db.createElement('Configuration', {'label': 'Test'});
      } finally {
        db.close();
      }
    });

    test('works with in-memory database', () {
      final db = Database.fromSchema(':memory:', schemaPath);
      try {
        db.createElement('Configuration', {'label': 'Test'});
        final labels = db.readScalarStrings('Configuration', 'label');
        expect(labels, equals(['Test']));
      } finally {
        db.close();
      }
    });

    test('creates database file on disk', () {
      final tempDir = Directory.systemTemp.createTempSync('quiver_test_');
      final dbPath = path.join(tempDir.path, 'test.db');
      try {
        final db = Database.fromSchema(dbPath, schemaPath);
        db.close();
        expect(File(dbPath).existsSync(), isTrue);
      } finally {
        tempDir.deleteSync(recursive: true);
      }
    });

    test('throws on nonexistent schema file', () {
      expect(
        () => Database.fromSchema(':memory:', 'nonexistent/path/schema.sql'),
        throwsA(isA<DatabaseException>()),
      );
    });

    test('throws on empty schema path', () {
      expect(
        () => Database.fromSchema(':memory:', ''),
        throwsA(isA<DatabaseException>()),
      );
    });
  });

  group('Database fromMigrations', () {
    test('creates database with migrations applied', () {
      // Migrations apply Test1, Test2 (dropped), Test3 - these are not Quiver collections
      // so we just verify the database is created successfully
      final db = Database.fromMigrations(':memory:', migrationsPath);
      try {
        // Database was created and migrations were applied
      } finally {
        db.close();
      }
    });

    test('works with in-memory database', () {
      final db = Database.fromMigrations(':memory:', migrationsPath);
      try {
        // Database was created successfully with all migrations applied
      } finally {
        db.close();
      }
    });

    test('creates database file on disk', () {
      final tempDir = Directory.systemTemp.createTempSync('quiver_test_');
      final dbPath = path.join(tempDir.path, 'test.db');
      try {
        final db = Database.fromMigrations(dbPath, migrationsPath);
        db.close();
        expect(File(dbPath).existsSync(), isTrue);
      } finally {
        tempDir.deleteSync(recursive: true);
      }
    });

    test('throws on invalid migrations path', () {
      // Invalid path should throw DatabaseException
      expect(
        () => Database.fromMigrations(':memory:', 'nonexistent/path'),
        throwsA(isA<DatabaseException>()),
      );
    });
  });

  group('Database currentVersion', () {
    test('returns 0 for schema-based database', () {
      final db = Database.fromSchema(':memory:', schemaPath);
      try {
        expect(db.currentVersion(), equals(0));
      } finally {
        db.close();
      }
    });

    test('returns migration count for migrations-based database', () {
      final db = Database.fromMigrations(':memory:', migrationsPath);
      try {
        // Migrations folder has 3 migrations
        expect(db.currentVersion(), equals(3));
      } finally {
        db.close();
      }
    });

    test('throws after close', () {
      final db = Database.fromSchema(':memory:', schemaPath);
      db.close();
      expect(
        () => db.currentVersion(),
        throwsA(isA<StateError>()),
      );
    });
  });

  group('Database describe', () {
    test('describe does not throw', () {
      final db = Database.fromSchema(':memory:', schemaPath);
      try {
        db.describe();
      } finally {
        db.close();
      }
    });
  });

  group('Database close', () {
    test('close is idempotent', () {
      final db = Database.fromSchema(':memory:', schemaPath);
      db.close();
      // Second close should not throw
      db.close();
    });

    test('operations throw after close', () {
      final db = Database.fromSchema(':memory:', schemaPath);
      db.close();
      expect(
        () => db.createElement('Configuration', {'label': 'Test'}),
        throwsA(isA<StateError>()),
      );
    });
  });
}
