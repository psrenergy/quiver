import 'package:quiverdb/quiverdb.dart';
import 'package:test/test.dart';
import 'package:path/path.dart' as path;

void main() {
  // Path to central tests folder
  final testsPath = path.join(path.current, '..', '..', 'tests');

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
          db.readScalarIntegers('Configuration', 'integer_attribute'),
          equals([42, 100]),
        );
      } finally {
        db.close();
      }
    });

    test('reads floats from Configuration', () {
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
          db.readScalarFloats('Configuration', 'float_attribute'),
          equals([3.14, 2.71]),
        );
      } finally {
        db.close();
      }
    });

    test('preserves a slot for NULL float values', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        // float_attribute has no default, so an unset value is SQL NULL.
        db.createElement('Configuration', {
          'label': 'Config 1',
          'float_attribute': 10.0,
        });
        db.createElement('Configuration', {'label': 'Config 2'}); // NULL float
        db.createElement('Configuration', {
          'label': 'Config 3',
          'float_attribute': 30.0,
        });
        db.createElement('Configuration', {
          'label': 'Config 4',
          'float_attribute': 40.0,
        });

        // One entry per element; the NULL occupies its slot positionally.
        expect(
          db.readScalarFloats('Configuration', 'float_attribute'),
          equals([10.0, null, 30.0, 40.0]),
        );
      } finally {
        db.close();
      }
    });

    test('preserves a slot for NULL integer values', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'all_types.sql'),
      );
      try {
        db.createElement('AllTypes', {'label': 'a', 'some_integer': 10});
        db.createElement('AllTypes', {'label': 'b'}); // NULL integer
        db.createElement('AllTypes', {'label': 'c', 'some_integer': 30});

        expect(
          db.readScalarIntegers('AllTypes', 'some_integer'),
          equals([10, null, 30]),
        );
      } finally {
        db.close();
      }
    });

    test('preserves a slot for NULL string values', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {
          'label': 'Config 1',
          'string_attribute': 'hello',
        });
        db.createElement('Configuration', {'label': 'Config 2'}); // NULL string
        db.createElement('Configuration', {
          'label': 'Config 3',
          'string_attribute': 'world',
        });

        expect(
          db.readScalarStrings('Configuration', 'string_attribute'),
          equals(['hello', null, 'world']),
        );
      } finally {
        db.close();
      }
    });

    test('returns all-null column as full-length nulls', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Config 1'});
        db.createElement('Configuration', {'label': 'Config 2'});

        expect(
          db.readScalarFloats('Configuration', 'float_attribute'),
          equals([null, null]),
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
          db.readScalarIntegers('Collection', 'some_integer'),
          equals([10, 20]),
        );
        expect(
          db.readScalarFloats('Collection', 'some_float'),
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
        expect(db.readScalarIntegers('Collection', 'some_integer'), isEmpty);
        expect(db.readScalarFloats('Collection', 'some_float'), isEmpty);
      } finally {
        db.close();
      }
    });
  });

  // Read by Id tests

  group('Read Scalar Integers by Id', () {
    test('reads integer by specific element Id', () {
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
          db.readScalarIntegerById('Configuration', 'integer_attribute', 1),
          equals(42),
        );
        expect(
          db.readScalarIntegerById('Configuration', 'integer_attribute', 2),
          equals(100),
        );
        expect(
          db.readScalarIntegerById('Configuration', 'integer_attribute', 999),
          isNull,
        );
      } finally {
        db.close();
      }
    });
  });

  group('Read Scalar Floats by Id', () {
    test('reads float by specific element Id', () {
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
          db.readScalarFloatById('Configuration', 'float_attribute', 1),
          equals(3.14),
        );
        expect(
          db.readScalarFloatById('Configuration', 'float_attribute', 2),
          equals(2.71),
        );
      } finally {
        db.close();
      }
    });
  });

  group('Read Scalar Strings by Id', () {
    test('reads string by specific element Id', () {
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
          db.readScalarStringById('Configuration', 'string_attribute', 1),
          equals('hello'),
        );
        expect(
          db.readScalarStringById('Configuration', 'string_attribute', 2),
          equals('world'),
        );
      } finally {
        db.close();
      }
    });
  });

  group('Read DateTime Attributes', () {
    test('reads date time values from Configuration', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {
          'label': 'Config 1',
          'date_attribute': '2024-01-15T10:30:00',
        });
        db.createElement('Configuration', {
          'label': 'Config 2',
          'date_attribute': '2024-06-20T14:45:30',
        });

        final dates = db.readScalarStrings('Configuration', 'date_attribute');
        expect(dates.length, equals(2));
        expect(dates[0], equals('2024-01-15T10:30:00'));
        expect(dates[1], equals('2024-06-20T14:45:30'));
      } finally {
        db.close();
      }
    });

    test('reads date time by Id', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {
          'label': 'Config 1',
          'date_attribute': '2024-01-15T10:30:00',
        });

        final date = db.readScalarStringById(
          'Configuration',
          'date_attribute',
          1,
        );
        expect(date, equals('2024-01-15T10:30:00'));
      } finally {
        db.close();
      }
    });

    test('readScalarsById returns native DateTime objects', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {
          'label': 'Config 1',
          'date_attribute': '2024-01-15T10:30:00',
        });

        final scalars = db.readScalarsById('Configuration', 1);
        expect(scalars['date_attribute'], isA<DateTime>());
        expect(
          scalars['date_attribute'],
          equals(DateTime(2024, 1, 15, 10, 30, 0)),
        );
      } finally {
        db.close();
      }
    });
  });

  // Read element Ids tests

  group('Read Element Ids', () {
    test('reads all element Ids from collection', () {
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
        db.createElement('Configuration', {
          'label': 'Config 3',
          'integer_attribute': 200,
        });

        final ids = db.readElementIds('Configuration');
        expect(ids.length, equals(3));
        expect(ids[0], equals(1));
        expect(ids[1], equals(2));
        expect(ids[2], equals(3));
      } finally {
        db.close();
      }
    });

    test('returns empty list when no elements', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});

        // No Collection elements created
        final ids = db.readElementIds('Collection');
        expect(ids, isEmpty);
      } finally {
        db.close();
      }
    });
  });

  // Error handling tests

  group('Read Invalid Collection', () {
    test('throws on nonexistent collection for strings', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        expect(
          () => db.readScalarStrings('NonexistentCollection', 'label'),
          throwsA(isA<DatabaseException>()),
        );
      } finally {
        db.close();
      }
    });

    test('throws on nonexistent collection for integers', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        expect(
          () => db.readScalarIntegers('NonexistentCollection', 'value'),
          throwsA(isA<DatabaseException>()),
        );
      } finally {
        db.close();
      }
    });

    test('throws on nonexistent collection for floats', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        expect(
          () => db.readScalarFloats('NonexistentCollection', 'value'),
          throwsA(isA<DatabaseException>()),
        );
      } finally {
        db.close();
      }
    });
  });

  group('Read Invalid Attribute', () {
    test('throws on nonexistent attribute for strings', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        expect(
          () => db.readScalarStrings('Configuration', 'nonexistent_attr'),
          throwsA(isA<DatabaseException>()),
        );
      } finally {
        db.close();
      }
    });

    test('throws on nonexistent attribute for integers', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        expect(
          () => db.readScalarIntegers('Configuration', 'nonexistent_attr'),
          throwsA(isA<DatabaseException>()),
        );
      } finally {
        db.close();
      }
    });

    test('throws on nonexistent attribute for floats', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        expect(
          () => db.readScalarFloats('Configuration', 'nonexistent_attr'),
          throwsA(isA<DatabaseException>()),
        );
      } finally {
        db.close();
      }
    });
  });

  group('Read By Id Not Found', () {
    test('returns null for nonexistent integer Id', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {
          'label': 'Config 1',
          'integer_attribute': 42,
        });

        expect(
          db.readScalarIntegerById('Configuration', 'integer_attribute', 999),
          isNull,
        );
      } finally {
        db.close();
      }
    });

    test('returns null for nonexistent float Id', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {
          'label': 'Config 1',
          'float_attribute': 3.14,
        });

        expect(
          db.readScalarFloatById('Configuration', 'float_attribute', 999),
          isNull,
        );
      } finally {
        db.close();
      }
    });

    test('returns null for nonexistent string Id', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {
          'label': 'Config 1',
          'string_attribute': 'hello',
        });

        expect(
          db.readScalarStringById('Configuration', 'string_attribute', 999),
          isNull,
        );
      } finally {
        db.close();
      }
    });
  });

  group('Read Element Ids Invalid Collection', () {
    test('throws on nonexistent collection', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        expect(
          () => db.readElementIds('NonexistentCollection'),
          throwsA(isA<DatabaseException>()),
        );
      } finally {
        db.close();
      }
    });
  });

  group('readElementById', () {
    test('returns scalars, vectors, and sets merged', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'composite_helpers.sql'),
      );
      try {
        final id = db.createElement('Items', {
          'label': 'Item 1',
          'amount': [10, 20, 30],
          'score': [1.1, 2.2],
          'note': ['hello', 'world'],
          'code': [5, 6],
          'weight': [9.9],
          'tag': ['alpha', 'beta'],
        });

        final result = db.readElementById('Items', id);

        // Scalars
        expect(result['label'], equals('Item 1'));

        // Vectors
        expect(result['amount'], equals([10, 20, 30]));
        expect(result['score'], equals([1.1, 2.2]));
        expect(result['note'], equals(['hello', 'world']));

        // Sets (unordered)
        expect((result['code'] as List)..sort(), equals([5, 6]));
        expect((result['tag'] as List)..sort(), equals(['alpha', 'beta']));
      } finally {
        db.close();
      }
    });
  });
}
