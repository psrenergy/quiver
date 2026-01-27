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
          db.readVectorIntegers('Collection', 'value_int'),
          equals([
            [1, 2, 3],
            [10, 20],
          ]),
        );
      } finally {
        db.close();
      }
    });

    test('reads float vectors from Collection', () {
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
          db.readVectorFloats('Collection', 'value_float'),
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

        expect(db.readVectorIntegers('Collection', 'value_int'), isEmpty);
        expect(db.readVectorFloats('Collection', 'value_float'), isEmpty);
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
        final result = db.readVectorIntegers('Collection', 'value_int');
        expect(result.length, equals(2));
        expect(result[0], equals([1, 2, 3]));
        expect(result[1], equals([4, 5]));
      } finally {
        db.close();
      }
    });
  });

  group('Read Set Attributes', () {
    test('reads string sets from Collection', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        db.createElement('Collection', {
          'label': 'Item 1',
          'tag': ['important', 'urgent'],
        });
        db.createElement('Collection', {
          'label': 'Item 2',
          'tag': ['review'],
        });

        final result = db.readSetStrings('Collection', 'tag');
        expect(result.length, equals(2));
        // Sets are unordered, so sort before comparison
        expect(result[0]..sort(), equals(['important', 'urgent']));
        expect(result[1], equals(['review']));
      } finally {
        db.close();
      }
    });
  });

  group('Read Set Empty Result', () {
    test('returns empty list when no elements', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});

        expect(db.readSetStrings('Collection', 'tag'), isEmpty);
      } finally {
        db.close();
      }
    });
  });

  group('Read Set Only Returns Elements With Data', () {
    test('only returns sets for elements with data', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        db.createElement('Collection', {
          'label': 'Item 1',
          'tag': ['important'],
        });
        db.createElement('Collection', {
          'label': 'Item 2',
          // No set data
        });
        db.createElement('Collection', {
          'label': 'Item 3',
          'tag': ['urgent', 'review'],
        });

        // Only elements with set data are returned
        final result = db.readSetStrings('Collection', 'tag');
        expect(result.length, equals(2));
      } finally {
        db.close();
      }
    });
  });

  group('Scalar Relations', () {
    test('set and read scalar relation', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'relations.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});

        // Create parents
        db.createElement('Parent', {'label': 'Parent 1'});
        db.createElement('Parent', {'label': 'Parent 2'});

        // Create children
        db.createElement('Child', {'label': 'Child 1'});
        db.createElement('Child', {'label': 'Child 2'});
        db.createElement('Child', {'label': 'Child 3'});

        // Set relations
        db.setScalarRelation('Child', 'parent_id', 'Child 1', 'Parent 1');
        db.setScalarRelation('Child', 'parent_id', 'Child 3', 'Parent 2');

        // Read relations
        final labels = db.readScalarRelation('Child', 'parent_id');
        expect(labels.length, equals(3));
        expect(labels[0], equals('Parent 1'));
        expect(labels[1], isNull); // Child 2 has no parent
        expect(labels[2], equals('Parent 2'));
      } finally {
        db.close();
      }
    });

    test('read scalar relation self-reference', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'relations.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});

        // Create children (Child references itself via sibling_id)
        db.createElement('Child', {'label': 'Child 1'});
        db.createElement('Child', {'label': 'Child 2'});

        // Set sibling relation (self-reference)
        db.setScalarRelation('Child', 'sibling_id', 'Child 1', 'Child 2');

        // Read sibling relations
        final labels = db.readScalarRelation('Child', 'sibling_id');
        expect(labels.length, equals(2));
        expect(labels[0], equals('Child 2')); // Child 1's sibling is Child 2
        expect(labels[1], isNull); // Child 2 has no sibling set
      } finally {
        db.close();
      }
    });

    test('read scalar relation empty result', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'relations.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});

        // No Child elements created
        final labels = db.readScalarRelation('Child', 'parent_id');
        expect(labels, isEmpty);
      } finally {
        db.close();
      }
    });
  });

  // Read by ID tests

  group('Read Scalar Integers by ID', () {
    test('reads integer by specific element ID', () {
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

        expect(db.readScalarIntegerById('Configuration', 'integer_attribute', 1), equals(42));
        expect(db.readScalarIntegerById('Configuration', 'integer_attribute', 2), equals(100));
        expect(db.readScalarIntegerById('Configuration', 'integer_attribute', 999), isNull);
      } finally {
        db.close();
      }
    });
  });

  group('Read Scalar Floats by ID', () {
    test('reads float by specific element ID', () {
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

        expect(db.readScalarFloatById('Configuration', 'float_attribute', 1), equals(3.14));
        expect(db.readScalarFloatById('Configuration', 'float_attribute', 2), equals(2.71));
      } finally {
        db.close();
      }
    });
  });

  group('Read Scalar Strings by ID', () {
    test('reads string by specific element ID', () {
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

        expect(db.readScalarStringById('Configuration', 'string_attribute', 1), equals('hello'));
        expect(db.readScalarStringById('Configuration', 'string_attribute', 2), equals('world'));
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

    test('reads date time by ID', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {
          'label': 'Config 1',
          'date_attribute': '2024-01-15T10:30:00',
        });

        final date = db.readScalarStringById('Configuration', 'date_attribute', 1);
        expect(date, equals('2024-01-15T10:30:00'));
      } finally {
        db.close();
      }
    });

    test('readAllScalarsById returns native DateTime objects', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {
          'label': 'Config 1',
          'date_attribute': '2024-01-15T10:30:00',
        });

        final scalars = db.readAllScalarsById('Configuration', 1);
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

  group('Read Vector Integers by ID', () {
    test('reads int vector by specific element ID', () {
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

        expect(db.readVectorIntegersById('Collection', 'value_int', 1), equals([1, 2, 3]));
        expect(db.readVectorIntegersById('Collection', 'value_int', 2), equals([10, 20]));
      } finally {
        db.close();
      }
    });
  });

  group('Read Vector Floats by ID', () {
    test('reads float vector by specific element ID', () {
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

        expect(db.readVectorFloatsById('Collection', 'value_float', 1), equals([1.5, 2.5, 3.5]));
      } finally {
        db.close();
      }
    });
  });

  group('Read Set Strings by ID', () {
    test('reads string set by specific element ID', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        db.createElement('Collection', {
          'label': 'Item 1',
          'tag': ['important', 'urgent'],
        });
        db.createElement('Collection', {
          'label': 'Item 2',
          'tag': ['review'],
        });

        final result1 = db.readSetStringsById('Collection', 'tag', 1);
        expect(result1..sort(), equals(['important', 'urgent']));
        expect(db.readSetStringsById('Collection', 'tag', 2), equals(['review']));
      } finally {
        db.close();
      }
    });
  });

  group('Read Vector by ID Empty', () {
    test('returns empty list when element has no vector data', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        db.createElement('Collection', {'label': 'Item 1'}); // No vector data

        expect(db.readVectorIntegersById('Collection', 'value_int', 1), isEmpty);
      } finally {
        db.close();
      }
    });
  });

  // Read element IDs tests

  group('Read Element IDs', () {
    test('reads all element IDs from collection', () {
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

  group('Read By ID Not Found', () {
    test('returns null for nonexistent integer ID', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {
          'label': 'Config 1',
          'integer_attribute': 42,
        });

        expect(db.readScalarIntegerById('Configuration', 'integer_attribute', 999), isNull);
      } finally {
        db.close();
      }
    });

    test('returns null for nonexistent float ID', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {
          'label': 'Config 1',
          'float_attribute': 3.14,
        });

        expect(db.readScalarFloatById('Configuration', 'float_attribute', 999), isNull);
      } finally {
        db.close();
      }
    });

    test('returns null for nonexistent string ID', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {
          'label': 'Config 1',
          'string_attribute': 'hello',
        });

        expect(db.readScalarStringById('Configuration', 'string_attribute', 999), isNull);
      } finally {
        db.close();
      }
    });
  });

  group('Read Vector Invalid Collection', () {
    test('throws on nonexistent collection for vector integers', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        expect(
          () => db.readVectorIntegers('NonexistentCollection', 'value_int'),
          throwsA(isA<DatabaseException>()),
        );
      } finally {
        db.close();
      }
    });

    test('throws on nonexistent collection for vector floats', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        expect(
          () => db.readVectorFloats('NonexistentCollection', 'value_float'),
          throwsA(isA<DatabaseException>()),
        );
      } finally {
        db.close();
      }
    });
  });

  group('Read Set Invalid Collection', () {
    test('throws on nonexistent collection for set strings', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        expect(
          () => db.readSetStrings('NonexistentCollection', 'tag'),
          throwsA(isA<DatabaseException>()),
        );
      } finally {
        db.close();
      }
    });
  });

  group('Read Element IDs Invalid Collection', () {
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

  group('Read Scalar Relation Invalid Collection', () {
    test('throws on nonexistent collection', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'relations.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        expect(
          () => db.readScalarRelation('NonexistentCollection', 'parent_id'),
          throwsA(isA<DatabaseException>()),
        );
      } finally {
        db.close();
      }
    });
  });
}
