import 'package:quiverdb/quiverdb.dart';
import 'package:test/test.dart';
import 'package:path/path.dart' as path;

void main() {
  // Path to central tests folder
  final testsPath = path.join(path.current, '..', '..', 'tests');

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

  group('Read Vector Integers by Id', () {
    test('reads int vector by specific element Id', () {
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
          db.readVectorIntegersById('Collection', 'value_int', 1),
          equals([1, 2, 3]),
        );
        expect(
          db.readVectorIntegersById('Collection', 'value_int', 2),
          equals([10, 20]),
        );
      } finally {
        db.close();
      }
    });
  });

  group('Read Vector Floats by Id', () {
    test('reads float vector by specific element Id', () {
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

        expect(
          db.readVectorFloatsById('Collection', 'value_float', 1),
          equals([1.5, 2.5, 3.5]),
        );
      } finally {
        db.close();
      }
    });
  });

  group('Read Vector by Id Empty', () {
    test('returns empty list when element has no vector data', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        db.createElement('Collection', {'label': 'Item 1'}); // No vector data

        expect(
          db.readVectorIntegersById('Collection', 'value_int', 1),
          isEmpty,
        );
      } finally {
        db.close();
      }
    });
  });

  // ===========================================================================
  // Gap-fill: String vector, integer set, float set reads (using all_types.sql)
  // ===========================================================================

  group('Read Vector Strings', () {
    test('reads vector strings bulk', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'all_types.sql'),
      );
      try {
        db.createElement('AllTypes', {'label': 'Item 1'});
        db.createElement('AllTypes', {'label': 'Item 2'});

        db.updateElement('AllTypes', 1, {
          'label_value': ['alpha', 'beta'],
        });
        db.updateElement('AllTypes', 2, {
          'label_value': ['gamma'],
        });

        final result = db.readVectorStrings('AllTypes', 'label_value');
        expect(result.length, equals(2));
        expect(result[0], equals(['alpha', 'beta']));
        expect(result[1], equals(['gamma']));
      } finally {
        db.close();
      }
    });

    test('reads vector strings by id', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'all_types.sql'),
      );
      try {
        db.createElement('AllTypes', {'label': 'Item 1'});
        db.updateElement('AllTypes', 1, {
          'label_value': ['alpha', 'beta', 'gamma'],
        });

        final result = db.readVectorStringsById('AllTypes', 'label_value', 1);
        expect(result, equals(['alpha', 'beta', 'gamma']));
      } finally {
        db.close();
      }
    });

    test('reads vector strings by id empty', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'all_types.sql'),
      );
      try {
        db.createElement('AllTypes', {'label': 'Item 1'});

        final result = db.readVectorStringsById('AllTypes', 'label_value', 1);
        expect(result, isEmpty);
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

  // ===========================================================================
  // Composite helper tests (using composite_helpers.sql)
  // ===========================================================================

  group('readVectorsById', () {
    test('returns all vector groups with correct types', () {
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
        });

        final result = db.readVectorsById('Items', id);

        expect(result.length, equals(3));
        expect(result['amount'], equals([10, 20, 30]));
        expect(result['score'], equals([1.1, 2.2]));
        expect(result['note'], equals(['hello', 'world']));

        // Verify types
        expect(result['amount']!.every((v) => v is int), isTrue);
        expect(result['score']!.every((v) => v is double), isTrue);
        expect(result['note']!.every((v) => v is String), isTrue);
      } finally {
        db.close();
      }
    });
  });

  group('Read Vector Group by Id', () {
    test('readVectorGroupById returns all columns as rows', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        db.createElement('Collection', {
          'label': 'Item 1',
          'value_int': [1, 2, 3],
          'value_float': [1.5, 2.5, 3.5],
        });

        final rows = db.readVectorGroupById('Collection', 'values', 1);
        expect(rows.length, equals(3));
        expect(rows[0]['value_int'], equals(1));
        expect(rows[0]['value_float'], equals(1.5));
        expect(rows[2]['value_int'], equals(3));
        expect(rows[2]['value_float'], equals(3.5));
      } finally {
        db.close();
      }
    });

    test('readVectorGroupById returns empty list for nonexistent id', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});

        final rows = db.readVectorGroupById('Collection', 'values', 999);
        expect(rows, isEmpty);
      } finally {
        db.close();
      }
    });
  });
}
