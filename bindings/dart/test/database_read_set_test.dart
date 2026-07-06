import 'package:quiverdb/quiverdb.dart';
import 'package:test/test.dart';
import 'package:path/path.dart' as path;

void main() {
  // Path to central tests folder
  final testsPath = path.join(path.current, '..', '..', 'tests');

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

  group('Read Set Strings by Id', () {
    test('reads string set by specific element Id', () {
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
        expect(
          db.readSetStringsById('Collection', 'tag', 2),
          equals(['review']),
        );
      } finally {
        db.close();
      }
    });
  });

  group('Read Set Integers', () {
    test('reads set integers bulk', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'all_types.sql'),
      );
      try {
        db.createElement('AllTypes', {'label': 'Item 1'});
        db.createElement('AllTypes', {'label': 'Item 2'});

        db.updateElement('AllTypes', 1, {
          'code': [10, 20, 30],
        });
        db.updateElement('AllTypes', 2, {
          'code': [40, 50],
        });

        final result = db.readSetIntegers('AllTypes', 'code');
        expect(result.length, equals(2));
        expect(result[0]..sort(), equals([10, 20, 30]));
        expect(result[1]..sort(), equals([40, 50]));
      } finally {
        db.close();
      }
    });

    test('reads set integers by id', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'all_types.sql'),
      );
      try {
        db.createElement('AllTypes', {'label': 'Item 1'});
        db.updateElement('AllTypes', 1, {
          'code': [10, 20, 30],
        });

        final result = db.readSetIntegersById('AllTypes', 'code', 1);
        expect(result..sort(), equals([10, 20, 30]));
      } finally {
        db.close();
      }
    });

    test('reads set integers by id empty', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'all_types.sql'),
      );
      try {
        db.createElement('AllTypes', {'label': 'Item 1'});

        final result = db.readSetIntegersById('AllTypes', 'code', 1);
        expect(result, isEmpty);
      } finally {
        db.close();
      }
    });
  });

  group('Read Set Floats', () {
    test('reads set floats bulk', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'all_types.sql'),
      );
      try {
        db.createElement('AllTypes', {'label': 'Item 1'});
        db.createElement('AllTypes', {'label': 'Item 2'});

        db.updateElement('AllTypes', 1, {
          'weight': [1.1, 2.2],
        });
        db.updateElement('AllTypes', 2, {
          'weight': [3.3, 4.4, 5.5],
        });

        final result = db.readSetFloats('AllTypes', 'weight');
        expect(result.length, equals(2));
        expect(result[0]..sort(), equals([1.1, 2.2]));
        expect(result[1]..sort(), equals([3.3, 4.4, 5.5]));
      } finally {
        db.close();
      }
    });

    test('reads set floats by id', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'all_types.sql'),
      );
      try {
        db.createElement('AllTypes', {'label': 'Item 1'});
        db.updateElement('AllTypes', 1, {
          'weight': [1.1, 2.2],
        });

        final result = db.readSetFloatsById('AllTypes', 'weight', 1);
        expect(result..sort(), equals([1.1, 2.2]));
      } finally {
        db.close();
      }
    });

    test('reads set floats by id empty', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'all_types.sql'),
      );
      try {
        db.createElement('AllTypes', {'label': 'Item 1'});

        final result = db.readSetFloatsById('AllTypes', 'weight', 1);
        expect(result, isEmpty);
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

  group('readSetsById', () {
    test('returns all set groups with correct types', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'composite_helpers.sql'),
      );
      try {
        final id = db.createElement('Items', {
          'label': 'Item 1',
          'code': [10, 20, 30],
          'weight': [1.1, 2.2],
          'tag': ['alpha', 'beta'],
        });

        final result = db.readSetsById('Items', id);

        expect(result.length, equals(3));
        expect(result['code']!..sort(), equals([10, 20, 30]));
        expect(result['weight']!..sort(), equals([1.1, 2.2]));
        expect(result['tag']!..sort(), equals(['alpha', 'beta']));

        // Verify types
        expect(result['code']!.every((v) => v is int), isTrue);
        expect(result['weight']!.every((v) => v is double), isTrue);
        expect(result['tag']!.every((v) => v is String), isTrue);
      } finally {
        db.close();
      }
    });
  });
}
