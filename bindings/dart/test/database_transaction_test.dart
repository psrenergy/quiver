import 'package:quiverdb/quiverdb.dart';
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

  group('Transaction', () {
    test('begin and commit persists', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Config'});
        db.beginTransaction();
        db.createElement('Collection', {
          'label': 'Item 1',
          'some_integer': 10,
        });
        db.commit();

        final labels = db.readScalarStrings('Collection', 'label');
        expect(labels.length, equals(1));
        expect(labels[0], equals('Item 1'));
      } finally {
        db.close();
      }
    });

    test('begin and rollback discards', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Config'});
        db.beginTransaction();
        db.createElement('Collection', {
          'label': 'Item 1',
          'some_integer': 10,
        });
        db.rollback();

        final labels = db.readScalarStrings('Collection', 'label');
        expect(labels.length, equals(0));
      } finally {
        db.close();
      }
    });

    test('double begin error', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Config'});
        db.beginTransaction();
        expect(
          () => db.beginTransaction(),
          throwsA(isA<DatabaseException>()),
        );
        // Clean up: rollback the first transaction
        db.rollback();
      } finally {
        db.close();
      }
    });

    test('commit without begin error', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Config'});
        expect(
          () => db.commit(),
          throwsA(isA<DatabaseException>()),
        );
      } finally {
        db.close();
      }
    });

    test('rollback without begin error', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Config'});
        expect(
          () => db.rollback(),
          throwsA(isA<DatabaseException>()),
        );
      } finally {
        db.close();
      }
    });

    test('inTransaction state', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Config'});

        expect(db.inTransaction(), isFalse);
        db.beginTransaction();
        expect(db.inTransaction(), isTrue);
        db.commit();
        expect(db.inTransaction(), isFalse);
      } finally {
        db.close();
      }
    });

    test('transaction block auto-commits', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Config'});

        final result = db.transaction((db) {
          db.createElement('Collection', {
            'label': 'Item 1',
            'some_integer': 42,
          });
          return 42;
        });

        expect(result, equals(42));
        final labels = db.readScalarStrings('Collection', 'label');
        expect(labels.length, equals(1));
        expect(labels[0], equals('Item 1'));
      } finally {
        db.close();
      }
    });

    test('transaction block rollback on exception', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Config'});

        expect(
          () => db.transaction((db) {
            db.createElement('Collection', {
              'label': 'Item 1',
              'some_integer': 10,
            });
            throw Exception('intentional error');
          }),
          throwsA(isA<Exception>()),
        );

        final labels = db.readScalarStrings('Collection', 'label');
        expect(labels.length, equals(0));
      } finally {
        db.close();
      }
    });

    test('multi-operation batch', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Config'});

        db.transaction((db) {
          db.createElement('Collection', {
            'label': 'Item 1',
            'some_integer': 10,
          });
          db.createElement('Collection', {
            'label': 'Item 2',
            'some_integer': 20,
          });

          db.updateElement('Collection', 1, {
            'some_integer': 100,
          });
          return null;
        });

        final labels = db.readScalarStrings('Collection', 'label');
        expect(labels.length, equals(2));
        expect(labels.contains('Item 1'), isTrue);
        expect(labels.contains('Item 2'), isTrue);

        final integers = db.readScalarIntegers('Collection', 'some_integer');
        expect(integers.contains(100), isTrue);
        expect(integers.contains(20), isTrue);
      } finally {
        db.close();
      }
    });
  });
}
