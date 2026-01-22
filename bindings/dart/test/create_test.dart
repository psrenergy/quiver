import 'package:quiver_database/quiver_database.dart';
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

  group('Create Scalar Attributes', () {
    test('creates Configuration with all scalar types', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        final id = db.createElement('Configuration', {
          'label': 'Test Config',
          'integer_attribute': 42,
          'float_attribute': 3.14,
          'string_attribute': 'hello',
          'date_attribute': '2024-01-01',
          'boolean_attribute': 1,
        });
        expect(id, greaterThan(0));
      } finally {
        db.close();
      }
    });

    test('creates Configuration with default values', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        final id = db.createElement('Configuration', {'label': 'Test Config'});
        expect(id, greaterThan(0));
      } finally {
        db.close();
      }
    });

    test('rejects wrong type for attribute', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        expect(
          () => db.createElement('Configuration', {
            'label': 'Test Config',
            'integer_attribute': 'not an int',
          }),
          throwsA(isA<DatabaseException>()),
        );
      } finally {
        db.close();
      }
    });

    test('rejects unknown attribute', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        expect(
          () => db.createElement('Configuration', {
            'label': 'Test Config',
            'unknown_attr': 123,
          }),
          throwsA(isA<DatabaseException>()),
        );
      } finally {
        db.close();
      }
    });

    test('rejects missing label', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        expect(
          () => db.createElement('Configuration', {}),
          throwsA(isA<DatabaseException>()),
        );
      } finally {
        db.close();
      }
    });
  });

  group('Create Collections with Vectors', () {
    test('creates element with vector attribute', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        final id = db.createElement('Collection', {
          'label': 'Item 1',
          'some_integer': 10,
          'value_int': [1, 2, 3],
          'value_float': [0.1, 0.2, 0.3],
        });
        expect(id, greaterThan(0));
      } finally {
        db.close();
      }
    });

    test('creates element with only scalars', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        final id = db.createElement('Collection', {
          'label': 'Item 1',
          'some_integer': 20,
        });
        expect(id, greaterThan(0));
      } finally {
        db.close();
      }
    });

    test('rejects empty vector', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        expect(
          () => db.createElement('Collection', {
            'label': 'Item 1',
            'value_int': <int>[],
          }),
          throwsA(isA<DatabaseException>()),
        );
      } finally {
        db.close();
      }
    });
  });

  group('Create Collections with Sets', () {
    test('creates element with set attribute', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        final id = db.createElement('Collection', {
          'label': 'Item 1',
          'tag': ['alpha', 'beta', 'gamma'],
        });
        expect(id, greaterThan(0));
      } finally {
        db.close();
      }
    });
  });

  group('Create Relations', () {
    test('creates child with FK to parent', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'relations.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        db.createElement('Parent', {'label': 'Parent 1'});
        final id = db.createElement('Child', {
          'label': 'Child 1',
          'parent_id': 1,
        });
        expect(id, greaterThan(0));
      } finally {
        db.close();
      }
    });

    test('creates child with self-reference', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'relations.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        db.createElement('Parent', {'label': 'Parent 1'});
        db.createElement('Child', {'label': 'Child 1'});
        final id = db.createElement('Child', {
          'label': 'Child 2',
          'sibling_id': 1,
        });
        expect(id, greaterThan(0));
      } finally {
        db.close();
      }
    });

    test('creates child with vector containing FK refs', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'relations.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        db.createElement('Parent', {'label': 'Parent 1'});
        db.createElement('Parent', {'label': 'Parent 2'});
        final id = db.createElement('Child', {
          'label': 'Child 1',
          'parent_ref': [1, 2],
        });
        expect(id, greaterThan(0));
      } finally {
        db.close();
      }
    });
  });

  // Edge case tests

  group('Create Single Element Vector', () {
    test('creates element with single element vector', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        final id = db.createElement('Collection', {
          'label': 'Item 1',
          'value_int': [42],
        });
        expect(id, greaterThan(0));

        final result = db.readVectorIntegersById('Collection', 'value_int', 1);
        expect(result.length, equals(1));
        expect(result[0], equals(42));
      } finally {
        db.close();
      }
    });
  });

  group('Create Large Vector', () {
    test('creates element with 100+ element vector', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});

        final largeVector = List.generate(150, (i) => i + 1);
        final id = db.createElement('Collection', {
          'label': 'Item 1',
          'value_int': largeVector,
        });
        expect(id, greaterThan(0));

        final result = db.readVectorIntegersById('Collection', 'value_int', 1);
        expect(result.length, equals(150));
        expect(result, equals(largeVector));
      } finally {
        db.close();
      }
    });
  });

  group('Create Invalid Collection', () {
    test('throws on nonexistent collection', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        expect(
          () => db.createElement('NonexistentCollection', {'label': 'Test'}),
          throwsA(isA<DatabaseException>()),
        );
      } finally {
        db.close();
      }
    });
  });

  group('Create With Only Required Label', () {
    test('creates element with only required label', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});

        // Create with only required label, no optional attributes
        final id = db.createElement('Collection', {'label': 'Minimal Item'});
        expect(id, greaterThan(0));

        final labels = db.readScalarStrings('Collection', 'label');
        expect(labels.length, equals(1));
        expect(labels[0], equals('Minimal Item'));
      } finally {
        db.close();
      }
    });
  });

  group('Create With Multiple Sets', () {
    test('creates element with set containing multiple values', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});

        final id = db.createElement('Collection', {
          'label': 'Item 1',
          'tag': ['a', 'b', 'c', 'd', 'e'],
        });
        expect(id, greaterThan(0));

        final result = db.readSetStringsById('Collection', 'tag', 1);
        expect(result.length, equals(5));
        expect(result..sort(), equals(['a', 'b', 'c', 'd', 'e']));
      } finally {
        db.close();
      }
    });
  });

  group('Create Duplicate Label Rejected', () {
    test('throws on duplicate label', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Config 1'});

        // Trying to create with same label should fail
        expect(
          () => db.createElement('Configuration', {'label': 'Config 1'}),
          throwsA(isA<DatabaseException>()),
        );
      } finally {
        db.close();
      }
    });
  });

  group('Create With Float Vector', () {
    test('creates element with float vector', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});

        final id = db.createElement('Collection', {
          'label': 'Item 1',
          'value_float': [1.1, 2.2, 3.3],
        });
        expect(id, greaterThan(0));

        final result = db.readVectorFloatsById('Collection', 'value_float', 1);
        expect(result.length, equals(3));
        expect(result, equals([1.1, 2.2, 3.3]));
      } finally {
        db.close();
      }
    });
  });
}
