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

  group('Element Set Values', () {
    test('sets integer value', () {
      final element = Element();
      try {
        element.set('value', 42);
        // No exception means success
      } finally {
        element.dispose();
      }
    });

    test('sets float value', () {
      final element = Element();
      try {
        element.set('value', 3.14);
      } finally {
        element.dispose();
      }
    });

    test('sets string value', () {
      final element = Element();
      try {
        element.set('value', 'hello world');
      } finally {
        element.dispose();
      }
    });

    test('sets DateTime value', () {
      final element = Element();
      try {
        element.set('value', DateTime(2024, 1, 15, 10, 30, 0));
      } finally {
        element.dispose();
      }
    });

    test('sets null value', () {
      final element = Element();
      try {
        element.set('value', null);
      } finally {
        element.dispose();
      }
    });

    test('sets array integer', () {
      final element = Element();
      try {
        element.set('values', [1, 2, 3, 4, 5]);
      } finally {
        element.dispose();
      }
    });

    test('sets array float', () {
      final element = Element();
      try {
        element.set('values', [1.1, 2.2, 3.3]);
      } finally {
        element.dispose();
      }
    });

    test('sets array string', () {
      final element = Element();
      try {
        element.set('values', ['alpha', 'beta', 'gamma']);
      } finally {
        element.dispose();
      }
    });

    test('rejects empty array', () {
      final element = Element();
      try {
        expect(
          () => element.set('values', []),
          throwsA(isA<ArgumentError>()),
        );
      } finally {
        element.dispose();
      }
    });
  });

  group('Element Type-Specific Setters', () {
    test('setInteger', () {
      final element = Element();
      try {
        element.setInteger('value', 42);
      } finally {
        element.dispose();
      }
    });

    test('setFloat', () {
      final element = Element();
      try {
        element.setFloat('value', 3.14);
      } finally {
        element.dispose();
      }
    });

    test('setString', () {
      final element = Element();
      try {
        element.setString('value', 'hello');
      } finally {
        element.dispose();
      }
    });

    test('setDateTime', () {
      final element = Element();
      try {
        element.setDateTime('value', DateTime(2024, 1, 15));
      } finally {
        element.dispose();
      }
    });

    test('setNull', () {
      final element = Element();
      try {
        element.setNull('value');
      } finally {
        element.dispose();
      }
    });

    test('setArrayInteger', () {
      final element = Element();
      try {
        element.setArrayInteger('values', [1, 2, 3]);
      } finally {
        element.dispose();
      }
    });

    test('setArrayFloat', () {
      final element = Element();
      try {
        element.setArrayFloat('values', [1.1, 2.2, 3.3]);
      } finally {
        element.dispose();
      }
    });

    test('setArrayString', () {
      final element = Element();
      try {
        element.setArrayString('values', ['a', 'b', 'c']);
      } finally {
        element.dispose();
      }
    });
  });

  group('Element Dispose', () {
    test('dispose is idempotent', () {
      final element = Element();
      element.dispose();
      element.dispose(); // Should not throw
    });

    test('throws after dispose', () {
      final element = Element();
      element.dispose();
      expect(
        () => element.set('value', 42),
        throwsA(isA<StateError>()),
      );
    });
  });

  group('Element Clear', () {
    test('clear removes all values', () {
      final element = Element();
      try {
        element.set('value1', 42);
        element.set('value2', 'hello');
        element.clear();
        // No exception means success
      } finally {
        element.dispose();
      }
    });
  });

  group('Element with Database', () {
    test('creates element with builder', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        final element = Element();
        try {
          element.set('label', 'Config via Builder');
          element.set('integer_attribute', 42);
          element.set('float_attribute', 3.14);
          element.set('string_attribute', 'hello');

          final id = db.createElementFromBuilder('Configuration', element);
          expect(id, greaterThan(0));

          // Verify the element was created correctly
          expect(
            db.readScalarStringById('Configuration', 'label', id),
            equals('Config via Builder'),
          );
          expect(
            db.readScalarIntegerById('Configuration', 'integer_attribute', id),
            equals(42),
          );
          expect(
            db.readScalarFloatById('Configuration', 'float_attribute', id),
            equals(3.14),
          );
          expect(
            db.readScalarStringById('Configuration', 'string_attribute', id),
            equals('hello'),
          );
        } finally {
          element.dispose();
        }
      } finally {
        db.close();
      }
    });

    test('creates element with vector via builder', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});

        final element = Element();
        try {
          element.set('label', 'Item via Builder');
          element.set('value_int', [1, 2, 3]);
          element.set('value_float', [1.5, 2.5, 3.5]);

          final id = db.createElementFromBuilder('Collection', element);
          expect(id, greaterThan(0));

          // Verify vectors were created correctly
          expect(
            db.readVectorIntegersById('Collection', 'value_int', id),
            equals([1, 2, 3]),
          );
          expect(
            db.readVectorFloatsById('Collection', 'value_float', id),
            equals([1.5, 2.5, 3.5]),
          );
        } finally {
          element.dispose();
        }
      } finally {
        db.close();
      }
    });

    test('creates element with set via builder', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});

        final element = Element();
        try {
          element.set('label', 'Item with Set');
          element.set('tag', ['alpha', 'beta', 'gamma']);

          final id = db.createElementFromBuilder('Collection', element);
          expect(id, greaterThan(0));

          // Verify set was created correctly
          final result = db.readSetStringsById('Collection', 'tag', id);
          expect(result..sort(), equals(['alpha', 'beta', 'gamma']));
        } finally {
          element.dispose();
        }
      } finally {
        db.close();
      }
    });

    test('updates element with builder', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {
          'label': 'Config 1',
          'integer_attribute': 100,
        });

        final element = Element();
        try {
          element.set('integer_attribute', 777);
          db.updateElementFromBuilder('Configuration', 1, element);
        } finally {
          element.dispose();
        }

        // Verify update
        expect(
          db.readScalarIntegerById('Configuration', 'integer_attribute', 1),
          equals(777),
        );
      } finally {
        db.close();
      }
    });
  });
}
