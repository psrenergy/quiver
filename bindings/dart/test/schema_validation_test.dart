import 'dart:io';
import 'package:psr_database/psr_database.dart';
import 'package:test/test.dart';
import 'package:path/path.dart' as path;

void main() {
  // Path to central tests folder (from bindings/dart, go up 2 levels to database/)
  final testsPath = path.join(
    Directory.current.path,
    '..',
    '..',
    'tests',
    'test_valid_database_definitions',
  );

  late String dbPath;
  late Directory tempDir;

  setUp(() {
    tempDir = Directory.systemTemp.createTempSync('psr_schema_test_');
    dbPath = path.join(tempDir.path, 'test.db');
  });

  tearDown(() {
    // On Windows, there may be a brief delay before file handles are released
    // after a failed schema validation. Try to delete, but ignore errors.
    try {
      if (tempDir.existsSync()) {
        tempDir.deleteSync(recursive: true);
      }
    } catch (_) {
      // Ignore file access errors - temp files will be cleaned up by OS
    }
  });

  group('Schema Validation - Invalid Schemas', () {
    test('rejects schema without Configuration table', () {
      expect(
        () => Database.fromSchema(
          dbPath,
          path.join(testsPath, 'test_invalid_database_without_configuration_table.sql'),
        ),
        throwsA(isA<SchemaException>()),
      );
    });

    test('rejects schema with duplicated attributes', () {
      expect(
        () => Database.fromSchema(
          dbPath,
          path.join(testsPath, 'test_invalid_database_with_duplicated_attributes.sql'),
        ),
        throwsA(isA<SchemaException>()),
      );
    });

    test('rejects schema with duplicated attributes 2', () {
      expect(
        () => Database.fromSchema(
          dbPath,
          path.join(testsPath, 'test_invalid_database_with_duplicated_attributes_2.sql'),
        ),
        throwsA(isA<SchemaException>()),
      );
    });

    test('rejects schema with invalid collection name', () {
      expect(
        () => Database.fromSchema(
          dbPath,
          path.join(testsPath, 'test_invalid_database_with_invalid_collection_name.sql'),
        ),
        throwsA(isA<SchemaException>()),
      );
    });

    test('rejects schema with vector table without vector index', () {
      expect(
        () => Database.fromSchema(
          dbPath,
          path.join(testsPath, 'test_invalid_database_vector_table_without_vector_index.sql'),
        ),
        throwsA(isA<SchemaException>()),
      );
    });

    test('rejects schema with foreign key with NOT NULL constraint', () {
      expect(
        () => Database.fromSchema(
          dbPath,
          path.join(testsPath, 'test_invalid_foreign_key_has_not_null_constraint.sql'),
        ),
        throwsA(isA<SchemaException>()),
      );
    });

    test('rejects schema with duplicated collection definition', () {
      expect(
        () => Database.fromSchema(
          dbPath,
          path.join(testsPath, 'test_invalid_duplicated_collection_definition.sql'),
        ),
        throwsA(isA<SchemaException>()),
      );
    });

    test('rejects schema with invalid relation attribute', () {
      expect(
        () => Database.fromSchema(
          dbPath,
          path.join(testsPath, 'test_invalid_relation_attribute.sql'),
        ),
        throwsA(isA<SchemaException>()),
      );
    });

    test('rejects schema with invalid vector table', () {
      expect(
        () => Database.fromSchema(
          dbPath,
          path.join(testsPath, 'test_invalid_vector_table.sql'),
        ),
        throwsA(isA<SchemaException>()),
      );
    });

    test('rejects schema with invalid foreign key actions', () {
      expect(
        () => Database.fromSchema(
          dbPath,
          path.join(testsPath, 'test_invalid_database_foreign_key_actions.sql'),
        ),
        throwsA(isA<SchemaException>()),
      );
    });

    test('rejects schema with label not UNIQUE', () {
      expect(
        () => Database.fromSchema(
          dbPath,
          path.join(testsPath, 'test_invalid_label_constraints_unique.sql'),
        ),
        throwsA(isA<SchemaException>()),
      );
    });

    test('rejects schema with label nullable', () {
      expect(
        () => Database.fromSchema(
          dbPath,
          path.join(testsPath, 'test_invalid_label_constraints_null.sql'),
        ),
        throwsA(isA<SchemaException>()),
      );
    });

    test('rejects schema with label not TEXT type', () {
      expect(
        () => Database.fromSchema(
          dbPath,
          path.join(testsPath, 'test_invalid_label_constraints_text.sql'),
        ),
        throwsA(isA<SchemaException>()),
      );
    });
  });

  group('Valid Schemas', () {
    test('loads full schema with multiple collections', () {
      final db = Database.fromSchema(
        dbPath,
        path.join(testsPath, 'test_valid_schema_full.sql'),
      );
      db.close();
    });

    test('loads schema with string default values', () {
      final db = Database.fromSchema(
        dbPath,
        path.join(testsPath, 'test_string_default_value.sql'),
      );
      db.close();
    });
  });
}
