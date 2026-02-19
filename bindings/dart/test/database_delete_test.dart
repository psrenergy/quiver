import 'package:quiver_db/quiver_db.dart';
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

  group('Delete Element By ID', () {
    test('deletes element by id', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Config 1'});
        db.createElement('Configuration', {'label': 'Config 2'});
        db.createElement('Configuration', {'label': 'Config 3'});

        var ids = db.readElementIds('Configuration');
        expect(ids.length, equals(3));

        db.deleteElement('Configuration', 2);

        ids = db.readElementIds('Configuration');
        expect(ids.length, equals(2));
        expect(ids.contains(2), isFalse);
        expect(ids.contains(1), isTrue);
        expect(ids.contains(3), isTrue);
      } finally {
        db.close();
      }
    });

    test('deletes element with vector data (CASCADE)', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});

        db.createElement('Collection', {
          'label': 'Item 1',
          'some_integer': 10,
          'value_int': [1, 2, 3],
          'value_float': [0.1, 0.2, 0.3],
        });

        db.createElement('Collection', {
          'label': 'Item 2',
          'some_integer': 20,
          'value_int': [4, 5, 6],
        });

        var ids = db.readElementIds('Collection');
        expect(ids.length, equals(2));

        db.deleteElement('Collection', 1);

        ids = db.readElementIds('Collection');
        expect(ids.length, equals(1));
        expect(ids[0], equals(2));

        // Verify remaining element's data is intact
        final values = db.readVectorIntegers('Collection', 'value_int');
        expect(values.length, equals(1));
        expect(values[0], equals([4, 5, 6]));
      } finally {
        db.close();
      }
    });

    test('deletes element with set data (CASCADE)', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});

        db.createElement('Collection', {
          'label': 'Item 1',
          'tag': ['alpha', 'beta'],
        });

        db.createElement('Collection', {
          'label': 'Item 2',
          'tag': ['gamma', 'delta'],
        });

        var ids = db.readElementIds('Collection');
        expect(ids.length, equals(2));

        db.deleteElement('Collection', 1);

        ids = db.readElementIds('Collection');
        expect(ids.length, equals(1));
        expect(ids[0], equals(2));

        // Verify remaining element's set data is intact
        final sets = db.readSetStrings('Collection', 'tag');
        expect(sets.length, equals(1));
        expect(sets[0]..sort(), equals(['delta', 'gamma']));
      } finally {
        db.close();
      }
    });

    test('deletes non-existent element (idempotent)', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Config 1'});

        // Delete non-existent element should succeed
        db.deleteElement('Configuration', 999);

        // Verify original element still exists
        final ids = db.readElementIds('Configuration');
        expect(ids.length, equals(1));
        expect(ids[0], equals(1));
      } finally {
        db.close();
      }
    });

    test('delete does not affect other elements', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {
          'label': 'Config 1',
          'integer_attribute': 100,
        });
        db.createElement('Configuration', {
          'label': 'Config 2',
          'integer_attribute': 200,
        });
        db.createElement('Configuration', {
          'label': 'Config 3',
          'integer_attribute': 300,
        });

        db.deleteElement('Configuration', 2);

        final labels = db.readScalarStrings('Configuration', 'label');
        expect(labels.length, equals(2));
        expect(labels.contains('Config 1'), isTrue);
        expect(labels.contains('Config 3'), isTrue);
        expect(labels.contains('Config 2'), isFalse);

        final values = db.readScalarIntegers('Configuration', 'integer_attribute');
        expect(values.length, equals(2));
        expect(values.contains(100), isTrue);
        expect(values.contains(300), isTrue);
        expect(values.contains(200), isFalse);
      } finally {
        db.close();
      }
    });
  });
}
