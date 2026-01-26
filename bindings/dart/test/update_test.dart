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

        final floatValue = db.readScalarFloatById('Configuration', 'float_attribute', 1);
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

    test('updates arrays', () {
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

        // Update with both scalar and array values - both should be updated
        db.updateElement('Collection', 1, {
          'some_integer': 999,
          'value_int': [7, 8, 9],
        });

        // Verify scalar was updated
        final intValue = db.readScalarIntegerById('Collection', 'some_integer', 1);
        expect(intValue, equals(999));

        // Verify vector was also updated
        final vecValues = db.readVectorIntegersById('Collection', 'value_int', 1);
        expect(vecValues, equals([7, 8, 9]));
      } finally {
        db.close();
      }
    });

    test('updates sets', () {
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

        // Update with only set attribute
        db.updateElement('Collection', 1, {
          'tag': ['new_tag1', 'new_tag2'],
        });

        // Verify set was updated
        final tagValues = db.readSetStringsById('Collection', 'tag', 1);
        expect(tagValues.toSet(), equals({'new_tag1', 'new_tag2'}));

        // Verify label unchanged
        final label = db.readScalarStringById('Collection', 'label', 1);
        expect(label, equals('Item 1'));
      } finally {
        db.close();
      }
    });

    test('updates vectors and sets atomically', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        db.createElement('Collection', {
          'label': 'Item 1',
          'value_int': [1, 2, 3],
          'tag': ['old_tag'],
        });

        // Update both vector and set atomically
        db.updateElement('Collection', 1, {
          'value_int': [100, 200],
          'tag': ['new_tag1', 'new_tag2'],
        });

        // Verify vector was updated
        final vec = db.readVectorIntegersById('Collection', 'value_int', 1);
        expect(vec, equals([100, 200]));

        // Verify set was updated
        final set = db.readSetStringsById('Collection', 'tag', 1);
        expect(set.toSet(), equals({'new_tag1', 'new_tag2'}));
      } finally {
        db.close();
      }
    });

    test('throws on invalid array attribute', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        db.createElement('Collection', {'label': 'Item 1'});

        expect(
          () => db.updateElement('Collection', 1, {
            'nonexistent_attr': [1, 2, 3],
          }),
          throwsA(isA<DatabaseException>()),
        );
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

        final value = db.readScalarFloatById('Configuration', 'float_attribute', 1);
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

  // ==========================================================================
  // Scalar update functions tests
  // ==========================================================================

  group('Update Scalar Integer', () {
    test('basic update', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Config 1', 'integer_attribute': 42});

        db.updateScalarInteger('Configuration', 'integer_attribute', 1, 100);

        final value = db.readScalarIntegerById('Configuration', 'integer_attribute', 1);
        expect(value, equals(100));
      } finally {
        db.close();
      }
    });

    test('update to zero', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Config 1', 'integer_attribute': 42});

        db.updateScalarInteger('Configuration', 'integer_attribute', 1, 0);

        final value = db.readScalarIntegerById('Configuration', 'integer_attribute', 1);
        expect(value, equals(0));
      } finally {
        db.close();
      }
    });

    test('update to negative', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Config 1', 'integer_attribute': 42});

        db.updateScalarInteger('Configuration', 'integer_attribute', 1, -999);

        final value = db.readScalarIntegerById('Configuration', 'integer_attribute', 1);
        expect(value, equals(-999));
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
        db.createElement('Configuration', {'label': 'Config 1', 'integer_attribute': 42});
        db.createElement('Configuration', {'label': 'Config 2', 'integer_attribute': 100});

        db.updateScalarInteger('Configuration', 'integer_attribute', 1, 999);

        expect(db.readScalarIntegerById('Configuration', 'integer_attribute', 1), equals(999));
        expect(db.readScalarIntegerById('Configuration', 'integer_attribute', 2), equals(100));
      } finally {
        db.close();
      }
    });

    test('throws on invalid collection', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        expect(
          () => db.updateScalarInteger('NonexistentCollection', 'integer_attribute', 1, 42),
          throwsA(isA<DatabaseException>()),
        );
      } finally {
        db.close();
      }
    });

    test('throws on invalid attribute', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Config 1', 'integer_attribute': 42});
        expect(
          () => db.updateScalarInteger('Configuration', 'nonexistent_attribute', 1, 100),
          throwsA(isA<DatabaseException>()),
        );
      } finally {
        db.close();
      }
    });
  });

  group('Update Scalar Float', () {
    test('basic update', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Config 1', 'float_attribute': 3.14});

        db.updateScalarFloat('Configuration', 'float_attribute', 1, 2.71);

        final value = db.readScalarFloatById('Configuration', 'float_attribute', 1);
        expect(value, equals(2.71));
      } finally {
        db.close();
      }
    });

    test('update to zero', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Config 1', 'float_attribute': 3.14});

        db.updateScalarFloat('Configuration', 'float_attribute', 1, 0.0);

        final value = db.readScalarFloatById('Configuration', 'float_attribute', 1);
        expect(value, equals(0.0));
      } finally {
        db.close();
      }
    });

    test('precision test', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Config 1', 'float_attribute': 1.0});

        db.updateScalarFloat('Configuration', 'float_attribute', 1, 1.23456789012345);

        final value = db.readScalarFloatById('Configuration', 'float_attribute', 1);
        expect(value, closeTo(1.23456789012345, 1e-10));
      } finally {
        db.close();
      }
    });
  });

  group('Update Scalar String', () {
    test('basic update', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Config 1', 'string_attribute': 'hello'});

        db.updateScalarString('Configuration', 'string_attribute', 1, 'world');

        final value = db.readScalarStringById('Configuration', 'string_attribute', 1);
        expect(value, equals('world'));
      } finally {
        db.close();
      }
    });

    test('update to empty string', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Config 1', 'string_attribute': 'hello'});

        db.updateScalarString('Configuration', 'string_attribute', 1, '');

        final value = db.readScalarStringById('Configuration', 'string_attribute', 1);
        expect(value, equals(''));
      } finally {
        db.close();
      }
    });

    test('unicode support', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Config 1', 'string_attribute': 'hello'});

        db.updateScalarString('Configuration', 'string_attribute', 1, '日本語テスト');

        final value = db.readScalarStringById('Configuration', 'string_attribute', 1);
        expect(value, equals('日本語テスト'));
      } finally {
        db.close();
      }
    });
  });

  // ==========================================================================
  // Vector update functions tests
  // ==========================================================================

  group('Update Vector Integers', () {
    test('replace existing vector', () {
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

        db.updateVectorIntegers('Collection', 'value_int', 1, [10, 20, 30, 40]);

        final values = db.readVectorIntegersById('Collection', 'value_int', 1);
        expect(values, equals([10, 20, 30, 40]));
      } finally {
        db.close();
      }
    });

    test('update to smaller vector', () {
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

        db.updateVectorIntegers('Collection', 'value_int', 1, [100]);

        final values = db.readVectorIntegersById('Collection', 'value_int', 1);
        expect(values, equals([100]));
      } finally {
        db.close();
      }
    });

    test('update to empty vector', () {
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

        db.updateVectorIntegers('Collection', 'value_int', 1, []);

        final values = db.readVectorIntegersById('Collection', 'value_int', 1);
        expect(values, isEmpty);
      } finally {
        db.close();
      }
    });

    test('update from empty to non-empty', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        db.createElement('Collection', {'label': 'Item 1'});

        // Verify initially empty
        expect(db.readVectorIntegersById('Collection', 'value_int', 1), isEmpty);

        db.updateVectorIntegers('Collection', 'value_int', 1, [1, 2, 3]);

        final values = db.readVectorIntegersById('Collection', 'value_int', 1);
        expect(values, equals([1, 2, 3]));
      } finally {
        db.close();
      }
    });

    test('other elements unchanged', () {
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

        db.updateVectorIntegers('Collection', 'value_int', 1, [100, 200]);

        expect(db.readVectorIntegersById('Collection', 'value_int', 1), equals([100, 200]));
        expect(db.readVectorIntegersById('Collection', 'value_int', 2), equals([10, 20]));
      } finally {
        db.close();
      }
    });

    test('throws on invalid collection', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        expect(
          () => db.updateVectorIntegers('NonexistentCollection', 'value_int', 1, [1, 2, 3]),
          throwsA(isA<DatabaseException>()),
        );
      } finally {
        db.close();
      }
    });
  });

  group('Update Vector Floats', () {
    test('replace existing vector', () {
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

        db.updateVectorFloats('Collection', 'value_float', 1, [10.5, 20.5]);

        final values = db.readVectorFloatsById('Collection', 'value_float', 1);
        expect(values, equals([10.5, 20.5]));
      } finally {
        db.close();
      }
    });

    test('precision test', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        db.createElement('Collection', {
          'label': 'Item 1',
          'value_float': [1.0],
        });

        db.updateVectorFloats('Collection', 'value_float', 1, [1.23456789, 9.87654321]);

        final values = db.readVectorFloatsById('Collection', 'value_float', 1);
        expect(values[0], closeTo(1.23456789, 1e-8));
        expect(values[1], closeTo(9.87654321, 1e-8));
      } finally {
        db.close();
      }
    });

    test('update to empty vector', () {
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

        db.updateVectorFloats('Collection', 'value_float', 1, []);

        final values = db.readVectorFloatsById('Collection', 'value_float', 1);
        expect(values, isEmpty);
      } finally {
        db.close();
      }
    });
  });

  // ==========================================================================
  // Set update functions tests
  // ==========================================================================

  group('Update Set Strings', () {
    test('replace existing set', () {
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

        db.updateSetStrings('Collection', 'tag', 1, ['new_tag1', 'new_tag2', 'new_tag3']);

        final values = db.readSetStringsById('Collection', 'tag', 1);
        expect(values.toSet(), equals({'new_tag1', 'new_tag2', 'new_tag3'}));
      } finally {
        db.close();
      }
    });

    test('update to single element', () {
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

        db.updateSetStrings('Collection', 'tag', 1, ['single_tag']);

        final values = db.readSetStringsById('Collection', 'tag', 1);
        expect(values, equals(['single_tag']));
      } finally {
        db.close();
      }
    });

    test('update to empty set', () {
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

        db.updateSetStrings('Collection', 'tag', 1, []);

        final values = db.readSetStringsById('Collection', 'tag', 1);
        expect(values, isEmpty);
      } finally {
        db.close();
      }
    });

    test('update from empty to non-empty', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        db.createElement('Collection', {'label': 'Item 1'});

        // Verify initially empty
        expect(db.readSetStringsById('Collection', 'tag', 1), isEmpty);

        db.updateSetStrings('Collection', 'tag', 1, ['important', 'urgent']);

        final values = db.readSetStringsById('Collection', 'tag', 1);
        expect(values.toSet(), equals({'important', 'urgent'}));
      } finally {
        db.close();
      }
    });

    test('other elements unchanged', () {
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
          'tag': ['urgent', 'review'],
        });

        db.updateSetStrings('Collection', 'tag', 1, ['updated']);

        expect(db.readSetStringsById('Collection', 'tag', 1), equals(['updated']));
        expect(db.readSetStringsById('Collection', 'tag', 2).toSet(), equals({'urgent', 'review'}));
      } finally {
        db.close();
      }
    });

    test('unicode support', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        db.createElement('Collection', {
          'label': 'Item 1',
          'tag': ['tag1'],
        });

        db.updateSetStrings('Collection', 'tag', 1, ['日本語', '中文', '한국어']);

        final values = db.readSetStringsById('Collection', 'tag', 1);
        expect(values.toSet(), equals({'日本語', '中文', '한국어'}));
      } finally {
        db.close();
      }
    });

    test('throws on invalid collection', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        expect(
          () => db.updateSetStrings('NonexistentCollection', 'tag', 1, ['tag1']),
          throwsA(isA<DatabaseException>()),
        );
      } finally {
        db.close();
      }
    });
  });

}
