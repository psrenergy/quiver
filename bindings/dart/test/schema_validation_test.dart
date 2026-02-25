import 'dart:io';
import 'package:quiverdb/quiverdb.dart';
import 'package:test/test.dart';
import 'package:path/path.dart' as path;

void main() {
  // Path to central tests folder
  final testsPath = path.join(
    path.current,
    '..',
    '..',
    'tests',
  );
  final invalidPath = path.join(testsPath, 'schemas', 'invalid');

  late String dbPath;
  late Directory tempDir;

  setUp(() {
    tempDir = Directory.systemTemp.createTempSync('quiver_schema_test_');
    dbPath = path.join(tempDir.path, 'test.db');
  });

  tearDown(() {
    try {
      if (tempDir.existsSync()) {
        tempDir.deleteSync(recursive: true);
      }
    } catch (_) {
      // Ignore file access errors
    }
  });

  group('Valid Schemas', () {
    test('loads basic schema', () {
      final db = Database.fromSchema(
        dbPath,
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      db.close();
    });

    test('loads collections schema', () {
      final db = Database.fromSchema(
        dbPath,
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      db.close();
    });

    test('loads relations schema', () {
      final db = Database.fromSchema(
        dbPath,
        path.join(testsPath, 'schemas', 'valid', 'relations.sql'),
      );
      db.close();
    });
  });

  group('Invalid Schemas', () {
    test('rejects schema without Configuration table', () {
      expect(
        () => Database.fromSchema(
          dbPath,
          path.join(invalidPath, 'no_configuration.sql'),
        ),
        throwsA(isA<DatabaseException>()),
      );
    });

    test('rejects schema with label missing NOT NULL', () {
      expect(
        () => Database.fromSchema(
          dbPath,
          path.join(invalidPath, 'label_not_null.sql'),
        ),
        throwsA(isA<DatabaseException>()),
      );
    });

    test('rejects schema with label missing UNIQUE', () {
      expect(
        () => Database.fromSchema(
          dbPath,
          path.join(invalidPath, 'label_not_unique.sql'),
        ),
        throwsA(isA<DatabaseException>()),
      );
    });

    test('rejects schema with label wrong type', () {
      expect(
        () => Database.fromSchema(
          dbPath,
          path.join(invalidPath, 'label_wrong_type.sql'),
        ),
        throwsA(isA<DatabaseException>()),
      );
    });

    test('rejects schema with duplicate attribute', () {
      expect(
        () => Database.fromSchema(
          dbPath,
          path.join(invalidPath, 'duplicate_attribute_vector.sql'),
        ),
        throwsA(isA<DatabaseException>()),
      );
    });

    test('rejects schema with duplicate attribute in time series', () {
      expect(
        () => Database.fromSchema(
          dbPath,
          path.join(invalidPath, 'duplicate_attribute_time_series.sql'),
        ),
        throwsA(isA<DatabaseException>()),
      );
    });

    test('rejects schema with vector table without vector_index', () {
      expect(
        () => Database.fromSchema(
          dbPath,
          path.join(invalidPath, 'vector_no_index.sql'),
        ),
        throwsA(isA<DatabaseException>()),
      );
    });

    test('rejects schema with set table without UNIQUE', () {
      expect(
        () => Database.fromSchema(
          dbPath,
          path.join(invalidPath, 'set_no_unique.sql'),
        ),
        throwsA(isA<DatabaseException>()),
      );
    });

    test('rejects schema with FK NOT NULL and ON DELETE SET NULL', () {
      expect(
        () => Database.fromSchema(
          dbPath,
          path.join(invalidPath, 'fk_not_null_set_null.sql'),
        ),
        throwsA(isA<DatabaseException>()),
      );
    });

    test('rejects schema with invalid FK actions', () {
      expect(
        () => Database.fromSchema(
          dbPath,
          path.join(invalidPath, 'fk_actions.sql'),
        ),
        throwsA(isA<DatabaseException>()),
      );
    });
  });
}
