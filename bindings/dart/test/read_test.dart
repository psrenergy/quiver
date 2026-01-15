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

  group('Read Scalar Attributes', () {
    test('reads strings from Configuration', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {
          'label': 'Config 1',
          'string_attribute': 'hello',
        });
        db.createElement('Configuration', {
          'label': 'Config 2',
          'string_attribute': 'world',
        });

        expect(
          db.readScalarStrings('Configuration', 'label'),
          equals(['Config 1', 'Config 2']),
        );
        expect(
          db.readScalarStrings('Configuration', 'string_attribute'),
          equals(['hello', 'world']),
        );
      } finally {
        db.close();
      }
    });

    test('reads integers from Configuration', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {
          'label': 'Config 1',
          'integer_attribute': 42,
        });
        db.createElement('Configuration', {
          'label': 'Config 2',
          'integer_attribute': 100,
        });

        expect(
          db.readScalarInts('Configuration', 'integer_attribute'),
          equals([42, 100]),
        );
      } finally {
        db.close();
      }
    });

    test('reads doubles from Configuration', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {
          'label': 'Config 1',
          'float_attribute': 3.14,
        });
        db.createElement('Configuration', {
          'label': 'Config 2',
          'float_attribute': 2.71,
        });

        expect(
          db.readScalarDoubles('Configuration', 'float_attribute'),
          equals([3.14, 2.71]),
        );
      } finally {
        db.close();
      }
    });
  });

  group('Read From Collections', () {
    test('reads from Collection table', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        db.createElement('Collection', {
          'label': 'Item 1',
          'some_integer': 10,
          'some_float': 1.5,
        });
        db.createElement('Collection', {
          'label': 'Item 2',
          'some_integer': 20,
          'some_float': 2.5,
        });

        expect(
          db.readScalarStrings('Collection', 'label'),
          equals(['Item 1', 'Item 2']),
        );
        expect(
          db.readScalarInts('Collection', 'some_integer'),
          equals([10, 20]),
        );
        expect(
          db.readScalarDoubles('Collection', 'some_float'),
          equals([1.5, 2.5]),
        );
      } finally {
        db.close();
      }
    });
  });

  group('Read Empty Result', () {
    test('returns empty list when no elements', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});

        expect(db.readScalarStrings('Collection', 'label'), isEmpty);
        expect(db.readScalarInts('Collection', 'some_integer'), isEmpty);
        expect(db.readScalarDoubles('Collection', 'some_float'), isEmpty);
      } finally {
        db.close();
      }
    });
  });

  group('Read Vector Attributes', () {
    test('reads int vectors from Collection', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        db.createElement('Collection', {
          'label': 'Item 1',
          'value_int': [1, 2, 3],
        });
        db.createElement('Collection', {
          'label': 'Item 2',
          'value_int': [10, 20],
        });

        expect(
          db.readVectorInts('Collection', 'value_int'),
          equals([
            [1, 2, 3],
            [10, 20],
          ]),
        );
      } finally {
        db.close();
      }
    });

    test('reads double vectors from Collection', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        db.createElement('Collection', {
          'label': 'Item 1',
          'value_float': [1.5, 2.5, 3.5],
        });
        db.createElement('Collection', {
          'label': 'Item 2',
          'value_float': [10.5, 20.5],
        });

        expect(
          db.readVectorDoubles('Collection', 'value_float'),
          equals([
            [1.5, 2.5, 3.5],
            [10.5, 20.5],
          ]),
        );
      } finally {
        db.close();
      }
    });
  });

  group('Read Vector Empty Result', () {
    test('returns empty list when no elements', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});

        expect(db.readVectorInts('Collection', 'value_int'), isEmpty);
        expect(db.readVectorDoubles('Collection', 'value_float'), isEmpty);
      } finally {
        db.close();
      }
    });
  });

  group('Read Vector Only Returns Elements With Data', () {
    test('only returns vectors for elements with data', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        db.createElement('Collection', {
          'label': 'Item 1',
          'value_int': [1, 2, 3],
        });
        db.createElement('Collection', {
          'label': 'Item 2',
          // No vector data
        });
        db.createElement('Collection', {
          'label': 'Item 3',
          'value_int': [4, 5],
        });

        // Only elements with vector data are returned
        final result = db.readVectorInts('Collection', 'value_int');
        expect(result.length, equals(2));
        expect(result[0], equals([1, 2, 3]));
        expect(result[1], equals([4, 5]));
      } finally {
        db.close();
      }
    });
  });
}
