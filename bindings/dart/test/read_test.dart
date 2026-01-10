import 'dart:io';

import 'package:psr_database/psr_database.dart';
import 'package:test/test.dart';

void main() {
  final schemaPath = '${Directory.current.path}../../../tests/test_read/test_read.sql';

  group('Database', () {
    late String dbPath;
    late Database db;

    setUp(() {
      // Create a temporary database file
      final tempDir = Directory.systemTemp.createTempSync('psr_database_test_');
      dbPath = '${tempDir.path}/test.db';
    });

    tearDown(() {
      // Clean up
      try {
        db.close();
      } catch (_) {}
      final file = File(dbPath);
      if (file.existsSync()) {
        file.deleteSync();
      }
      final dir = File(dbPath).parent;
      if (dir.existsSync()) {
        dir.deleteSync(recursive: true);
      }
    });

    test('fromSchema creates database', () {
      db = Database.fromSchema(dbPath, schemaPath);
      expect(File(dbPath).existsSync(), isTrue);
    });

    test('createElement with string values', () {
      db = Database.fromSchema(dbPath, schemaPath);

      final id = db.createElement('Configuration', {
        'label': 'Test Config',
        'date_initial': '2024-01-01 00:00:00',
      });

      expect(id, greaterThan(0));
    });

    test('createElement with numeric values', () {
      db = Database.fromSchema(dbPath, schemaPath);

      final id = db.createElement('Plant', {
        'label': 'Plant 1',
        'capacity': 100.5,
      });

      expect(id, greaterThan(0));
    });

    test('createElement with multiple elements', () {
      db = Database.fromSchema(dbPath, schemaPath);

      final id1 = db.createElement('Resource', {'label': 'Resource 1'});
      final id2 = db.createElement('Resource', {'label': 'Resource 2'});
      final id3 = db.createElement('Resource', {'label': 'Resource 3'});

      expect(id1, equals(1));
      expect(id2, equals(2));
      expect(id3, equals(3));
    });

    test('createElement with vector values', () {
      db = Database.fromSchema(dbPath, schemaPath);

      final id = db.createElement('Resource', {
        'label': 'Resource with vector',
        'some_value': [1.0, 2.0, 3.0],
      });

      expect(id, greaterThan(0));
    });

    test('createElement with set values', () {
      db = Database.fromSchema(dbPath, schemaPath);

      final id = db.createElement('Resource', {
        'label': 'Resource with set',
        'some_other_value': [4.0, 5.0, 6.0],
      });

      expect(id, greaterThan(0));
    });

    test('createElement with null values', () {
      db = Database.fromSchema(dbPath, schemaPath);

      final id = db.createElement('Cost', {
        'label': 'Cost with null',
        'value_without_default': null,
      });

      expect(id, greaterThan(0));
    });

    test('createElement with default values', () {
      db = Database.fromSchema(dbPath, schemaPath);

      // value has default of 100
      final id = db.createElement('Cost', {
        'label': 'Cost with default',
      });

      expect(id, greaterThan(0));
    });

    test('close prevents further operations', () {
      db = Database.fromSchema(dbPath, schemaPath);
      db.close();

      expect(
        () => db.createElement('Configuration', {'label': 'Test'}),
        throwsA(isA<DatabaseOperationException>()),
      );
    });

    test('double close is safe', () {
      db = Database.fromSchema(dbPath, schemaPath);
      db.close();
      db.close(); // Should not throw
    });
  });

  group('Element', () {
    test('set various types', () {
      final element = Element();

      element.setInt('int_val', 42);
      element.setDouble('double_val', 3.14);
      element.setString('string_val', 'hello');
      element.setNull('null_val');

      element.dispose();
    });

    test('set array types', () {
      final element = Element();

      element.setArrayInt('int_array', [1, 2, 3]);
      element.setArrayDouble('double_array', [1.1, 2.2, 3.3]);
      element.setArrayString('string_array', ['a', 'b', 'c']);

      element.dispose();
    });

    test('set with dynamic dispatch', () {
      final element = Element();

      element.set('int_val', 42);
      element.set('double_val', 3.14);
      element.set('string_val', 'hello');
      element.set('null_val', null);
      element.set('int_list', [1, 2, 3]);
      element.set('double_list', [1.1, 2.2]);
      element.set('string_list', ['a', 'b']);

      element.dispose();
    });

    test('dispose prevents further operations', () {
      final element = Element();
      element.dispose();

      expect(
        () => element.setInt('test', 42),
        throwsA(isA<DatabaseOperationException>()),
      );
    });

    test('double dispose is safe', () {
      final element = Element();
      element.dispose();
      element.dispose(); // Should not throw
    });

    test('clear removes all values', () {
      final element = Element();
      element.setInt('test', 42);
      element.clear();
      element.dispose();
    });

    test('unsupported type throws', () {
      final element = Element();

      expect(
        () => element.set('invalid', DateTime.now()),
        throwsA(isA<InvalidArgumentException>()),
      );

      element.dispose();
    });

    test('empty list throws', () {
      final element = Element();

      expect(
        () => element.set('empty', []),
        throwsA(isA<InvalidArgumentException>()),
      );

      element.dispose();
    });
  });

  group('Exceptions', () {
    test('DatabaseException.fromError creates correct types', () {
      expect(
        DatabaseException.fromError(-1),
        isA<InvalidArgumentException>(),
      );
      expect(
        DatabaseException.fromError(-2),
        isA<DatabaseOperationException>(),
      );
      expect(
        DatabaseException.fromError(-3),
        isA<MigrationException>(),
      );
      expect(
        DatabaseException.fromError(-4),
        isA<SchemaException>(),
      );
      expect(
        DatabaseException.fromError(-5),
        isA<CreateElementException>(),
      );
      expect(
        DatabaseException.fromError(-6),
        isA<NotFoundException>(),
      );
      expect(
        DatabaseException.fromError(-999),
        isA<UnknownDatabaseException>(),
      );
    });

    test('exceptions include context message', () {
      final exception = DatabaseException.fromError(-1, 'Custom context');
      expect(exception.message, equals('Custom context'));
    });
  });
}
