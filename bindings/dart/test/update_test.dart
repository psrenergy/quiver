import 'package:psr_database/psr_database.dart';
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

  group('Update Element', () {
    test('updates single scalar attribute', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Config 1', 'integer_attribute': 100});
        db.createElement('Configuration', {'label': 'Config 2', 'integer_attribute': 200});

        // Update single attribute
        db.updateElement('Configuration', 1, {'integer_attribute': 999});

        // Verify update
        final value = db.readScalarIntegerById('Configuration', 'integer_attribute', 1);
        expect(value, equals(999));

        // Verify label unchanged
        final label = db.readScalarStringById('Configuration', 'label', 1);
        expect(label, equals('Config 1'));
      } finally {
        db.close();
      }
    });

    test('updates multiple scalars at once', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {
          'label': 'Config 1',
          'integer_attribute': 100,
          'float_attribute': 1.5,
        });

        // Update multiple attributes at once
        db.updateElement('Configuration', 1, {
          'integer_attribute': 500,
          'float_attribute': 9.9,
        });

        // Verify updates
        final intValue = db.readScalarIntegerById('Configuration', 'integer_attribute', 1);
        expect(intValue, equals(500));

        final floatValue = db.readScalarDoubleById('Configuration', 'float_attribute', 1);
        expect(floatValue, equals(9.9));

        // Verify label unchanged
        final label = db.readScalarStringById('Configuration', 'label', 1);
        expect(label, equals('Config 1'));
      } finally {
        db.close();
      }
    });

    test('other elements unchanged', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Config 1', 'integer_attribute': 100});
        db.createElement('Configuration', {'label': 'Config 2', 'integer_attribute': 200});
        db.createElement('Configuration', {'label': 'Config 3', 'integer_attribute': 300});

        // Update only element 2
        db.updateElement('Configuration', 2, {'integer_attribute': 999});

        // Verify element 2 updated
        final value2 = db.readScalarIntegerById('Configuration', 'integer_attribute', 2);
        expect(value2, equals(999));

        // Verify elements 1 and 3 unchanged
        final value1 = db.readScalarIntegerById('Configuration', 'integer_attribute', 1);
        expect(value1, equals(100));

        final value3 = db.readScalarIntegerById('Configuration', 'integer_attribute', 3);
        expect(value3, equals(300));
      } finally {
        db.close();
      }
    });

    test('arrays are ignored', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});

        // Create element with vector data
        db.createElement('Collection', {
          'label': 'Item 1',
          'some_integer': 10,
          'value_int': [1, 2, 3],
        });

        // Try to update with array values (should be ignored)
        db.updateElement('Collection', 1, {
          'some_integer': 999,
          'value_int': [7, 8, 9],
        });

        // Verify scalar was updated
        final intValue = db.readScalarIntegerById('Collection', 'some_integer', 1);
        expect(intValue, equals(999));

        // Verify vector was NOT updated (arrays ignored in update_element)
        final vecValues = db.readVectorIntegersById('Collection', 'value_int', 1);
        expect(vecValues, equals([1, 2, 3]));
      } finally {
        db.close();
      }
    });

    test('updates using Element builder', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Config 1', 'integer_attribute': 100});

        // Update using Element builder
        final element = Element();
        try {
          element.set('integer_attribute', 777);
          db.updateElementFromBuilder('Configuration', 1, element);
        } finally {
          element.dispose();
        }

        // Verify update
        final value = db.readScalarIntegerById('Configuration', 'integer_attribute', 1);
        expect(value, equals(777));
      } finally {
        db.close();
      }
    });
  });

  // Error handling tests

  group('Update Invalid Collection', () {
    test('throws on nonexistent collection', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Config 1', 'integer_attribute': 100});
        expect(
          () => db.updateElement('NonexistentCollection', 1, {'integer_attribute': 999}),
          throwsA(isA<DatabaseException>()),
        );
      } finally {
        db.close();
      }
    });
  });

  group('Update Invalid Element ID', () {
    test('does not throw for nonexistent ID', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Config 1', 'integer_attribute': 100});

        // Update element that doesn't exist - should not throw
        db.updateElement('Configuration', 999, {'integer_attribute': 500});

        // Verify original element unchanged
        final value = db.readScalarIntegerById('Configuration', 'integer_attribute', 1);
        expect(value, equals(100));
      } finally {
        db.close();
      }
    });
  });

  group('Update Invalid Attribute', () {
    test('throws on nonexistent attribute', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Config 1', 'integer_attribute': 100});
        expect(
          () => db.updateElement('Configuration', 1, {'nonexistent_attribute': 999}),
          throwsA(isA<DatabaseException>()),
        );
      } finally {
        db.close();
      }
    });
  });

  group('Update String Attribute', () {
    test('updates string attribute correctly', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Config 1', 'string_attribute': 'original'});

        db.updateElement('Configuration', 1, {'string_attribute': 'updated'});

        final value = db.readScalarStringById('Configuration', 'string_attribute', 1);
        expect(value, equals('updated'));
      } finally {
        db.close();
      }
    });
  });

  group('Update Float Attribute', () {
    test('updates float attribute correctly', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Config 1', 'float_attribute': 1.5});

        db.updateElement('Configuration', 1, {'float_attribute': 99.99});

        final value = db.readScalarDoubleById('Configuration', 'float_attribute', 1);
        expect(value, equals(99.99));
      } finally {
        db.close();
      }
    });
  });

  group('Update Multiple Elements Sequentially', () {
    test('updates multiple elements independently', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Config 1', 'integer_attribute': 100});
        db.createElement('Configuration', {'label': 'Config 2', 'integer_attribute': 200});

        // Update both elements
        db.updateElement('Configuration', 1, {'integer_attribute': 111});
        db.updateElement('Configuration', 2, {'integer_attribute': 222});

        expect(db.readScalarIntegerById('Configuration', 'integer_attribute', 1), equals(111));
        expect(db.readScalarIntegerById('Configuration', 'integer_attribute', 2), equals(222));
      } finally {
        db.close();
      }
    });
  });
}
