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

  group('Query String', () {
    test('returns value when row exists', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {
          'label': 'Test Label',
          'string_attribute': 'hello world',
        });

        final result = db.queryString(
          "SELECT string_attribute FROM Configuration WHERE label = 'Test Label'",
        );
        expect(result, equals('hello world'));
      } finally {
        db.close();
      }
    });

    test('returns null when no rows', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        final result = db.queryString(
          'SELECT string_attribute FROM Configuration WHERE 1 = 0',
        );
        expect(result, isNull);
      } finally {
        db.close();
      }
    });

    test('returns first row when multiple rows', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {
          'label': 'First',
          'string_attribute': 'first value',
        });
        db.createElement('Configuration', {
          'label': 'Second',
          'string_attribute': 'second value',
        });

        final result = db.queryString(
          'SELECT string_attribute FROM Configuration ORDER BY label',
        );
        expect(result, equals('first value'));
      } finally {
        db.close();
      }
    });
  });

  group('Query Integer', () {
    test('returns value when row exists', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {
          'label': 'Test',
          'integer_attribute': 42,
        });

        final result = db.queryInteger(
          "SELECT integer_attribute FROM Configuration WHERE label = 'Test'",
        );
        expect(result, equals(42));
      } finally {
        db.close();
      }
    });

    test('returns null when no rows', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        final result = db.queryInteger(
          'SELECT integer_attribute FROM Configuration WHERE 1 = 0',
        );
        expect(result, isNull);
      } finally {
        db.close();
      }
    });

    test('returns count', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'A'});
        db.createElement('Configuration', {'label': 'B'});
        db.createElement('Configuration', {'label': 'C'});

        final result = db.queryInteger('SELECT COUNT(*) FROM Configuration');
        expect(result, equals(3));
      } finally {
        db.close();
      }
    });
  });

  group('Query Float', () {
    test('returns value when row exists', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {
          'label': 'Test',
          'float_attribute': 3.14159,
        });

        final result = db.queryFloat(
          "SELECT float_attribute FROM Configuration WHERE label = 'Test'",
        );
        expect(result, closeTo(3.14159, 0.00001));
      } finally {
        db.close();
      }
    });

    test('returns null when no rows', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        final result = db.queryFloat(
          'SELECT float_attribute FROM Configuration WHERE 1 = 0',
        );
        expect(result, isNull);
      } finally {
        db.close();
      }
    });

    test('returns average', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {
          'label': 'A',
          'float_attribute': 10.0,
        });
        db.createElement('Configuration', {
          'label': 'B',
          'float_attribute': 20.0,
        });

        final result = db.queryFloat(
          'SELECT AVG(float_attribute) FROM Configuration',
        );
        expect(result, equals(15.0));
      } finally {
        db.close();
      }
    });
  });

  group('Query String with Params', () {
    test('returns value with string param', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {
          'label': 'Test Label',
          'string_attribute': 'hello world',
        });

        final result = db.queryStringParams(
          'SELECT string_attribute FROM Configuration WHERE label = ?',
          ['Test Label'],
        );
        expect(result, equals('hello world'));
      } finally {
        db.close();
      }
    });

    test('returns null when no match', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {
          'label': 'Test',
          'string_attribute': 'hello',
        });

        final result = db.queryStringParams(
          'SELECT string_attribute FROM Configuration WHERE label = ?',
          ['NoMatch'],
        );
        expect(result, isNull);
      } finally {
        db.close();
      }
    });
  });

  group('Query Integer with Params', () {
    test('returns value with string param', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {
          'label': 'Test',
          'integer_attribute': 42,
        });

        final result = db.queryIntegerParams(
          'SELECT integer_attribute FROM Configuration WHERE label = ?',
          ['Test'],
        );
        expect(result, equals(42));
      } finally {
        db.close();
      }
    });

    test('returns value with integer param', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {
          'label': 'A',
          'integer_attribute': 10,
        });
        db.createElement('Configuration', {
          'label': 'B',
          'integer_attribute': 20,
        });

        final result = db.queryIntegerParams(
          'SELECT integer_attribute FROM Configuration WHERE integer_attribute > ? ORDER BY integer_attribute',
          [15],
        );
        expect(result, equals(20));
      } finally {
        db.close();
      }
    });
  });

  group('Query Float with Params', () {
    test('returns value with string param', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {
          'label': 'Test',
          'float_attribute': 3.14159,
        });

        final result = db.queryFloatParams(
          'SELECT float_attribute FROM Configuration WHERE label = ?',
          ['Test'],
        );
        expect(result, closeTo(3.14159, 0.00001));
      } finally {
        db.close();
      }
    });
  });

  group('Query with Multiple Params', () {
    test('returns value with mixed params', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {
          'label': 'A',
          'integer_attribute': 10,
        });
        db.createElement('Configuration', {
          'label': 'B',
          'integer_attribute': 20,
        });

        final result = db.queryIntegerParams(
          'SELECT integer_attribute FROM Configuration WHERE label = ? AND integer_attribute > ?',
          ['B', 5],
        );
        expect(result, equals(20));
      } finally {
        db.close();
      }
    });
  });

  group('Query DateTime with Params', () {
    test('returns value with string param', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {
          'label': 'Test',
          'date_attribute': DateTime(2024, 6, 15, 10, 30, 0),
        });

        final result = db.queryDateTimeParams(
          'SELECT date_attribute FROM Configuration WHERE label = ?',
          ['Test'],
        );
        expect(result, equals(DateTime(2024, 6, 15, 10, 30, 0)));
      } finally {
        db.close();
      }
    });
  });

  group('Query DateTime', () {
    test('returns value when row exists', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {
          'label': 'Test',
          'date_attribute': DateTime(2024, 6, 15, 10, 30, 0),
        });

        final result = db.queryDateTime(
          "SELECT date_attribute FROM Configuration WHERE label = 'Test'",
        );
        expect(result, equals(DateTime(2024, 6, 15, 10, 30, 0)));
      } finally {
        db.close();
      }
    });

    test('returns null when no rows', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        final result = db.queryDateTime(
          'SELECT date_attribute FROM Configuration WHERE 1 = 0',
        );
        expect(result, isNull);
      } finally {
        db.close();
      }
    });
  });
}
