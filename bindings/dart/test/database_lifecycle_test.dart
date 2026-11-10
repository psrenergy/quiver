import 'dart:io';

import 'package:quiverdb/quiverdb.dart';
import 'package:test/test.dart';
import 'package:path/path.dart' as path;

void main() {
  final testsPath = path.join(path.current, '..', '..', 'tests');
  final schemaPath = path.join(testsPath, 'schemas', 'valid', 'basic.sql');
  final migrationsPath = path.join(testsPath, 'schemas', 'migrations');

  group('Database logPath', () {
    late Directory tempDir;
    late String dbPath;
    const quiet = quiver_log_level_t.QUIVER_LOG_OFF;

    setUp(() {
      tempDir = Directory.systemTemp.createTempSync('quiver_log_test_');
      dbPath = path.join(tempDir.path, 'test.db');
    });
    tearDown(() => tempDir.deleteSync(recursive: true));

    for (final factory in ['open', 'fromSchema', 'fromMigrations']) {
      test('selects the file for $factory and receives Lua prints', () {
        final logPath = path.join(tempDir.path, 'logs', '$factory.log');
        final selectedPath = factory == 'fromSchema' ? logPath : path.join('logs', '$factory.log');
        final db = switch (factory) {
          'open' => Database.open(dbPath, consoleLevel: quiet, logPath: selectedPath),
          'fromSchema' => Database.fromSchema(dbPath, schemaPath, consoleLevel: quiet, logPath: selectedPath),
          _ => Database.fromMigrations(dbPath, migrationsPath, consoleLevel: quiet, logPath: selectedPath),
        };
        try {
          final sandbox = Sandbox(db);
          try {
            sandbox.run("print('custom $factory')");
            expect(File(logPath).readAsStringSync(), contains('[info] custom $factory'));
          } finally {
            sandbox.dispose();
          }
        } finally {
          db.close();
        }
        expect(File(logPath).readAsStringSync(), contains('Opening database:'));
        expect(File(path.join(tempDir.path, 'quiver_database.log')).existsSync(), isFalse);
      });
    }

    test('enables read-only and in-memory file logging explicitly', () {
      Database.fromMigrations(dbPath, migrationsPath, consoleLevel: quiet).close();
      final readonlyLog = path.join(tempDir.path, 'readonly.log');
      Database.open(dbPath, readOnly: true, consoleLevel: quiet, logPath: readonlyLog).close();
      final migrationsLog = path.join(tempDir.path, 'migrations-readonly.log');
      Database.fromMigrations(
        dbPath,
        migrationsPath,
        readOnly: true,
        consoleLevel: quiet,
        logPath: migrationsLog,
      ).close();
      expect(File(readonlyLog).readAsStringSync(), contains('Database opened successfully:'));
      expect(File(migrationsLog).readAsStringSync(), contains('Database opened successfully:'));
      final memoryLog = path.join(tempDir.path, 'memory.log');
      Database.fromSchema(':memory:', schemaPath, consoleLevel: quiet, logPath: path.relative(memoryLog)).close();
      expect(File(memoryLog).readAsStringSync(), contains('Database opened successfully: :memory:'));
    });

    test('omitted and empty paths keep defaults and explicit files truncate', () {
      final log = File(path.join(tempDir.path, 'quiver_database.log'));
      Database.open(dbPath, consoleLevel: quiet).close();
      final original = log.readAsStringSync();
      Database.open(dbPath, readOnly: true, consoleLevel: quiet, logPath: '').close();
      expect(log.readAsStringSync(), original);
      log.deleteSync();
      Database.open(dbPath, consoleLevel: quiet, logPath: '').close();
      expect(log.existsSync(), isTrue);
      log.writeAsStringSync('old log history');
      Database.open(dbPath, consoleLevel: quiet, logPath: log.path).close();
      expect(log.readAsStringSync(), isNot(contains('old log history')));
    });

    test('failures propagate through every factory and preserve the database', () {
      final db = Database.fromSchema(dbPath, schemaPath, consoleLevel: quiet);
      db.createElement('Configuration', {'label': 'preserved'});
      db.close();
      for (final open in [
        () => Database.open(dbPath, consoleLevel: quiet, logPath: tempDir.path),
        () => Database.fromSchema(dbPath, schemaPath, consoleLevel: quiet, logPath: tempDir.path),
        () => Database.fromMigrations(dbPath, migrationsPath, consoleLevel: quiet, logPath: tempDir.path),
      ]) {
        expect(open, throwsA(isA<DatabaseException>()));
        final reopened = Database.open(dbPath, readOnly: true, consoleLevel: quiet);
        try {
          expect(reopened.readScalarStrings('Configuration', 'label'), ['preserved']);
        } finally {
          reopened.close();
        }
      }
      expect(
        () => Database.open(dbPath, consoleLevel: quiet, logPath: dbPath),
        throwsA(isA<DatabaseException>()),
      );
    });
  });

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

    test('readOnly throws when the database is behind its migrations', () {
      expect(
        () => Database.fromMigrations(':memory:', migrationsPath, readOnly: true),
        throwsA(
          isA<DatabaseException>().having(
            (e) => e.message,
            'message',
            contains('a read-only open cannot apply them'),
          ),
        ),
      );
    });

    test('readOnly opens a database already at its migrations version', () {
      final tempDir = Directory.systemTemp.createTempSync('quiver_test_');
      final dbPath = path.join(tempDir.path, 'test.db');
      try {
        Database.fromMigrations(dbPath, migrationsPath).close();

        final reopened = Database.fromMigrations(dbPath, migrationsPath, readOnly: true);
        try {
          expect(reopened.currentVersion(), equals(3));
        } finally {
          reopened.close();
        }
      } finally {
        tempDir.deleteSync(recursive: true);
      }
    });
  });

  group('Database validateMigrations', () {
    test('succeeds for a valid migrations directory', () {
      expect(
        () => Database.validateMigrations(migrationsPath),
        returnsNormally,
      );
    });

    test('throws on nonexistent migrations path', () {
      final badPath = path.join('nonexistent', 'path');
      expect(
        () => Database.validateMigrations(badPath),
        throwsA(
          isA<DatabaseException>().having(
            (e) => e.message,
            'message',
            equals('Cannot validate_migrations: migrations path not found: $badPath'),
          ),
        ),
      );
    });
  });

  group('Database open', () {
    test('reopens existing database file', () {
      final tempDir = Directory.systemTemp.createTempSync('quiver_test_');
      final dbPath = path.join(tempDir.path, 'test.db');
      try {
        Database.fromSchema(dbPath, schemaPath).close();

        final reopened = Database.open(dbPath);
        try {
          expect(reopened.isHealthy(), isTrue);
        } finally {
          reopened.close();
        }
      } finally {
        tempDir.deleteSync(recursive: true);
      }
    });

    // open() does not read the schema; the first metadata or CRUD call loads it. Without that,
    // an opened database answered every one of them with "no schema loaded".
    test('reopened database can read and write', () {
      final tempDir = Directory.systemTemp.createTempSync('quiver_test_');
      final dbPath = path.join(tempDir.path, 'test.db');
      try {
        final created = Database.fromSchema(dbPath, schemaPath);
        created.createElement('Configuration', {'label': 'Config'});
        created.close();

        final reopened = Database.open(dbPath);
        try {
          expect(reopened.readScalarStrings('Configuration', 'label'), equals(['Config']));
          expect(reopened.listScalarAttributes('Configuration'), isNotEmpty);
          expect(
            reopened.createElement('Configuration', {'label': 'Config 2'}),
            equals(2),
          );
        } finally {
          reopened.close();
        }
      } finally {
        tempDir.deleteSync(recursive: true);
      }
    });

    test('readOnly open reads the schema but rejects writes', () {
      final tempDir = Directory.systemTemp.createTempSync('quiver_test_');
      final dbPath = path.join(tempDir.path, 'test.db');
      try {
        Database.fromSchema(dbPath, schemaPath).close();

        final reopened = Database.open(dbPath, readOnly: true);
        try {
          // Reached the schema (so the write below fails for the right reason).
          expect(reopened.listScalarAttributes('Configuration'), isNotEmpty);
          expect(
            () => reopened.createElement('Configuration', {'label': 'nope'}),
            throwsA(isA<DatabaseException>()),
          );
        } finally {
          reopened.close();
        }
      } finally {
        tempDir.deleteSync(recursive: true);
      }
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
      expect(() => db.currentVersion(), throwsA(isA<StateError>()));
    });
  });

  group('Database isHealthy', () {
    test('returns true for open database', () {
      final db = Database.fromSchema(':memory:', schemaPath);
      try {
        expect(db.isHealthy(), isTrue);
      } finally {
        db.close();
      }
    });
  });

  group('Database path', () {
    test('returns path for file-based database', () {
      final tempDir = Directory.systemTemp.createTempSync('quiver_test_');
      final dbPath = path.join(tempDir.path, 'test.db');
      try {
        final db = Database.fromSchema(dbPath, schemaPath);
        try {
          final result = db.path();
          expect(result, contains('test.db'));
        } finally {
          db.close();
        }
      } finally {
        tempDir.deleteSync(recursive: true);
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
