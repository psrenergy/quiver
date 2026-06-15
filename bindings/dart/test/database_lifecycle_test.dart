import 'dart:io';

import 'package:quiverdb/quiverdb.dart';
import 'package:test/test.dart';
import 'package:path/path.dart' as path;

void main() {
  final testsPath = path.join(path.current, '..', '..', 'tests');
  final schemaPath = path.join(testsPath, 'schemas', 'valid', 'basic.sql');
  final hubDir = path.join(testsPath, 'schemas', 'from_hub');

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

  group('Database fromHub', () {
    test('applies migrations and loads UI metadata', () {
      final tempDir = Directory.systemTemp.createTempSync('quiver_test_');
      final dbPath = path.join(tempDir.path, 'test.db');
      try {
        final db = Database.fromHub(dbPath, hubDir);
        try {
          expect(db.currentVersion(), equals(1));
          expect(db.getModelName(), equals('demo_model'));
          expect(db.getAttributeUnit('Material', 'demand'), equals('unit/year'));
          expect(db.getAttributeUnit('Material', 'label'), equals(''));
          expect(db.getAttributeUnit('Nonexistent', 'x'), equals(''));
        } finally {
          db.close();
        }
      } finally {
        tempDir.deleteSync(recursive: true);
      }
    });

    test('throws on invalid directory', () {
      expect(
        () => Database.fromHub(':memory:', 'nonexistent/path'),
        throwsA(isA<DatabaseException>()),
      );
    });

    test('UI metadata is empty without fromHub', () {
      final db = Database.fromSchema(':memory:', schemaPath);
      try {
        expect(db.getModelName(), equals(''));
        expect(db.getAttributeUnit('Material', 'demand'), equals(''));
      } finally {
        db.close();
      }
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

    test('readOnly open rejects writes', () {
      final tempDir = Directory.systemTemp.createTempSync('quiver_test_');
      final dbPath = path.join(tempDir.path, 'test.db');
      try {
        Database.fromSchema(dbPath, schemaPath).close();

        final reopened = Database.open(dbPath, readOnly: true);
        try {
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

    test('returns migration count for a model directory', () {
      // schemas/ holds the shared 3-migration fixture under migrations/ (no ui/ -> empty UI).
      final db = Database.fromHub(':memory:', path.join(testsPath, 'schemas'));
      try {
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
