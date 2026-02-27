import 'package:quiverdb/quiverdb.dart';
import 'package:test/test.dart';
import 'package:path/path.dart' as path;

void main() {
  // Path to central tests folder
  final testsPath = path.join(path.current, '..', '..', 'tests');

  group('Update Element', () {
    test('updates single scalar attribute', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {
          'label': 'Config 1',
          'integer_attribute': 100,
        });
        db.createElement('Configuration', {
          'label': 'Config 2',
          'integer_attribute': 200,
        });

        // Update single attribute
        db.updateElement('Configuration', 1, {'integer_attribute': 999});

        // Verify update
        final value = db.readScalarIntegerById(
          'Configuration',
          'integer_attribute',
          1,
        );
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
        final intValue = db.readScalarIntegerById(
          'Configuration',
          'integer_attribute',
          1,
        );
        expect(intValue, equals(500));

        final floatValue = db.readScalarFloatById(
          'Configuration',
          'float_attribute',
          1,
        );
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
        db.createElement('Configuration', {
          'label': 'Config 1',
          'integer_attribute': 100,
        });
        db.createElement('Configuration', {
          'label': 'Config 2',
          'integer_attribute': 200,
        });
        db.createElement('Configuration', {
          'label': 'Config 3',
          'integer_attribute': 300,
        });

        // Update only element 2
        db.updateElement('Configuration', 2, {'integer_attribute': 999});

        // Verify element 2 updated
        final value2 = db.readScalarIntegerById(
          'Configuration',
          'integer_attribute',
          2,
        );
        expect(value2, equals(999));

        // Verify elements 1 and 3 unchanged
        final value1 = db.readScalarIntegerById(
          'Configuration',
          'integer_attribute',
          1,
        );
        expect(value1, equals(100));

        final value3 = db.readScalarIntegerById(
          'Configuration',
          'integer_attribute',
          3,
        );
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
        final intValue = db.readScalarIntegerById(
          'Collection',
          'some_integer',
          1,
        );
        expect(intValue, equals(999));

        // Verify vector was also updated
        final vecValues = db.readVectorIntegersById(
          'Collection',
          'value_int',
          1,
        );
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
        db.createElement('Configuration', {
          'label': 'Config 1',
          'integer_attribute': 100,
        });

        // Update using Element builder
        final element = Element();
        try {
          element.set('integer_attribute', 777);
          db.updateElementFromBuilder('Configuration', 1, element);
        } finally {
          element.dispose();
        }

        // Verify update
        final value = db.readScalarIntegerById(
          'Configuration',
          'integer_attribute',
          1,
        );
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
        db.createElement('Configuration', {
          'label': 'Config 1',
          'integer_attribute': 100,
        });
        expect(
          () => db.updateElement('NonexistentCollection', 1, {
            'integer_attribute': 999,
          }),
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
        db.createElement('Configuration', {
          'label': 'Config 1',
          'integer_attribute': 100,
        });

        // Update element that doesn't exist - should not throw
        db.updateElement('Configuration', 999, {'integer_attribute': 500});

        // Verify original element unchanged
        final value = db.readScalarIntegerById(
          'Configuration',
          'integer_attribute',
          1,
        );
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
        db.createElement('Configuration', {
          'label': 'Config 1',
          'integer_attribute': 100,
        });
        expect(
          () => db.updateElement('Configuration', 1, {
            'nonexistent_attribute': 999,
          }),
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
        db.createElement('Configuration', {
          'label': 'Config 1',
          'string_attribute': 'original',
        });

        db.updateElement('Configuration', 1, {'string_attribute': 'updated'});

        final value = db.readScalarStringById(
          'Configuration',
          'string_attribute',
          1,
        );
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
        db.createElement('Configuration', {
          'label': 'Config 1',
          'float_attribute': 1.5,
        });

        db.updateElement('Configuration', 1, {'float_attribute': 99.99});

        final value = db.readScalarFloatById(
          'Configuration',
          'float_attribute',
          1,
        );
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
        db.createElement('Configuration', {
          'label': 'Config 1',
          'integer_attribute': 100,
        });
        db.createElement('Configuration', {
          'label': 'Config 2',
          'integer_attribute': 200,
        });

        // Update both elements
        db.updateElement('Configuration', 1, {'integer_attribute': 111});
        db.updateElement('Configuration', 2, {'integer_attribute': 222});

        expect(
          db.readScalarIntegerById('Configuration', 'integer_attribute', 1),
          equals(111),
        );
        expect(
          db.readScalarIntegerById('Configuration', 'integer_attribute', 2),
          equals(222),
        );
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
        db.createElement('Configuration', {
          'label': 'Config 1',
          'integer_attribute': 42,
        });

        db.updateElement('Configuration', 1, {'integer_attribute': 100});

        final value = db.readScalarIntegerById(
          'Configuration',
          'integer_attribute',
          1,
        );
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
        db.createElement('Configuration', {
          'label': 'Config 1',
          'integer_attribute': 42,
        });

        db.updateElement('Configuration', 1, {'integer_attribute': 0});

        final value = db.readScalarIntegerById(
          'Configuration',
          'integer_attribute',
          1,
        );
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
        db.createElement('Configuration', {
          'label': 'Config 1',
          'integer_attribute': 42,
        });

        db.updateElement('Configuration', 1, {'integer_attribute': -999});

        final value = db.readScalarIntegerById(
          'Configuration',
          'integer_attribute',
          1,
        );
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
        db.createElement('Configuration', {
          'label': 'Config 1',
          'integer_attribute': 42,
        });
        db.createElement('Configuration', {
          'label': 'Config 2',
          'integer_attribute': 100,
        });

        db.updateElement('Configuration', 1, {'integer_attribute': 999});

        expect(
          db.readScalarIntegerById('Configuration', 'integer_attribute', 1),
          equals(999),
        );
        expect(
          db.readScalarIntegerById('Configuration', 'integer_attribute', 2),
          equals(100),
        );
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
          () => db.updateElement('NonexistentCollection', 1, {
            'integer_attribute': 42,
          }),
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
        db.createElement('Configuration', {
          'label': 'Config 1',
          'integer_attribute': 42,
        });
        expect(
          () => db.updateElement('Configuration', 1, {
            'nonexistent_attribute': 100,
          }),
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
        db.createElement('Configuration', {
          'label': 'Config 1',
          'float_attribute': 3.14,
        });

        db.updateElement('Configuration', 1, {'float_attribute': 2.71});

        final value = db.readScalarFloatById(
          'Configuration',
          'float_attribute',
          1,
        );
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
        db.createElement('Configuration', {
          'label': 'Config 1',
          'float_attribute': 3.14,
        });

        db.updateElement('Configuration', 1, {'float_attribute': 0.0});

        final value = db.readScalarFloatById(
          'Configuration',
          'float_attribute',
          1,
        );
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
        db.createElement('Configuration', {
          'label': 'Config 1',
          'float_attribute': 1.0,
        });

        db.updateElement('Configuration', 1, {
          'float_attribute': 1.23456789012345,
        });

        final value = db.readScalarFloatById(
          'Configuration',
          'float_attribute',
          1,
        );
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
        db.createElement('Configuration', {
          'label': 'Config 1',
          'string_attribute': 'hello',
        });

        db.updateElement('Configuration', 1, {'string_attribute': 'world'});

        final value = db.readScalarStringById(
          'Configuration',
          'string_attribute',
          1,
        );
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
        db.createElement('Configuration', {
          'label': 'Config 1',
          'string_attribute': 'hello',
        });

        db.updateElement('Configuration', 1, {'string_attribute': ''});

        final value = db.readScalarStringById(
          'Configuration',
          'string_attribute',
          1,
        );
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
        db.createElement('Configuration', {
          'label': 'Config 1',
          'string_attribute': 'hello',
        });

        db.updateElement('Configuration', 1, {'string_attribute': '日本語テスト'});

        final value = db.readScalarStringById(
          'Configuration',
          'string_attribute',
          1,
        );
        expect(value, equals('日本語テスト'));
      } finally {
        db.close();
      }
    });
  });

  group('Update DateTime Attributes', () {
    test('updates date time value', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {
          'label': 'Config 1',
          'date_attribute': '2024-01-01T00:00:00',
        });

        db.updateElement('Configuration', 1, {
          'date_attribute': '2025-12-31T23:59:59',
        });

        final date = db.readScalarStringById(
          'Configuration',
          'date_attribute',
          1,
        );
        expect(date, equals('2025-12-31T23:59:59'));
      } finally {
        db.close();
      }
    });

    test('updates with native DateTime object via Element builder', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {
          'label': 'Config 1',
          'date_attribute': '2024-01-01T00:00:00',
        });

        final element = Element();
        try {
          element.set('date_attribute', DateTime(2025, 6, 15, 12, 30, 45));
          db.updateElementFromBuilder('Configuration', 1, element);
        } finally {
          element.dispose();
        }

        // Verify it was stored correctly
        final dateStr = db.readScalarStringById(
          'Configuration',
          'date_attribute',
          1,
        );
        expect(dateStr, equals('2025-06-15T12:30:45'));

        // Verify readAllScalarsById returns native DateTime
        final scalars = db.readAllScalarsById('Configuration', 1);
        expect(scalars['date_attribute'], isA<DateTime>());
        expect(
          scalars['date_attribute'],
          equals(DateTime(2025, 6, 15, 12, 30, 45)),
        );
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

        db.updateElement('Collection', 1, {
          'value_int': [10, 20, 30, 40],
        });

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

        db.updateElement('Collection', 1, {
          'value_int': [100],
        });

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

        db.updateElement('Collection', 1, {'value_int': <int>[]});

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
        expect(
          db.readVectorIntegersById('Collection', 'value_int', 1),
          isEmpty,
        );

        db.updateElement('Collection', 1, {
          'value_int': [1, 2, 3],
        });

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

        db.updateElement('Collection', 1, {
          'value_int': [100, 200],
        });

        expect(
          db.readVectorIntegersById('Collection', 'value_int', 1),
          equals([100, 200]),
        );
        expect(
          db.readVectorIntegersById('Collection', 'value_int', 2),
          equals([10, 20]),
        );
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
          () => db.updateElement('NonexistentCollection', 1, {
            'value_int': [1, 2, 3],
          }),
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

        db.updateElement('Collection', 1, {
          'value_float': [10.5, 20.5],
        });

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

        db.updateElement('Collection', 1, {
          'value_float': [1.23456789, 9.87654321],
        });

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

        db.updateElement('Collection', 1, {'value_float': <double>[]});

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

        db.updateElement('Collection', 1, {
          'tag': ['new_tag1', 'new_tag2', 'new_tag3'],
        });

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

        db.updateElement('Collection', 1, {
          'tag': ['single_tag'],
        });

        final values = db.readSetStringsById('Collection', 'tag', 1);
        expect(values, equals(['single_tag']));
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

        db.updateElement('Collection', 1, {
          'tag': ['important', 'urgent'],
        });

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

        db.updateElement('Collection', 1, {
          'tag': ['updated'],
        });

        expect(
          db.readSetStringsById('Collection', 'tag', 1),
          equals(['updated']),
        );
        expect(
          db.readSetStringsById('Collection', 'tag', 2).toSet(),
          equals({'urgent', 'review'}),
        );
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

        db.updateElement('Collection', 1, {
          'tag': ['日本語', '中文', '한국어'],
        });

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
          () => db.updateElement('NonexistentCollection', 1, {
            'tag': ['tag1'],
          }),
          throwsA(isA<DatabaseException>()),
        );
      } finally {
        db.close();
      }
    });
  });

  // ==========================================================================
  // Gap-fill: String vector, integer set, float set updates (using all_types.sql)
  // ==========================================================================

  group('Update Vector Strings', () {
    test('updates vector strings', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'all_types.sql'),
      );
      try {
        db.createElement('AllTypes', {'label': 'Item 1'});

        db.updateElement('AllTypes', 1, {
          'label_value': ['alpha', 'beta'],
        });

        final result = db.readVectorStringsById('AllTypes', 'label_value', 1);
        expect(result, equals(['alpha', 'beta']));
      } finally {
        db.close();
      }
    });
  });

  group('Update Set Integers', () {
    test('updates set integers', () {
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
  });

  group('Update Set Floats', () {
    test('updates set floats', () {
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
  });

  // ==========================================================================
  // FK Resolution - Update tests
  // ==========================================================================

  group('FK Resolution - Update', () {
    test('resolves scalar FK labels to IDs', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'relations.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        db.createElement('Parent', {'label': 'Parent 1'});
        db.createElement('Parent', {'label': 'Parent 2'});
        db.createElement('Child', {
          'label': 'Child 1',
          'parent_id': 'Parent 1',
        });

        // Update child: change parent_id to Parent 2 using string label
        db.updateElement('Child', 1, {'parent_id': 'Parent 2'});

        // Verify: parent_id resolved to Parent 2's ID (2)
        final parentIds = db.readScalarIntegers('Child', 'parent_id');
        expect(parentIds, equals([2]));
      } finally {
        db.close();
      }
    });

    test('updates scalar FK with integer ID directly', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'relations.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        db.createElement('Parent', {'label': 'Parent 1'});
        db.createElement('Parent', {'label': 'Parent 2'});
        db.createElement('Child', {'label': 'Child 1', 'parent_id': 1});

        // Update child: change parent_id to 2 using integer ID directly
        db.updateElement('Child', 1, {'parent_id': 2});

        // Verify: parent_id updated to 2
        final parentIds = db.readScalarIntegers('Child', 'parent_id');
        expect(parentIds, equals([2]));
      } finally {
        db.close();
      }
    });

    test('resolves vector FK labels to IDs', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'relations.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        db.createElement('Parent', {'label': 'Parent 1'});
        db.createElement('Parent', {'label': 'Parent 2'});
        db.createElement('Child', {
          'label': 'Child 1',
          'parent_ref': ['Parent 1'],
        });

        // Update child: change vector FK to [Parent 2, Parent 1]
        db.updateElement('Child', 1, {
          'parent_ref': ['Parent 2', 'Parent 1'],
        });

        // Verify: vector resolved to [2, 1] (order preserved)
        final refs = db.readVectorIntegersById('Child', 'parent_ref', 1);
        expect(refs, equals([2, 1]));
      } finally {
        db.close();
      }
    });

    test('resolves set FK labels to IDs', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'relations.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        db.createElement('Parent', {'label': 'Parent 1'});
        db.createElement('Parent', {'label': 'Parent 2'});
        db.createElement('Child', {
          'label': 'Child 1',
          'mentor_id': ['Parent 1'],
        });

        // Update child: change set FK to [Parent 2]
        db.updateElement('Child', 1, {
          'mentor_id': ['Parent 2'],
        });

        // Verify: set resolved to [2]
        final mentors = db.readSetIntegersById('Child', 'mentor_id', 1);
        expect(mentors, equals([2]));
      } finally {
        db.close();
      }
    });

    test('resolves time series FK labels to IDs', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'relations.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        db.createElement('Parent', {'label': 'Parent 1'});
        db.createElement('Parent', {'label': 'Parent 2'});
        db.createElement('Child', {
          'label': 'Child 1',
          'date_time': ['2024-01-01'],
          'sponsor_id': ['Parent 1'],
        });

        // Update child: change time series FK to [Parent 2, Parent 1]
        db.updateElement('Child', 1, {
          'date_time': ['2024-06-01', '2024-06-02'],
          'sponsor_id': ['Parent 2', 'Parent 1'],
        });

        // Verify: time series resolved to [2, 1]
        final result = db.readTimeSeriesGroup('Child', 'events', 1);
        expect(result['sponsor_id'], equals([2, 1]));
        expect(result['date_time']!.length, equals(2));
      } finally {
        db.close();
      }
    });

    test('resolves all FK types in one call', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'relations.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        db.createElement('Parent', {'label': 'Parent 1'});
        db.createElement('Parent', {'label': 'Parent 2'});

        // Create child with all FK types pointing to Parent 1
        db.createElement('Child', {
          'label': 'Child 1',
          'parent_id': 'Parent 1',
          'mentor_id': ['Parent 1'],
          'parent_ref': ['Parent 1'],
          'date_time': ['2024-01-01'],
          'sponsor_id': ['Parent 1'],
        });

        // Update child: change all FK types to Parent 2
        db.updateElement('Child', 1, {
          'parent_id': 'Parent 2',
          'mentor_id': ['Parent 2'],
          'parent_ref': ['Parent 2'],
          'date_time': ['2025-01-01'],
          'sponsor_id': ['Parent 2'],
        });

        // Verify scalar FK
        final parentIds = db.readScalarIntegers('Child', 'parent_id');
        expect(parentIds, equals([2]));

        // Verify set FK
        final mentors = db.readSetIntegersById('Child', 'mentor_id', 1);
        expect(mentors, equals([2]));

        // Verify vector FK
        final refs = db.readVectorIntegersById('Child', 'parent_ref', 1);
        expect(refs, equals([2]));

        // Verify time series FK
        final ts = db.readTimeSeriesGroup('Child', 'events', 1);
        expect(ts['sponsor_id'], equals([2]));
      } finally {
        db.close();
      }
    });

    test('non-FK integer columns pass through unchanged', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {
          'label': 'Config 1',
          'integer_attribute': 42,
          'float_attribute': 3.14,
          'string_attribute': 'hello',
        });

        // Update scalar attributes in a non-FK schema
        db.updateElement('Configuration', 1, {
          'integer_attribute': 100,
          'float_attribute': 2.71,
          'string_attribute': 'world',
        });

        // Verify values updated correctly (pre-resolve passthrough safe for non-FK schemas)
        expect(
          db.readScalarIntegerById('Configuration', 'integer_attribute', 1),
          equals(100),
        );
        expect(
          db.readScalarFloatById('Configuration', 'float_attribute', 1),
          equals(2.71),
        );
        expect(
          db.readScalarStringById('Configuration', 'string_attribute', 1),
          equals('world'),
        );
      } finally {
        db.close();
      }
    });

    test('failure preserves existing data', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'relations.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        db.createElement('Parent', {'label': 'Parent 1'});
        db.createElement('Child', {
          'label': 'Child 1',
          'parent_id': 'Parent 1',
        });

        // Attempt update with nonexistent FK label
        expect(
          () => db.updateElement('Child', 1, {'parent_id': 'Nonexistent Parent'}),
          throwsA(isA<DatabaseException>()),
        );

        // Verify original value preserved
        final parentIds = db.readScalarIntegers('Child', 'parent_id');
        expect(parentIds, equals([1]));
      } finally {
        db.close();
      }
    });
  });

  group('Update Trims Whitespace', () {
    test('trims scalar string on update', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'basic.sql'),
      );
      try {
        db.createElement('Configuration', {
          'label': 'Config 1',
          'string_attribute': 'hello',
        });

        db.updateElement('Configuration', 1, {'string_attribute': '  world  '});

        final value = db.readScalarStringById(
          'Configuration',
          'string_attribute',
          1,
        );
        expect(value, equals('world'));
      } finally {
        db.close();
      }
    });

    test('trims set strings on update', () {
      final db = Database.fromSchema(
        ':memory:',
        path.join(testsPath, 'schemas', 'valid', 'collections.sql'),
      );
      try {
        db.createElement('Configuration', {'label': 'Test Config'});
        db.createElement('Collection', {
          'label': 'Item 1',
          'tag': ['old'],
        });

        db.updateElement('Collection', 1, {
          'tag': ['  alpha  ', '\tbeta\n', ' gamma '],
        });

        final values = db.readSetStringsById('Collection', 'tag', 1);
        final sorted = List<String>.from(values)..sort();
        expect(sorted, equals(['alpha', 'beta', 'gamma']));
      } finally {
        db.close();
      }
    });
  });
}
